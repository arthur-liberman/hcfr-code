# Microsoft Developer Studio Project File - Name="ColorHCFR" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=ColorHCFR - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ColorHCFR.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ColorHCFR.mak" CFG="ColorHCFR - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ColorHCFR - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "ColorHCFR - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ColorHCFR - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GR /GX /O1 /I "./" /I "Tools" /I "Tools/CxImage" /I "Tools/NewMenu" /I "Tools/Graph" /I "Tools/ButtonST" /I "Tools/GridCtrl" /I "Tools/CppTooltip" /I "Generators" /I "Sensors" /I "Controls" /I "Views" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /FR /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x40c /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 psapi.lib msimg32.lib cximage.lib jpeg.lib tiff.lib jasper.lib zlib.lib png.lib ddraw.lib htmlhelp.lib devlib\chcfrdi1.lib devlib\chcfrdi2.lib devlib\chcfrdi3.lib devlib\chcfrdi4.lib /nologo /subsystem:windows /machine:I386 /libpath:"Tools/CxImage/Libs/"

!ELSEIF  "$(CFG)" == "ColorHCFR - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GR /GX /ZI /Od /I "Tools/CxImage" /I "./" /I "Tools" /I "Tools/NewMenu" /I "Tools/Graph" /I "Tools/ButtonST" /I "Tools/GridCtrl" /I "Tools/CppTooltip" /I "Generators" /I "Sensors" /I "Controls" /I "Views" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_AFXDLL" /FR /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x40c /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 psapi.lib msimg32.lib cximage.lib jpeg.lib tiff.lib jasper.lib zlib.lib png.lib ddraw.lib htmlhelp.lib devlib\chcfrdi1.lib devlib\chcfrdi2.lib devlib\chcfrdi3.lib devlib\chcfrdi4.lib /nologo /subsystem:windows /debug /machine:I386 /libpath:"Tools/CxImage/Libs/"
# SUBTRACT LINK32 /profile /map /nodefaultlib

!ENDIF 

# Begin Target

# Name "ColorHCFR - Win32 Release"
# Name "ColorHCFR - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "Views"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Views\CIEChartView.cpp
# End Source File
# Begin Source File

SOURCE=.\Views\ColorTempHistoView.cpp
# End Source File
# Begin Source File

SOURCE=.\Views\GammaHistoView.cpp
# End Source File
# Begin Source File

SOURCE=.\Views\graphcontrol.cpp
# ADD CPP /I ".."
# End Source File
# Begin Source File

SOURCE=.\Views\graphscalepropdialog.cpp
# ADD CPP /I ".."
# End Source File
# Begin Source File

SOURCE=.\Views\graphsettingsdialog.cpp
# ADD CPP /I ".."
# End Source File
# Begin Source File

SOURCE=.\Views\LuminanceHistoView.cpp
# End Source File
# Begin Source File

SOURCE=.\Views\MainView.cpp
# End Source File
# Begin Source File

SOURCE=.\Views\measureshistoview.cpp
# End Source File
# Begin Source File

SOURCE=.\Views\NearBlackHistoView.cpp
# End Source File
# Begin Source File

SOURCE=.\Views\NearWhiteHistoView.cpp
# End Source File
# Begin Source File

SOURCE=.\Views\RGBHistoView.cpp
# End Source File
# Begin Source File

SOURCE=.\Views\SatLumHistoView.cpp
# End Source File
# Begin Source File

SOURCE=.\Views\SatLumShiftView.cpp
# End Source File
# Begin Source File

SOURCE=.\Views\savegraphdialog.cpp
# ADD CPP /I ".."
# End Source File
# End Group
# Begin Group "Tools"

