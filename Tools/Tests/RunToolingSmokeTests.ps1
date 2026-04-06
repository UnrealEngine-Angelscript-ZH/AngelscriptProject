param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $ProjectRoot "Tools\Shared\UnrealCommandUtils.ps1")

$testFailures = New-Object System.Collections.Generic.List[string]
$completedTests = New-Object System.Collections.Generic.List[string]
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("AngelscriptToolingTests_{0}_{1}" -f (Get-Date -Format "yyyyMMdd_HHmmss_fff"), $PID)
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

function New-FixtureProjectRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $root = Join-Path $tempRoot $Name
    New-Item -ItemType Directory -Path $root -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $root 'AngelscriptProject.uproject') -Encoding UTF8 -Value '{ "FileVersion": 3 }'
    return $root
}

function Write-AgentConfigFixture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot,

        [Parameter(Mandatory = $true)]
        [string]$EngineRoot,

        [string]$ProjectFile = '',

        [string]$BuildDefaultTimeoutMs = '',

        [string]$TestDefaultTimeoutMs = '600000'
    )

    if ([string]::IsNullOrWhiteSpace($ProjectFile)) {
        $ProjectFile = Join-Path $ProjectRoot 'AngelscriptProject.uproject'
    }

    $lines = @(
        '[Paths]'
        ('EngineRoot={0}' -f $EngineRoot)
        ('ProjectFile={0}' -f $ProjectFile)
        ''
        '[Build]'
        'EditorTarget=AngelscriptProjectEditor'
        'Platform=Win64'
        'Configuration=Development'
        'Architecture=x64'
    )

    if (-not [string]::IsNullOrWhiteSpace($BuildDefaultTimeoutMs)) {
        $lines += ('DefaultTimeoutMs={0}' -f $BuildDefaultTimeoutMs)
    }

    $lines += @(
        ''
        '[Test]'
        ('DefaultTimeoutMs={0}' -f $TestDefaultTimeoutMs)
        ''
        '[References]'
        'HazelightAngelscriptEngineRoot='
    )

    Set-Content -LiteralPath (Join-Path $ProjectRoot 'AgentConfig.ini') -Encoding UTF8 -Value $lines
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

Invoke-TestCase -Name "TimeoutLimitRejectsTooLargeValues" -Body {
    $failed = $false
    try {
        Resolve-TimeoutMs -RequestedTimeoutMs 900001 -DefaultTimeoutMs 1000 -ParameterName "TimeoutMs" | Out-Null
    } catch {
        $failed = $_.Exception.Message -like "*900000*"
    }

    Assert-True -Condition $failed -Message "Resolve-TimeoutMs should reject values above 900000ms."
}

Invoke-TestCase -Name "BuildDefaultTimeoutFallsBackTo180000" -Body {
    $fixtureRoot = New-FixtureProjectRoot -Name "timeout-default"
    Write-AgentConfigFixture -ProjectRoot $fixtureRoot -EngineRoot 'J:\UnrealEngine\UERelease'

    $resolved = Resolve-AgentConfiguration -ProjectRoot $fixtureRoot
    Assert-Equal -Actual $resolved.BuildDefaultTimeoutMs -Expected 180000 -Message "Build default timeout should fall back to 180000ms when Build.DefaultTimeoutMs is missing."
}

Invoke-TestCase -Name "UhtTimestampConflictSummaryDetectsSharedEnginePaths" -Body {
    $fixtureLog = Join-Path $tempRoot 'uht-timestamp-conflict.log'
    @(
        "[stderr] Couldn't write Timestamp file: The process cannot access the file 'J:\UnrealEngine\UERelease\Engine\Intermediate\Build\Win64\UnrealEditor\Inc\Chaos\UHT\Timestamp' because it is being used by another process."
        "[stderr] IOException: The process cannot access the file 'J:\UnrealEngine\AngelscriptProject\Intermediate\Build\Win64\AngelscriptProjectEditor\Inc\AngelscriptRuntime\UHT\Timestamp' because it is being used by another process."
    ) | Set-Content -LiteralPath $fixtureLog -Encoding UTF8

    $summary = Get-UhtTimestampConflictSummary -LogPaths @($fixtureLog) -EngineRoot 'J:\UnrealEngine\UERelease'
    Assert-True -Condition $summary.Detected -Message "UHT timestamp conflict summary should detect timestamp contention in logs."
    Assert-True -Condition $summary.SharedEngineDetected -Message "UHT timestamp conflict summary should detect shared engine timestamp contention."
    Assert-Equal -Actual $summary.Paths.Count -Expected 2 -Message "UHT timestamp conflict summary should return all distinct timestamp paths."
    Assert-Equal -Actual $summary.SharedEnginePaths.Count -Expected 1 -Message "UHT timestamp conflict summary should isolate only engine-root timestamp paths as shared."
}

