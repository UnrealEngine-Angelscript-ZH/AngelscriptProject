[CmdletBinding()]
param(
    [string]$ProjectRoot = '',

    [switch]$AllRegisteredWorktrees,

    [string]$EngineRoot = '',

    [switch]$NoPrewarm,

    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'Shared\UnrealCommandUtils.ps1')

function Resolve-BootstrapProjectRoot {
    param(
        [string]$ProjectRootValue
    )

    if ([string]::IsNullOrWhiteSpace($ProjectRootValue)) {
        return (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
    }

    return (Resolve-Path $ProjectRootValue).Path
}

function Get-RegisteredWorktreeRoots {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot
    )

    $gitOutput = & git -C $RepositoryRoot worktree list --porcelain 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to enumerate git worktrees from '$RepositoryRoot'."
    }

    $roots = New-Object System.Collections.Generic.List[string]
    foreach ($line in $gitOutput) {
        if ($line -like 'worktree *') {
            $roots.Add((Normalize-PathValue -Path $line.Substring(9))) | Out-Null
        }
    }

    return @($roots)
}

function Get-WorktreeConfigSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorktreeRoot
    )

    $configPath = Join-Path $WorktreeRoot 'AgentConfig.ini'
    if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
        return $null
    }

    return Read-IniFile -Path $configPath
}

function Get-PreferredConfigValue {
    param(
        [hashtable]$PrimaryConfig,
        [hashtable]$FallbackConfig,
        [Parameter(Mandatory = $true)]
        [string]$Section,
        [Parameter(Mandatory = $true)]
        [string]$Key,
        [string]$DefaultValue = ''
    )

    $primaryValue = if ($null -ne $PrimaryConfig) {
        Get-IniValue -Config $PrimaryConfig -Section $Section -Key $Key -DefaultValue ''
    }
    else {
        ''
    }

    if (-not [string]::IsNullOrWhiteSpace($primaryValue)) {
        return $primaryValue
    }

    $fallbackValue = if ($null -ne $FallbackConfig) {
        Get-IniValue -Config $FallbackConfig -Section $Section -Key $Key -DefaultValue ''
    }
    else {
        ''
    }

    if (-not [string]::IsNullOrWhiteSpace($fallbackValue)) {
        return $fallbackValue
    }

    return $DefaultValue
}

function Resolve-ProjectFileForBootstrap {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorktreeRoot
    )

    $uproject = Get-ChildItem -LiteralPath $WorktreeRoot -Filter *.uproject -File | Select-Object -First 1
    if ($null -eq $uproject) {
        throw "Could not resolve a .uproject file from '$WorktreeRoot'."
    }

    return (Normalize-PathValue -Path $uproject.FullName)
}

function Get-BootstrapConfigTemplate {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorktreeRoot,

        [string]$EngineRootOverride,

        [hashtable]$ExistingConfig,

        [hashtable]$SourceConfig
    )

    $resolvedProjectFile = Resolve-ProjectFileForBootstrap -WorktreeRoot $WorktreeRoot
    $resolvedEngineRoot = if (-not [string]::IsNullOrWhiteSpace($EngineRootOverride)) {
        Normalize-PathValue -Path $EngineRootOverride
    }
    else {
        $candidateEngineRoot = Get-PreferredConfigValue -PrimaryConfig $ExistingConfig -FallbackConfig $SourceConfig -Section 'Paths' -Key 'EngineRoot' -DefaultValue ''
        if ([string]::IsNullOrWhiteSpace($candidateEngineRoot)) {
            throw "Could not resolve EngineRoot for '$WorktreeRoot'. Pass -EngineRoot or bootstrap from a worktree that already has AgentConfig.ini."
        }
        Normalize-PathValue -Path $candidateEngineRoot
    }

    return [ordered]@{
        Paths      = [ordered]@{
            EngineRoot  = $resolvedEngineRoot
            ProjectFile = $resolvedProjectFile
        }
        Build      = [ordered]@{
            Architecture     = Get-PreferredConfigValue -PrimaryConfig $ExistingConfig -FallbackConfig $SourceConfig -Section 'Build' -Key 'Architecture' -DefaultValue 'x64'
            Configuration    = Get-PreferredConfigValue -PrimaryConfig $ExistingConfig -FallbackConfig $SourceConfig -Section 'Build' -Key 'Configuration' -DefaultValue 'Development'
            DefaultTimeoutMs = Get-PreferredConfigValue -PrimaryConfig $ExistingConfig -FallbackConfig $SourceConfig -Section 'Build' -Key 'DefaultTimeoutMs' -DefaultValue '180000'
            EditorTarget     = Get-PreferredConfigValue -PrimaryConfig $ExistingConfig -FallbackConfig $SourceConfig -Section 'Build' -Key 'EditorTarget' -DefaultValue 'AngelscriptProjectEditor'
            Platform         = Get-PreferredConfigValue -PrimaryConfig $ExistingConfig -FallbackConfig $SourceConfig -Section 'Build' -Key 'Platform' -DefaultValue 'Win64'
        }
        References = [ordered]@{
            HazelightAngelscriptEngineRoot = Get-PreferredConfigValue -PrimaryConfig $ExistingConfig -FallbackConfig $SourceConfig -Section 'References' -Key 'HazelightAngelscriptEngineRoot' -DefaultValue ''
        }
        Test       = [ordered]@{
            DefaultTimeoutMs = Get-PreferredConfigValue -PrimaryConfig $ExistingConfig -FallbackConfig $SourceConfig -Section 'Test' -Key 'DefaultTimeoutMs' -DefaultValue '600000'
        }
    }
}

