[CmdletBinding()]
param(
    [string]$ProjectRoot = '',

    [string]$DateSuffix = '',

    [string[]]$Repos,

    [string[]]$Dimensions,

    [int]$MaxIterations = 3,

    [int]$Timeout = 600,

    [switch]$Preview,

    [switch]$DryRun,

    [switch]$VerboseLog
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
}
else {
    $ProjectRoot = (Resolve-Path $ProjectRoot).Path
}

$ruleRelativePath = 'Documents/Rules/ReferenceComparisonRule_ZH.md'
$rulePath = Join-Path $ProjectRoot $ruleRelativePath

if (-not (Test-Path -LiteralPath $rulePath -PathType Leaf)) {
    throw ("Required rule document was not found: {0}" -f $rulePath)
}

$pythonCandidates = @('python', 'python3', 'py')
$pythonExe = $null

foreach ($candidate in $pythonCandidates) {
    if (Get-Command $candidate -ErrorAction SilentlyContinue) {
        $pythonExe = $candidate
        break
    }
}

if ($null -eq $pythonExe) {
    throw 'Python was not found in PATH. Install Python 3.8+ and ensure it is on PATH.'
}

$scriptModule = Join-Path (Join-Path (Join-Path $PSScriptRoot '..\python') 'ReferenceComparison') 'main.py'

if (-not (Test-Path -LiteralPath $scriptModule -PathType Leaf)) {
    throw ("Python module not found: {0}" -f $scriptModule)
}

$argumentList = [System.Collections.Generic.List[string]]::new()
$argumentList.Add('-m') | Out-Null
$argumentList.Add('ReferenceComparison.main') | Out-Null
$argumentList.Add('--project-root') | Out-Null
$argumentList.Add($ProjectRoot) | Out-Null

if (-not [string]::IsNullOrWhiteSpace($DateSuffix)) {
    $argumentList.Add('--date-suffix') | Out-Null
    $argumentList.Add($DateSuffix) | Out-Null
}

if ($null -ne $Repos -and $Repos.Count -gt 0) {
    $argumentList.Add('--repos') | Out-Null
    foreach ($repo in $Repos) {
        $argumentList.Add($repo) | Out-Null
    }
}

if ($null -ne $Dimensions -and $Dimensions.Count -gt 0) {
    $argumentList.Add('--dimensions') | Out-Null
    foreach ($dim in $Dimensions) {
        $argumentList.Add($dim) | Out-Null
    }
}

$argumentList.Add('--max-iterations') | Out-Null
$argumentList.Add([string]$MaxIterations) | Out-Null

$argumentList.Add('--timeout') | Out-Null
$argumentList.Add([string]$Timeout) | Out-Null

if ($Preview) {
    $argumentList.Add('--preview') | Out-Null
}

if ($DryRun) {
    $argumentList.Add('--dry-run') | Out-Null
}

if ($VerboseLog) {
    $argumentList.Add('--verbose') | Out-Null
}

if ($Preview) {
    Write-Output 'Status=Preview'
    Write-Output ('ProjectRoot={0}' -f $ProjectRoot)
    Write-Output ('RulePath={0}' -f $rulePath)
    Write-Output ('PythonExe={0}' -f $pythonExe)
    $displayArgs = ($argumentList -join ' ')
    Write-Output ('PythonArgs={0}' -f $displayArgs)
}

if ([string]::IsNullOrWhiteSpace($DateSuffix)) {
    $datePart = Get-Date -Format 'yyyy-MM-dd'
}
else {
    $datePart = $DateSuffix
}

$outputDir   = Join-Path $ProjectRoot ('Documents/Comparisons/{0}' -f $datePart)
$runStamp    = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss_fff'
$consolePath = Join-Path $outputDir ('console_opencode_{0}_{1}.log' -f $runStamp, $PID)
$stdoutPath  = Join-Path $outputDir ('opencode_stdout_{0}_{1}.tmp' -f $runStamp, $PID)
$stderrPath  = Join-Path $outputDir ('opencode_stderr_{0}_{1}.tmp' -f $runStamp, $PID)

