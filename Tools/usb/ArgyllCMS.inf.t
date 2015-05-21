;--------------------------------------------------------------------------
; Copyright 2012 Graeme W. Gill.
; 
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this file, to deal
; in this file without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of this file, and to permit persons to whom this file is
; furnished to do so, subject to the following conditions:
; 
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of this file.
; 
; THIS FILE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THIS FILE OR THE USE OR OTHER DEALINGS IN
; THIS FILE.
;--------------------------------------------------------------------------

; ==== Strings ====

[Strings]

DeviceGUID          = {247e32a0-32f9-11df-aacc-0002a5d5c51b}

Date                = "01/17/2012"		; MM/DD/YYYY
libusb0ver          = "1.2.6.0"

Libusb_ClassName    = "Argyll LibUSB-win32 devices"
Libusb_DiskName     = "LibUSB-win32 Device Install Disk"

libusb0_SvcDesc     = "LibUSB-win32 libusb0 - Kernel Driver 2012/1/17, 1.2.6.0"

; ==== Version ====

[Version]
Signature = "$Windows NT$"
DriverVer = %Date%,%libusb0ver%
Provider  = "ArgyllCMS"

; (Note the ClassGuid must not be quoted or a string substitution to work on Win2K)
Class               = %Libusb_ClassName%
ClassGuid           = {817cffe0-328b-11df-9b9f-0002a5d5c51b}	; LibUSB-win32 ClassGUID
CatalogFile         = "ArgyllCMS.cat"
CatalogFile.NTAMD64 = "ArgyllCMS_x64.cat"

[ClassInstall32]
AddReg=class_install_add_reg

[class_install_add_reg]
HKR,,,,%Libusb_ClassName%
HKR,,Icon,,"-20"	; -20 is for the USB icon

; ==== Files Sources and Destinations ====

[SourceDisksNames]
1 = %Libusb_DiskName%

[SourceDisksFiles]
libusb0.sys = 1,\bin\x86,

[SourceDisksFiles.amd64]
libusb0.sys = 1,\bin\amd64,

[DestinationDirs]
libusb0_files_sys = 10,system32\drivers

; ==== libusb0 Device driver ====

[libusb0_files_sys]
libusb0.sys,libusb0.sys

; For each one of these, there must be one for Services !!!
[LIBUSB0_DEV]
CopyFiles = Libusb0_files_sys

[LIBUSB0_DEV.HW]
AddReg = libusb0_add_reg_hw

[libusb0_add_reg]
HKR,,DevLoader,,*ntkern
HKR,,NTMPDriver,,libusb0.sys

[libusb0_add_reg_hw]
HKR,,DeviceInterfaceGUIDs,0x10000,%DeviceGUID%
HKR,,SurpriseRemovalOK,0x00010001,1				; Device properties

[LIBUSB0_DEV.Services]
AddService = libusb0, 0x00000002, libusb0_add_service

[libusb0_add_service]
DisplayName    = %libusb0_SvcDesc%
ServiceType    = 1
StartType      = 3
ErrorControl   = 0
ServiceBinary  = %12%\libusb0.sys

; ==== Manufacturers ====

[Manufacturer]
"HCFR Association"=HCFR_Devices,NTx86,NTamd64
"Sequel Imaging"=Sequel_Devices,NTx86,NTamd64
"X-Rite"=X_Rite_Devices,NTx86,NTamd64
"ColorVision"=ColorVision_Devices,NTx86,NTamd64
"Gretag Macbeth/X-Rite"=GM_X_Rite_Devices,NTx86,NTamd64
"Hughski Ltd"=Hughski_Devices,NTx86,NTamd64
"Image Engineering"=ImageEngineering_Devices,NTx86,NTamd64

; ==== Devices ====
