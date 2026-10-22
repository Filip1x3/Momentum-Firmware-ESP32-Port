@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /W4 /DMOMENTUM_DUALBOOT /I tools\tests\dualboot_worker\stubs /Fo..\.diagnostics\dualboot-worker-test.obj /Fe..\.diagnostics\dualboot-worker-test.exe tools\tests\dualboot_worker\worker_test.c
if errorlevel 1 exit /b 1
..\.diagnostics\dualboot-worker-test.exe