Invoke-TestCase -Name "ExecutionDeadlineTracksRemainingBudget" -Body {
    $deadline = New-ExecutionDeadline -TimeoutMs 200
    Start-Sleep -Milliseconds 80

    $remainingMs = Get-RemainingTimeoutMs -DeadlineUtc $deadline -PhaseName "deadline-smoke"
    Assert-True -Condition ($remainingMs -gt 0) -Message "Remaining timeout budget should stay positive before the deadline expires."
    Assert-True -Condition ($remainingMs -lt 200) -Message "Remaining timeout budget should shrink after time elapses."
}

Invoke-TestCase -Name "ExecutionDeadlineRejectsExpiredBudget" -Body {
    $deadline = New-ExecutionDeadline -TimeoutMs 50
    Start-Sleep -Milliseconds 90

    $failed = $false
    try {
        Get-RemainingTimeoutMs -DeadlineUtc $deadline -PhaseName "expired-budget" | Out-Null
    }
    catch {
        $failed = $_.Exception.Message -like '*expired-budget*timeout*'
    }

    Assert-True -Condition $failed -Message "Expired timeout budgets should fail instead of silently reusing the original timeout."
}

Invoke-TestCase -Name "ResolveAgentConfigurationRejectsCrossWorktreeProjectFile" -Body {
    $fixtureRoot = New-FixtureProjectRoot -Name "config-scope"
    $otherRoot = New-FixtureProjectRoot -Name "config-scope-other"
    Write-AgentConfigFixture -ProjectRoot $fixtureRoot -EngineRoot 'J:\UnrealEngine\UERelease' -ProjectFile (Join-Path $otherRoot 'AngelscriptProject.uproject')

    $failed = $false
    try {
        Resolve-AgentConfiguration -ProjectRoot $fixtureRoot | Out-Null
    }
    catch {
        $failed = $_.Exception.Message -like '*ProjectFile*project root*'
    }

    Assert-True -Condition $failed -Message "Resolve-AgentConfiguration should reject AgentConfig.ini values that point at another worktree."
}

Invoke-TestCase -Name "WorktreeMutexRejectsSecondAcquire" -Body {
    $mutexName = Get-NamedMutexName -Scope "tooling-smoke" -KeyPath $ProjectRoot
    $primaryLock = $null
    try {
        $primaryLock = Acquire-NamedMutex -Name $mutexName -TimeoutMs 0
        Assert-True -Condition ($null -ne $primaryLock) -Message "Expected the first mutex acquisition to succeed."

        $secondaryLock = Acquire-NamedMutex -Name $mutexName -TimeoutMs 0
        Assert-Null -Actual $secondaryLock -Message "Expected the second mutex acquisition to fail immediately."
    } finally {
        if ($null -ne $primaryLock) {
            Release-NamedMutex -Mutex $primaryLock
        }
    }
}

