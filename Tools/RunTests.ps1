[CmdletBinding(DefaultParameterSetName = 'Prefix')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Prefix')]
    [Alias('Prefix', 'TestFilter', 'TestTarget')]
    [string]$TestPrefix,

    [Parameter(Mandatory = $true, ParameterSetName = 'Group')]
    [string]$Group,

    [string]$Label = '',

    [string]$OutputRoot = '',

    [int]$TimeoutMs = 0,

    [switch]$Render,

    [switch]$NoReport,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExtraArgs = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'Shared\UnrealCommandUtils.ps1')

$exitCodes = @{
    Success      = 0
    TestFailed   = 1
    TimedOut     = 2
    ConfigError  = 3
    WorktreeBusy = 4
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$worktreeMutex = $null
$metadataPath = $null
$scriptExitCode = $exitCodes.ConfigError

try {
    $agentConfig = Resolve-AgentConfiguration -ProjectRoot $projectRoot

    $defaultTimeoutMs = $agentConfig.TestDefaultTimeoutMs
    if ($defaultTimeoutMs -gt 300000) {
        Write-Host ("[warn] Test.DefaultTimeoutMs={0} exceeds the 300000ms hard cap. Falling back to 300000ms." -f $defaultTimeoutMs) -ForegroundColor Yellow
        $defaultTimeoutMs = 300000
    }

    $resolvedTimeoutMs = Resolve-TimeoutMs -RequestedTimeoutMs $TimeoutMs -DefaultTimeoutMs $defaultTimeoutMs -ParameterName 'TimeoutMs'
    $editorCmd = Join-Path $agentConfig.EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
        throw "UnrealEditor-Cmd.exe was not found: $editorCmd"
    }

    $target = if ($PSCmdlet.ParameterSetName -eq 'Group') {
        $definedGroups = @(Get-DefinedAutomationGroups -ProjectRoot $projectRoot)
        if ($definedGroups -notcontains $Group) {
            throw "Unknown automation group '$Group'. Defined groups: $($definedGroups -join ', ')"
        }

        "Group:$Group"
    }
    else {
        $TestPrefix
    }

    if ([string]::IsNullOrWhiteSpace($Label)) {
        $Label = $target
    }

    $outputLayout = New-CommandOutputLayout -ProjectRoot $projectRoot -Category 'Tests' -Label $Label -RequestedOutputRoot $OutputRoot -LogFileName 'Automation.log'
    $metadataPath = Join-Path $outputLayout.OutputRoot 'RunMetadata.json'
    $summaryPath = Join-Path $outputLayout.OutputRoot 'Summary.json'

    $worktreeMutexName = Get-NamedMutexName -Scope 'ue-command-worktree' -KeyPath $projectRoot
    $worktreeMutex = Acquire-NamedMutex -Name $worktreeMutexName -TimeoutMs 0
    if ($null -eq $worktreeMutex) {
        Write-Host '[error] Another build or test command is already running for this worktree.' -ForegroundColor Red
        $scriptExitCode = $exitCodes.WorktreeBusy
        return
    }

    $argumentList = @(
        $agentConfig.ProjectFile
        "-ExecCmds=Automation RunTests $target; Quit"
        '-TestExit=Automation Test Queue Empty'
        '-Unattended'
        '-NoPause'
        '-NoSplash'
        '-stdout'
        '-FullStdOutLogOutput'
        '-UTF8Output'
        "-ABSLOG=$($outputLayout.LogPath)"
        "-ReportExportPath=$($outputLayout.ReportPath)"
        '-NOSOUND'
    )

    if (-not $Render) {
        $argumentList += '-NullRHI'
    }

    if ($ExtraArgs.Count -gt 0) {
        $argumentList += $ExtraArgs
    }

    Write-Utf8JsonFile -Path $metadataPath -Value ([PSCustomObject]@{
            Label             = $Label
            Target            = $target
            ProjectRoot       = $projectRoot
            ProjectFile       = $agentConfig.ProjectFile
            EngineRoot        = $agentConfig.EngineRoot
            EditorCmd         = $editorCmd
            TimeoutMs         = $resolvedTimeoutMs
            OutputRoot        = $outputLayout.OutputRoot
            LogPath           = $outputLayout.LogPath
            ReportPath        = $outputLayout.ReportPath
            SummaryPath       = $summaryPath
            Arguments         = $argumentList
            TimedOut          = $false
            ProcessExitCode   = $null
            ExitCode          = $null
        })

    Write-Host '================================================================'
    Write-Host 'Angelscript Automation Test Runner'
    Write-Host '================================================================'
    Write-Host ('Target          : {0}' -f $target)
    Write-Host ('ProjectFile     : {0}' -f $agentConfig.ProjectFile)
    Write-Host ('EditorCmd       : {0}' -f $editorCmd)
    Write-Host ('TimeoutMs       : {0}' -f $resolvedTimeoutMs)
    Write-Host ('LogPath         : {0}' -f $outputLayout.LogPath)
    Write-Host ('ReportPath      : {0}' -f $outputLayout.ReportPath)
    Write-Host ('Render          : {0}' -f ([bool]$Render))
    Write-Host '----------------------------------------------------------------'

    $result = Invoke-StreamingProcess `
        -FilePath $editorCmd `
        -ArgumentList $argumentList `
        -WorkingDirectory $projectRoot `
        -TimeoutMs $resolvedTimeoutMs `
        -LogPath $outputLayout.LogPath `
        -Label 'automation-tests'

    $processExitCode = [int]$result.ExitCode
    $scriptExitCode = if ($result.TimedOut) {
        $exitCodes.TimedOut
    }
    elseif ($processExitCode -eq 0) {
        $exitCodes.Success
    }
    else {
        $exitCodes.TestFailed
    }

    if (-not $NoReport) {
        $summaryObject = & (Join-Path $PSScriptRoot 'GetAutomationReportSummary.ps1') `
            -ReportPath $outputLayout.ReportPath `
            -LogPath $outputLayout.LogPath `
            -ExitCode $processExitCode `
            -BucketName $target `
            -SummaryPath $summaryPath

        $summaryRecord = @($summaryObject | Select-Object -Last 1)[0]
        if ($processExitCode -eq 0 -and $null -ne $summaryRecord) {
            $failedCount = if ($null -ne $summaryRecord.Failed) { [int]$summaryRecord.Failed } else { 0 }
            $failedTests = @($summaryRecord.FailedTests)
            $logHints = @($summaryRecord.LogFailureHints)
            if ($failedCount -gt 0 -or $failedTests.Count -gt 0 -or $logHints.Count -gt 0) {
                Write-Host '[warn] Structured report indicates failures despite a zero process exit code. Promoting final exit code to 1.' -ForegroundColor Yellow
                $scriptExitCode = $exitCodes.TestFailed
            }
        }
    }

    Write-Utf8JsonFile -Path $metadataPath -Value ([PSCustomObject]@{
            Label             = $Label
            Target            = $target
            ProjectRoot       = $projectRoot
            ProjectFile       = $agentConfig.ProjectFile
            EngineRoot        = $agentConfig.EngineRoot
            EditorCmd         = $editorCmd
            TimeoutMs         = $resolvedTimeoutMs
            OutputRoot        = $outputLayout.OutputRoot
            LogPath           = $outputLayout.LogPath
            ReportPath        = $outputLayout.ReportPath
            SummaryPath       = if ($NoReport) { $null } else { $summaryPath }
            Arguments         = $argumentList
            TimedOut          = [bool]$result.TimedOut
            ProcessExitCode   = $processExitCode
            ExitCode          = $scriptExitCode
            DurationMs        = [int]$result.DurationMs
        })

    Write-Host '----------------------------------------------------------------'
    Write-Host ('ProcessExitCode : {0}' -f $processExitCode)
    Write-Host ('FinalExitCode   : {0}' -f $scriptExitCode)
    Write-Host ('DurationMs      : {0}' -f $result.DurationMs)
    Write-Host ('MetadataPath    : {0}' -f $metadataPath)
}
catch {
    Write-Host ("[error] {0}" -f $_.Exception.Message) -ForegroundColor Red

    if (-not [string]::IsNullOrWhiteSpace($metadataPath)) {
        Write-Utf8JsonFile -Path $metadataPath -Value ([PSCustomObject]@{
                Label       = $Label
                ProjectRoot = $projectRoot
                Message     = $_.Exception.Message
                ExitCode    = $exitCodes.ConfigError
            })
    }

    $scriptExitCode = $exitCodes.ConfigError
}
finally {
    if ($null -ne $worktreeMutex) {
        Release-NamedMutex -Mutex $worktreeMutex
    }
}

exit $scriptExitCode