function Invoke-WorktreeBootstrap {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorktreeRoot,

        [string]$EngineRootOverride,

        [hashtable]$SourceConfig,

        [switch]$SkipPrewarm,

        [switch]$ForceRewrite
    )

    $resolvedWorktreeRoot = Normalize-PathValue -Path $WorktreeRoot
    $configPath = Join-Path $resolvedWorktreeRoot 'AgentConfig.ini'
    $existingConfig = Get-WorktreeConfigSnapshot -WorktreeRoot $resolvedWorktreeRoot
    $targetConfig = Get-BootstrapConfigTemplate -WorktreeRoot $resolvedWorktreeRoot -EngineRootOverride $EngineRootOverride -ExistingConfig $existingConfig -SourceConfig $SourceConfig

    $shouldWrite = $true
    if (-not $ForceRewrite -and $null -ne $existingConfig) {
        $existingJson = $existingConfig | ConvertTo-Json -Depth 8
        $targetJson = $targetConfig | ConvertTo-Json -Depth 8
        $shouldWrite = $existingJson -ne $targetJson
    }

    if ($shouldWrite) {
        Write-IniFile -Path $configPath -Config $targetConfig
        Write-Host ("[bootstrap] Wrote AgentConfig.ini: {0}" -f $configPath)
    }
    else {
        Write-Host ("[bootstrap] AgentConfig.ini already normalized: {0}" -f $configPath)
    }

    if ($SkipPrewarm) {
        return
    }

    $prewarm = Ensure-TargetInfoJson -EngineRoot $targetConfig.Paths.EngineRoot -ProjectFile $targetConfig.Paths.ProjectFile -ProjectRoot $resolvedWorktreeRoot
    if ($prewarm.Status -eq 'TimedOut' -or $prewarm.Status -eq 'Failed') {
        throw "TargetInfo.json prewarm failed for '$resolvedWorktreeRoot': $($prewarm.Message)"
    }
}

$resolvedProjectRoot = Resolve-BootstrapProjectRoot -ProjectRootValue $ProjectRoot
$worktreeRoots = if ($AllRegisteredWorktrees) {
    Get-RegisteredWorktreeRoots -RepositoryRoot $resolvedProjectRoot
}
else {
    @($resolvedProjectRoot)
}

$normalizedWorktreeRoots = @($worktreeRoots | ForEach-Object { Normalize-PathValue -Path $_ } | Select-Object -Unique)
foreach ($worktreeRoot in $normalizedWorktreeRoots) {
    $sourceConfig = $null
    foreach ($candidateRoot in $normalizedWorktreeRoots) {
        if ($candidateRoot -eq $worktreeRoot) {
            continue
        }

        $candidateConfig = Get-WorktreeConfigSnapshot -WorktreeRoot $candidateRoot
        if ($null -ne $candidateConfig) {
            $sourceConfig = $candidateConfig
            break
        }
    }

    Invoke-WorktreeBootstrap -WorktreeRoot $worktreeRoot -EngineRootOverride $EngineRoot -SourceConfig $sourceConfig -SkipPrewarm:$NoPrewarm -ForceRewrite:$Force
}
