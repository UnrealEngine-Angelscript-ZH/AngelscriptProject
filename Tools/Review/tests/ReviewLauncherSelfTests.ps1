[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-True {
    param(
        [Parameter(Mandatory = $true)]
        [bool]$Condition,

        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Assert-Equal {
    param(
        [Parameter(Mandatory = $true)]
        $Expected,

        [Parameter(Mandatory = $true)]
        $Actual,

        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    if ($Expected -ne $Actual) {
        throw "$Message Expected=[$Expected] Actual=[$Actual]"
    }
}

function Invoke-CapturedProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(Mandatory = $true)]
        [string[]]$ArgumentList,

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [hashtable]$Environment = @{}
    )

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FilePath
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $true

    foreach ($entry in $Environment.GetEnumerator()) {
        $startInfo.EnvironmentVariables[[string]$entry.Key] = [string]$entry.Value
    }

    $quotedArguments = foreach ($argument in $ArgumentList) {
        if ($argument -match '[\s"]') {
            '"{0}"' -f ($argument -replace '"', '\"')
        }
        else {
            $argument
        }
    }

    $startInfo.Arguments = ($quotedArguments -join ' ')

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()

    return [PSCustomObject]@{
        ExitCode = $process.ExitCode
        StdOut   = $stdout
        StdErr   = $stderr
    }
}

function Invoke-TestCase {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Body
    )

    Write-Host ("[test] {0}" -f $Name)
    & $Body
    Write-Host ("[pass] {0}" -f $Name) -ForegroundColor Green
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$launcherScript = Join-Path $repoRoot 'Tools\Review\powershell\RunMainBranchReview.ps1'
$launcherBatch = Join-Path $repoRoot 'Tools\Review\RunMainBranchReview.bat'
$ruleDocument = Join-Path $repoRoot 'Documents\Rules\ReviewRule_ZH.md'

Invoke-TestCase -Name 'LauncherPreviewIncludesRuleAndOutputPath' -Body {
    Assert-True -Condition (Test-Path -LiteralPath $launcherScript -PathType Leaf) -Message 'RunMainBranchReview.ps1 should exist.'
    Assert-True -Condition (Test-Path -LiteralPath $ruleDocument -PathType Leaf) -Message 'ReviewRule_ZH.md should exist.'

    $run = Invoke-CapturedProcess -FilePath 'powershell.exe' -ArgumentList @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $launcherScript,
        '-DateSuffix', '2026-04-06_13-45-20',
        '-Preview'
    ) -WorkingDirectory $repoRoot

    $combined = $run.StdOut + $run.StdErr
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Message 'Preview mode should exit successfully.'
    Assert-True -Condition ($combined -match 'Status=Preview') -Message 'Preview mode should emit Status=Preview.'
    Assert-True -Condition ($combined -match 'RulePath=.*ReviewRule_ZH\.md') -Message 'Preview output should reference ReviewRule_ZH.md.'
    Assert-True -Condition ($combined -match 'OutputRelativePath=Documents[\\/]+Reviews[\\/]+Review_2026-04-06_13-45-20\.md') -Message 'Preview output should include the timestamped review output path.'
    Assert-True -Condition ($combined -match 'LogRelativePath=Documents[\\/]+Reviews[\\/]+run\.log') -Message 'Preview output should include the shared run.log path.'
    Assert-True -Condition ($combined -match '--agent Hephaestus') -Message 'Preview output should show Hephaestus agent override.'
    Assert-True -Condition ($combined -match '--model codez-gpt/gpt-5\.4') -Message 'Preview output should show GPT-5.4 model override.'
    Assert-True -Condition ($combined -match '--variant xhigh') -Message 'Preview output should show xhigh variant override.'
    Assert-True -Condition ($combined -match '--command ralph-loop') -Message 'Preview output should show ralph-loop command invocation.'
}

