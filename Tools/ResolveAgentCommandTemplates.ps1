[CmdletBinding()]
param(
    [string]$ProjectRoot = '',

    [string]$ConfigPath = '',

    [string]$TestName = '<TestName>',

    [string]$BuildLabel = 'agent-build',

    [string]$TestLabel = 'agent-test'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'Shared\UnrealCommandUtils.ps1')

$resolvedProjectRoot = if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
}
else {
    Normalize-PathValue -Path $ProjectRoot
}

$resolvedConfigPath = if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
    Join-Path $resolvedProjectRoot 'AgentConfig.ini'
}
else {
    Normalize-PathValue -Path $ConfigPath
}

$agentConfig = Resolve-AgentConfiguration -ProjectRoot $resolvedProjectRoot -ConfigPath $resolvedConfigPath
$powerShell = Get-ConsolePowerShellPath

$buildTimeoutMs = if ($agentConfig.BuildDefaultTimeoutMs -gt 300000) { 180000 } else { $agentConfig.BuildDefaultTimeoutMs }
$testTimeoutMs = if ($agentConfig.TestDefaultTimeoutMs -gt 300000) { 300000 } else { $agentConfig.TestDefaultTimeoutMs }

$buildScript = Join-Path $resolvedProjectRoot 'Tools\RunBuild.ps1'
$testScript = Join-Path $resolvedProjectRoot 'Tools\RunTests.ps1'
$ubtProcessScript = Join-Path $resolvedProjectRoot 'Tools\Get-UbtProcess.ps1'

$resolved = [ordered]@{
    ConfigPath              = $resolvedConfigPath
    ProjectRoot             = $resolvedProjectRoot
    EngineRoot              = $agentConfig.EngineRoot
    ProjectFile             = $agentConfig.ProjectFile
    EditorTarget            = $agentConfig.EditorTarget
    Platform                = $agentConfig.Platform
    Configuration           = $agentConfig.Configuration
    Architecture            = $agentConfig.Architecture
    BuildDefaultTimeoutMs   = $buildTimeoutMs
    TestDefaultTimeoutMs    = $testTimeoutMs
    BuildCommand            = ('{0} -NoProfile -ExecutionPolicy Bypass -File "{1}" -Label "{2}" -TimeoutMs {3}' -f $powerShell, $buildScript, $BuildLabel, $buildTimeoutMs)
    SerializedBuildCommand  = ('{0} -NoProfile -ExecutionPolicy Bypass -File "{1}" -Label "{2}" -TimeoutMs {3} -SerializeByEngine' -f $powerShell, $buildScript, $BuildLabel, $buildTimeoutMs)
    TestCommand             = ('{0} -NoProfile -ExecutionPolicy Bypass -File "{1}" -TestPrefix "{2}" -Label "{3}" -TimeoutMs {4}' -f $powerShell, $testScript, $TestName, $TestLabel, $testTimeoutMs)
    UbtProcessCommand       = ('{0} -NoProfile -ExecutionPolicy Bypass -File "{1}" -CurrentWorktreeOnly' -f $powerShell, $ubtProcessScript)
}

$resolved.GetEnumerator() | ForEach-Object {
    Write-Output ("{0}={1}" -f $_.Key, $_.Value)
}
