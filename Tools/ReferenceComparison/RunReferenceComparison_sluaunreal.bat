@echo off
setlocal
echo [RunReferenceComparison] sluaunreal (all dimensions, 3 rounds)
echo.
call "%~dp0RunReferenceComparison.bat" -Repos sluaunreal %*
exit /b %ERRORLEVEL%
