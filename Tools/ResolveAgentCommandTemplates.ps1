param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$ConfigPath = (Join-Path (Resolve-Path (Join-Path $PSScriptRoot "..")).Path "AgentConfig.ini"),
    [string]$TestName = "<TestName>"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-IniMap {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "AgentConfig.ini not found at '$Path'. Run Tools\GenerateAgentConfigTemplate.bat first."
    }

    $result = @{}
    $currentSection = ""
    foreach ($line in Get-Content -LiteralPath $Path) {
        $trimmed = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith(";")) {
            continue
        }

        if ($trimmed.StartsWith("[") -and $trimmed.EndsWith("]")) {
            $currentSection = $trimmed.Substring(1, $trimmed.Length - 2)
            if (-not $result.ContainsKey($currentSection)) {
                $result[$currentSection] = @{}
            }
            continue
        }

        $separatorIndex = $trimmed.IndexOf("=")
        if ($separatorIndex -lt 0 -or [string]::IsNullOrWhiteSpace($currentSection)) {
            continue
        }

        $key = $trimmed.Substring(0, $separatorIndex).Trim()
        $value = $trimmed.Substring($separatorIndex + 1).Trim()
        $result[$currentSection][$key] = $value
    }

    return $result
}

function Get-IniValue {
    param(
        [hashtable]$Ini,
        [string]$Section,
        [string]$Key,
        [string]$Default = ""
    )

    if ($Ini.ContainsKey($Section) -and $Ini[$Section].ContainsKey($Key)) {
        $value = [string]$Ini[$Section][$Key]
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            return $value
        }
    }

    return $Default
}

$ini = Get-IniMap -Path $ConfigPath

$engineRoot = Get-IniValue -Ini $ini -Section "Paths" -Key "EngineRoot"
if ([string]::IsNullOrWhiteSpace($engineRoot)) {
    throw "Paths.EngineRoot is required in '$ConfigPath'."
}

$resolvedProjectFile = Get-IniValue -Ini $ini -Section "Paths" -Key "ProjectFile" -Default (Join-Path $ProjectRoot "AngelscriptProject.uproject")
$editorTarget = Get-IniValue -Ini $ini -Section "Build" -Key "EditorTarget" -Default "AngelscriptProjectEditor"
$platform = Get-IniValue -Ini $ini -Section "Build" -Key "Platform" -Default "Win64"
$configuration = Get-IniValue -Ini $ini -Section "Build" -Key "Configuration" -Default "Development"
$architecture = Get-IniValue -Ini $ini -Section "Build" -Key "Architecture" -Default "x64"
$defaultTimeoutMs = Get-IniValue -Ini $ini -Section "Test" -Key "DefaultTimeoutMs" -Default "600000"

$buildBat = Join-Path $engineRoot "Engine\Build\BatchFiles\Build.bat"
$editorCmd = Join-Path $engineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if (-not (Test-Path -LiteralPath $resolvedProjectFile)) {
    throw "Resolved project file not found at '$resolvedProjectFile'."
}

if (-not (Test-Path -LiteralPath $buildBat)) {
    throw "Build.bat not found at '$buildBat'. Check Paths.EngineRoot in '$ConfigPath'."
}

if (-not (Test-Path -LiteralPath $editorCmd)) {
    throw "UnrealEditor-Cmd.exe not found at '$editorCmd'. Check Paths.EngineRoot in '$ConfigPath'."
}

$buildCommand = 'powershell.exe -Command "& ''{0}'' {1} {2} {3} ''-Project={4}'' -WaitMutex -FromMsBuild -architecture={5} 2>&1 | Out-String"' -f $buildBat, $editorTarget, $platform, $configuration, $resolvedProjectFile, $architecture
$testCommand = 'powershell.exe -Command "Start-Process -FilePath ''{0}'' -ArgumentList ''""{1}""'',''-ExecCmds=""Automation RunTests {2}; Quit""'',''-Unattended'',''-NoPause'',''-NoSplash'',''-NullRHI'',''-NOSOUND'' -Wait -NoNewWindow; Write-Host ''DONE''"' -f $editorCmd, $resolvedProjectFile, $TestName

$resolved = [ordered]@{
    ConfigPath = $ConfigPath
    ProjectRoot = $ProjectRoot
    EngineRoot = $engineRoot
    ProjectFile = $resolvedProjectFile
    EditorTarget = $editorTarget
    Platform = $platform
    Configuration = $configuration
    Architecture = $architecture
    DefaultTimeoutMs = [int]$defaultTimeoutMs
    BuildCommand = $buildCommand
    TestCommand = $testCommand
}

$resolved.GetEnumerator() | ForEach-Object {
    Write-Output ("{0}={1}" -f $_.Key, $_.Value)
}
