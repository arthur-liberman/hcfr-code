# Microsoft Developer Studio Project File - Name="CHCFR21_PATTERNS" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=CHCFR21_PATTERNS - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "CHCFR21_PATTERNS.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "CHCFR21_PATTERNS.mak" CFG="CHCFR21_PATTERNS - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "CHCFR21_PATTERNS - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "CHCFR21_PATTERNS - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "CHCFR21_PATTERNS - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "CHCFR21_PATTERNS___Win32_Release"
# PROP BASE Intermediate_Dir "CHCFR21_PATTERNS___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
F90=fl32.exe
LIB32=link.exe -lib
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CHCFR21_PATTERNS_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CHCFR21_PATTERNS_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 /nologo /dll /pdb:none /machine:I386 /nodefaultlib /noentry

!ELSEIF  "$(CFG)" == "CHCFR21_PATTERNS - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "CHCFR21_PATTERNS___Win32_Debug"
# PROP BASE Intermediate_Dir "CHCFR21_PATTERNS___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
F90=fl32.exe
LIB32=link.exe -lib
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CHCFR21_PATTERNS_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /GX /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CHCFR21_PATTERNS_EXPORTS" /FR /YX /FD /GZ /NOENTRY /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /dll /pdb:none /machine:I386 /nodefaultlib /noentry

!ENDIF 

# Begin Target

# Name "CHCFR21_PATTERNS - Win32 Release"
# Name "CHCFR21_PATTERNS - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\CHCFR21_PATTERNS.rc

!IF  "$(CFG)" == "CHCFR21_PATTERNS - Win32 Release"

!ELSEIF  "$(CFG)" == "CHCFR21_PATTERNS - Win32 Debug"

# PROP Intermediate_Dir "Debug"
# ADD BASE RSC /l 0x40c
# ADD RSC /l 0x40c

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=".\res\patterns\16x16-v6\01-16x16-blanche-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\01-barresCalibrage-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\16x16-v6\02-16x16-noire-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\02-barresCalibrageFond30-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\16x16-v6\03-16x16-rouge-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\03-barresCalibrageFond50-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\16x16-v6\04-16x16-verte-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\04-barresMoyennesCalibrage-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\16x16-v6\05-16x16-bleue-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\05-barresMoyennesCalibrageFond30-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\16x16-v6\06-16x16-gris-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\06-barresMoyennesCalibrageFond50-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\16x16-v6\07-16x16-4gris-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\07-barresSombresCalibrage-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\16x16-v6\08-16x16-4gris-1-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_7200\08-barresSombresCalibrageFond30-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\08-barresSombresCalibrageFond30-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\16x16-v6\09-16x16-4gris-2-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\09-barresSombresCalibrageFond50-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\16x16-v6\10-16x16-4gris-3-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\10-barresTresSombresCalibrage-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\16x16-v6\11-16x16-4gris-4-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\11-barresTresSombresCalibrageFond30-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\12-barresTresSombresCalibrageFond50-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\13-contrasteCalibrage-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\14-contrasteCalibrageBlanc30-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\15-contrasteCalibrageFond30-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\16-couleursCalibrage-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\17-resolutionCalibrage-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\18-resolutionCalibrage-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\19-resolutionCalibrage-1280-720.png"
# End Source File
# Begin Source File

SOURCE=.\res\patterns\1956.png
# End Source File
# Begin Source File

SOURCE=".\res\patterns\test_bench_720\20-resolutionCalibrage-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi1-00-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi1-00-2-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi1-00-5-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi1-01-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi1-02-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi1-05-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi1-10-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi1-20-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi1-30-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi1-40-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi1-50-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi2-00-2-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi2-00-5-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi2-01-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi2-02-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi2-05-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi2-10-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi2-20-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi2-30-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi2-40-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\ansi2-50-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\patterns_720\barres-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\patterns_720\barresFondNoir-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\patterns_720\barresSaturees-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\contraste(0-100)1280-720-01-50.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\contraste(0-100)1280-720-02-50.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\contraste(0-100)1280-720-05-50.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\contraste(0-100)1280-720-10-50.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\patterns_720\contraste(0-100)1280-720-10-80.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\contraste(0-100)1280-720-20-50.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\contraste(0-100)1280-720-30-50.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\contraste(0-100)1280-720-40-50.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\contrast_720\contraste(0-100)1280-720-50-50.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\patterns_720\resolution-1280-720.png"
# End Source File
# Begin Source File

SOURCE=".\res\patterns\smpte-75-bars1024.png"
# End Source File
# Begin Source File

SOURCE=.\res\patterns\testimg.jpg
# End Source File
# End Group
# End Target
# End Project
