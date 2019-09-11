rem Create plausible .cat files
rem got directory %1
cd %1
rmdir /S /Q tmp 
mkdir tmp\bin\x86
mkdir tmp\bin\amd64
copy ArgyllCMS.inf tmp
copy bin\libusb-win32-bin-README.txt tmp\bin
copy bin\amd64\libusb0.sys tmp\bin\amd64\libusb0.sys 
copy bin\x86\libusb0.sys tmp\bin\x86\libusb0.sys 
inf2cat /v /driver:tmp /os:2000,XP_X86,Vista_X86,7_X86,XP_X64,Vista_X64,7_X64 
copy tmp\argyllcms.cat ArgyllCMS.cat
copy tmp\argyllcms_x64.cat ArgyllCMS_x64.cat