New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

$repoDisplay = if ($null -ne $Repos -and $Repos.Count -gt 0) { $Repos -join ', ' } else { '(all)' }
$dimDisplay  = if ($null -ne $Dimensions -and $Dimensions.Count -gt 0) { $Dimensions -join ', ' } else { '(all)' }

$previousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'

$pythonProcessArgs = @('-u') + @($argumentList)

Push-Location (Join-Path $PSScriptRoot '..\python')
try {
    Write-Host ''
    Write-Host '================================================================' -ForegroundColor Cyan
    Write-Host '  ReferenceComparison (opencode) — starting'                       -ForegroundColor Cyan
    Write-Host ('  Output dir : {0}' -f $outputDir)
    Write-Host ('  Log file   : {0}' -f $consolePath)
    Write-Host ('  Repos      : {0}' -f $repoDisplay)
    Write-Host ('  Dimensions : {0}' -f $dimDisplay)
    Write-Host ('  Max rounds : {0}' -f $MaxIterations)
    Write-Host '================================================================' -ForegroundColor Cyan
    Write-Host ''

    $process = Start-Process `
        -FilePath $pythonExe `
        -ArgumentList $pythonProcessArgs `
        -WorkingDirectory (Get-Location).Path `
        -NoNewWindow `
        -PassThru `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError  $stderrPath

    $spinChars = @('|', '/', '-', '\')
    $spinIdx   = 0
    $sw        = [System.Diagnostics.Stopwatch]::StartNew()

    while (-not $process.HasExited) {
        $elapsed = $sw.Elapsed.ToString('hh\:mm\:ss')
        $spin    = $spinChars[$spinIdx % $spinChars.Length]
        Write-Host ("`r  [{0}] opencode running... {1}  " -f $spin, $elapsed) -NoNewline
        $spinIdx++
        Start-Sleep -Milliseconds 500
    }

    $sw.Stop()
    $elapsed = $sw.Elapsed.ToString('hh\:mm\:ss')
    $lastExitCode = $process.ExitCode
    Write-Host ''

    # Merge stdout + stderr into the console log file
    foreach ($tmpPath in @($stdoutPath, $stderrPath)) {
        if (Test-Path -LiteralPath $tmpPath -PathType Leaf) {
            $raw = [System.IO.File]::ReadAllBytes($tmpPath)
            if ($raw.Length -gt 0) {
                [System.IO.File]::AppendAllText($consolePath, [System.Text.Encoding]::UTF8.GetString($raw))
            }
        }
    }

    Write-Host ''
    if ($lastExitCode -eq 0) {
        Write-Host ('  Done (exit=0, elapsed={0})' -f $elapsed) -ForegroundColor Green
    }
    else {
        Write-Host ('  Finished with exit={0} (elapsed={1})' -f $lastExitCode, $elapsed) -ForegroundColor Yellow
    }
    Write-Host ('  Log : {0}' -f $consolePath)

    if (Test-Path -LiteralPath $consolePath -PathType Leaf) {
        $tailLines = Get-Content -LiteralPath $consolePath -Tail 6 -Encoding UTF8 -ErrorAction SilentlyContinue
        if ($tailLines) {
            Write-Host ''
            Write-Host '  --- last log lines ---' -ForegroundColor DarkGray
            foreach ($tl in $tailLines) { Write-Host ('  {0}' -f $tl) -ForegroundColor DarkGray }
            Write-Host '  ----------------------' -ForegroundColor DarkGray
        }
    }
    Write-Host ''
}
finally {
    foreach ($tmpPath in @($stdoutPath, $stderrPath)) {
        if (-not [string]::IsNullOrWhiteSpace($tmpPath) -and (Test-Path -LiteralPath $tmpPath -PathType Leaf)) {
            Remove-Item -LiteralPath $tmpPath -Force -ErrorAction SilentlyContinue
        }
    }
    Pop-Location
    $ErrorActionPreference = $previousErrorActionPreference
}

exit $lastExitCode
