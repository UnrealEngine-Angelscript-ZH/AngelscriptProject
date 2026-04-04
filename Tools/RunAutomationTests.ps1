[CmdletBinding(DefaultParameterSetName = 'Group')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Group')]
    [string]$Group,

    [Parameter(Mandatory = $true, ParameterSetName = 'Prefix')]
    [Alias('TestFilter', 'TestTarget')]
    [string]$Prefix,

    [string]$BucketName,

    [string]$AbsLog,

    [string]$ReportExportPath,

    [int]$TimeoutMs,

    [switch]$AllowGraphics,

    [switch]$KeepSound,

    [switch]$SkipSummary,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExtraArgs = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-ProjectRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
}

function Read-IniFile {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "AgentConfig file was not found: $Path"
    }

    $result = @{}
    $section = ''

    foreach ($line in Get-Content -LiteralPath $Path -Encoding UTF8) {
        $trimmed = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith(';') -or $trimmed.StartsWith('#')) {
            continue
        }

        if ($trimmed.StartsWith('[') -and $trimmed.EndsWith(']')) {
            $section = $trimmed.Substring(1, $trimmed.Length - 2)
            if (-not $result.ContainsKey($section)) {
                $result[$section] = @{}
            }
            continue
        }

        $separatorIndex = $trimmed.IndexOf('=')
        if ($separatorIndex -lt 0) {
            continue
        }

        $key = $trimmed.Substring(0, $separatorIndex).Trim()
        $value = $trimmed.Substring($separatorIndex + 1).Trim()
        if (-not $result.ContainsKey($section)) {
            $result[$section] = @{}
        }
        $result[$section][$key] = $value
    }

    return $result
}

function Get-IniValue {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Config,

        [Parameter(Mandatory = $true)]
        [string]$Section,

        [Parameter(Mandatory = $true)]
        [string]$Key,

        [string]$DefaultValue,

        [switch]$Required
    )

    $sectionValues = $Config[$Section]
    if ($null -ne $sectionValues -and $sectionValues.ContainsKey($Key)) {
        $value = [string]$sectionValues[$Key]
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            return $value
        }
    }

    if ($Required) {
        throw "Missing required AgentConfig.ini value [$Section] $Key"
    }

    return $DefaultValue
}

function Resolve-ProjectFile {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Config,

        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot
    )

    $projectFile = Get-IniValue -Config $Config -Section 'Paths' -Key 'ProjectFile'
    if (-not [string]::IsNullOrWhiteSpace($projectFile)) {
        return $projectFile
    }

    $uproject = Get-ChildItem -LiteralPath $ProjectRoot -Filter *.uproject -File | Select-Object -First 1
    if ($null -eq $uproject) {
        throw 'Could not resolve a .uproject file from repo root and AgentConfig.ini Paths.ProjectFile is empty.'
    }

    return $uproject.FullName
}

function Get-DefinedAutomationGroups {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)

    $configPath = Join-Path $ProjectRoot 'Config/DefaultEngine.ini'
    if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
        return @()
    }

    $groups = @()
    foreach ($line in Get-Content -LiteralPath $configPath -Encoding UTF8) {
        if ($line -match '\+Groups=\(Name="([^"]+)"') {
            $groups += $matches[1]
        }
    }

    return @($groups | Select-Object -Unique)
}

function Get-BucketSlug {
    param([Parameter(Mandatory = $true)][string]$Value)

    $slug = $Value -replace '^Group:', '' -replace '[^A-Za-z0-9._-]', '-'
    $slug = $slug -replace '\.+', '.'
    $slug = $slug.Trim('-')
    if ([string]::IsNullOrWhiteSpace($slug)) {
        return 'Automation'
    }

    return $slug
}

function Write-Utf8JsonFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [object]$Value,

        [int]$Depth = 6
    )

    $directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($directory)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }

    $json = $Value | ConvertTo-Json -Depth $Depth
    $utf8 = New-Object System.Text.UTF8Encoding($true)
    [System.IO.File]::WriteAllText($Path, $json, $utf8)
}

