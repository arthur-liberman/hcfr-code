;////////////////////////////////////////////////////////////////////////////
;// Based off DSCaler.iss Copyright (c) 2002 Rob Muller. All rights reserved.
;// Copyright (c) 2012 John Adcock. All rights reserved.
;/////////////////////////////////////////////////////////////////////////////
;//
;//  This file is subject to the terms of the GNU General Public License as
;//  published by the Free Software Foundation.  A copy of this license is
;//  included with this software distribution in the file COPYING.  If you
;//  do not have a copy, you may obtain a copy by writing to the Free
;//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
;//
;//  This software is distributed in the hope that it will be useful,
;//  but WITHOUT ANY WARRANTY; without even the implied warranty of
;//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;//  GNU General Public License for more details
;/////////////////////////////////////////////////////////////////////////////
;
;  This is an InnoSetup script.
;  For more information about InnoSetup see http://www.innosetup.com

#define MyAppVersion GetFileVersion("..\Release\ColorHCFR.exe")

[Setup]
AppName=HCFR Calibration
AppVerName=HCFR {#MyAppVersion}
AppPublisherURL=http://hcfr.sourceforge.net/
AppSupportURL=http://hcfr.sourceforge.net/
AppUpdatesURL=http://hcfr.sourceforge.net/
AppVersion={#MyAppVersion}
DefaultDirName={pf}\HCFR Calibration
DefaultGroupName=HCFR Calibration
OutputBaseFilename=HCFRSetup
AllowNoIcons=yes
LicenseFile=gpl.rtf
AppMutex=HCFR
;required for installing the driver on NT platforms
PrivilegesRequired=Admin
DisableStartupPrompt=yes
AppCopyright=Copyright (C) 2012 HCFR Team
VersionInfoDescription=HCFR Setup
VersionInfoProductVersion={#MyAppVersion}
VersionInfoVersion={#MyAppVersion}
SetupIconFile=..\res\ColorHCFR.ico
OutputDir=..\Release



[Messages]
BeveledLabel=HCFR
WizardLicense=GPL License Agreement
LicenseLabel3=Do you want to continue to install [name]?.
LicenseAccepted=Yes, I would like to &continue
LicenseNotAccepted=&No
WizardInfoBefore=Warning
InfoBeforeLabel=Please read the following important warning before continuing.
InfoBeforeClickLabel=When you are ready and happy to continue with Setup, click Next.

[Dirs]
Name: "{app}"; Permissions: everyone-modify
Name: "{app}\Etalon_Argyll"; Permissions: everyone-modify

[Components]
Name: "main"; Description: "Main Files"; Types: full; Flags: fixed

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; MinVersion: 4,4
Name: "quicklaunchicon"; Description: "Create a &Quick Launch icon"; GroupDescription: "Additional icons:"; MinVersion: 4,4; Flags: unchecked

[Files]
; main
Source: "..\Release\ColorHCFR.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "..\Release\oeminst.exe"; DestDir: "{app}\Tools"; Flags: ignoreversion; Components: main
Source: "..\Release\spotread.exe"; DestDir: "{app}\Tools"; Flags: ignoreversion; Components: main
Source: "..\Release\*.dll"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "..\Release\*.chm"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "..\Install\Etalon_HCFR\*.thc"; DestDir: "{app}\Etalon_HCFR"; Flags: ignoreversion; Components: main
Source: "..\Install\Profils_IR\*.ihc"; DestDir: "{app}\Profils_IR"; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\*.cat"; DestDir: "{app}\Drivers"; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\*.inf"; DestDir: "{app}\Drivers"; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\*.sys"; DestDir: "{app}\Drivers"; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\*.dll"; DestDir: "{app}\Drivers"; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\AUTHORS"; DestDir: "{app}\Drivers"; DestName: Authors.txt; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\COPYING"; DestDir: "{app}\Drivers"; DestName: Copying.txt; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\COPYING_GPL"; DestDir: "{app}\Drivers"; DestName: Copying_GPL.txt; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\KDRIVER_LICENSE"; DestDir: "{app}\Drivers"; DestName: KernelDriverLicense.txt; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\NEWS"; DestDir: "{app}\Drivers"; DestName: News.txt; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\THANKS"; DestDir: "{app}\Drivers"; DestName: Thanks.txt; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\TODO"; DestDir: "{app}\Drivers"; DestName: Todo.txt; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\license.rtf"; DestDir: "{app}\Drivers"; DestName: CoInstaller_License.rtf; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\WinCoInstallers\amd64\*.*"; DestDir: "{app}\Drivers\WinCoInstallers\amd64"; Flags: ignoreversion; Components: main
Source: "..\Tools\libusb1\WinCoInstallers\x86\*.*"; DestDir: "{app}\Drivers\WinCoInstallers\x86"; Flags: ignoreversion; Components: main

[INI]
Filename: "{app}\HCFR.url"; Section: "InternetShortcut"; Key: "URL"; String: "http://hcfr.sourceforge.net/"

[Icons]
Name: "{group}\HCFR"; Filename: "{app}\ColorHCFR.exe"
Name: "{group}\HCFR on the Web"; Filename: "{app}\HCFR.url"
Name: "{userdesktop}\HCFR"; Filename: "{app}\ColorHCFR.exe"; MinVersion: 4,4; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\HCFR"; Filename: "{app}\ColorHCFR.exe"; MinVersion: 4,4; Tasks: quicklaunchicon

[Run]
Filename: "{app}\ColorHCFR.exe"; Description: "Launch HCFR"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\HCFR.url"
Type: files; Name: "{app}\Support.url"
Type: files; Name: "{app}\ColorHCFR.ini";
Type: files; Name: "{app}\ColorHCFR.log";



