@echo off 

rem ------------------------------------------------------------
rem This batch builds both simulators and all FMUs of the FmuSDK
rem Copyright QTronic GmbH. All rights reserved.
rem ------------------------------------------------------------

rem First argument %1 should be empty for win32, and '-win64' for win64 build.
call build_fmusim_me %1
call build_fmusim_cs %1
echo -----------------------------------------------------------
echo Making the FMUs of the FmuSDK ...
pushd models

call build_fmu me dq %1
call build_fmu me inc %1
call build_fmu me values %1
call build_fmu me vanDerPol %1
call build_fmu me bouncingBall %1

call build_fmu cs dq %1
call build_fmu cs inc %1
call build_fmu cs values %1
call build_fmu cs vanDerPol %1
call build_fmu cs bouncingBall %1

popd


