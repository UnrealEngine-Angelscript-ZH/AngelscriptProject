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

function Invoke-TestCase {
    param(
        [string]$Name,
        [scriptblock]$Body
    )

    Write-Host ("[test] {0}" -f $Name)
    try {
        & $Body
        $completedTests.Add($Name) | Out-Null
    }
    catch {
        $failureMessage = "{0}: {1}" -f $Name, $_.Exception.Message
        if (-not [string]::IsNullOrWhiteSpace($_.ScriptStackTrace)) {
            $failureMessage = "{0} Stack={1}" -f $failureMessage, $_.ScriptStackTrace
        }
        $testFailures.Add($failureMessage) | Out-Null
        Write-Host ("[fail] {0}" -f $_.Exception.Message) -ForegroundColor Red
    }
}

function Get-PolicyAuditDocumentPaths {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot
    )

    $paths = New-Object System.Collections.Generic.List[string]
    $guidesRoot = Join-Path $RepositoryRoot 'Documents\Guides'
    if (Test-Path -LiteralPath $guidesRoot -PathType Container) {
        foreach ($doc in @(Get-ChildItem -LiteralPath $guidesRoot -File -Recurse)) {
            $paths.Add($doc.FullName) | Out-Null
        }
    }

    $plansRoot = Join-Path $RepositoryRoot 'Documents\Plans'
    if (Test-Path -LiteralPath $plansRoot -PathType Container) {
        foreach ($doc in @(Get-ChildItem -LiteralPath $plansRoot -File -Recurse)) {
            $paths.Add($doc.FullName) | Out-Null
        }
    }

    $toolCatalog = Join-Path $RepositoryRoot 'Documents\Tools\Tool.md'
    if (Test-Path -LiteralPath $toolCatalog -PathType Leaf) {
        $paths.Add($toolCatalog) | Out-Null
    }

    foreach ($agentDocName in @('AGENTS.md', 'AGENTS_ZH.md')) {
        $agentDocPath = Join-Path $RepositoryRoot $agentDocName
        if (Test-Path -LiteralPath $agentDocPath -PathType Leaf) {
            $paths.Add($agentDocPath) | Out-Null
        }
    }

    $deduped = @($paths | Sort-Object -Unique | Where-Object {
            $_ -notmatch '[\\/]+Archives[\\/]'
        })
    return $deduped
}

function Get-FencedCodeLines {
    param(
        [string[]]$Lines
    )

    $insideFence = $false
    $results = New-Object System.Collections.Generic.List[object]
    for ($index = 0; $index -lt $Lines.Count; $index++) {
        $lineText = [string]$Lines[$index]
        if ($lineText.TrimStart().StartsWith('```')) {
            $insideFence = -not $insideFence
            continue
        }

        if ($insideFence) {
            $results.Add([pscustomobject]@{
                    Source      = 'Fence'
                    LineNumber  = $index + 1
                    Content     = $lineText.Trim()
                }) | Out-Null
        }
    }

    return $results.ToArray()
}

function Get-InlineCodeSnippets {
    param(
        [string[]]$Lines
    )

    $results = New-Object System.Collections.Generic.List[object]
    $inlineRegex = [regex]'`([^`]+)`'

    for ($index = 0; $index -lt $Lines.Count; $index++) {
        $lineText = [string]$Lines[$index]
        foreach ($match in $inlineRegex.Matches($lineText)) {
            $snippet = [string]$match.Groups[1].Value
            $results.Add([pscustomobject]@{
                    Source      = 'Inline'
                    LineNumber  = $index + 1
                    Content     = $snippet.Trim()
                    RawLine     = $lineText
                }) | Out-Null
        }
    }

    return $results.ToArray()
}