# PROP Default_Filter ""
# Begin Group "Matrix"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Tools\Matrix.cpp
# End Source File
# End Group
# Begin Group "Grid"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Tools\GridCtrl\GridCell.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\GridCtrl\GridCellBase.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\GridCtrl\GridCtrl.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\GridCtrl\GridDropTarget.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\GridCtrl\InPlaceEdit.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\GridCtrl\TitleTip.cpp
# End Source File
# End Group
# Begin Group "Sizebar"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Tools\scbarcf.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\scbarg.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\sizecbar.cpp
# End Source File
# End Group
# Begin Group "CPPToolTips"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Tools\CppTooltip\CeXDib.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\CppTooltip\PPDrawManager.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\CppTooltip\PPHtmlDrawer.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\CppTooltip\PPTooltip.cpp
# End Source File
# End Group
# Begin Group "BtnST"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Tools\ButtonST\BtnST.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\ButtonST\ShadeButtonST.cpp
# End Source File
# End Group
# Begin Group "NewMenu"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Tools\NewMenu\MyTrace.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\NewMenu\NewDialogBar.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\NewMenu\NewMenu.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\NewMenu\NewMenuBar.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\NewMenu\NewProperty.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\NewMenu\NewStatusBar.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\NewMenu\NewToolBar.cpp
# End Source File
# End Group
# Begin Group "ColorPicker"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Tools\ColourPickerXP.cpp
# End Source File
# End Group
# Begin Group "CustomTabControl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Tools\CustomTabControl\CustomTabCtrl.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\CustomTabControl\ThemeUtil.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\Tools\BitmapTools.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\CIEChartImage.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\CreditsCtrl.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\fxcolor.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\HyperLink.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\SerialCom.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\Subclass.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools\XPGroupBox.cpp
# End Source File
# End Group
# Begin Group "Controls"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Controls\LuminanceWnd.cpp
# End Source File
# Begin Source File

SOURCE=.\Controls\RGBLevelWnd.cpp
# End Source File
# Begin Source File

SOURCE=.\Controls\SpectrumDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\Controls\TargetWnd.cpp
# End Source File
# Begin Source File

SOURCE=.\Controls\testcolorwnd.cpp
# ADD CPP /I ".."
# End Source File
# End Group
# Begin Group "Generators"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Generators\FullScreenWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\Generators\GDIGenePropPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Generators\GDIGenerator.cpp
# End Source File
# Begin Source File

SOURCE=.\Generators\Generator.cpp
# End Source File
# Begin Source File

SOURCE=.\Generators\GeneratorPropPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Generators\IRProfile.cpp
# End Source File
# Begin Source File

SOURCE=.\Generators\IRProfilePropPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Generators\KiGenePropPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Generators\KiGenerator.cpp
# End Source File
# Begin Source File

SOURCE=.\Generators\ManualDVDGenerator.cpp
# End Source File
# End Group
# Begin Group "Sensors"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Sensors\DTPSensor.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\DTPsensorproppage.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\EyeOneSensor.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\EyeOnesensorproppage.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\KiSensor.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\KiSensorPropPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\MTCSSensor.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\MTCSsensorproppage.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\OneDeviceSensor.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\OneDeviceSensorPropPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\Sensor.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\SensorPropPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\SimulatedSensor.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\SimulatedSensorPropPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\Spyder3Sensor.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\Spyder3sensorproppage.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\SpyderIISensor.cpp
# End Source File
# Begin Source File

SOURCE=.\Sensors\SpyderIIsensorproppage.cpp
# End Source File
# End Group
# Begin Group "Color"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Color.cpp
# End Source File
# End Group
# Begin Group "Wizards"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\NewDocPropertyPages.cpp
# End Source File
# Begin Source File

SOURCE=.\NewDocWizard.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\AdjustMatrixDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\AdvancedPropPage.cpp
# End Source File
# Begin Source File

SOURCE=.\AppearancePropPage.cpp
# End Source File
# Begin Source File

SOURCE=.\ColorHCFR.cpp
# End Source File
# Begin Source File

SOURCE=.\ColorHCFRConfig.cpp
# End Source File
# Begin Source File

SOURCE=.\DataSetDoc.cpp
# End Source File
# Begin Source File

