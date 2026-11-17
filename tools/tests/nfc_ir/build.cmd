@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
"C:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe" tools\tests\nfc_ir\extract_startup.py
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /W4 /WX /wd4996 /I ..\.diagnostics\nfc-ir-tests /Fo..\.diagnostics\nfc-ir-tests\ /Fe..\.diagnostics\nfc-ir-tests\startup.exe tools\tests\nfc_ir\startup_test.c
if errorlevel 1 exit /b 1
..\.diagnostics\nfc-ir-tests\startup.exe
exit /b %errorlevel%
