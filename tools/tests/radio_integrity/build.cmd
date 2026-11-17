@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "..\.diagnostics\radio-integrity-tests" mkdir "..\.diagnostics\radio-integrity-tests"
cl /nologo /std:c11 /W4 /WX /LD /Fo..\.diagnostics\radio-integrity-tests\ /Fe..\.diagnostics\radio-integrity-tests\beacon_frame.dll components\wifi\beacon_frame.c /link /EXPORT:wlan_beacon_frame_set_ssid
if errorlevel 1 exit /b 1
"C:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe" tools\test_beacon_frame.py --library ..\.diagnostics\radio-integrity-tests\beacon_frame.dll
exit /b %errorlevel%
