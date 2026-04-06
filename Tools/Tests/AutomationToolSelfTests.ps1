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
        [string]$WorkingDirectory
    )

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FilePath
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $true

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

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$summaryScript = Join-Path $repoRoot 'Tools\GetAutomationReportSummary.ps1'
$runnerScript = Join-Path $repoRoot 'Tools\RunAutomationTests.ps1'
$runnerBat = Join-Path $repoRoot 'Tools\RunAutomationTests.bat'
$fixtureDir = Join-Path $PSScriptRoot 'Fixtures\FailedReport'
$missingReportFixtureDir = Join-Path $PSScriptRoot 'Fixtures\MissingReport'
$warningsFixtureDir = Join-Path $PSScriptRoot 'Fixtures\SucceededWithWarningsReport'

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("AutomationToolSelfTests-{0}" -f ([guid]::NewGuid().ToString('N')))
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

$results = [System.Collections.Generic.List[object]]::new()

try {
    $summaryPath = Join-Path $tempRoot 'Summary.json'
    $summaryRun = Invoke-CapturedProcess -FilePath 'powershell.exe' -ArgumentList @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $summaryScript,
        '-ReportPath', $fixtureDir,
        '-LogPath', (Join-Path $fixtureDir 'Automation.log'),
        '-ExitCode', '0',
        '-BucketName', 'SyntheticFailure',
        '-SummaryPath', $summaryPath
    ) -WorkingDirectory $repoRoot

    Assert-Equal 0 $summaryRun.ExitCode 'Summary script should succeed on fixture report.'
    Assert-True (Test-Path -LiteralPath $summaryPath -PathType Leaf) 'Summary script should emit Summary.json.'

    $summary = Get-Content -LiteralPath $summaryPath -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-Equal 'ReportJson' $summary.SummarySource 'SummarySource should indicate JSON parsing.'
    Assert-Equal 2 ([int]$summary.Total) 'Summary total count mismatch.'
    Assert-Equal 1 ([int]$summary.Passed) 'Summary passed count mismatch.'
    Assert-Equal 1 ([int]$summary.Failed) 'Summary failed count mismatch.'
    Assert-Equal 1 (@($summary.FailedTests).Count) 'FailedTests should contain one entry.'
    Assert-Equal 'Angelscript.TestModule.Synthetic.Fail' $summary.FailedTests[0].Name 'Failed test name mismatch.'
    $results.Add('PASS summary parser fixture')

    $warningsSummaryPath = Join-Path $tempRoot 'WarningsSummary.json'
    $warningsRun = Invoke-CapturedProcess -FilePath 'powershell.exe' -ArgumentList @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $summaryScript,
        '-ReportPath', $warningsFixtureDir,
        '-LogPath', (Join-Path $warningsFixtureDir 'Automation.log'),
        '-ExitCode', '0',
        '-BucketName', 'SyntheticWarnings',
        '-SummaryPath', $warningsSummaryPath
    ) -WorkingDirectory $repoRoot

    Assert-Equal 0 $warningsRun.ExitCode 'Summary script should succeed on warnings-only fixture.'
    Assert-True (Test-Path -LiteralPath $warningsSummaryPath -PathType Leaf) 'Summary script should emit warnings-only Summary.json.'

    $warningsSummary = Get-Content -LiteralPath $warningsSummaryPath -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-Equal 'ReportJson' $warningsSummary.SummarySource 'Warnings-only fixture should still parse from JSON.'
    Assert-Equal 2 ([int]$warningsSummary.Total) 'Warnings-only total count mismatch.'
    Assert-Equal 2 ([int]$warningsSummary.Passed) 'Warnings-only passed count should include succeededWithWarnings.'
    Assert-Equal 0 ([int]$warningsSummary.Failed) 'Warnings-only failed count mismatch.'
    Assert-Equal 0 (@($warningsSummary.LogFailureHints).Count) 'Warnings-only fixture should not surface blocking log hints.'
    $results.Add('PASS summary parser warnings-only fixture')

    $missingSummaryPath = Join-Path $tempRoot 'MissingReportSummary.json'
    $missingReportRun = Invoke-CapturedProcess -FilePath 'powershell.exe' -ArgumentList @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $summaryScript,
        '-ReportPath', $missingReportFixtureDir,
        '-LogPath', (Join-Path $missingReportFixtureDir 'Automation.log'),
        '-ExitCode', '0',
        '-BucketName', 'SyntheticMissingReport',
        '-SummaryPath', $missingSummaryPath
    ) -WorkingDirectory $repoRoot

    Assert-Equal 0 $missingReportRun.ExitCode 'Summary script should succeed for missing-report fixture.'
    Assert-True (Test-Path -LiteralPath $missingSummaryPath -PathType Leaf) 'Summary script should emit Summary.json for missing-report fixture.'

    $missingSummary = Get-Content -LiteralPath $missingSummaryPath -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-Equal 'None' $missingSummary.SummarySource 'Missing-report fixture should not produce a structured summary.'
    Assert-True (@($missingSummary.LogFailureHints).Count -gt 0) 'Missing-report fixture should surface blocking log hints.'
    $results.Add('PASS summary parser missing-report fixture')

    $unknownGroup = 'DefinitelyMissingAutomationBucket'
    $runnerFailure = Invoke-CapturedProcess -FilePath 'powershell.exe' -ArgumentList @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $runnerScript,
        '-Group', $unknownGroup
    ) -WorkingDirectory $repoRoot

    Assert-True ($runnerFailure.ExitCode -ne 0) 'Runner should fail for an unknown automation group.'
    Assert-True (($runnerFailure.StdErr + $runnerFailure.StdOut) -match 'Unknown automation group') 'Runner failure output should mention unknown automation group.'
    $results.Add('PASS runner unknown-group preflight')

    $batFailure = Invoke-CapturedProcess -FilePath 'cmd.exe' -ArgumentList @(
        '/c',
        '"' + $runnerBat + '" -Group ' + $unknownGroup
    ) -WorkingDirectory $repoRoot

    Assert-True ($batFailure.ExitCode -ne 0) 'Batch wrapper should fail for an unknown automation group.'
    $results.Add('PASS batch wrapper forwarding')

    Write-Host 'Automation tool self-tests passed:'
    foreach ($result in $results) {
        Write-Host ("- {0}" -f $result)
    }
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