SOURCE=.\DocEnumerator.cpp
# End Source File
# Begin Source File

SOURCE=.\DocTempl.cpp
# End Source File
# Begin Source File

SOURCE=.\DuplicateDocDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\Export.cpp
# End Source File
# Begin Source File

SOURCE=.\ExportReplaceDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\GeneralPropPage.cpp
# End Source File
# Begin Source File

SOURCE=.\LangSelection.cpp
# End Source File
# Begin Source File

SOURCE=.\LuxScaleAdvisor.cpp
# End Source File
# Begin Source File

SOURCE=.\MainFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\Measure.cpp
# End Source File
# Begin Source File

SOURCE=.\ModelessPropertySheet.cpp
# End Source File
# Begin Source File

SOURCE=.\MultiFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\PatternDisplay.cpp
# End Source File
# Begin Source File

SOURCE=.\RefColorDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\ReferencesPropPage.cpp
# End Source File
# Begin Source File

SOURCE=.\ScaleSizes.cpp
# End Source File
# Begin Source File

SOURCE=.\Signature.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# End Source File
# Begin Source File

SOURCE=.\ToolbarPropPage.cpp
# End Source File
# Begin Source File

SOURCE=.\WebUpdate.cpp
# End Source File
# Begin Source File

SOURCE=.\WinAppEx.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "Views headers"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\Views\CIEChartView.h
# End Source File
# Begin Source File

SOURCE=.\Views\ColorTempHistoView.h
# End Source File
# Begin Source File

SOURCE=.\CSpreadSheet.h
# End Source File
# Begin Source File

SOURCE=.\DuplicateDocDlg.h
# End Source File
# Begin Source File

SOURCE=.\Export.h
# End Source File
# Begin Source File

SOURCE=.\ExportReplaceDialog.h
# End Source File
# Begin Source File

SOURCE=.\Views\GammaHistoView.h
# End Source File
# Begin Source File

SOURCE=.\GeneralPropPage.h
# End Source File
# Begin Source File

SOURCE=.\Views\GraphControl.h
# End Source File
# Begin Source File

SOURCE=.\Views\graphscalepropdialog.h
# End Source File
# Begin Source File

SOURCE=.\Views\graphsettingsdialog.h
# End Source File
# Begin Source File

SOURCE=.\LangSelection.h
# End Source File
# Begin Source File

SOURCE=.\Views\LuminanceHistoView.h
# End Source File
# Begin Source File

SOURCE=.\Views\MainView.h
# End Source File
# Begin Source File

SOURCE=.\Views\measureshistoview.h
# End Source File
# Begin Source File

SOURCE=.\Views\NearBlackHistoView.h
# End Source File
# Begin Source File

SOURCE=.\Views\NearWhiteHistoView.h
# End Source File
# Begin Source File

SOURCE=.\Views\RGBHistoView.h
# End Source File
# Begin Source File

SOURCE=.\Views\SatLumHistoView.h
# End Source File
# Begin Source File

SOURCE=.\Views\SatLumShiftView.h
# End Source File
# Begin Source File

SOURCE=.\Views\savegraphdialog.h
# End Source File
# Begin Source File

SOURCE=.\WebUpdate.h
# End Source File
# End Group
# Begin Group "Tools headers"

# PROP Default_Filter "h"
# Begin Group "Matric headers"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\Tools\Matrix.h
# End Source File
# End Group
# Begin Group "Grid headers"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\Tools\GridCtrl\CellRange.h
# End Source File
# Begin Source File

SOURCE=.\Tools\GridCtrl\GridCell.h
# End Source File
# Begin Source File

SOURCE=.\Tools\GridCtrl\GridCellBase.h
# End Source File
# Begin Source File

SOURCE=.\Tools\GridCtrl\GridCtrl.h
# End Source File
# Begin Source File

SOURCE=.\Tools\GridCtrl\GridDropTarget.h
# End Source File
# Begin Source File