function Test-IsCommandSnippet {
    param(
        [string]$Snippet
    )

    if ([string]::IsNullOrWhiteSpace($Snippet)) {
        return $false
    }

    $trimmed = $Snippet.Trim()
    if ($trimmed -match '^(?:-|\|)') {
        return $false
    }

    if ($trimmed -match '(?i)^(?:PS [^>]+>\s*)?(?:\.\\|./|Tools[\\/]|powershell(?:\.exe)?\b|pwsh(?:\.exe)?\b|cmd(?:\.exe)?\b|dotnet\b|Start-Process\b|<EngineRoot>|[A-Za-z]:\\|UnrealEditor-Cmd\.exe\b|Build\.bat\b|RunUBT\.bat\b)') {
        return $true
    }

    if ($trimmed -match '(?i)(Build\.bat|RunUBT\.bat|RunAutomationTests\.ps1|UnrealEditor-Cmd\.exe)') {
        if ($trimmed -match '(?i)(-ExecCmds|-Group|-Prefix|-TestPrefix|-File|-Command|-Unattended|-NoPause|-NoSplash|-NullRHI|-NOSOUND|\.uproject)') {
            return $true
        }
    }

    return $false
}

function Get-ForbiddenCommandViolations {
    param(
        [Parameter(Mandatory = $true)]
        [string]$DocumentPath
    )

    $lines = @(Get-Content -LiteralPath $DocumentPath -Encoding UTF8)
    $violations = New-Object System.Collections.Generic.List[object]

    $candidates = New-Object System.Collections.Generic.List[object]
    foreach ($entry in @(Get-FencedCodeLines -Lines $lines)) {
        if (Test-IsCommandSnippet -Snippet $entry.Content) {
            $candidates.Add($entry) | Out-Null
        }
    }

    foreach ($entry in @(Get-InlineCodeSnippets -Lines $lines)) {
        if (Test-IsCommandSnippet -Snippet $entry.Content) {
            $candidates.Add($entry) | Out-Null
        }
    }

    foreach ($candidate in $candidates.ToArray()) {
        $snippet = [string]$candidate.Content
        if ([string]::IsNullOrWhiteSpace($snippet)) {
            continue
        }

        if ($candidate.Source -eq 'Inline') {
            $contextLine = [string]$candidate.RawLine
            if ($contextLine -match '(?i)(不再允许|不允许|不得|不要|禁止|not\s+allow|do\s+not|don''t|forbid|forbidden|avoid)') {
                continue
            }
        }

        if ($snippet -match '(?i)\bBuild\.bat\b' -and $snippet -match '(?i)(^|[\s''"\\/])Build\.bat\b.*\s-\w') {
            $violations.Add([pscustomobject]@{
                    Policy     = 'BuildBat'
                    Path       = $DocumentPath
                    LineNumber = [int]$candidate.LineNumber
                    Snippet    = $snippet
                }) | Out-Null
        }

        if ($snippet -match '(?i)\bRunUBT\.bat\b' -and $snippet -match '(?i)(^|[\s''"\\/])RunUBT\.bat\b.*\s-\w') {
            $violations.Add([pscustomobject]@{
                    Policy     = 'RunUbtBat'
                    Path       = $DocumentPath
                    LineNumber = [int]$candidate.LineNumber
                    Snippet    = $snippet
                }) | Out-Null
        }

        if ($snippet -match '(?i)\bRunAutomationTests\.ps1\b' -and $snippet -match '(?i)\bRunAutomationTests\.ps1\b.*\s-\w') {
            $violations.Add([pscustomobject]@{
                    Policy     = 'RunAutomationTestsPs1'
                    Path       = $DocumentPath
                    LineNumber = [int]$candidate.LineNumber
                    Snippet    = $snippet
                }) | Out-Null
        }

        if ($snippet -match '(?i)\bUnrealEditor-Cmd\.exe\b' -and $snippet -match '(?i)(-ExecCmds|\.uproject|-Unattended|-NoPause|-NoSplash|-NullRHI|-NOSOUND)') {
            $violations.Add([pscustomobject]@{
                    Policy     = 'DirectUnrealEditorCmd'
                    Path       = $DocumentPath
                    LineNumber = [int]$candidate.LineNumber
                    Snippet    = $snippet
                }) | Out-Null
        }
    }

    return $violations.ToArray()
}

