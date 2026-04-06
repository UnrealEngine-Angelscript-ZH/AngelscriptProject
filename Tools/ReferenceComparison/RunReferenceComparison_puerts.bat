@echo off
setlocal
echo [RunReferenceComparison] puerts (all dimensions, 3 rounds)
echo.
call "%~dp0RunReferenceComparison.bat" -Repos puerts %*
exit /b %ERRORLEVEL%