SOURCE=.\Tools\GridCtrl\InPlaceEdit.h
# End Source File
# Begin Source File

SOURCE=.\Tools\GridCtrl\MemDC.h
# End Source File
# Begin Source File

SOURCE=.\Tools\GridCtrl\TitleTip.h
# End Source File
# End Group
# Begin Group "Sizebar headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Tools\scbarcf.h
# End Source File
# Begin Source File

SOURCE=.\Tools\scbarg.h
# End Source File
# Begin Source File

SOURCE=.\Tools\sizecbar.h
# End Source File
# End Group
# Begin Group "CPPToolTips headers"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\Tools\CppTooltip\CeXDib.h
# End Source File
# Begin Source File

SOURCE=.\Tools\CppTooltip\PPDrawManager.h
# End Source File
# Begin Source File

SOURCE=.\Tools\CppTooltip\PPHtmlDrawer.h
# End Source File
# Begin Source File

SOURCE=.\Tools\CppTooltip\PPTooltip.h
# End Source File
# End Group
# Begin Group "BtnST headers"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\Tools\ButtonST\BtnST.h
# End Source File
# Begin Source File

SOURCE=.\Tools\ButtonST\ShadeButtonST.h
# End Source File
# End Group
# Begin Group "NewMenu headers"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\Tools\NewMenu\MyTrace.h
# End Source File
# Begin Source File

SOURCE=.\Tools\NewMenu\NewDialogBar.h
# End Source File
# Begin Source File

SOURCE=.\Tools\NewMenu\NewMenu.h
# End Source File
# Begin Source File

SOURCE=.\Tools\NewMenu\NewMenuBar.h
# End Source File
# Begin Source File

SOURCE=.\Tools\NewMenu\NewProperty.h
# End Source File
# Begin Source File

SOURCE=.\Tools\NewMenu\NewStatusBar.h
# End Source File
# Begin Source File

SOURCE=.\Tools\NewMenu\NewToolBar.h
# End Source File
# End Group
# Begin Group "ColorPicker headers"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\Tools\ColourPickerXP.h
# End Source File
# End Group
# Begin Group "CustomTabControl headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Tools\CustomTabControl\CustomTabCtrl.h
# End Source File
# Begin Source File

SOURCE=.\Tools\CustomTabControl\Schemadef.h
# End Source File
# Begin Source File

SOURCE=.\Tools\CustomTabControl\ThemeUtil.h
# End Source File
# Begin Source File

SOURCE=.\Tools\CustomTabControl\Tmschema.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Tools\BitmapTools.h
# End Source File
# Begin Source File

SOURCE=.\Tools\CreditsCtrl.h
# End Source File
# Begin Source File

SOURCE=.\Tools\fxcolor.h
# End Source File
# Begin Source File

SOURCE=.\Tools\HyperLink.h
# End Source File
# Begin Source File

SOURCE=.\Tools\SerialCom.h
# End Source File
# Begin Source File

SOURCE=.\Tools\Subclass.h
# End Source File
# Begin Source File

SOURCE=.\Tools\XPGroupBox.h
# End Source File
# End Group
# Begin Group "Controls headers"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\Controls\LuminanceWnd.h
# End Source File
# Begin Source File

SOURCE=.\Controls\RGBLevelWnd.h
# End Source File
# Begin Source File

SOURCE=.\Controls\SpectrumDlg.h
# End Source File
# Begin Source File

SOURCE=.\Controls\TargetWnd.h
# End Source File
# Begin Source File

SOURCE=.\Controls\testcolorwnd.h
# End Source File
# End Group
# Begin Group "Generators headers"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\Generators\FullScreenWindow.h
# End Source File
# Begin Source File

SOURCE=.\Generators\GDIGenePropPage.h
# End Source File
# Begin Source File

SOURCE=.\Generators\GDIGenerator.h
# End Source File
# Begin Source File

SOURCE=.\Generators\Generator.h
# End Source File
# Begin Source File

