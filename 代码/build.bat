@echo off
rem ============================================================
rem  502 build helper: compile one stage and produce .bin
rem  usage:  build.bat 01_uart
rem  output: <stage>\Objects\<target>.bin  (for serial ISP flash)
rem ============================================================
setlocal enabledelayedexpansion
set BASE=%~dp0
set UV4=T:\TE\UV4\UV4.exe
set FROMELF=T:\TE\ARM\ARMCLANG\bin\fromelf.exe

set STAGE=%1
if "%STAGE%"=="" (
    echo usage: build.bat 01_uart
    exit /b 1
)
set DIR=%BASE%%STAGE%
if not exist "%DIR%" (
    echo [ERR] stage folder not found: %DIR%
    exit /b 1
)

set PROJ=
for %%f in ("%DIR%\*.uvprojx") do set PROJ=%%f
if "%PROJ%"=="" (
    echo [ERR] no .uvprojx in %DIR%
    exit /b 1
)

echo [1/2] compiling %PROJ%
"%UV4%" -b "%PROJ%" -j0 -o "%DIR%\build_log.txt"
type "%DIR%\build_log.txt"
if errorlevel 1 echo [WARN] UV4 exit code %errorlevel%

echo [2/2] converting to bin
for %%f in ("%DIR%\Objects\*.axf") do (
    "%FROMELF%" --bin --output "%DIR%\Objects\%%~nf.bin" "%%~ff"
)
dir /b "%DIR%\Objects\*.bin" 2>nul
if not exist "%DIR%\Objects\*.bin" echo [ERR] no bin produced
endlocal
