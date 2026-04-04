<#
.SYNOPSIS
    Run the configured Unreal Editor target build with an enforced timeout.

.PARAMETER TimeoutMs
    Build timeout in milliseconds. Default: Build.DefaultTimeoutMs -> Test.DefaultTimeoutMs -> 600000.
#>
param(
    [int]$TimeoutMs = 0,
    [switch]$UseWaitMutex,
    [switch]$NoMutex
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

function Get-DescendantProcessIds {
    param([int]$RootProcessId)

    $descendants = New-Object System.Collections.Generic.List[int]
    $queue = New-Object System.Collections.Generic.Queue[int]
    $queue.Enqueue($RootProcessId)

    while ($queue.Count -gt 0) {
        $current = $queue.Dequeue()
        foreach ($child in @(Get-CimInstance Win32_Process -Filter "ParentProcessId = $current" -ErrorAction SilentlyContinue)) {
            $descendants.Add([int]$child.ProcessId)
            $queue.Enqueue([int]$child.ProcessId)
        }
    }

    return $descendants
}

function Get-ProjectProcessIds {
    param(
        [string]$ProjectFile,
        [string]$ProjectRoot,
        [datetime]$StartedAfter
    )

    $result = New-Object System.Collections.Generic.List[int]
    $allowedNames = @('dotnet.exe', 'UnrealBuildTool.exe', 'MSBuild.exe', 'cl.exe', 'link.exe', 'rc.exe', 'ShaderCompileWorker.exe', 'cmd.exe')
    foreach ($proc in @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue)) {
        if ($allowedNames -notcontains $proc.Name) {
            continue
        }

        $commandLine = [string]$proc.CommandLine
        if ([string]::IsNullOrWhiteSpace($commandLine)) {
            continue
        }

        $matchesProject = $commandLine.IndexOf($ProjectFile, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 -or
            $commandLine.IndexOf($ProjectRoot, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
        if (-not $matchesProject) {
            continue
        }

        if (-not (Test-IsTrackedBuildProcess -ProcessName $proc.Name -CommandLine $commandLine)) {
            continue
        }

        try {
            $startTime = [System.Management.ManagementDateTimeConverter]::ToDateTime($proc.CreationDate)
        } catch {
            $startTime = $StartedAfter
        }

        if ($startTime -ge $StartedAfter) {
            $result.Add([int]$proc.ProcessId)
        }
    }

    return $result
}

function Stop-BuildProcesses {
    param(
        [int]$RootProcessId,
        [string]$ProjectFile,
        [string]$ProjectRoot,
        [datetime]$StartedAfter
    )

    $processIds = New-Object System.Collections.Generic.HashSet[int]
    $processIds.Add($RootProcessId) | Out-Null
    foreach ($procId in @(Get-DescendantProcessIds -RootProcessId $RootProcessId)) {
        $processIds.Add($procId) | Out-Null
    }
    foreach ($procId in @(Get-ProjectProcessIds -ProjectFile $ProjectFile -ProjectRoot $ProjectRoot -StartedAfter $StartedAfter)) {
        $processIds.Add($procId) | Out-Null
    }

    foreach ($procId in $processIds) {
        Stop-ProcessTree -ProcessId $procId
    }

    for ($attempt = 0; $attempt -lt 5; $attempt++) {
        Start-Sleep -Milliseconds 500
        $remaining = @(Get-ProjectProcessIds -ProjectFile $ProjectFile -ProjectRoot $ProjectRoot -StartedAfter $StartedAfter)
        if ($remaining.Count -eq 0) {
            break
        }
        foreach ($procId in $remaining) {
            Stop-ProcessTree -ProcessId $procId
        }
    }
}

function Test-IsTrackedBuildProcess {
    param(
        [string]$ProcessName,
        [string]$CommandLine
    )

    switch ($ProcessName.ToLowerInvariant()) {
        'dotnet.exe' {
            return $CommandLine.IndexOf('UnrealBuildTool', [System.StringComparison]::OrdinalIgnoreCase) -ge 0 -or
                $CommandLine.IndexOf('UnrealHeaderTool', [System.StringComparison]::OrdinalIgnoreCase) -ge 0
        }
        'cmd.exe' {
            return $CommandLine.IndexOf('Build.bat', [System.StringComparison]::OrdinalIgnoreCase) -ge 0 -or
                $CommandLine.IndexOf('RunUBT.bat', [System.StringComparison]::OrdinalIgnoreCase) -ge 0
        }
        default {
            return $true
        }
    }
}

function Test-ExistingProjectBuild {
    param(
        [string]$ProjectFile,
        [string]$ProjectRoot
    )

    $currentPid = $PID
    $matches = @()
    $allowedNames = @('dotnet.exe', 'UnrealBuildTool.exe', 'MSBuild.exe', 'cl.exe', 'link.exe', 'rc.exe', 'ShaderCompileWorker.exe', 'cmd.exe')
    foreach ($proc in @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue)) {
        if ([int]$proc.ProcessId -eq $currentPid) {
            continue
        }

        if ($allowedNames -notcontains $proc.Name) {
            continue
        }

        $commandLine = [string]$proc.CommandLine
        if ([string]::IsNullOrWhiteSpace($commandLine)) {
            continue
        }

        $matchesProject = $commandLine.IndexOf($ProjectFile, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 -or
            $commandLine.IndexOf($ProjectRoot, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
        if (-not $matchesProject) {
            continue
        }

        if (-not (Test-IsTrackedBuildProcess -ProcessName $proc.Name -CommandLine $commandLine)) {
            continue
        }

        $matches += $proc
    }

    return $matches
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

if ($UseWaitMutex -and $NoMutex) {
    throw "-UseWaitMutex and -NoMutex are mutually exclusive."
}

if (-not $UseWaitMutex -and -not $NoMutex) {
    $NoMutex = $true
}

if ($TimeoutMs -le 0) {
    $TimeoutMs = [int](Get-IniValue -Ini $ini -Section "Build" -Key "DefaultTimeoutMs" -Default (Get-IniValue -Ini $ini -Section "Test" -Key "DefaultTimeoutMs" -Default "600000"))
}

$runUbtBat = Join-Path $engineRoot "Engine\Build\BatchFiles\RunUBT.bat"
if (-not (Test-Path -LiteralPath $runUbtBat)) {
    throw "RunUBT.bat not found: $runUbtBat"
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
    "-FromMsBuild"
    "-architecture=$architecture"
)

if ($UseWaitMutex) {
    $argList += "-WaitMutex"
}

if ($NoMutex) {
    $argList += "-NoMutex"
}

Write-Host "================================================================"
Write-Host "  Angelscript Build Runner"
Write-Host "================================================================"
Write-Host "Target      : $editorTarget"
Write-Host "Project     : $projectFile"
Write-Host "TimeoutMs   : $TimeoutMs"
Write-Host "WaitMutex   : $UseWaitMutex"
Write-Host "NoMutex     : $NoMutex"
Write-Host "RunUBT.bat  : $runUbtBat"
Write-Host "Stdout log  : $stdoutLog"
Write-Host "Stderr log  : $stderrLog"
Write-Host "----------------------------------------------------------------"

$existingBuilds = @(Test-ExistingProjectBuild -ProjectFile $projectFile -ProjectRoot $projectRoot)
if ($existingBuilds.Count -gt 0) {
    Write-Host "[ERROR] Found existing build-related process(es) for this project. Refusing to overlap the same worktree build."
    foreach ($proc in $existingBuilds) {
        Write-Host ("  PID {0} {1}" -f $proc.ProcessId, $proc.Name)
    }
    exit 125
}

$startedAt = Get-Date
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$proc = Start-Process -FilePath $runUbtBat -ArgumentList $argList -PassThru -NoNewWindow -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog
$completed = $proc.WaitForExit($TimeoutMs)
if (-not $completed) {
    Stop-BuildProcesses -RootProcessId $proc.Id -ProjectFile $projectFile -ProjectRoot $projectRoot -StartedAfter $startedAt.AddSeconds(-2)
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
