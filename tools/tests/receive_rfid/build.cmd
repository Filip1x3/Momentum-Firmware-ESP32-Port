@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
"C:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe" tools\tests\receive_rfid\extract.py
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /W4 /WX /wd4996 /wd4244 /I ..\.diagnostics\receive-rfid-tests /Fo..\.diagnostics\receive-rfid-tests\ /Fe..\.diagnostics\receive-rfid-tests\receive.exe tools\tests\receive_rfid\receive.c
if errorlevel 1 exit /b 1
..\.diagnostics\receive-rfid-tests\receive.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /W4 /WX /I ..\.diagnostics\receive-rfid-tests /Fo..\.diagnostics\receive-rfid-tests\ /Fe..\.diagnostics\receive-rfid-tests\capability-default.exe tools\tests\receive_rfid\capability.c
if errorlevel 1 exit /b 1
..\.diagnostics\receive-rfid-tests\capability-default.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /W4 /WX /DBOARD_HAS_RFID=1 /DEXPECT_EXTERNAL=1 /I ..\.diagnostics\receive-rfid-tests /Fo..\.diagnostics\receive-rfid-tests\ /Fe..\.diagnostics\receive-rfid-tests\capability-external.exe tools\tests\receive_rfid\capability.c
if errorlevel 1 exit /b 1
..\.diagnostics\receive-rfid-tests\capability-external.exe
exit /b %errorlevel%
