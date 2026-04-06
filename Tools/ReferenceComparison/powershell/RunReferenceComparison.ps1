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

Push-Location (Join-Path $PSScriptRoot '..\python')
try {
    & $pythonExe @argumentList 2>&1 | ForEach-Object { $_ }
}
finally {
    Pop-Location
}

exit $LASTEXITCODE
