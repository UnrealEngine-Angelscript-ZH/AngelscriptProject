@echo off
setlocal
echo [RunReferenceComparison] Hazelight-Angelscript (all dimensions, 3 rounds)
echo.
call "%~dp0RunReferenceComparison.bat" -Repos hazelight %*
exit /b %ERRORLEVEL%
