# Microsoft Developer Studio Project File - Name="CHCFR21_DEUTSCH" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=CHCFR21_DEUTSCH - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "CHCFR21_DEUTSCH.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "CHCFR21_DEUTSCH.mak" CFG="CHCFR21_DEUTSCH - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "CHCFR21_DEUTSCH - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "CHCFR21_DEUTSCH - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "CHCFR21_DEUTSCH - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "CHCFR21_DEUTSCH___Win32_Release"
# PROP BASE Intermediate_Dir "CHCFR21_DEUTSCH___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
F90=fl32.exe
LIB32=link.exe -lib
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CHCFR21_DEUTSCH_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CHCFR21_DEUTSCH_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 /nologo /dll /pdb:none /machine:I386 /nodefaultlib /noentry

!ELSEIF  "$(CFG)" == "CHCFR21_DEUTSCH - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "CHCFR21_DEUTSCH___Win32_Debug"
# PROP BASE Intermediate_Dir "CHCFR21_DEUTSCH___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
F90=fl32.exe
LIB32=link.exe -lib
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CHCFR21_DEUTSCH_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /GX /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CHCFR21_DEUTSCH_EXPORTS" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /dll /pdb:none /machine:I386 /nodefaultlib /noentry

!ENDIF 

# Begin Target

# Name "CHCFR21_DEUTSCH - Win32 Release"
# Name "CHCFR21_DEUTSCH - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\CHCFR21_DEUTSCH.rc

!IF  "$(CFG)" == "CHCFR21_DEUTSCH - Win32 Release"

!ELSEIF  "$(CFG)" == "CHCFR21_DEUTSCH - Win32 Debug"

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

SOURCE=.\res\bitmap1.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bitmap2.bmp
# End Source File
# Begin Source File

SOURCE=.\res\blueprimary.bmp
# End Source File
# Begin Source File

SOURCE=.\res\blueprimaryplot.bmp
# End Source File
# Begin Source File

SOURCE=.\res\camera.ico
# End Source File
# Begin Source File

SOURCE=.\res\camera_b.bmp
# End Source File
# Begin Source File

SOURCE=.\res\cieplot.ico
# End Source File
# Begin Source File

SOURCE=.\res\ColorHCFR.ico
# End Source File
# Begin Source File

SOURCE=.\res\ColorHCFR.rc2
# End Source File
# Begin Source File

SOURCE=.\res\ColorMeasurementsDoc.ico
# End Source File
# Begin Source File

SOURCE=.\res\colors_i.ico
# End Source File
# Begin Source File

SOURCE=.\res\colortempplot.bmp
# End Source File
# Begin Source File

SOURCE=.\res\contrast.bmp
# End Source File
# Begin Source File

SOURCE=.\res\cursor_meas.cur
# End Source File
# Begin Source File

SOURCE=.\res\cyansecondary.bmp
# End Source File
# Begin Source File

SOURCE=.\res\delete.ico
# End Source File
# Begin Source File

SOURCE=.\res\delete_b.bmp
# End Source File
# Begin Source File

SOURCE=.\res\docref.ico
# End Source File
# Begin Source File

SOURCE=.\res\DRAGMOVE.CUR
# End Source File
# Begin Source File

SOURCE=.\res\graycolo.ico
# End Source File
# Begin Source File

SOURCE=.\res\grayplot.bmp
# End Source File
# Begin Source File

SOURCE=.\res\grayscal.ico
# End Source File
# Begin Source File

SOURCE=.\res\greenprimary.bmp
# End Source File
# Begin Source File

SOURCE=.\res\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\res\icon_controlled.ico
# End Source File
# Begin Source File

SOURCE=.\res\idi_start.ico
# End Source File
# Begin Source File

SOURCE=.\res\idi_stop.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_ciec.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_coltemp.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_doct.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_gamm.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_lumi.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_meas.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_nebl.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_newh.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_prop.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_rgbh.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_rgbv.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_slum.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_sshi.ico
# End Source File
# Begin Source File

SOURCE=.\res\illuminantplot.bmp
# End Source File
# Begin Source File

SOURCE=.\res\logo.bmp
# End Source File
# Begin Source File

SOURCE=.\res\magentasecondary.bmp
# End Source File
# Begin Source File

SOURCE=.\res\meas_cont.bmp
# End Source File
# Begin Source File

SOURCE=.\res\meas_g.bmp
# End Source File
# Begin Source File

SOURCE=.\res\meas_p.bmp
# End Source File
# Begin Source File

