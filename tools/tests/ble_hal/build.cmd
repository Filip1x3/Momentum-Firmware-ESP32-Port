@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
"C:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe" tools\tests\ble_hal\extract_policy.py
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /W4 /WX /I tools\tests\ble_hal\stubs /I components\btshim /I ..\.diagnostics\ble-hal-tests /Fo..\.diagnostics\ble-hal-tests\ /Fe..\.diagnostics\ble-hal-tests\lifecycle.exe tools\tests\ble_hal\lifecycle.c components\btshim\bt_stack_lifecycle.c
if errorlevel 1 exit /b 1
..\.diagnostics\ble-hal-tests\lifecycle.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /W4 /WX /DBLE_TEST_WALK /I tools\tests\ble_hal\stubs /I components\btshim /I ..\.diagnostics\ble-hal-tests /Fo..\.diagnostics\ble-hal-tests\ /Fe..\.diagnostics\ble-hal-tests\walk-lifecycle.exe tools\tests\ble_hal\lifecycle.c components\btshim\bt_stack_lifecycle.c
if errorlevel 1 exit /b 1
..\.diagnostics\ble-hal-tests\walk-lifecycle.exe
exit /b %errorlevel%
