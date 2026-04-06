[CmdletBinding()]
param(
    [string]$ProjectRoot = '',

    [string]$DateSuffix = '',

    [switch]$Preview,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AdditionalRequirements
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Format-DisplayArgument {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    if ($Value -match '[\s"]') {
        return '"' + ($Value -replace '"', '\"') + '"'
    }

    return $Value
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
}
else {
    $ProjectRoot = (Resolve-Path $ProjectRoot).Path
}

if ([string]::IsNullOrWhiteSpace($DateSuffix)) {
    $DateSuffix = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
}

$ruleRelativePath = 'Documents/Rules/ReviewRule_ZH.md'
$outputRelativePath = 'Documents/Reviews/Review_{0}.md' -f $DateSuffix
$logRelativePath = 'Documents/Reviews/run.log'
$rulePath = Join-Path $ProjectRoot $ruleRelativePath
$outputPath = Join-Path $ProjectRoot $outputRelativePath
$logPath = Join-Path $ProjectRoot $logRelativePath
$outputDirectory = Split-Path -Parent $outputPath

if (-not (Test-Path -LiteralPath $rulePath -PathType Leaf)) {
    throw ("Required review rule document was not found: {0}" -f $rulePath)
}

$promptLines = [System.Collections.Generic.List[string]]::new()
$promptLines.Add('Strictly follow Documents/Rules/ReviewRule_ZH.md and perform a full audit of the current mainline state of this repository.') | Out-Null
$promptLines.Add(('Write the review document to {0}.' -f ($outputRelativePath -replace '\\', '/'))) | Out-Null
$promptLines.Add('Create Documents/Reviews first if it does not exist.') | Out-Null
$promptLines.Add('Focus on Plugins/Angelscript as the primary deliverable; treat the host project only as supporting context.') | Out-Null
$promptLines.Add('The output must cover progress, project problems, risk levels, documentation and test gaps, suggested actions, and a final conclusion.') | Out-Null
$promptLines.Add('Distinguish verified facts from inferences, and ground conclusions in concrete modules, files, or documents whenever possible.') | Out-Null

if ($null -ne $AdditionalRequirements -and $AdditionalRequirements.Count -gt 0) {
    $extraText = ($AdditionalRequirements -join ' ').Trim()
    if (-not [string]::IsNullOrWhiteSpace($extraText)) {
        $promptLines.Add(('Additional requirement: {0}' -f $extraText)) | Out-Null
    }
}

$prompt = ($promptLines -join [Environment]::NewLine)
$argumentList = @(
    'run',
    '--dir',
    $ProjectRoot,
    '--agent',
    'Hephaestus',
    '--model',
    'codez-gpt/gpt-5.4',
    '--variant',
    'xhigh',
    '--command',
    'ralph-loop',
    $prompt
)

if ($Preview) {
    $displayCommand = 'opencode ' + (($argumentList | ForEach-Object { Format-DisplayArgument -Value ([string]$_) }) -join ' ')
    Write-Output 'Status=Preview'
    Write-Output ('ProjectRoot={0}' -f $ProjectRoot)
    Write-Output ('RulePath={0}' -f $rulePath)
    Write-Output ('OutputRelativePath={0}' -f ($outputRelativePath -replace '\\', '/'))
    Write-Output ('LogRelativePath={0}' -f ($logRelativePath -replace '\\', '/'))
    Write-Output ('Command={0}' -f $displayCommand)
    Write-Output ('Prompt={0}' -f ($prompt -replace "\r?\n", ' \n '))
    exit 0
}

if (-not (Get-Command opencode -ErrorAction SilentlyContinue)) {
    throw 'The opencode command was not found in PATH.'
}

New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

$previousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'

try {
    & opencode @argumentList 2>&1 |
        ForEach-Object {
            if ($_ -is [System.Management.Automation.ErrorRecord]) {
                $_.ToString()
            }
            else {
                [string]$_
            }
        } |
        Tee-Object -FilePath $logPath
}
finally {
    $ErrorActionPreference = $previousErrorActionPreference
}

if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
    Set-Content -LiteralPath $logPath -Value $null -Encoding UTF8
}

$exitCode = $LASTEXITCODE
exit $exitCode
