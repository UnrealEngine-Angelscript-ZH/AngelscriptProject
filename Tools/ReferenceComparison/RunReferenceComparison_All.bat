@echo off
setlocal
echo [RunReferenceComparison] All repos (all dimensions, 3 rounds)
echo.
call "%~dp0RunReferenceComparison.bat" %*
exit /b %ERRORLEVEL%
