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
