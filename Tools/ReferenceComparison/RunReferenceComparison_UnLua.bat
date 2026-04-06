@echo off
setlocal
echo [RunReferenceComparison] UnLua (all dimensions, 3 rounds)
echo.
call "%~dp0RunReferenceComparison.bat" -Repos unlua %*
exit /b %ERRORLEVEL%
