@echo off
setlocal
echo [RunReferenceComparison] UnrealCSharp (all dimensions, 3 rounds)
echo.
call "%~dp0RunReferenceComparison.bat" -Repos unrealcsharp %*
exit /b %ERRORLEVEL%