Invoke-TestCase -Name 'PolicyAuditDocsAvoidForbiddenLiveCommands' -Body {
    $docPaths = @(Get-PolicyAuditDocumentPaths -RepositoryRoot $ProjectRoot)
    Assert-True -Condition ($docPaths.Count -gt 0) -Message 'Expected at least one policy-audited documentation file.'

    $allViolations = New-Object System.Collections.Generic.List[object]
    foreach ($docPath in $docPaths) {
        foreach ($violation in @(Get-ForbiddenCommandViolations -DocumentPath $docPath)) {
            $allViolations.Add($violation) | Out-Null
        }
    }

    if ($allViolations.Count -gt 0) {
        $preview = @($allViolations | Select-Object -First 12 | ForEach-Object {
                '{0} {1}:{2} => {3}' -f $_.Policy, $_.Path, $_.LineNumber, $_.Snippet
            })
        $message = "Forbidden live command examples found in policy-audited docs. " + ($preview -join ' | ')
        throw $message
    }
}

Invoke-TestCase -Name 'PolicyAuditDocsReferenceOfficialRunners' -Body {
    $docPaths = @(Get-PolicyAuditDocumentPaths -RepositoryRoot $ProjectRoot)
    $fullText = ($docPaths | ForEach-Object {
            Get-Content -LiteralPath $_ -Raw -Encoding UTF8
        }) -join "`n"

    $runBuildHits = ([regex]::Matches($fullText, '(?i)\bRunBuild\.ps1\b')).Count
    $runTestsHits = ([regex]::Matches($fullText, '(?i)\bRunTests\.ps1\b')).Count
    Assert-True -Condition ($runBuildHits -gt 0) -Message 'Expected policy-audited docs to reference RunBuild.ps1.'
    Assert-True -Condition ($runTestsHits -gt 0) -Message 'Expected policy-audited docs to reference RunTests.ps1.'
}

Invoke-TestCase -Name 'OutputLayoutContractForBuildAndTests' -Body {
    $tempProjectRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('PolicyAuditOutputLayout_{0}' -f ([Guid]::NewGuid().ToString('N')))
    New-Item -ItemType Directory -Path $tempProjectRoot -Force | Out-Null

    try {
        $buildLayout = New-CommandOutputLayout -ProjectRoot $tempProjectRoot -Category 'Build' -Label 'policy-audit' -LogFileName 'Build.log'
        Assert-True -Condition ($buildLayout.OutputRoot -match [regex]::Escape('\Saved\Build\policy-audit\')) -Message 'Build output root should resolve under Saved\Build\<Label>\<Timestamp>.'
        Assert-Equal -Actual ([System.IO.Path]::GetFileName($buildLayout.LogPath)) -Expected 'Build.log' -Message 'Build layout should use Build.log.'
        Assert-Equal -Actual ([System.IO.Path]::GetFileName($buildLayout.ReportPath)) -Expected 'Report' -Message 'Build layout should keep the default report folder.'
        Assert-True -Condition (Test-Path -LiteralPath $buildLayout.OutputRoot -PathType Container) -Message 'Build output directory should be created.'

        $testLayout = New-CommandOutputLayout -ProjectRoot $tempProjectRoot -Category 'Tests' -Label 'policy-audit' -LogFileName 'Automation.log'
        Assert-True -Condition ($testLayout.OutputRoot -match [regex]::Escape('\Saved\Tests\policy-audit\')) -Message 'Test output root should resolve under Saved\Tests\<Label>\<Timestamp>.'
        Assert-Equal -Actual ([System.IO.Path]::GetFileName($testLayout.LogPath)) -Expected 'Automation.log' -Message 'Test layout should use Automation.log.'
        Assert-Equal -Actual ([System.IO.Path]::GetFileName($testLayout.ReportPath)) -Expected 'Report' -Message 'Test layout should keep the default report folder.'
        Assert-True -Condition (Test-Path -LiteralPath $testLayout.OutputRoot -PathType Container) -Message 'Test output directory should be created.'
    }
    finally {
        if (Test-Path -LiteralPath $tempProjectRoot -PathType Container) {
            Remove-Item -LiteralPath $tempProjectRoot -Recurse -Force
        }
    }
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
