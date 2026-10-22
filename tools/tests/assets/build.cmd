@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "..\.diagnostics\asset-tests-20261021" mkdir "..\.diagnostics\asset-tests-20261021"
cl /nologo /std:c11 /W4 /LD /I components\heatshrink /Fo..\.diagnostics\asset-tests-20261021\ /Fe..\.diagnostics\asset-tests-20261021\asset_file.dll components\toolbox\asset_file.c components\heatshrink\heatshrink_decoder.c /link /EXPORT:asset_file_decode_bitmap /EXPORT:asset_file_validate_font /EXPORT:asset_file_fit_dimensions /EXPORT:asset_file_scale_bitmap /EXPORT:asset_file_valid_name
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /W4 /I tools\tests\assets\include /I components\gui /I components\toolbox /I components\heatshrink /Fo..\.diagnostics\asset-tests-20261021\ /Fe..\.diagnostics\asset-tests-20261021\lifecycle.exe tools\tests\assets\lifecycle.c components\gui\icon_animation.c components\toolbox\compress.c components\heatshrink\heatshrink_decoder.c components\heatshrink\heatshrink_encoder.c
if errorlevel 1 exit /b 1
..\.diagnostics\asset-tests-20261021\lifecycle.exe
