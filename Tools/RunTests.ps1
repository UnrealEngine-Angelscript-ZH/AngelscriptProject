<#
.SYNOPSIS
    Run Angelscript automation tests via UnrealEditor-Cmd and produce a structured summary.

.PARAMETER TestPrefix
    UE automation test filter string. Default: "Angelscript" (runs all Angelscript tests).

.PARAMETER Label
    Human-readable label for this run (used in output directory name).
    Default: derived from TestPrefix.

.PARAMETER OutputRoot
    Root directory for test outputs. Default: <ProjectRoot>/Saved/Automation.

.PARAMETER NoReport
    Skip -ReportExportPath (JSON report generation).

.EXAMPLE
    # Run all Angelscript tests
    .\Tools\RunTests.ps1

    # Run a specific test prefix
    .\Tools\RunTests.ps1 -TestPrefix "Angelscript.CppTests.MultiEngine"

    # Run with custom label
    .\Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.HotReload" -Label "HotReload_Verify"
#>
param(
    [string]$TestPrefix = "Angelscript",
    [string]$Label = "",
    [string]$OutputRoot = "",
    [int]$TimeoutMs = 0,
    [switch]$NoReport
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ── INI parser (shared with ResolveAgentCommandTemplates.ps1) ──

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

# ── Resolve paths from AgentConfig.ini ──

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$configPath  = Join-Path $projectRoot "AgentConfig.ini"
$ini         = Get-IniMap -Path $configPath

$engineRoot = Get-IniValue -Ini $ini -Section "Paths" -Key "EngineRoot"
if ([string]::IsNullOrWhiteSpace($engineRoot)) {
    throw "Paths.EngineRoot is required in '$configPath'."
}

$projectFile = Get-IniValue -Ini $ini -Section "Paths" -Key "ProjectFile" `
    -Default (Join-Path $projectRoot "AngelscriptProject.uproject")
$editorCmd   = Join-Path $engineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if ($TimeoutMs -le 0) {
    $TimeoutMs = [int](Get-IniValue -Ini $ini -Section "Test" -Key "DefaultTimeoutMs" -Default "600000")
}

if (-not (Test-Path -LiteralPath $projectFile)) {
    throw "Project file not found: $projectFile"
}
if (-not (Test-Path -LiteralPath $editorCmd)) {
    throw "UnrealEditor-Cmd.exe not found: $editorCmd"
}

# ── Build output directory ──

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($Label)) {
    $Label = ($TestPrefix -replace '[^A-Za-z0-9._]', '_')
}
$runDir = "${timestamp}_${Label}"

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $projectRoot "Saved\Automation"
}
$outputDir = Join-Path $OutputRoot $runDir
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

$logFile    = Join-Path $outputDir "test.log"
$stderrFile = Join-Path $outputDir "test.stderr.log"
$reportDir  = Join-Path $outputDir "Reports"

# ── Assemble arguments ──

$argList = @(
    "`"$projectFile`""
    "-ExecCmds=`"Automation RunTests $TestPrefix; Quit`""
    "-Unattended"
    "-NoPause"
    "-NoSplash"
    "-NullRHI"
    "-NOSOUND"
)

if (-not $NoReport) {
    $argList += "-ReportExportPath=`"$reportDir`""
}

# ── Run ──

Write-Host "================================================================"
Write-Host "  Angelscript Test Runner"
Write-Host "================================================================"
Write-Host "Test prefix : $TestPrefix"
Write-Host "Output dir  : $outputDir"
Write-Host "Log file    : $logFile"
Write-Host "Stderr file : $stderrFile"
if (-not $NoReport) {
    Write-Host "Report dir  : $reportDir"
}
Write-Host "TimeoutMs   : $TimeoutMs"
Write-Host "Engine      : $editorCmd"
Write-Host "Project     : $projectFile"
Write-Host "----------------------------------------------------------------"
Write-Host "Starting test run at $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') ..."
Write-Host ""

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$proc = Start-Process -FilePath $editorCmd -ArgumentList $argList `
    -NoNewWindow -PassThru -RedirectStandardOutput $logFile -RedirectStandardError $stderrFile
$completed = $proc.WaitForExit($TimeoutMs)
if (-not $completed) {
    Stop-ProcessTree -ProcessId $proc.Id
    $sw.Stop()
    Try-AppendTimeoutMarker -Path $stderrFile -Message "[TIMEOUT] Test run exceeded ${TimeoutMs}ms and was terminated."
    Write-Host ""
    Write-Host "[ERROR] Test run timed out after $TimeoutMs ms and was terminated."
    Write-Host "Log file    : $logFile"
    Write-Host "Stderr file : $stderrFile"
    exit 124
}

$proc.WaitForExit()
$sw.Stop()

# ── Parse results ──

Write-Host ""
Write-Host "================================================================"
Write-Host "  Results"
Write-Host "================================================================"
Write-Host "Exit code   : $($proc.ExitCode)"
Write-Host "Elapsed     : $([math]::Round($sw.Elapsed.TotalSeconds, 1))s"

$content = Get-Content $logFile -Raw -ErrorAction SilentlyContinue
if ([string]::IsNullOrWhiteSpace($content)) {
    Write-Host ""
    Write-Host "[WARN] Log file is empty — engine may have failed to start."
    Write-Host "       Check $logFile for details."
    exit $proc.ExitCode
}

if ($content -match "GIsCriticalError=(\d+)") {
    $critErr = $Matches[1]
    Write-Host "Critical err: GIsCriticalError=$critErr"
    if ($critErr -ne "0") {
        Write-Host "[ERROR] Engine reported critical error!"
    }
}

$passed = 0
$failed = 0
if ($content -match "TEST COMPLETE\.\s*(\d+)\s*tests?\s*passed,\s*(\d+)\s*tests?\s*failed") {
    $passed = [int]$Matches[1]
    $failed = [int]$Matches[2]
    Write-Host "Passed      : $passed"
    Write-Host "Failed      : $failed"
} else {
    Write-Host "[WARN] Could not parse TEST COMPLETE summary from log."
}

# Show failure lines
$failLines = Select-String -Path $logFile -Pattern "\bFail\b|\bfailed\b" -AllMatches |
    Select-Object -First 50
if ($failLines) {
    Write-Host ""
    Write-Host "--- Failure lines (first 50) ---"
    $failLines | ForEach-Object { Write-Host $_.Line }
    Write-Host "--- End failure lines ---"
}

Write-Host ""
Write-Host "Log file    : $logFile"
Write-Host "Stderr file : $stderrFile"
if (-not $NoReport) {
    Write-Host "Report dir  : $reportDir"
}
Write-Host "================================================================"

if ($failed -gt 0 -or $proc.ExitCode -ne 0) {
    exit 1
}
exit 0