SOURCE=.\res\meas_ps.bmp
# End Source File
# Begin Source File

SOURCE=.\res\meas_psg.bmp
# End Source File
# Begin Source File

SOURCE=.\res\meas_stop.bmp
# End Source File
# Begin Source File

SOURCE=.\res\measureplot.bmp
# End Source File
# Begin Source File

SOURCE=.\res\medium_t.bmp
# End Source File
# Begin Source File

SOURCE=.\res\medium_tb_main_hicol.bmp
# End Source File
# Begin Source File

SOURCE=.\res\medium_tb_measures_ex_hicol.bmp
# End Source File
# Begin Source File

SOURCE=.\res\medium_tb_measures_hicol.bmp
# End Source File
# Begin Source File

SOURCE=.\res\medium_tb_measures_sat_hicol.bmp
# End Source File
# Begin Source File

SOURCE=.\res\medium_tb_sensor_hicol.bmp
# End Source File
# Begin Source File

SOURCE=.\res\medium_tb_views_hicol.bmp
# End Source File
# Begin Source File

SOURCE=.\res\mediumto.bmp
# End Source File
# Begin Source File

SOURCE=.\res\medtoot.bmp
# End Source File
# Begin Source File

SOURCE=.\res\menubarg.bmp
# End Source File
# Begin Source File

SOURCE=.\res\nearblac.ico
# End Source File
# Begin Source File

SOURCE=.\res\nearblack.bmp
# End Source File
# Begin Source File

SOURCE=.\res\nearwhit.ico
# End Source File
# Begin Source File

SOURCE=.\res\nearwhite.bmp
# End Source File
# Begin Source File

SOURCE=.\res\newtab_i.ico
# End Source File
# Begin Source File

SOURCE=.\res\point.bmp
# End Source File
# Begin Source File

SOURCE=.\res\properties.ico
# End Source File
# Begin Source File

SOURCE=.\res\redprimary.bmp
# End Source File
# Begin Source File

SOURCE=.\res\refblueprimary.bmp
# End Source File
# Begin Source File

SOURCE=.\res\refcrossblue.bmp
# End Source File
# Begin Source File

SOURCE=.\res\refcrosscyan.bmp
# End Source File
# Begin Source File

SOURCE=.\res\refcrossgreen.bmp
# End Source File
# Begin Source File

SOURCE=.\res\refcrossmagenta.bmp
# End Source File
# Begin Source File

SOURCE=.\res\refcrossred.bmp
# End Source File
# Begin Source File

SOURCE=.\res\refcrossyellow.bmp
# End Source File
# Begin Source File

SOURCE=.\res\refcyansecondary.bmp
# End Source File
# Begin Source File

SOURCE=.\res\refgreenprimary.bmp
# End Source File
# Begin Source File

SOURCE=.\res\refmagentasecondary.bmp
# End Source File
# Begin Source File

SOURCE=.\res\refredprimary.bmp
# End Source File
# Begin Source File

SOURCE=.\res\refyellowsecondary.bmp
# End Source File
# Begin Source File

SOURCE=.\res\sat_all.bmp
# End Source File
# Begin Source File

SOURCE=.\res\satblue.bmp
# End Source File
# Begin Source File

SOURCE=.\res\satcyan.bmp
# End Source File
# Begin Source File

SOURCE=.\res\satgreen.bmp
# End Source File
# Begin Source File

SOURCE=.\res\satmagenta.bmp
# End Source File
# Begin Source File

SOURCE=.\res\satred.bmp
# End Source File
# Begin Source File

SOURCE=.\res\satyellow.bmp
# End Source File
# Begin Source File

SOURCE=.\res\selectedplot.bmp
# End Source File
# Begin Source File

SOURCE=.\res\settings.ico
# End Source File
# Begin Source File

SOURCE=.\res\spectrum.ico
# End Source File
# Begin Source File

SOURCE=.\res\target.bmp
# End Source File
# Begin Source File

SOURCE=.\res\tbmeasures.bmp
# End Source File
# Begin Source File

SOURCE=.\res\tbmeasures_ex.bmp
# End Source File
# Begin Source File

SOURCE=.\res\tbmeasures_sat.bmp
# End Source File
# Begin Source File

SOURCE=.\res\Toolbar.bmp
# End Source File
# Begin Source File

SOURCE=.\res\trash.ico
# End Source File
# Begin Source File

SOURCE=.\res\yellowsecondary.bmp
# End Source File
# End Group
# End Target
# End Project
