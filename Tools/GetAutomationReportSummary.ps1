[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ReportPath,

    [string]$LogPath,

    [int]$ExitCode = 0,

    [string]$BucketName,

    [string]$SummaryPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-CaseInsensitivePropertyValue {
    param(
        [Parameter(Mandatory = $true)]
        $Object,

        [Parameter(Mandatory = $true)]
        [string[]]$Names
    )

    if ($null -eq $Object) {
        return $null
    }

    foreach ($property in $Object.PSObject.Properties) {
        foreach ($name in $Names) {
            if ($property.Name.Equals($name, [System.StringComparison]::OrdinalIgnoreCase)) {
                return $property.Value
            }
        }
    }

    return $null
}

function ConvertTo-NormalizedState {
    param([AllowNull()][string]$State)

    if ([string]::IsNullOrWhiteSpace($State)) {
        return 'Unknown'
    }

    switch -Regex ($State.Trim()) {
        '^(Success|Succeeded|Pass|Passed)$' { return 'Passed' }
        '^(Fail|Failed|Error)$' { return 'Failed' }
        '^(Skipped|Skip|NotRun|NotExecuted)$' { return 'Skipped' }
        '^(InProcess|Running)$' { return 'InProcess' }
        default { return $State.Trim() }
    }
}

function Get-PreferredReportFiles {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return @()
    }

    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        return ,(Get-Item -LiteralPath $Path)
    }

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        return @()
    }

    $priorityNames = @('index.json', 'report.json', 'AutomationReport.json', 'TestReport.json')
    $files = Get-ChildItem -LiteralPath $Path -Recurse -File -Filter *.json

    return $files | Sort-Object @(
        { $priorityIndex = [Array]::IndexOf($priorityNames, $_.Name); if ($priorityIndex -ge 0) { $priorityIndex } else { 100 } },
        { $_.FullName.Length },
        { $_.FullName }
    )
}

function Add-ReportRecord {
    param(
        [Parameter(Mandatory = $true)]
        [System.Collections.Generic.List[object]]$Records,

        [Parameter(Mandatory = $true)]
        $Node
    )

    $name = Get-CaseInsensitivePropertyValue $Node @('FullTestPath', 'FullTestName', 'DisplayName', 'TestDisplayName', 'TestName', 'Name')
    $state = Get-CaseInsensitivePropertyValue $Node @('State', 'Status', 'Result', 'Outcome', 'TestState')

    if ([string]::IsNullOrWhiteSpace([string]$name) -or [string]::IsNullOrWhiteSpace([string]$state)) {
        return
    }

    $message = Get-CaseInsensitivePropertyValue $Node @('Message', 'Error', 'Errors', 'Warnings')
    if ($message -is [System.Collections.IEnumerable] -and -not ($message -is [string])) {
        $message = (($message | ForEach-Object { [string]$_ }) -join '; ').Trim()
    }

    $Records.Add([PSCustomObject]@{
            Name    = [string]$name
            State   = ConvertTo-NormalizedState ([string]$state)
            Message = [string]$message
        })
}

function Visit-JsonNode {
    param(
        [Parameter(Mandatory = $true)]
        $Node,

        [Parameter(Mandatory = $true)]
        [System.Collections.Generic.List[object]]$Records
    )

    if ($null -eq $Node) {
        return
    }

    if ($Node -is [string] -or $Node -is [ValueType]) {
        return
    }

    if ($Node -is [System.Collections.IEnumerable] -and -not ($Node -is [System.Collections.IDictionary])) {
        foreach ($item in $Node) {
            Visit-JsonNode -Node $item -Records $Records
        }

        return
    }

    if ($Node -is [System.Collections.IDictionary] -or $Node.PSObject.Properties.Count -gt 0) {
        Add-ReportRecord -Records $Records -Node $Node

        foreach ($property in $Node.PSObject.Properties) {
            Visit-JsonNode -Node $property.Value -Records $Records
        }

        return
    }
}

function Get-TotalsFromNode {
    param([Parameter(Mandatory = $true)]$Node)

    $passed = Get-CaseInsensitivePropertyValue $Node @('Succeeded', 'Passed', 'PassCount')
    $failed = Get-CaseInsensitivePropertyValue $Node @('Failed', 'FailureCount', 'Failures')
    $skipped = Get-CaseInsensitivePropertyValue $Node @('NotRun', 'Skipped', 'SkipCount')
    $total = Get-CaseInsensitivePropertyValue $Node @('Total', 'TotalCount', 'NumTests')

    if ($null -eq $passed -and $null -eq $failed -and $null -eq $skipped -and $null -eq $total) {
        return $null
    }

    return [PSCustomObject]@{
        Total   = if ($null -ne $total) { [int]$total } else { $null }
        Passed  = if ($null -ne $passed) { [int]$passed } else { $null }
        Failed  = if ($null -ne $failed) { [int]$failed } else { $null }
        Skipped = if ($null -ne $skipped) { [int]$skipped } else { $null }
    }
}