Invoke-TestCase -Name "StreamingRunnerEmitsIncrementalLines" -Body {
    $timestamps = New-Object System.Collections.Generic.List[datetime]
    $logPath = Join-Path $tempRoot "streaming.log"
    $helperPath = Join-Path $ProjectRoot "Tools\Tests\Helpers\WriteLines.ps1"
    $powerShell = Get-ConsolePowerShellPath

    $result = Invoke-StreamingProcess `
        -FilePath $powerShell `
        -ArgumentList @("-NoProfile", "-File", $helperPath, "-Count", "3", "-DelayMs", "250") `
        -WorkingDirectory $tempRoot `
        -TimeoutMs 5000 `
        -LogPath $logPath `
        -Label "streaming-smoke" `
        -OnLine {
            param($StreamName, $Line)
            if ($Line -like "tick:*") {
                $timestamps.Add([datetime]::UtcNow) | Out-Null
            }
        }

    Assert-Equal -Actual $result.ExitCode -Expected 0 -Message "Streaming helper should exit successfully."
    Assert-Equal -Actual $timestamps.Count -Expected 3 -Message "Expected three streamed lines."
    $spreadMs = ($timestamps[$timestamps.Count - 1] - $timestamps[0]).TotalMilliseconds
    Assert-True -Condition ($spreadMs -ge 300) -Message "Line callbacks should be spread across the process lifetime."
}

Invoke-TestCase -Name "StreamingRunnerPreservesArgumentsWithSpaces" -Body {
    $observedLines = New-Object System.Collections.Generic.List[string]
    $logPath = Join-Path $tempRoot "args.log"
    $helperPath = Join-Path $ProjectRoot "Tools\Tests\Helpers\EchoArgs.ps1"
    $powerShell = Get-ConsolePowerShellPath
    $spacedArgumentA = '-ExecCmds=Automation RunTests Group:AngelscriptSmoke; Quit'
    $spacedArgumentB = '-TestExit=Automation Test Queue Empty'

    $result = Invoke-StreamingProcess `
        -FilePath $powerShell `
        -ArgumentList @("-NoProfile", "-File", $helperPath, $spacedArgumentA, $spacedArgumentB) `
        -WorkingDirectory $tempRoot `
        -TimeoutMs 5000 `
        -LogPath $logPath `
        -Label "args-smoke" `
        -OnLine {
            param($StreamName, $Line)
            $observedLines.Add([string]$Line) | Out-Null
        }

    Assert-Equal -Actual $result.ExitCode -Expected 0 -Message "Argument echo helper should exit successfully."
    Assert-True -Condition ($observedLines.Contains('arg-count:2')) -Message "Arguments with spaces should be preserved as two arguments."
    Assert-True -Condition ($observedLines.Contains(("arg[0]:{0}" -f $spacedArgumentA))) -Message "The first spaced argument should be preserved verbatim."
    Assert-True -Condition ($observedLines.Contains(("arg[1]:{0}" -f $spacedArgumentB))) -Message "The second spaced argument should be preserved verbatim."
}

Invoke-TestCase -Name "TimeoutKillsProcessTree" -Body {
    $marker = "ANGELSCRIPT_TIMEOUT_{0}" -f ([Guid]::NewGuid().ToString("N"))
    $logPath = Join-Path $tempRoot "timeout.log"
    $helperPath = Join-Path $ProjectRoot "Tools\Tests\Helpers\SpawnSleepTree.ps1"
    $powerShell = Get-ConsolePowerShellPath

    $result = Invoke-StreamingProcess `
        -FilePath $powerShell `
        -ArgumentList @("-NoProfile", "-File", $helperPath, "-Marker", $marker, "-Seconds", "30") `
        -WorkingDirectory $tempRoot `
        -TimeoutMs 1000 `
        -LogPath $logPath `
        -Label "timeout-smoke"

    Assert-True -Condition $result.TimedOut -Message "Expected the spawned process tree to time out."

    Start-Sleep -Milliseconds 800
    $remaining = @(Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $commandLine = [string]$_.CommandLine
            -not [string]::IsNullOrWhiteSpace($commandLine) -and $commandLine.Contains($marker)
        })
    Assert-Equal -Actual $remaining.Count -Expected 0 -Message "Timed out process tree should be terminated."
}

Invoke-TestCase -Name "RequestedOutputRootGetsUniqueRunDirectory" -Body {
    $requestedRoot = Join-Path $tempRoot "shared-output-root"
    New-Item -ItemType Directory -Path $requestedRoot -Force | Out-Null

    $first = New-CommandOutputLayout -ProjectRoot $ProjectRoot -Category "Tests" -Label "OutputIsolation" -RequestedOutputRoot $requestedRoot -LogFileName "Automation.log"
    Start-Sleep -Milliseconds 30
    $second = New-CommandOutputLayout -ProjectRoot $ProjectRoot -Category "Tests" -Label "OutputIsolation" -RequestedOutputRoot $requestedRoot -LogFileName "Automation.log"

    Assert-True -Condition ($first.OutputRoot -ne $requestedRoot) -Message "Requested output root should be treated as a parent directory, not the final run directory."
    Assert-True -Condition ($second.OutputRoot -ne $requestedRoot) -Message "Requested output root should be treated as a parent directory, not the final run directory."
    Assert-True -Condition ($first.OutputRoot -ne $second.OutputRoot) -Message "Each call should allocate a unique run directory under the requested output root."
    Assert-True -Condition ($first.OutputRoot.StartsWith($requestedRoot, [System.StringComparison]::OrdinalIgnoreCase)) -Message "First output root should stay under the requested root."
    Assert-True -Condition ($second.OutputRoot.StartsWith($requestedRoot, [System.StringComparison]::OrdinalIgnoreCase)) -Message "Second output root should stay under the requested root."
}

