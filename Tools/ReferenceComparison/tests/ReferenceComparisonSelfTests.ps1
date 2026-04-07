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

    $stderrTask = $process.StandardError.ReadToEndAsync()
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $stderrTask.GetAwaiter().GetResult()
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
$launcherScript = Join-Path $repoRoot 'Tools\ReferenceComparison\powershell\RunReferenceComparison.ps1'
$launcherBatch = Join-Path $repoRoot 'Tools\ReferenceComparison\RunReferenceComparison.bat'
$codexLauncherScript = Join-Path $repoRoot 'Tools\ReferenceComparison\powershell\RunReferenceComparison_Codex.ps1'
$codexLauncherBatch = Join-Path $repoRoot 'Tools\ReferenceComparison\RunReferenceComparison_Codex.bat'
$ruleDocument = Join-Path $repoRoot 'Documents\Rules\ReferenceComparisonRule_ZH.md'
$pythonPackage = Join-Path $repoRoot 'Tools\ReferenceComparison\python\ReferenceComparison\main.py'

# -----------------------------------------------------------------------
# Test: Required files exist
# -----------------------------------------------------------------------
Invoke-TestCase -Name 'RequiredFilesExist' -Body {
    Assert-True -Condition (Test-Path -LiteralPath $launcherScript -PathType Leaf) `
        -Message 'RunReferenceComparison.ps1 should exist.'
    Assert-True -Condition (Test-Path -LiteralPath $launcherBatch -PathType Leaf) `
        -Message 'RunReferenceComparison.bat should exist.'
    Assert-True -Condition (Test-Path -LiteralPath $codexLauncherScript -PathType Leaf) `
        -Message 'RunReferenceComparison_Codex.ps1 should exist.'
    Assert-True -Condition (Test-Path -LiteralPath $codexLauncherBatch -PathType Leaf) `
        -Message 'RunReferenceComparison_Codex.bat should exist.'
    Assert-True -Condition (Test-Path -LiteralPath $ruleDocument -PathType Leaf) `
        -Message 'ReferenceComparisonRule_ZH.md should exist.'
    Assert-True -Condition (Test-Path -LiteralPath $pythonPackage -PathType Leaf) `
        -Message 'ReferenceComparison/main.py should exist.'
}

