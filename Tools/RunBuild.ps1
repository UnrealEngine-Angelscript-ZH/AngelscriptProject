<#
.SYNOPSIS
    Run the configured Unreal Editor target build with an enforced timeout.

.PARAMETER TimeoutMs
    Build timeout in milliseconds. Default: Build.DefaultTimeoutMs -> Test.DefaultTimeoutMs -> 600000.
#>
param(
    [int]$TimeoutMs = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-IniMap {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "AgentConfig.ini not found at '$Path'. Run Tools\GenerateAgentConfigTemplate.bat first."
    }
    $result = @{}
    $currentSection = ""
    foreach ($line in Get-Content -LiteralPath $Path) {
        $trimmed = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith(";")) { continue }
        if ($trimmed.StartsWith("[") -and $trimmed.EndsWith("]")) {
            $currentSection = $trimmed.Substring(1, $trimmed.Length - 2)
            if (-not $result.ContainsKey($currentSection)) { $result[$currentSection] = @{} }
            continue
        }
        $sep = $trimmed.IndexOf("=")
        if ($sep -lt 0 -or [string]::IsNullOrWhiteSpace($currentSection)) { continue }
        $result[$currentSection][$trimmed.Substring(0, $sep).Trim()] = $trimmed.Substring($sep + 1).Trim()
    }
    return $result
}

function Get-IniValue {
    param([hashtable]$Ini, [string]$Section, [string]$Key, [string]$Default = "")
    if ($Ini.ContainsKey($Section) -and $Ini[$Section].ContainsKey($Key)) {
        $v = [string]$Ini[$Section][$Key]
        if (-not [string]::IsNullOrWhiteSpace($v)) { return $v }
    }
    return $Default
}

function Stop-ProcessTree {
    param([int]$ProcessId)
    try {
        Stop-Process -Id $ProcessId -Force -ErrorAction SilentlyContinue
    } catch {
    }
    try {
        & taskkill /PID $ProcessId /T /F 2>$null | Out-Null
    } catch {
    }
}

function Try-AppendTimeoutMarker {
    param([string]$Path, [string]$Message)
    try {
        $Message | Out-File -FilePath $Path -Append -Encoding utf8
    } catch {
    }
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$configPath = Join-Path $projectRoot "AgentConfig.ini"
$ini = Get-IniMap -Path $configPath

$engineRoot = Get-IniValue -Ini $ini -Section "Paths" -Key "EngineRoot"
if ([string]::IsNullOrWhiteSpace($engineRoot)) {
    throw "Paths.EngineRoot is required in '$configPath'."
}

$projectFile = Get-IniValue -Ini $ini -Section "Paths" -Key "ProjectFile" -Default (Join-Path $projectRoot "AngelscriptProject.uproject")
$editorTarget = Get-IniValue -Ini $ini -Section "Build" -Key "EditorTarget" -Default "AngelscriptProjectEditor"
$platform = Get-IniValue -Ini $ini -Section "Build" -Key "Platform" -Default "Win64"
$configuration = Get-IniValue -Ini $ini -Section "Build" -Key "Configuration" -Default "Development"
$architecture = Get-IniValue -Ini $ini -Section "Build" -Key "Architecture" -Default "x64"

if ($TimeoutMs -le 0) {
    $TimeoutMs = [int](Get-IniValue -Ini $ini -Section "Build" -Key "DefaultTimeoutMs" -Default (Get-IniValue -Ini $ini -Section "Test" -Key "DefaultTimeoutMs" -Default "600000"))
}

$buildBat = Join-Path $engineRoot "Engine\Build\BatchFiles\Build.bat"
if (-not (Test-Path -LiteralPath $buildBat)) {
    throw "Build.bat not found: $buildBat"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$outputDir = Join-Path $projectRoot "Saved\Build\${timestamp}_${editorTarget}"
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
$stdoutLog = Join-Path $outputDir "build.stdout.log"
$stderrLog = Join-Path $outputDir "build.stderr.log"

$argList = @(
    $editorTarget
    $platform
    $configuration
    "-Project=$projectFile"
    "-WaitMutex"
    "-FromMsBuild"
    "-architecture=$architecture"
)

Write-Host "================================================================"
Write-Host "  Angelscript Build Runner"
Write-Host "================================================================"
Write-Host "Target      : $editorTarget"
Write-Host "Project     : $projectFile"
Write-Host "TimeoutMs   : $TimeoutMs"
Write-Host "Stdout log  : $stdoutLog"
Write-Host "Stderr log  : $stderrLog"
Write-Host "----------------------------------------------------------------"

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$proc = Start-Process -FilePath $buildBat -ArgumentList $argList -PassThru -NoNewWindow -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog
$completed = $proc.WaitForExit($TimeoutMs)
if (-not $completed) {
    Stop-ProcessTree -ProcessId $proc.Id
    $sw.Stop()
    Try-AppendTimeoutMarker -Path $stderrLog -Message "[TIMEOUT] Build exceeded ${TimeoutMs}ms and was terminated."
    Write-Host "[ERROR] Build timed out after $TimeoutMs ms and was terminated."
    Write-Host "Stdout log  : $stdoutLog"
    Write-Host "Stderr log  : $stderrLog"
    exit 124
}

$proc.WaitForExit()
$sw.Stop()

Write-Host "Exit code   : $($proc.ExitCode)"
Write-Host "Elapsed     : $([math]::Round($sw.Elapsed.TotalSeconds, 1))s"
Write-Host "Stdout log  : $stdoutLog"
Write-Host "Stderr log  : $stderrLog"

if ($proc.ExitCode -ne 0) {
    exit $proc.ExitCode
}

exit 0
