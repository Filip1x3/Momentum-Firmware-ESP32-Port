@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /W4 /WX /I tools\tests\bt_lifecycle\stubs /Fo..\.diagnostics\bt-lifecycle-test.obj /c tools\tests\bt_lifecycle\lifecycle_test.c
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /W4 /WX /I tools\tests\bt_lifecycle\stubs /Fo..\.diagnostics\bt-stack-lifecycle.obj /c components\btshim\bt_stack_lifecycle.c
if errorlevel 1 exit /b 1
link /nologo /out:..\.diagnostics\bt-lifecycle-test.exe ..\.diagnostics\bt-lifecycle-test.obj ..\.diagnostics\bt-stack-lifecycle.obj
if errorlevel 1 exit /b 1
..\.diagnostics\bt-lifecycle-test.exe
exit /b %errorlevel%