# -----------------------------------------------------------------------
# Test: Python package structure
# -----------------------------------------------------------------------
Invoke-TestCase -Name 'PythonPackageStructure' -Body {
    $packageDir = Join-Path $repoRoot 'Tools\ReferenceComparison\python\ReferenceComparison'
    $requiredFiles = @(
        '__init__.py',
        'main.py',
        'config.py',
        'opencode_runner.py',
        'critic.py',
        'prompts.py',
        'utils.py'
    )

    foreach ($fileName in $requiredFiles) {
        $filePath = Join-Path $packageDir $fileName
        Assert-True -Condition (Test-Path -LiteralPath $filePath -PathType Leaf) `
            -Message ("Python module should exist: {0}" -f $fileName)
    }
}

# -----------------------------------------------------------------------
# Test: Python module imports successfully
# -----------------------------------------------------------------------
Invoke-TestCase -Name 'PythonModuleImports' -Body {
    $pythonExe = $null
    foreach ($candidate in @('python', 'python3', 'py')) {
        if (Get-Command $candidate -ErrorAction SilentlyContinue) {
            $pythonExe = $candidate
            break
        }
    }

    if ($null -eq $pythonExe) {
        Write-Host '  [skip] Python not found in PATH, skipping import test' -ForegroundColor Yellow
        return
    }

    $pythonDir = Join-Path $repoRoot 'Tools\ReferenceComparison\python'
    $run = Invoke-CapturedProcess -FilePath $pythonExe -ArgumentList @(
        '-c',
        'from ReferenceComparison.config import REPOS, DIMENSIONS; print(len(REPOS), len(DIMENSIONS))'
    ) -WorkingDirectory $pythonDir

    Assert-Equal -Expected 0 -Actual $run.ExitCode `
        -Message ('Python import should succeed. stderr: {0}' -f $run.StdErr)
    Assert-True -Condition ($run.StdOut.Trim() -eq '5 11') `
        -Message ('Expected 5 repos and 11 dimensions, got: {0}' -f $run.StdOut.Trim())
}

# -----------------------------------------------------------------------
# Test: ReferenceComparison defaults to ralph-loop
# -----------------------------------------------------------------------
Invoke-TestCase -Name 'DefaultCommandIsRalphLoop' -Body {
    $pythonExe = $null
    foreach ($candidate in @('python', 'python3', 'py')) {
        if (Get-Command $candidate -ErrorAction SilentlyContinue) {
            $pythonExe = $candidate
            break
        }
    }

    if ($null -eq $pythonExe) {
        Write-Host '  [skip] Python not found in PATH, skipping default command test' -ForegroundColor Yellow
        return
    }

    $pythonDir = Join-Path $repoRoot 'Tools\ReferenceComparison\python'
    $run = Invoke-CapturedProcess -FilePath $pythonExe -ArgumentList @(
        '-c',
        'from ReferenceComparison.config import RunConfig; print(RunConfig.create(dry_run=True).opencode_command)'
    ) -WorkingDirectory $pythonDir

    Assert-Equal -Expected 0 -Actual $run.ExitCode `
        -Message ('Python default command query should succeed. stderr: {0}' -f $run.StdErr)
    Assert-True -Condition ($run.StdOut.Trim() -eq 'ralph-loop') `
        -Message ('Expected default opencode command to be ralph-loop, got: {0}' -f $run.StdOut.Trim())
}

# -----------------------------------------------------------------------
# Test: Preview mode outputs expected fields
# -----------------------------------------------------------------------
Invoke-TestCase -Name 'PreviewModeOutputsExpectedFields' -Body {
    $pythonExe = $null
    foreach ($candidate in @('python', 'python3', 'py')) {
        if (Get-Command $candidate -ErrorAction SilentlyContinue) {
            $pythonExe = $candidate
            break
        }
    }

    if ($null -eq $pythonExe) {
        Write-Host '  [skip] Python not found in PATH, skipping preview test' -ForegroundColor Yellow
        return
    }

    $pythonDir = Join-Path $repoRoot 'Tools\ReferenceComparison\python'
    $run = Invoke-CapturedProcess -FilePath $pythonExe -ArgumentList @(
        '-m',
        'ReferenceComparison.main',
        '--project-root', $repoRoot,
        '--date-suffix', '2026-04-06',
        '--preview'
    ) -WorkingDirectory $pythonDir

    $combined = $run.StdOut + $run.StdErr
    Assert-Equal -Expected 0 -Actual $run.ExitCode `
        -Message ('Preview mode should exit successfully. Output: {0}' -f $combined)
    Assert-True -Condition ($combined -match 'Status=Preview') `
        -Message 'Preview output should contain Status=Preview.'
    Assert-True -Condition ($combined -match 'OutputDir=.*Comparisons.*2026-04-06') `
        -Message 'Preview output should include dated output directory.'
    Assert-True -Condition ($combined -match 'Repos=') `
        -Message 'Preview output should list repos.'
    Assert-True -Condition ($combined -match 'Max(Iterations|Rounds)=3') `
        -Message 'Preview output should show max rounds.'
}

# -----------------------------------------------------------------------
# Test: Batch wrapper forwards preview arguments
# -----------------------------------------------------------------------
Invoke-TestCase -Name 'BatchWrapperForwardsPreviewArguments' -Body {
    $run = Invoke-CapturedProcess -FilePath 'cmd.exe' -ArgumentList @(
        '/c',
        $launcherBatch,
        '-DateSuffix', '2026-04-06',
        '-Preview'
    ) -WorkingDirectory $repoRoot

    $combined = $run.StdOut + $run.StdErr
    Assert-Equal -Expected 0 -Actual $run.ExitCode `
        -Message ('Batch wrapper preview should exit successfully. Output: {0}' -f $combined)
    Assert-True -Condition ($combined -match 'Status=Preview') `
        -Message 'Batch wrapper should forward preview arguments.'
}

# -----------------------------------------------------------------------
# Test: Codex preview omits explicit model overrides
# -----------------------------------------------------------------------
Invoke-TestCase -Name 'CodexPreviewUsesDefaultModelConfig' -Body {
    $run = Invoke-CapturedProcess -FilePath 'powershell.exe' -ArgumentList @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $codexLauncherScript,
        '-Repos', 'hazelight',
        '-Dimensions', 'D1',
        '-DateSuffix', '2026-04-07',
        '-Preview'
    ) -WorkingDirectory $repoRoot

    $combined = $run.StdOut + $run.StdErr
    Assert-Equal -Expected 0 -Actual $run.ExitCode `
        -Message ('Codex preview should exit successfully. Output: {0}' -f $combined)
    Assert-True -Condition ($combined -match 'Status=Preview') `
        -Message 'Codex preview should contain Status=Preview.'
    Assert-True -Condition ($combined -notmatch '--model\b') `
        -Message 'Codex preview should not pass an explicit --model override.'
    Assert-True -Condition ($combined -notmatch 'model_reasoning_effort') `
        -Message 'Codex preview should not pass a reasoning override.'
}

