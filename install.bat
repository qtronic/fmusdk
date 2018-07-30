@echo off 

rem ------------------------------------------------------------
rem This batch builds all simulators and FMUs of the FmuSDK20
rem for Windows platforms
rem Command to build 32 bit version: install
rem Commant to build 64 bit version: install -win64
rem Copyright QTronic GmbH. All rights reserved.
rem ------------------------------------------------------------

echo -----------------------------------------------------------
echo Making the simulators and models for FMI 1.0 of the FmuSDK ...
pushd fmu10\src
call build_all %1
popd

echo -----------------------------------------------------------
echo Making the simulators and models for FMI 2.0 of the FmuSDK ...
pushd fmu20\src
call build_all %1
popd

rem keep window visible for user
pause
