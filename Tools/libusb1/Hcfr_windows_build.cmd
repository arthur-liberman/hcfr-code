@echo off
rem **************************************************************************
rem build the x86 version of the driver and dll
rem **************************************************************************
if "%DDK_VER%" == "" set DDK_VER=7600.16385.0
setlocal
call c:\WinDDK\%DDK_VER%\bin\setenv.bat c:\WinDDK\%DDK_VER%\ fre x86 WXP no_oacr
cd /d %~dp0%
call ddk_build.cmd
endlocal
rem **************************************************************************
rem build the x64 version of the driver and dll
rem **************************************************************************
setlocal
call "c:\WinDDK\%DDK_VER%\bin\setenv.bat" c:\WinDDK\%DDK_VER%\ fre x64 WLH no_oacr
cd /d %~dp0%
call ddk_build.cmd
endlocal
