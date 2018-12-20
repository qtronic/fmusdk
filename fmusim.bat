@echo off 
rem ------------------------------------------------------------
rem To run a simulation, start this batch in this directory. 
rem Example: fmusim me10 fmu10/fmu/me/win32/dq.fmu 0.3 0.1 1 c
rem Example: fmusim cs20 fmu20/fmu/cs/win64/dq.fmu 0.3 0.1 1 c -win64
rem To build simulators and FMUs, run install.bat
rem Copyright QTronic GmbH. All rights reserved.
rem ------------------------------------------------------------

setlocal
set FMUSDK_HOME=.

rem first parameter is the type of FMI simulation to run
set SIM_TYPE=%1

rem get all command line argument after the %1
set SIM_OPTIONS=

rem if not speficied them win32
set FMI_PLATFORM=win32

rem shift all arguments down by one
SHIFT
:loop1
if "%1"=="-win64" (set FMI_PLATFORM=win64) else (
  if "%1"=="" goto after_loop
  set SIM_OPTIONS=%SIM_OPTIONS% %1
)
shift
goto loop1

:after_loop
if %SIM_TYPE%==me10 (fmu10\bin\%FMI_PLATFORM%\fmusim_me.exe %SIM_OPTIONS%
) else (if %SIM_TYPE%==cs10 (fmu10\bin\%FMI_PLATFORM%\fmusim_cs.exe %SIM_OPTIONS%
) else (if %SIM_TYPE%==me20 (fmu20\bin\%FMI_PLATFORM%\fmusim_me.exe %SIM_OPTIONS%
) else (if %SIM_TYPE%==cs20 (fmu20\bin\%FMI_PLATFORM%\fmusim_cs.exe %SIM_OPTIONS%
) else (echo Use one of cs10 cs20 me10 me20 as first argument))))

endlocal