function Get-BlockingLogHintCount {
    param([Parameter(Mandatory = $true)][string]$LogPath)

    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        return 0
    }

    $count = 0
    foreach ($line in Get-Content -LiteralPath $LogPath -Encoding UTF8) {
        $hasBlockingMessage = $line -match '无法被找到|could not be found|No automation tests matched|Fatal error:'
        $hasBlockingError = $false
        if ($hasBlockingMessage -or $hasBlockingError) {
            $count++
        }
    }

    return $count
}

function New-OutputLayout {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$BucketSlug,
        [string]$RequestedLogPath,
        [string]$RequestedReportPath
    )

    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $defaultRoot = Join-Path $ProjectRoot (Join-Path 'Saved/Automation' (Join-Path $BucketSlug $timestamp))
    $outputRoot = if ([string]::IsNullOrWhiteSpace($RequestedLogPath) -and [string]::IsNullOrWhiteSpace($RequestedReportPath)) {
        $defaultRoot
    }
    else {
        $defaultRoot
    }

    $resolvedLogPath = if ([string]::IsNullOrWhiteSpace($RequestedLogPath)) {
        Join-Path $outputRoot 'Automation.log'
    }
    else {
        $RequestedLogPath
    }

    $resolvedReportPath = if ([string]::IsNullOrWhiteSpace($RequestedReportPath)) {
        Join-Path $outputRoot 'Report'
    }
    else {
        $RequestedReportPath
    }

    New-Item -ItemType Directory -Path (Split-Path -Parent $resolvedLogPath) -Force | Out-Null

    $reportParent = if ([IO.Path]::GetExtension($resolvedReportPath)) {
        Split-Path -Parent $resolvedReportPath
    }
    else {
        $resolvedReportPath
    }

    if (-not [string]::IsNullOrWhiteSpace($reportParent)) {
        New-Item -ItemType Directory -Path $reportParent -Force | Out-Null
    }

    return [PSCustomObject]@{
        OutputRoot       = $outputRoot
        AbsLog           = $resolvedLogPath
        ReportExportPath = $resolvedReportPath
        Timestamp        = $timestamp
    }
}

$projectRoot = Get-ProjectRoot
$agentConfigPath = Join-Path $projectRoot 'AgentConfig.ini'
$config = Read-IniFile -Path $agentConfigPath

$engineRoot = Get-IniValue -Config $config -Section 'Paths' -Key 'EngineRoot' -Required
$projectFile = Resolve-ProjectFile -Config $config -ProjectRoot $projectRoot

if (-not $PSBoundParameters.ContainsKey('TimeoutMs')) {
    $TimeoutMs = [int](Get-IniValue -Config $config -Section 'Test' -Key 'DefaultTimeoutMs' -DefaultValue '600000')
}

$target = if ($PSCmdlet.ParameterSetName -eq 'Group') {
    "Group:$Group"
}
else {
    $Prefix
}

if ($PSCmdlet.ParameterSetName -eq 'Group') {
    $definedGroups = Get-DefinedAutomationGroups -ProjectRoot $projectRoot
    if ($definedGroups -notcontains $Group) {
        throw "Unknown automation group '$Group'. Defined groups: $($definedGroups -join ', ')"
    }
}

if ([string]::IsNullOrWhiteSpace($BucketName)) {
    $BucketName = Get-BucketSlug -Value $target
}

$outputLayout = New-OutputLayout -ProjectRoot $projectRoot -BucketSlug $BucketName -RequestedLogPath $AbsLog -RequestedReportPath $ReportExportPath
$editorCmd = Join-Path $engineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
    throw "UnrealEditor-Cmd.exe was not found: $editorCmd"
}

$argumentList = @(
    $projectFile,
    "-ExecCmds=Automation RunTests $target; Quit",
    '-TestExit=Automation Test Queue Empty',
    '-Unattended',
    '-NoPause',
    '-NoSplash',
    "-ABSLOG=$($outputLayout.AbsLog)",
    "-ReportExportPath=$($outputLayout.ReportExportPath)"
)

if (-not $AllowGraphics) {
    $argumentList += '-NullRHI'
}