# -----------------------------------------------------------------------
# Test: Codex batch wrapper forwards preview arguments
# -----------------------------------------------------------------------
Invoke-TestCase -Name 'CodexBatchWrapperForwardsPreviewArguments' -Body {
    $run = Invoke-CapturedProcess -FilePath 'cmd.exe' -ArgumentList @(
        '/c',
        $codexLauncherBatch,
        '-Repos', 'hazelight',
        '-Dimensions', 'D1',
        '-DateSuffix', '2026-04-07',
        '-Preview'
    ) -WorkingDirectory $repoRoot

    $combined = $run.StdOut + $run.StdErr
    Assert-Equal -Expected 0 -Actual $run.ExitCode `
        -Message ('Codex batch preview should exit successfully. Output: {0}' -f $combined)
    Assert-True -Condition ($combined -match 'Status=Preview') `
        -Message 'Codex batch wrapper should forward preview arguments.'
}

# -----------------------------------------------------------------------
# Test: DryRun mode does not invoke opencode
# -----------------------------------------------------------------------
Invoke-TestCase -Name 'DryRunModeNoOpencode' -Body {
    $pythonExe = $null
    foreach ($candidate in @('python', 'python3', 'py')) {
        if (Get-Command $candidate -ErrorAction SilentlyContinue) {
            $pythonExe = $candidate
            break
        }
    }

    if ($null -eq $pythonExe) {
        Write-Host '  [skip] Python not found in PATH, skipping dry-run test' -ForegroundColor Yellow
        return
    }

    $pythonDir = Join-Path $repoRoot 'Tools\ReferenceComparison\python'
    $run = Invoke-CapturedProcess -FilePath $pythonExe -ArgumentList @(
        '-m',
        'ReferenceComparison.main',
        '--project-root', $repoRoot,
        '--date-suffix', 'test-dryrun',
        '--repos', 'unrealcsharp',
        '--dimensions', 'D1',
        '--dry-run'
    ) -WorkingDirectory $pythonDir

    Assert-Equal -Expected 0 -Actual $run.ExitCode `
        -Message ('Dry-run mode should exit successfully. stderr: {0}' -f $run.StdErr)
    $combined = $run.StdOut + $run.StdErr
    Assert-True -Condition ($combined -match 'dry.run') `
        -Message 'Dry-run output should mention dry-run.'
}

# -----------------------------------------------------------------------
# Test: Rule document contains required sections
# -----------------------------------------------------------------------
Invoke-TestCase -Name 'RuleDocumentContainsRequiredSections' -Body {
    $content = [System.IO.File]::ReadAllText($ruleDocument, [System.Text.Encoding]::UTF8)
    $requiredPatterns = @(
        @{ Pattern = 'Round 1'; Label = 'Iterative exploration rounds' },
        @{ Pattern = 'ASCII'; Label = 'ASCII diagram section' },
        @{ Pattern = '\bD1\b'; Label = 'Dimension D1 definition' },
        @{ Pattern = '\bD11\b'; Label = 'Dimension D11 definition' },
        @{ Pattern = 'Per-Repo'; Label = 'Per-repo document structure' },
        @{ Pattern = 'Cross-Comparison'; Label = 'Cross-comparison document structure' }
    )

    foreach ($entry in $requiredPatterns) {
        Assert-True -Condition ($content -match $entry.Pattern) `
            -Message ("Rule document should contain: {0}" -f $entry.Label)
    }
}

Write-Host ''
Write-Host 'Reference comparison self-tests passed.' -ForegroundColor Green
