[CmdletBinding()]
param(
    [string]$ProjectRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

. (Join-Path $ProjectRoot 'Tools\Shared\UnrealCommandUtils.ps1')

$testFailures = New-Object System.Collections.Generic.List[string]
$completedTests = New-Object System.Collections.Generic.List[string]
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("AngelscriptToolingTests_{0}_{1}" -f (Get-Date -Format 'yyyyMMdd_HHmmss_fff'), $PID)
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Assert-Equal {
    param(
        $Actual,
        $Expected,
        [string]$Message
    )

    if ($Actual -ne $Expected) {
        throw ("{0} Expected='{1}' Actual='{2}'." -f $Message, $Expected, $Actual)
    }
}

function Assert-Null {
    param(
        $Actual,
        [string]$Message
    )

    if ($null -ne $Actual) {
        throw ("{0} Expected a null value." -f $Message)
    }
}

function Invoke-TestCase {
    param(
        [string]$Name,
        [scriptblock]$Body
    )

    Write-Host ("[test] {0}" -f $Name)
    try {
        & $Body
        $completedTests.Add($Name) | Out-Null
    } catch {
        $testFailures.Add(("{0}: {1}" -f $Name, $_.Exception.Message)) | Out-Null
        Write-Host ("[fail] {0}" -f $_.Exception.Message) -ForegroundColor Red
    }
}

Invoke-TestCase -Name 'TimeoutLimitRejectsTooLargeValues' -Body {
    $failed = $false
    try {
        Resolve-TimeoutMs -RequestedTimeoutMs 900001 -DefaultTimeoutMs 1000 -ParameterName 'TimeoutMs' | Out-Null
    } catch {
        $failed = $_.Exception.Message -like '*900000*'
    }

    Assert-True -Condition $failed -Message 'Resolve-TimeoutMs should reject values above 900000ms.'
}

Invoke-TestCase -Name 'WorktreeMutexRejectsSecondAcquire' -Body {
    $mutexName = Get-NamedMutexName -Scope 'tooling-smoke' -KeyPath $ProjectRoot
    $primaryLock = $null
    try {
        $primaryLock = Acquire-NamedMutex -Name $mutexName -TimeoutMs 0
        Assert-True -Condition ($null -ne $primaryLock) -Message 'Expected the first mutex acquisition to succeed.'

        $secondaryLock = Acquire-NamedMutex -Name $mutexName -TimeoutMs 0
        Assert-Null -Actual $secondaryLock -Message 'Expected the second mutex acquisition to fail immediately.'
    } finally {
        if ($null -ne $primaryLock) {
            Release-NamedMutex -Mutex $primaryLock
        }
    }
}

Invoke-TestCase -Name 'StreamingRunnerEmitsIncrementalLines' -Body {
    $timestamps = New-Object System.Collections.Generic.List[datetime]
    $logPath = Join-Path $tempRoot 'streaming.log'
    $helperPath = Join-Path $ProjectRoot 'Tools\Tests\Helpers\WriteLines.ps1'
    $powerShell = Get-ConsolePowerShellPath

    $result = Invoke-StreamingProcess `
        -FilePath $powerShell `
        -ArgumentList @('-NoProfile', '-File', $helperPath, '-Count', '3', '-DelayMs', '250') `
        -WorkingDirectory $tempRoot `
        -TimeoutMs 5000 `
        -LogPath $logPath `
        -Label 'streaming-smoke' `
        -OnLine {
            param($StreamName, $Line)
            if ($Line -like 'tick:*') {
                $timestamps.Add([datetime]::UtcNow) | Out-Null
            }
        }

    Assert-Equal -Actual $result.ExitCode -Expected 0 -Message 'Streaming helper should exit successfully.'
    Assert-Equal -Actual $timestamps.Count -Expected 3 -Message 'Expected three streamed lines.'
    $spreadMs = ($timestamps[$timestamps.Count - 1] - $timestamps[0]).TotalMilliseconds
    Assert-True -Condition ($spreadMs -ge 300) -Message 'Line callbacks should be spread across the process lifetime.'
}

Invoke-TestCase -Name 'TimeoutKillsProcessTree' -Body {
    $marker = 'ANGELSCRIPT_TIMEOUT_{0}' -f ([Guid]::NewGuid().ToString('N'))
    $logPath = Join-Path $tempRoot 'timeout.log'
    $helperPath = Join-Path $ProjectRoot 'Tools\Tests\Helpers\SpawnSleepTree.ps1'
    $powerShell = Get-ConsolePowerShellPath

    $result = Invoke-StreamingProcess `
        -FilePath $powerShell `
        -ArgumentList @('-NoProfile', '-File', $helperPath, '-Marker', $marker, '-Seconds', '30') `
        -WorkingDirectory $tempRoot `
        -TimeoutMs 1000 `
        -LogPath $logPath `
        -Label 'timeout-smoke'

    Assert-True -Condition $result.TimedOut -Message 'Expected the spawned process tree to time out.'

    Start-Sleep -Milliseconds 800
    $remaining = @(Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $commandLine = [string]$_.CommandLine
            -not [string]::IsNullOrWhiteSpace($commandLine) -and $commandLine.Contains($marker)
        })
    Assert-Equal -Actual $remaining.Count -Expected 0 -Message 'Timed out process tree should be terminated.'
}

Invoke-TestCase -Name 'CommandLineResolutionMapsWorktree' -Body {
    $worktreeRoot = Normalize-PathValue -Path $ProjectRoot
    $commandLine = 'dotnet.exe "J:\UnrealEngine\UERelease\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" AngelscriptProjectEditor Win64 Development "-Project={0}\AngelscriptProject.uproject" -NoMutex -NoEngineChanges' -f $worktreeRoot
    $descriptor = Resolve-UbtCommandDescriptor `
        -ProcessName 'dotnet.exe' `
        -CommandLine $commandLine `
        -WorktreeMap @(
            [pscustomobject]@{
                WorktreeRoot = $worktreeRoot
                Branch = 'test-execution-infra'
                Head = '1234567'
            }
        )

    Assert-Equal -Actual $descriptor.Kind -Expected 'DotNetUbt' -Message 'Process kind should resolve to DotNetUbt.'
    Assert-Equal -Actual $descriptor.WorktreeRoot -Expected $worktreeRoot -Message 'Worktree root should resolve from the project path.'
    Assert-Equal -Actual $descriptor.Branch -Expected 'test-execution-infra' -Message 'Branch should be copied from the worktree map.'
    Assert-Equal -Actual $descriptor.ProjectFile -Expected (Join-Path $worktreeRoot 'AngelscriptProject.uproject') -Message 'Project file should resolve from the command line.'
}

Write-Host ''
Write-Host ('Completed tests: {0}' -f $completedTests.Count)
foreach ($testName in $completedTests) {
    Write-Host ('  PASS {0}' -f $testName) -ForegroundColor Green
}

if ($testFailures.Count -gt 0) {
    Write-Host ''
    Write-Host ('Failures: {0}' -f $testFailures.Count) -ForegroundColor Red
    foreach ($failure in $testFailures) {
        Write-Host ('  {0}' -f $failure) -ForegroundColor Red
    }
    exit 1
}

exit 0