Invoke-TestCase -Name 'LauncherPreviewDefaultsToTimestampedSuffix' -Body {
    $run = Invoke-CapturedProcess -FilePath 'powershell.exe' -ArgumentList @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $launcherScript,
        '-Preview'
    ) -WorkingDirectory $repoRoot

    $combined = $run.StdOut + $run.StdErr
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Message 'Preview mode with default suffix should exit successfully.'
    Assert-True -Condition ($combined -match 'OutputRelativePath=Documents[\\/]+Reviews[\\/]+Review_\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}\.md') -Message 'Default preview output should use a timestamped review file name.'
}

Invoke-TestCase -Name 'BatchWrapperForwardsPreviewArguments' -Body {
    Assert-True -Condition (Test-Path -LiteralPath $launcherBatch -PathType Leaf) -Message 'RunMainBranchReview.bat should exist.'

    $run = Invoke-CapturedProcess -FilePath 'cmd.exe' -ArgumentList @(
        '/c',
        $launcherBatch,
        '-DateSuffix',
        '2026-04-06_13-45-20',
        '-Preview'
    ) -WorkingDirectory $repoRoot

    $combined = $run.StdOut + $run.StdErr
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Message 'Batch wrapper preview should exit successfully.'
    Assert-True -Condition ($combined -match 'Status=Preview') -Message 'Batch wrapper should forward preview arguments to the PowerShell launcher.'
}

Invoke-TestCase -Name 'LauncherWritesCombinedConsoleOutputToRunLog' -Body {
    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('review-launcher-test-' + [System.Guid]::NewGuid().ToString('N'))
    $tempBin = Join-Path $tempRoot 'bin'
    $tempRuleDirectory = Join-Path $tempRoot 'Documents\Rules'
    $tempReviewDirectory = Join-Path $tempRoot 'Documents\Reviews'
    $fakeOpencodePath = Join-Path $tempBin 'opencode.cmd'
    $logPath = Join-Path $tempReviewDirectory 'run.log'

    try {
        New-Item -ItemType Directory -Path $tempBin -Force | Out-Null
        New-Item -ItemType Directory -Path $tempRuleDirectory -Force | Out-Null

        Set-Content -LiteralPath (Join-Path $tempRuleDirectory 'ReviewRule_ZH.md') -Value '# mock rule' -Encoding UTF8
        Set-Content -LiteralPath $fakeOpencodePath -Encoding ASCII -Value @'
@echo off
echo fake stdout
echo fake stderr 1>&2
echo args:%*
exit /b 7
'@

        $run = Invoke-CapturedProcess -FilePath 'powershell.exe' -ArgumentList @(
            '-NoProfile',
            '-ExecutionPolicy', 'Bypass',
            '-File', $launcherScript,
            '-ProjectRoot', $tempRoot,
            '-DateSuffix', '2026-04-06_13-45-20'
        ) -WorkingDirectory $repoRoot -Environment @{
            PATH = ('{0};{1}' -f $tempBin, $env:PATH)
        }

        $combined = $run.StdOut + $run.StdErr
        Assert-Equal -Expected 7 -Actual $run.ExitCode -Message 'Launcher should preserve the opencode exit code.'
        Assert-True -Condition (Test-Path -LiteralPath $logPath -PathType Leaf) -Message 'Launcher should write a shared run.log file.'
        Assert-True -Condition ($combined -match 'fake stdout') -Message 'Console output should include opencode stdout.'
        Assert-True -Condition ($combined -match 'fake stderr') -Message 'Console output should include opencode stderr.'

        $logContent = Get-Content -LiteralPath $logPath -Raw
        Assert-True -Condition ($logContent -match 'fake stdout') -Message 'run.log should include opencode stdout.'
        Assert-True -Condition ($logContent -match 'fake stderr') -Message 'run.log should include opencode stderr.'
        Assert-True -Condition ($logContent -match '--command ralph-loop') -Message 'run.log should capture the launched command arguments.'
    }
    finally {
        if (Test-Path -LiteralPath $tempRoot) {
            Remove-Item -LiteralPath $tempRoot -Recurse -Force
        }
    }
}

Write-Host 'Review launcher self-tests passed.'
