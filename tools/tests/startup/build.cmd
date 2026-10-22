@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "..\.diagnostics\startup-tests" mkdir "..\.diagnostics\startup-tests"
rem Production cache offsets are <=1024 bytes; size_t is 64-bit on this host.
cl /nologo /std:c11 /W4 /WX /wd4267 /I tools\tests\startup\stubs /I components\toolbox\stream /Fo..\.diagnostics\startup-tests\ /Fe..\.diagnostics\startup-tests\buffered-read.exe tools\tests\startup\buffered_read_test.c components\toolbox\stream\buffered_file_stream.c components\toolbox\stream\stream_cache.c
if errorlevel 1 exit /b 1
..\.diagnostics\startup-tests\buffered-read.exe
if errorlevel 1 exit /b 1
"C:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe" tools\tests\startup\extract_usb_handoff.py
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /W4 /WX /I ..\.diagnostics\startup-tests /Fo..\.diagnostics\startup-tests\ /Fe..\.diagnostics\startup-tests\usb-handoff.exe tools\tests\startup\usb_handoff_test.c
if errorlevel 1 exit /b 1
..\.diagnostics\startup-tests\usb-handoff.exe
exit /b %errorlevel%
