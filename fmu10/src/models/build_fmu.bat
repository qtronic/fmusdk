@echo off 
rem ------------------------------------------------------------
rem This batch builds an FMU of the FMU SDK
rem Usage: build_fmu (me|cs) <fmu_dir_name> 
rem Copyright QTronic GmbH. All rights reserved
rem ------------------------------------------------------------

setlocal

echo -----------------------------------------------------------
if %1==cs (^
echo building FMU %2 - FMI for Co-Simulation 1.0) else ^
echo building FMU %2 - FMI for Model Exchange 1.0

rem save env variable settings
set PREV_PATH=%PATH%
if defined INCLUDE set PREV_INCLUDE=%INLUDE%
if defined LIB     set PREV_LIB=%LIB%
if defined LIBPATH set PREV_LIBPATH=%LIBPATH%

if "%3"=="-win64" (set FMI_PLATFORM=win64) else (set FMI_PLATFORM=win32)

rem setup the compiler
if %FMI_PLATFORM%==win64 (
if defined VS140COMNTOOLS (call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x86_amd64) else ^
if defined VS120COMNTOOLS (call "%VS120COMNTOOLS%\..\..\VC\vcvarsall.bat" x86_amd64) else ^
if defined VS110COMNTOOLS (call "%VS110COMNTOOLS%\..\..\VC\vcvarsall.bat" x86_amd64) else ^
if defined VS100COMNTOOLS (call "%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat" x86_amd64) else ^
if defined VS90COMNTOOLS (call "%VS90COMNTOOLS%\..\..\VC\vcvarsall.bat" x86_amd64) else ^
if defined VS80COMNTOOLS (call "%VS80COMNTOOLS%\..\..\VC\vcvarsall.bat" x86_amd64) else ^
goto noCompiler
) else (
if defined VS140COMNTOOLS (call "%VS140COMNTOOLS%\vsvars32.bat") else ^
if defined VS120COMNTOOLS (call "%VS120COMNTOOLS%\vsvars32.bat") else ^
if defined VS110COMNTOOLS (call "%VS110COMNTOOLS%\vsvars32.bat") else ^
if defined VS100COMNTOOLS (call "%VS100COMNTOOLS%\vsvars32.bat") else ^
if defined VS90COMNTOOLS (call "%VS90COMNTOOLS%\vsvars32.bat") else ^
if defined VS80COMNTOOLS (call "%VS80COMNTOOLS%\vsvars32.bat") else ^
goto noCompiler
)

rem create the %2.dll in the temp dir
if not exist temp mkdir temp 
pushd temp
if exist *.dll del /Q *.dll

if %1==cs (set DEF=/DFMI_COSIMULATION) else set DEF=
cl /LD /nologo %DEF% ..\%2\%2.c /I ..\. /I ..\..\shared\include
if not exist %2.dll goto compileError

rem create FMU dir structure with root 'fmu'
set BIN_DIR=fmu\binaries\%FMI_PLATFORM%
set SRC_DIR=fmu\sources
set DOC_DIR=fmu\documentation
if not exist %BIN_DIR% mkdir %BIN_DIR%
if not exist %SRC_DIR% mkdir %SRC_DIR%
if not exist %DOC_DIR% mkdir %DOC_DIR%
move /Y %2.dll %BIN_DIR%
if exist ..\%2\*~ del /Q ..\%2\*~
copy ..\%2\%2.c %SRC_DIR%
copy ..\%2\modelDescription_%1.xml fmu\modelDescription.xml
copy ..\%2\model.png fmu
copy ..\fmuTemplate.c %SRC_DIR%
copy ..\fmuTemplate.h %SRC_DIR%
copy ..\%2\*.html %DOC_DIR%
copy ..\%2\*.png  %DOC_DIR%
del %DOC_DIR%\model.png

rem zip the directory tree and move to fmu directory 
cd fmu
set FMU_FILE=..\..\..\..\fmu\%1\%FMI_PLATFORM%\%2.fmu
if exist %FMU_FILE% del %FMU_FILE%
..\..\..\..\bin\7z.exe a -tzip %FMU_FILE% ^
  modelDescription.xml model.png binaries sources documentation
goto cleanup

:noCompiler
echo No Microsoft Visual C compiler found

:compileError
echo build of %2 failed

:cleanup
popd
if exist temp rmdir /S /Q temp

rem undo variable settings performed by vsvars32.bat
set PATH=%PREV_PATH%
if defined PREV_INCLUDE set INCLUDE=%PREV_INLUDE%
if defined PREV_LIB     set LIB=%PREV_LIB%
if defined PREV_LIBPATH set LIBPATH=%PREV_LIBPATH%
echo done.

endlocal