function Try-GetReportSummaryFromJson {
    param([Parameter(Mandatory = $true)][string]$JsonFile)

    try {
        $raw = Get-Content -LiteralPath $JsonFile -Raw -Encoding UTF8
        if ([string]::IsNullOrWhiteSpace($raw)) {
            return $null
        }

        $node = $raw | ConvertFrom-Json
        $records = [System.Collections.Generic.List[object]]::new()

        $topLevelTotals = [PSCustomObject]@{
            Total   = Get-CaseInsensitivePropertyValue $node @('Total', 'TotalCount', 'NumTests')
            Passed  = Get-CaseInsensitivePropertyValue $node @('Succeeded', 'Passed', 'PassCount')
            Failed  = Get-CaseInsensitivePropertyValue $node @('Failed', 'FailureCount', 'Failures')
            Skipped = Get-CaseInsensitivePropertyValue $node @('NotRun', 'Skipped', 'SkipCount')
        }

        $candidateCollections = @(
            (Get-CaseInsensitivePropertyValue $node @('Tests', 'TestResults', 'Results', 'Entries', 'FailedTests')),
            (Get-CaseInsensitivePropertyValue $node @('SucceededTests')),
            (Get-CaseInsensitivePropertyValue $node @('FailedTests'))
        ) | Where-Object { $null -ne $_ }

        foreach ($collection in $candidateCollections) {
            foreach ($item in @($collection)) {
                $name = Get-CaseInsensitivePropertyValue $item @('FullTestPath', 'FullTestName', 'DisplayName', 'TestDisplayName', 'TestName', 'Name')
                $state = Get-CaseInsensitivePropertyValue $item @('State', 'Status', 'Result', 'Outcome', 'TestState')
                $message = Get-CaseInsensitivePropertyValue $item @('Message', 'Error', 'Errors', 'Warnings')

                if ($message -is [System.Collections.IEnumerable] -and -not ($message -is [string])) {
                    $message = (($message | ForEach-Object { [string]$_ }) -join '; ').Trim()
                }

                if (-not [string]::IsNullOrWhiteSpace([string]$name) -and -not [string]::IsNullOrWhiteSpace([string]$state)) {
                    $records.Add([PSCustomObject]@{
                            Name    = [string]$name
                            State   = ConvertTo-NormalizedState ([string]$state)
                            Message = [string]$message
                        })
                }
            }
        }

        $passedCount = @($records | Where-Object { $_.State -eq 'Passed' }).Count
        $failedCount = @($records | Where-Object { $_.State -eq 'Failed' }).Count
        $skippedCount = @($records | Where-Object { $_.State -eq 'Skipped' }).Count
        $totalCount = if ($records.Count -gt 0) { $records.Count } else { $null }
        $totals = [PSCustomObject]@{
            Total   = if ($null -ne $topLevelTotals.Total) { [int]$topLevelTotals.Total } else { $totalCount }
            Passed  = if ($null -ne $topLevelTotals.Passed) { [int]$topLevelTotals.Passed } else { if ($totalCount -ne $null) { $passedCount } else { $null } }
            Failed  = if ($null -ne $topLevelTotals.Failed) { [int]$topLevelTotals.Failed } else { if ($totalCount -ne $null) { $failedCount } else { $null } }
            Skipped = if ($null -ne $topLevelTotals.Skipped) { [int]$topLevelTotals.Skipped } else { if ($totalCount -ne $null) { $skippedCount } else { $null } }
        }

        return [PSCustomObject]@{
            SourceFile   = $JsonFile
            Total        = $totals.Total
            Passed       = $totals.Passed
            Failed       = $totals.Failed
            Skipped      = $totals.Skipped
            FailedTests  = @($records | Where-Object { $_.State -eq 'Failed' } | Select-Object -Unique Name, Message)
            RecordCount  = $records.Count
        }
    }
    catch {
        return $null
    }
}

function Get-LogFailureHints {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return @()
    }

    $content = Get-Content -LiteralPath $Path -Encoding UTF8
    $matches = foreach ($line in $content) {
        $hasBlockingMessage = $line -match '无法被找到|could not be found|No automation tests matched|Fatal error:'
        $hasBlockingError = $false
        if ($hasBlockingMessage -or $hasBlockingError) {
            $line
        }
    }

    return @($matches | Select-Object -First 20)
}

