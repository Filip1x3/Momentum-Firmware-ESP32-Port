@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "..\.diagnostics\app-crash-tests" mkdir "..\.diagnostics\app-crash-tests"
cl /nologo /std:clatest /W4 /wd4100 /wd4204 /wd4820 /wd4267 /wd4048 /D_CRT_SECURE_NO_WARNINGS /I tools\tests\app_crashes\stubs /Fo..\.diagnostics\app-crash-tests\ /Fe..\.diagnostics\app-crash-tests\lifecycle.exe tools\tests\app_crashes\lifecycle.c
if errorlevel 1 exit /b 1
..\.diagnostics\app-crash-tests\lifecycle.exe
exit /b %errorlevel%
