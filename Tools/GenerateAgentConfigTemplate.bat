@echo off
setlocal

for %%I in ("%~dp0..") do set "PROJECT_ROOT=%%~fI"
set "TARGET_FILE=%PROJECT_ROOT%\AgentConfig.ini"

if /I not "%~1"=="--force" if exist "%TARGET_FILE%" (
    echo AgentConfig.ini already exists:
    echo   "%TARGET_FILE%"
    echo.
    echo Use GenerateAgentConfigTemplate.bat --force to overwrite it.
    exit /b 1
)

> "%TARGET_FILE%" (
    echo ; Local machine-specific configuration for AI agents.
    echo ; This file is git-ignored and should not be committed.
    echo.
    echo [Paths]
    echo ; Required. Unreal Engine root directory on this machine.
    echo EngineRoot=C:\UnrealEngine\UE_5.7
    echo ; Optional. Absolute path to the project file.
    echo ; Leave empty if tools should resolve the .uproject from repo root.
    echo ProjectFile=%PROJECT_ROOT%\AngelscriptProject.uproject
    echo.
    echo [Build]
    echo ; Default editor target to build.
    echo EditorTarget=AngelscriptProjectEditor
    echo Platform=Win64
    echo Configuration=Development
    echo Architecture=x64
    echo DefaultTimeoutMs=1200000
    echo.
    echo [Test]
    echo ; Default timeout in milliseconds for long-running commands.
    echo DefaultTimeoutMs=600000
    echo.
    echo [References]
    echo ; Optional. Manually set this to your local HazelightAngelscriptEngine reference path.
    echo ; This is reference-only and is not required for normal build or test flows.
    echo HazelightAngelscriptEngineRoot=
)

echo Generated:
echo   "%TARGET_FILE%"
echo.
echo Update EngineRoot and other values for your local machine before running build or test commands.