SOURCE=.\Generators\GeneratorPropPage.h
# End Source File
# Begin Source File

SOURCE=.\Generators\IRProfile.h
# End Source File
# Begin Source File

SOURCE=.\Generators\IRProfilePropPage.h
# End Source File
# Begin Source File

SOURCE=.\Generators\KiGenePropPage.h
# End Source File
# Begin Source File

SOURCE=.\Generators\KiGenerator.h
# End Source File
# Begin Source File

SOURCE=.\Generators\ManualDVDGenerator.h
# End Source File
# End Group
# Begin Group "Sensors headers"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\Sensors\DTPSensor.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\DTPsensorproppage.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\EyeOneSensor.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\EyeOnesensorproppage.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\KiSensor.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\KiSensorPropPage.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\MTCSSensor.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\MTCSsensorproppage.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\OneDeviceSensor.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\OneDeviceSensorPropPage.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\Sensor.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\SensorPropPage.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\SimulatedSensor.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\SimulatedSensorPropPage.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\Spyder3Sensor.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\Spyder3sensorproppage.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\SpyderIISensor.h
# End Source File
# Begin Source File

SOURCE=.\Sensors\SpyderIIsensorproppage.h
# End Source File
# End Group
# Begin Group "Color headers"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\Color.h
# End Source File
# End Group
# Begin Group "Wizards headers"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\NewDocPropertyPages.h
# End Source File
# Begin Source File

SOURCE=.\NewDocWizard.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\AdjustMatrixDlg.h
# End Source File
# Begin Source File

SOURCE=.\AdvancedPropPage.h
# End Source File
# Begin Source File

SOURCE=.\AppearancePropPage.h
# End Source File
# Begin Source File

SOURCE=.\ColorHCFR.h
# End Source File
# Begin Source File

SOURCE=.\ColorHCFRConfig.h
# End Source File
# Begin Source File

SOURCE=.\DataSetDoc.h
# End Source File
# Begin Source File

SOURCE=.\DocEnumerator.h
# End Source File
# Begin Source File

SOURCE=.\DocTempl.h
# End Source File
# Begin Source File

SOURCE=.\HelpID.h
# End Source File
# Begin Source File

SOURCE=.\htmlhelp.h
# End Source File
# Begin Source File

SOURCE=.\LuxScaleAdvisor.h
# End Source File
# Begin Source File

SOURCE=.\MainFrm.h
# End Source File
# Begin Source File

SOURCE=.\Tools\MainWndPlacement.h
# End Source File
# Begin Source File

SOURCE=.\Measure.h
# End Source File
# Begin Source File

SOURCE=.\ModelessPropertySheet.h
# End Source File
# Begin Source File

SOURCE=.\MultiFrm.h
# End Source File
# Begin Source File

SOURCE=.\PatternDisplay.h
# End Source File
# Begin Source File

SOURCE=.\RefColorDlg.h
# End Source File
# Begin Source File

SOURCE=.\ReferencesPropPage.h
# End Source File
# Begin Source File

SOURCE=.\ScaleSizes.h
# End Source File
# Begin Source File

SOURCE=.\Signature.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\ToolbarPropPage.h
# End Source File
# Begin Source File

SOURCE=.\WinAppEx.h
# End Source File
# End Group
# Begin Group "Resources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ColorHCFR.rc
# End Source File
# Begin Source File

SOURCE=.\res\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\res\splash_screen.png
# End Source File
# End Group
# End Target
# End Project
# Section ColorHCFR : {3A2B370C-BA0A-11D1-B137-0000F8753F5D}
# 	2:21:DefaultSinkHeaderFile:msrgbchart.h
# 	2:16:DefaultSinkClass:CMSRGBChart
# End Section
# Section ColorHCFR : {3A2B370A-BA0A-11D1-B137-0000F8753F5D}
# 	2:5:Class:CMSRGBChart
# 	2:10:HeaderFile:msrgbchart.h
# 	2:8:ImplFile:msrgbchart.cpp
# End Section
