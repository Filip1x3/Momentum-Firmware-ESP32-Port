@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /W4 /WX /I tools\tests\thread_tls\stubs /Fo..\.diagnostics\thread-tls-test.obj /Fe..\.diagnostics\thread-tls-test.exe tools\tests\thread_tls\thread_tls_test.c
if errorlevel 1 exit /b 1
..\.diagnostics\thread-tls-test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /c /DCONFIG_FREERTOS_THREAD_LOCAL_STORAGE_POINTERS=1 /I tools\tests\thread_tls\stubs /Fo..\.diagnostics\thread-tls-invalid.obj tools\tests\thread_tls\thread_tls_test.c >..\.diagnostics\thread-tls-invalid.log 2>&1
if not errorlevel 1 exit /b 1
findstr /C:"Furi requires CONFIG_FREERTOS_THREAD_LOCAL_STORAGE_POINTERS" ..\.diagnostics\thread-tls-invalid.log >nul
if errorlevel 1 exit /b 1
echo PASS: one-slot configuration rejected even with deletion callbacks enabled
exit /b 0
