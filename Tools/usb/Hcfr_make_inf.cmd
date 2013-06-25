rem **************************************************************************
rem create the inf file from the template
rem **************************************************************************
copy ArgyllCMS.inf.t ArgyllCMS.inf
sed s/#PLAT#// ArgyllCMS.inf.d >> ArgyllCMS.inf 
sed s/#PLAT#/.NTx86/ ArgyllCMS.inf.d >> ArgyllCMS.inf 
sed s/#PLAT#/.NTamd64/ ArgyllCMS.inf.d >> ArgyllCMS.inf 
rem **************************************************************************
rem create and sign the cat file
rem **************************************************************************
inf2cat /drv:. /os:2000,XP_x86,XP_x64,Vista_x86,Vista_x64,7_x86,7_x64,Server2003_x86,Server2003_x64,Server2008_x86,Server2008R2_x64
for %%f in (*.cat) do signtool sign /v /a /ac .\MSCV-GlobalSign.cer /t http://timestamp.globalsign.com/scripts/timstamp.dll %%f