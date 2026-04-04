[CmdletBinding()]
param(
    [int]$TimeoutMs = 0,

    [string]$Label = 'build',

    [string]$LogRoot = '',

    [switch]$SerializeByEngine,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExtraArgs = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'Shared\UnrealCommandUtils.ps1')

$exitCodes = @{
    Success      = 0
    BuildFailed  = 1
    TimedOut     = 2
    ConfigError  = 3
    WorktreeBusy = 4
    EngineBusy   = 5
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$worktreeMutex = $null
$engineMutex = $null
$metadataPath = $null
$scriptExitCode = $exitCodes.ConfigError

try {
    $agentConfig = Resolve-AgentConfiguration -ProjectRoot $projectRoot

    $defaultTimeoutMs = $agentConfig.BuildDefaultTimeoutMs
    $resolvedTimeoutMs = Resolve-TimeoutMs -RequestedTimeoutMs $TimeoutMs -DefaultTimeoutMs $defaultTimeoutMs -ParameterName 'TimeoutMs'
    $ubtPaths = Resolve-UbtPaths -EngineRoot $agentConfig.EngineRoot
    $outputLayout = New-CommandOutputLayout -ProjectRoot $projectRoot -Category 'Build' -Label $Label -RequestedOutputRoot $LogRoot -LogFileName 'Build.log'
    $metadataPath = Join-Path $outputLayout.OutputRoot 'RunMetadata.json'
    $ubtLogPath = Join-Path $outputLayout.OutputRoot 'UnrealBuildTool.log'

    $worktreeMutexName = Get-NamedMutexName -Scope 'ue-command-worktree' -KeyPath $projectRoot
    $worktreeMutex = Acquire-NamedMutex -Name $worktreeMutexName -TimeoutMs 0
    if ($null -eq $worktreeMutex) {
        Write-Host '[error] Another build or test command is already running for this worktree.' -ForegroundColor Red
        $scriptExitCode = $exitCodes.WorktreeBusy
        return
    }

    if ($SerializeByEngine) {
        $engineMutexName = Get-NamedMutexName -Scope 'ue-build-engine' -KeyPath $agentConfig.EngineRoot
        Write-Host ('Serialized engine mode enabled for: {0}' -f $agentConfig.EngineRoot)
        $engineMutex = Acquire-NamedMutex -Name $engineMutexName -TimeoutMs $resolvedTimeoutMs -PollIntervalMs 5000 -OnWait {
            param($ElapsedMs, $RemainingMs)
            Write-Host ("[wait] Engine build slot busy. elapsed={0}ms remaining={1}ms" -f $ElapsedMs, $RemainingMs)
        }

        if ($null -eq $engineMutex) {
            Write-Host '[error] Failed to acquire the engine build slot within the requested timeout.' -ForegroundColor Red
            $scriptExitCode = $exitCodes.EngineBusy
            return
        }
    }

    $buildMode = if ($SerializeByEngine) { 'SerializedEngine' } else { 'ConcurrentNoEngineChanges' }
    $ubtLogPath = Join-Path $outputLayout.OutputRoot 'UBT.log'
    $argumentList = @(
        $ubtPaths.UbtDllPath
        $agentConfig.EditorTarget
        $agentConfig.Platform
        $agentConfig.Configuration
        "-Project=$($agentConfig.ProjectFile)"
        "-architecture=$($agentConfig.Architecture)"
        "-Log=$ubtLogPath"
        '-NoMutex'
    )

    if (-not $SerializeByEngine) {
        $argumentList += '-NoEngineChanges'
    }

    if ($ExtraArgs.Count -gt 0) {
        $argumentList += $ExtraArgs
    }

    Write-Utf8JsonFile -Path $metadataPath -Value ([PSCustomObject]@{
            Label             = $Label
            Mode              = $buildMode
            ProjectRoot       = $projectRoot
            ProjectFile       = $agentConfig.ProjectFile
            EngineRoot        = $agentConfig.EngineRoot
            DotNetExecutable  = $ubtPaths.DotNetExecutablePath
            UbtDllPath        = $ubtPaths.UbtDllPath
            WorkingDirectory  = $ubtPaths.WorkingDirectory
            TimeoutMs         = $resolvedTimeoutMs
            OutputRoot        = $outputLayout.OutputRoot
            LogPath           = $outputLayout.LogPath
            UbtLogPath        = $ubtLogPath
            Arguments         = $argumentList
            TimedOut          = $false
            ProcessExitCode   = $null
            ExitCode          = $null
        })

    Write-Host '================================================================'
    Write-Host 'Angelscript UBT Build Runner'
    Write-Host '================================================================'
    Write-Host ('Mode            : {0}' -f $buildMode)
    Write-Host ('Target          : {0}' -f $agentConfig.EditorTarget)
    Write-Host ('Platform        : {0}' -f $agentConfig.Platform)
    Write-Host ('Configuration   : {0}' -f $agentConfig.Configuration)
    Write-Host ('ProjectFile     : {0}' -f $agentConfig.ProjectFile)
    Write-Host ('EngineRoot      : {0}' -f $agentConfig.EngineRoot)
    Write-Host ('DotNet          : {0} ({1})' -f $ubtPaths.DotNetExecutablePath, $ubtPaths.DotNetSource)
    Write-Host ('TimeoutMs       : {0}' -f $resolvedTimeoutMs)
    Write-Host ('LogPath         : {0}' -f $outputLayout.LogPath)
    Write-Host ('UBT LogPath     : {0}' -f $ubtLogPath)
    if (-not $SerializeByEngine) {
        Write-Host 'Guard           : -NoMutex -NoEngineChanges'
        Write-Host 'Hint            : rerun with -SerializeByEngine if engine outputs must change.'
    }
    Write-Host '----------------------------------------------------------------'

    $result = Invoke-StreamingProcess `
        -FilePath $ubtPaths.DotNetExecutablePath `
        -ArgumentList $argumentList `
        -WorkingDirectory $ubtPaths.WorkingDirectory `
        -TimeoutMs $resolvedTimeoutMs `
        -LogPath $outputLayout.LogPath `
        -Label 'ubt-build' `
        -Environment $ubtPaths.Environment

    $scriptExitCode = if ($result.TimedOut) {
        $exitCodes.TimedOut
    }
    elseif ([int]$result.ExitCode -eq 0) {
        $exitCodes.Success
    }
    else {
        $exitCodes.BuildFailed
    }

    if ($scriptExitCode -eq $exitCodes.Success -and (Test-Path -LiteralPath $outputLayout.LogPath -PathType Leaf)) {
        $buildLog = Get-Content -LiteralPath $outputLayout.LogPath -Raw -Encoding UTF8
        if ($buildLog -match '(?m)^Result:\s+Failed\b') {
            Write-Host '[warn] Build log reports failure despite a zero process exit code. Promoting final exit code to 1.' -ForegroundColor Yellow
            $scriptExitCode = $exitCodes.BuildFailed
        }
    }

    Write-Utf8JsonFile -Path $metadataPath -Value ([PSCustomObject]@{
            Label             = $Label
            Mode              = $buildMode
            ProjectRoot       = $projectRoot
            ProjectFile       = $agentConfig.ProjectFile
            EngineRoot        = $agentConfig.EngineRoot
            DotNetExecutable  = $ubtPaths.DotNetExecutablePath
            UbtDllPath        = $ubtPaths.UbtDllPath
            WorkingDirectory  = $ubtPaths.WorkingDirectory
            TimeoutMs         = $resolvedTimeoutMs
            OutputRoot        = $outputLayout.OutputRoot
            LogPath           = $outputLayout.LogPath
            UbtLogPath        = $ubtLogPath
            Arguments         = $argumentList
            TimedOut          = [bool]$result.TimedOut
            ProcessExitCode   = [int]$result.ExitCode
            ExitCode          = $scriptExitCode
            DurationMs        = [int]$result.DurationMs
        })

    Write-Host '----------------------------------------------------------------'
    Write-Host ('ProcessExitCode : {0}' -f $result.ExitCode)
    Write-Host ('FinalExitCode   : {0}' -f $scriptExitCode)
    Write-Host ('DurationMs      : {0}' -f $result.DurationMs)
    Write-Host ('MetadataPath    : {0}' -f $metadataPath)
    if ($scriptExitCode -eq $exitCodes.BuildFailed -and -not $SerializeByEngine) {
        Write-Host 'If the failure is due to engine output changes, rerun with -SerializeByEngine.'
    }
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
    if ($null -ne $engineMutex) {
        Release-NamedMutex -Mutex $engineMutex
    }

    if ($null -ne $worktreeMutex) {
        Release-NamedMutex -Mutex $worktreeMutex
    }
}

exit $scriptExitCode
