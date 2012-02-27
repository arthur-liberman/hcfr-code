rem **************************************************************************
rem create the inf files that can use winUSB
rem **************************************************************************
call hcfr_make_inf.cmd ColorMunki winusbDrivers
call hcfr_make_inf.cmd DTP20 winusbDrivers
call hcfr_make_inf.cmd DTP92 winusbDrivers
call hcfr_make_inf.cmd DTP94 winusbDrivers
call hcfr_make_inf.cmd Huey winusbDrivers
call hcfr_make_inf.cmd MonacoOptix winusbDrivers
call hcfr_make_inf.cmd Spyd3 winusbDrivers
call hcfr_make_inf.cmd i1Monitor winusbDrivers 
call hcfr_make_inf.cmd i1Disp winusbDrivers
call hcfr_make_inf.cmd i1D3 winusbDrivers
rem **************************************************************************
rem create the inf files that can only use libUSB
rem **************************************************************************
call hcfr_make_inf.cmd i1Pro libusbDrivers
call hcfr_make_inf.cmd Spyd2 libusbDrivers
call hcfr_make_inf.cmd HCFR3.1 libusbDrivers
call hcfr_make_inf.cmd HCFR4.0 libusbDrivers
rem **************************************************************************
rem Sign the files
rem **************************************************************************
for %%f in (*.sys) do signtool sign /v /a /ac .\MSCV-GlobalSign.cer /t http://timestamp.globalsign.com/scripts/timstamp.dll %%f
inf2cat /drv:. /os:XP_x86,XP_x64
for %%f in (*.cat) do signtool sign /v /a /ac .\MSCV-GlobalSign.cer /t http://timestamp.globalsign.com/scripts/timstamp.dll %%f