Invoke-TestCase -Name "CommandLineResolutionMapsWorktree" -Body {
    $worktreeRoot = Normalize-PathValue -Path $ProjectRoot
    $commandLine = 'dotnet.exe "J:\UnrealEngine\UERelease\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" AngelscriptProjectEditor Win64 Development "-Project={0}\AngelscriptProject.uproject" -NoMutex -NoEngineChanges' -f $worktreeRoot
    $descriptor = Resolve-UbtCommandDescriptor `
        -ProcessName "dotnet.exe" `
        -CommandLine $commandLine `
        -WorktreeMap @(
            [pscustomobject]@{
                WorktreeRoot = $worktreeRoot
                Branch = "test-engine-isolation"
                Head = "1234567"
            }
        )

    Assert-Equal -Actual $descriptor.Kind -Expected "DotNetUbt" -Message "Process kind should resolve to DotNetUbt."
    Assert-Equal -Actual $descriptor.WorktreeRoot -Expected $worktreeRoot -Message "Worktree root should resolve from the project path."
    Assert-Equal -Actual $descriptor.Branch -Expected "test-engine-isolation" -Message "Branch should be copied from the worktree map."
    Assert-Equal -Actual $descriptor.ProjectFile -Expected (Join-Path $worktreeRoot "AngelscriptProject.uproject") -Message "Project file should resolve from the command line."
}

Invoke-TestCase -Name "BootstrapWorktreeCreatesConfigAndNormalizesProjectFile" -Body {
    $fixtureRoot = New-FixtureProjectRoot -Name "bootstrap"
    $bootstrapScript = Join-Path $ProjectRoot 'Tools\BootstrapWorktree.ps1'

    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $bootstrapScript -ProjectRoot $fixtureRoot -EngineRoot 'J:\UnrealEngine\UERelease' -NoPrewarm | Out-Null
    $exitCode = $LASTEXITCODE

    Assert-Equal -Actual $exitCode -Expected 0 -Message "BootstrapWorktree should succeed for a local fixture project."
    Assert-True -Condition (Test-Path -LiteralPath (Join-Path $fixtureRoot 'AgentConfig.ini') -PathType Leaf) -Message "BootstrapWorktree should create AgentConfig.ini."

    $resolved = Resolve-AgentConfiguration -ProjectRoot $fixtureRoot
    Assert-Equal -Actual $resolved.ProjectFile -Expected (Join-Path $fixtureRoot 'AngelscriptProject.uproject') -Message "BootstrapWorktree should set ProjectFile to the current worktree project."
    Assert-Equal -Actual $resolved.BuildDefaultTimeoutMs -Expected 180000 -Message "BootstrapWorktree should backfill Build.DefaultTimeoutMs."
    Assert-Equal -Actual $resolved.TestDefaultTimeoutMs -Expected 600000 -Message "BootstrapWorktree should preserve the standard test timeout."
}

Invoke-TestCase -Name "RunTestSuiteDryRunForwardsTimeout" -Body {
    $suiteScript = Join-Path $ProjectRoot 'Tools\RunTestSuite.ps1'
    $powerShell = Get-ConsolePowerShellPath
    $observedLines = New-Object System.Collections.Generic.List[string]
    $logPath = Join-Path $tempRoot 'suite-dryrun.log'

    $result = Invoke-StreamingProcess `
        -FilePath $powerShell `
        -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $suiteScript, '-Suite', 'Smoke', '-DryRun', '-TimeoutMs', '123456') `
        -WorkingDirectory $ProjectRoot `
        -TimeoutMs 5000 `
        -LogPath $logPath `
        -Label 'suite-dryrun' `
        -OnLine {
            param($StreamName, $Line)
            $observedLines.Add([string]$Line) | Out-Null
        }

    Assert-Equal -Actual $result.ExitCode -Expected 0 -Message "RunTestSuite dry run should succeed."
    Assert-True -Condition ($observedLines.Count -gt 0) -Message "RunTestSuite dry run should emit command lines."
    Assert-True -Condition (($observedLines -join "`n") -match '-TimeoutMs 123456') -Message "RunTestSuite should forward TimeoutMs to each RunTests invocation."
}

