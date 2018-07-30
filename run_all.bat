@echo off 

rem ------------------------------------------------------------
rem This batch runs all FMUs of the FmuSDK and stores simulation
rem results in CSV files, one file per simulation run.
rem Command to run the 32 bit version: run_all
rem Command to run the 64 bit version: run_all -win64
rem Copyright QTronic GmbH. All rights reserved.
rem ------------------------------------------------------------

setlocal
if "%1"=="-win64" (
set x64=x64\
set x64NAME=x64
) else (
set x64=
set x64NAME=
)

echo -----------------------------------------------------------
echo Running all FMUs 1.0 of the FmuSDK ...

echo -----------------------------------------------------------
call fmusim me10 fmu10\fmu\me\%x64%bouncingBall.fmu 4 0.01 0 c %1
move /Y result.csv result_me10%x64NAME%_bouncingBall.csv

echo -----------------------------------------------------------
call fmusim cs10 fmu10\fmu\cs\%x64%bouncingBall.fmu 4 0.01 0 c %1
move /Y result.csv result_cs10%x64NAME%_bouncingBall.csv

echo -----------------------------------------------------------
call fmusim me10 fmu10\fmu\me\%x64%vanDerPol.fmu 5 0.1 0 c %1
move /Y result.csv result_me10%x64NAME%_vanDerPol.csv

echo -----------------------------------------------------------
call fmusim cs10 fmu10\fmu\cs\%x64%vanDerPol.fmu 5 0.1 0 c %1
move /Y result.csv result_cs10%x64NAME%_vanDerPol.csv

echo -----------------------------------------------------------
call fmusim me10 fmu10\fmu\me\%x64%dq.fmu 1 0.1 0 c %1
move /Y result.csv result_me10%x64NAME%_dq.csv

echo -----------------------------------------------------------
call fmusim cs10 fmu10\fmu\cs\%x64%dq.fmu 1 0.1 0 c %1
move /Y result.csv result_cs10%x64NAME%_dq.csv

echo -----------------------------------------------------------
call fmusim me10 fmu10\fmu\me\%x64%inc.fmu 15 0.1 0 c %1
move /Y result.csv result_me10%x64NAME%_inc.csv

echo -----------------------------------------------------------
call fmusim cs10 fmu10\fmu\cs\%x64%inc.fmu 15 0.1 0 c %1
move /Y result.csv result_cs10%x64NAME%_inc.csv

echo -----------------------------------------------------------
call fmusim me10 fmu10\fmu\me\%x64%values.fmu 12 0.1 0 c %1
move /Y result.csv result_me10%x64NAME%_values.csv

echo -----------------------------------------------------------
call fmusim cs10 fmu10\fmu\cs\%x64%values.fmu 12 0.1 0 c %1
move /Y result.csv result_cs10%x64NAME%_values.csv

echo -----------------------------------------------------------
echo Running all FMUs 2.0 of the FmuSDK ...

echo -----------------------------------------------------------
call fmusim me20 fmu20\fmu\me\%x64%bouncingBall.fmu 4 0.01 0 c %1
move /Y result.csv result_me20%x64NAME%_bouncingBall.csv

echo -----------------------------------------------------------
call fmusim cs20 fmu20\fmu\cs\%x64%bouncingBall.fmu 4 0.01 0 c %1
move /Y result.csv result_cs20%x64NAME%_bouncingBall.csv

echo -----------------------------------------------------------
call fmusim me20 fmu20\fmu\me\%x64%vanDerPol.fmu 5 0.1 0 c %1
move /Y result.csv result_me20%x64NAME%_vanDerPol.csv

echo -----------------------------------------------------------
call fmusim cs20 fmu20\fmu\cs\%x64%vanDerPol.fmu 5 0.1 0 c %1
move /Y result.csv result_cs20%x64NAME%_vanDerPol.csv

echo -----------------------------------------------------------
call fmusim me20 fmu20\fmu\me\%x64%dq.fmu 1 0.1 0 c %1
move /Y result.csv result_me20%x64NAME%_dq.csv

echo -----------------------------------------------------------
call fmusim cs20 fmu20\fmu\cs\%x64%dq.fmu 1 0.1 0 c %1
move /Y result.csv result_cs20%x64NAME%_dq.csv

echo -----------------------------------------------------------
call fmusim me20 fmu20\fmu\me\%x64%inc.fmu 15 0.1 0 c %1
move /Y result.csv result_me20%x64NAME%_inc.csv

echo -----------------------------------------------------------
call fmusim cs20 fmu20\fmu\cs\%x64%inc.fmu 15 0.1 0 c %1
move /Y result.csv result_cs20%x64NAME%_inc.csv

echo -----------------------------------------------------------
call fmusim me20 fmu20\fmu\me\%x64%values.fmu 12 0.1 0 c %1
move /Y result.csv result_me20%x64NAME%_values.csv

echo -----------------------------------------------------------
call fmusim cs20 fmu20\fmu\cs\%x64%values.fmu 12 0.1 0 c %1
move /Y result.csv result_cs20%x64NAME%_values.csv

endlocal

rem keep window visible for user
pause