function Get-NonEmptyItemCount {
    param($Value)

    return @($Value | Where-Object { $null -ne $_ -and -not [string]::IsNullOrWhiteSpace([string]$_) }).Count
}

$summary = [ordered]@{
    BucketName          = $BucketName
    ExitCode            = $ExitCode
    ReportPath          = $ReportPath
    LogPath             = $LogPath
    ReportJsonPath      = $null
    Total               = $null
    Passed              = $null
    Failed              = $null
    Skipped             = $null
    FailedTests         = @()
    LogFailureHints     = @()
    SummarySource       = 'None'
}

$reportSummary = $null
foreach ($candidate in Get-PreferredReportFiles -Path $ReportPath) {
    $candidateSummary = Try-GetReportSummaryFromJson -JsonFile $candidate.FullName
    if ($null -ne $candidateSummary) {
        $reportSummary = $candidateSummary
        break
    }
}

if ($null -ne $reportSummary) {
    $summary.ReportJsonPath = $reportSummary.SourceFile
    $summary.Total = $reportSummary.Total
    $summary.Passed = $reportSummary.Passed
    $summary.Failed = $reportSummary.Failed
    $summary.Skipped = $reportSummary.Skipped
    $summary.FailedTests = $reportSummary.FailedTests
    $summary.SummarySource = 'ReportJson'
}

$summary.LogFailureHints = @(Get-LogFailureHints -Path $LogPath)

$summaryObject = [PSCustomObject]$summary
$serializableSummary = [PSCustomObject]@{
    BucketName      = [string]$summaryObject.BucketName
    ExitCode        = [int]$summaryObject.ExitCode
    ReportPath      = [string]$summaryObject.ReportPath
    LogPath         = [string]$summaryObject.LogPath
    ReportJsonPath  = if ($null -ne $summaryObject.ReportJsonPath) { [string]$summaryObject.ReportJsonPath } else { $null }
    Total           = if ($null -ne $summaryObject.Total) { [int]$summaryObject.Total } else { $null }
    Passed          = if ($null -ne $summaryObject.Passed) { [int]$summaryObject.Passed } else { $null }
    Failed          = if ($null -ne $summaryObject.Failed) { [int]$summaryObject.Failed } else { $null }
    Skipped         = if ($null -ne $summaryObject.Skipped) { [int]$summaryObject.Skipped } else { $null }
    FailedTests     = @($summaryObject.FailedTests | ForEach-Object {
            [PSCustomObject]@{
                Name    = [string]$_.Name
                Message = [string]$_.Message
            }
        })
    LogFailureHints = @($summaryObject.LogFailureHints | ForEach-Object { [string]$_ })
    SummarySource   = [string]$summaryObject.SummarySource
}

if (-not [string]::IsNullOrWhiteSpace($SummaryPath)) {
    $summaryDirectory = Split-Path -Parent $SummaryPath
    if (-not [string]::IsNullOrWhiteSpace($summaryDirectory)) {
        New-Item -ItemType Directory -Path $summaryDirectory -Force | Out-Null
    }

    $serializableSummary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $SummaryPath -Encoding UTF8
}

if ([string]::IsNullOrWhiteSpace($BucketName)) {
    Write-Host 'Bucket: <none>'
}
else {
    Write-Host ("Bucket: {0}" -f $BucketName)
}
Write-Host ("ExitCode: {0}" -f $ExitCode)
if ($summaryObject.ReportJsonPath) {
    Write-Host ("ReportJson: {0}" -f $summaryObject.ReportJsonPath)
}
if ($summaryObject.Total -ne $null) {
    Write-Host ("Totals: total={0} passed={1} failed={2} skipped={3}" -f $summaryObject.Total, $summaryObject.Passed, $summaryObject.Failed, $summaryObject.Skipped)
}
if (Get-NonEmptyItemCount $summaryObject.FailedTests -gt 0) {
    Write-Host 'FailedTests:'
    foreach ($failedTest in $summaryObject.FailedTests) {
        if ([string]::IsNullOrWhiteSpace($failedTest.Message)) {
            Write-Host ("- {0}" -f $failedTest.Name)
        }
        else {
            Write-Host ("- {0}: {1}" -f $failedTest.Name, $failedTest.Message)
        }
    }
}
elseif (Get-NonEmptyItemCount $summaryObject.LogFailureHints -gt 0) {
    Write-Host 'LogFailureHints:'
    foreach ($hint in $summaryObject.LogFailureHints) {
        Write-Host ("- {0}" -f $hint)
    }
}

$serializableSummary