if (-not $KeepSound) {
    $argumentList += '-NOSOUND'
}

if ($ExtraArgs.Count -gt 0) {
    $argumentList += $ExtraArgs
}

$metadataPath = Join-Path $outputLayout.OutputRoot 'RunMetadata.json'
$summaryPath = Join-Path $outputLayout.OutputRoot 'Summary.json'

$initialMetadata = [PSCustomObject]@{
    BucketName       = $BucketName
    Target           = $target
    ProjectFile      = $projectFile
    EngineRoot       = $engineRoot
    EditorCmd        = $editorCmd
    TimeoutMs        = $TimeoutMs
    OutputRoot       = $outputLayout.OutputRoot
    AbsLog           = $outputLayout.AbsLog
    ReportExportPath = $outputLayout.ReportExportPath
    ExtraArgs        = $ExtraArgs
    ProcessExitCode  = $null
    ExitCode         = $null
    TimedOut         = $false
}

Write-Utf8JsonFile -Path $metadataPath -Value $initialMetadata -Depth 6

Write-Host ("Bucket: {0}" -f $BucketName)
Write-Host ("Target: {0}" -f $target)
Write-Host ("LogPath: {0}" -f $outputLayout.AbsLog)
Write-Host ("ReportPath: {0}" -f $outputLayout.ReportExportPath)
Write-Host ("TimeoutMs: {0}" -f $TimeoutMs)

$process = Start-Process -FilePath $editorCmd -ArgumentList $argumentList -PassThru -NoNewWindow
$timedOut = -not $process.WaitForExit($TimeoutMs)

if ($timedOut) {
    try {
        Stop-Process -Id $process.Id -Force -ErrorAction Stop
    }
    catch {
    }
}
else {
    $process.WaitForExit()
    $process.Refresh()
}

$processExitCode = if ($timedOut) { 124 } elseif ($null -ne $process.ExitCode) { [int]$process.ExitCode } else { 0 }
$finalExitCode = $processExitCode

if (-not $SkipSummary) {
    & (Join-Path $PSScriptRoot 'GetAutomationReportSummary.ps1') -ReportPath $outputLayout.ReportExportPath -LogPath $outputLayout.AbsLog -ExitCode $processExitCode -BucketName $BucketName -SummaryPath $summaryPath | Out-Null
    Write-Host ("SummaryPath: {0}" -f $summaryPath)

    if ($processExitCode -eq 0 -and (Test-Path -LiteralPath $summaryPath -PathType Leaf)) {
        $summaryJson = Get-Content -LiteralPath $summaryPath -Raw -Encoding UTF8 | ConvertFrom-Json
        $hasBlockingLogHints = @($summaryJson.LogFailureHints).Count -gt 0
        $hasFailedSummary = ($null -ne $summaryJson.Failed -and [int]$summaryJson.Failed -gt 0) -or (@($summaryJson.FailedTests).Count -gt 0)

        if ($hasBlockingLogHints -or $hasFailedSummary) {
            $finalExitCode = 1
            if ($hasFailedSummary) {
                Write-Host 'Summary.json reports failed tests despite a zero process exit code; promoting final exit code to 1.'
            }
            else {
                Write-Host 'Blocking log errors were detected despite a zero process exit code; promoting final exit code to 1.'
            }
        }
    }
}

$finalMetadata = [PSCustomObject]@{
    BucketName       = $BucketName
    Target           = $target
    ProjectFile      = $projectFile
    EngineRoot       = $engineRoot
    EditorCmd        = $editorCmd
    TimeoutMs        = $TimeoutMs
    OutputRoot       = $outputLayout.OutputRoot
    AbsLog           = $outputLayout.AbsLog
    ReportExportPath = $outputLayout.ReportExportPath
    ExtraArgs        = $ExtraArgs
    ProcessExitCode  = $processExitCode
    ExitCode         = $finalExitCode
    TimedOut         = $timedOut
}

Write-Utf8JsonFile -Path $metadataPath -Value $finalMetadata -Depth 6

Write-Host ("FinalExitCode: {0}" -f $finalExitCode)

exit $finalExitCode