Invoke-TestCase -Name "ResolveAgentCommandTemplatesFallsBackToBootstrapGuidance" -Body {
    $fixtureRoot = New-FixtureProjectRoot -Name 'command-templates-bootstrap'
    $templatesScript = Join-Path $ProjectRoot 'Tools\ResolveAgentCommandTemplates.ps1'
    $powerShell = Get-ConsolePowerShellPath
    $observedLines = New-Object System.Collections.Generic.List[string]
    $logPath = Join-Path $tempRoot 'command-templates-bootstrap.log'

    $result = Invoke-StreamingProcess `
        -FilePath $powerShell `
        -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $templatesScript, '-ProjectRoot', $fixtureRoot) `
        -WorkingDirectory $ProjectRoot `
        -TimeoutMs 5000 `
        -LogPath $logPath `
        -Label 'command-templates-bootstrap' `
        -OnLine {
            param($StreamName, $Line)
            $observedLines.Add([string]$Line) | Out-Null
        }

    Assert-Equal -Actual $result.ExitCode -Expected 0 -Message "ResolveAgentCommandTemplates should emit bootstrap guidance when AgentConfig.ini is missing."
    $joinedOutput = $observedLines -join "`n"
    Assert-True -Condition ($observedLines.Contains('Status=BootstrapRequired')) -Message "ResolveAgentCommandTemplates should report BootstrapRequired status."
    Assert-True -Condition ($joinedOutput -match 'BootstrapCommand=.*BootstrapWorktree\.ps1') -Message "ResolveAgentCommandTemplates should emit a BootstrapCommand."
}

Invoke-TestCase -Name "ResolveAgentCommandTemplatesEmitsFirstClassBuildVariants" -Body {
    $templatesScript = Join-Path $ProjectRoot 'Tools\ResolveAgentCommandTemplates.ps1'
    $powerShell = Get-ConsolePowerShellPath
    $observedLines = New-Object System.Collections.Generic.List[string]
    $logPath = Join-Path $tempRoot 'command-templates-ready.log'

    $result = Invoke-StreamingProcess `
        -FilePath $powerShell `
        -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $templatesScript) `
        -WorkingDirectory $ProjectRoot `
        -TimeoutMs 5000 `
        -LogPath $logPath `
        -Label 'command-templates-ready' `
        -OnLine {
            param($StreamName, $Line)
            $observedLines.Add([string]$Line) | Out-Null
        }

    Assert-Equal -Actual $result.ExitCode -Expected 0 -Message "ResolveAgentCommandTemplates should succeed for the current worktree."
    $joinedOutput = $observedLines -join "`n"
    Assert-True -Condition ($joinedOutput -match 'NoXgeBuildCommand=.*-NoXGE') -Message "ResolveAgentCommandTemplates should emit a first-class noxge build command."
    Assert-True -Condition ($joinedOutput -match 'SerializedBuildCommand=.*-SerializeByEngine') -Message "ResolveAgentCommandTemplates should emit a serialized build command."
    Assert-True -Condition (-not ($joinedOutput -match 'UniqueBuildCommand=')) -Message "ResolveAgentCommandTemplates should not emit prohibited unique build environment commands."
    Assert-True -Condition (-not ($joinedOutput -match 'IsolatedBuildCommand=')) -Message "ResolveAgentCommandTemplates should not emit prohibited isolated build commands."
}

Invoke-TestCase -Name "RunBuildRejectsUniqueBuildEnvironment" -Body {
    $buildScript = Join-Path $ProjectRoot 'Tools\RunBuild.ps1'
    $powerShell = Get-ConsolePowerShellPath
    $observedLines = New-Object System.Collections.Generic.List[string]
    $logPath = Join-Path $tempRoot 'runbuild-unique-prohibited.log'

    $result = Invoke-StreamingProcess `
        -FilePath $powerShell `
        -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $buildScript, '-TimeoutMs', '1000', '-UniqueBuildEnvironment') `
        -WorkingDirectory $ProjectRoot `
        -TimeoutMs 5000 `
        -LogPath $logPath `
        -Label 'runbuild-unique-prohibited' `
        -OnLine {
            param($StreamName, $Line)
            $observedLines.Add([string]$Line) | Out-Null
        }

    Assert-Equal -Actual $result.ExitCode -Expected 3 -Message "RunBuild should reject prohibited unique build environment requests."
    $joinedOutput = $observedLines -join "`n"
    Assert-True -Condition ($joinedOutput -match 'prohibited') -Message "RunBuild should explain that unique build environment is prohibited."
}

Write-Host ""
Write-Host ("Completed tests: {0}" -f $completedTests.Count)
foreach ($testName in $completedTests) {
    Write-Host ("  PASS {0}" -f $testName) -ForegroundColor Green
}

if ($testFailures.Count -gt 0) {
    Write-Host ""
    Write-Host ("Failures: {0}" -f $testFailures.Count) -ForegroundColor Red
    foreach ($failure in $testFailures) {
        Write-Host ("  {0}" -f $failure) -ForegroundColor Red
    }
    exit 1
}

exit 0
