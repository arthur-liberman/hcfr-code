// ***************************************************************
//  madTPG.h                  version: 1.2.0  ·  date: 2014-12-01
//  -------------------------------------------------------------
//  madTPG remote controlling
//  -------------------------------------------------------------
//  Copyright (C) 2013 - 2014 www.madshi.net, BSD license
// ***************************************************************

// 2014-12-01 1.2.0 (1) added Connect, deprecated BindConnect and ConnectDialog
//                  (2) added APIs to get/set "stay on top"    button state
//                  (3) added APIs to get/set "use fullscreen" button state
//                  (4) added APIs to get/set "disable OSD"    button state
//                  (5) added Get/SetPatternConfig APIs
//                  (6) added ShowRGBEx API
//                  (7) added various 3dlut conversion and import APIs
// 2013-11-27 1.1.0 added madVR_GetBlackAndWhiteLevel
// 2013-06-15 1.0.0 initial version

// ----------------------------------------------------------------------------

#if !defined(MADTPG_H)
#define MADTPG_H

#ifdef __cplusplus
  extern "C" {
#endif

#include <windows.h>

#pragma pack(1)

// ============================================================================
// I. THE EASY WAY
// ============================================================================

// ----------------------------------------------------------------------------
// connecting to madVR - two options

// (1) "madVR_Connect" is the recommended API to find a madTPG instance to
//     connect to. You can let this API search the local PC and/or the LAN for
//     already running madTPG instances, and/or if no instances are found in a
//     specific time interval, you can simply let the API start a new madTPG
//     instance for you on the local PC. Or you can let the API show a dialog
//     to the end user, allowing the end user to enter an IP address to locate
//     an already running madTPG instance.

const static int CM_ConnectToLocalInstance = 0;  // search for madTPG on the local PC, connect to the first found instance
const static int CM_ConnectToLanInstance   = 1;  // search for madTPG on the LAN,      connect to the first found instance
const static int CM_StartLocalInstance     = 2;  // start madTPG on the local PC and connect to it
const static int CM_ShowListDialog         = 3;  // search local PC and LAN, and let the user choose which instance to connect to
const static int CM_ShowIpAddrDialog       = 4;  // let the user enter the IP address of a PC which runs madTPG, then connect
const static int CM_Fail                   = 5;  // fail immediately

#ifdef __cplusplus
  BOOL madVR_Connect(int method1 = CM_ConnectToLocalInstance, DWORD timeOut1 = 1000,
                     int method2 = CM_ConnectToLanInstance,   DWORD timeOut2 = 3000,
                     int method3 = CM_ShowListDialog,         DWORD timeOut3 = INFINITE,
                     int method4 = CM_Fail,                   DWORD timeOut4 = 0,
                     HWND parentWindow = NULL);
#else
  BOOL madVR_Connect(int method1, DWORD timeOut1,
                     int method2, DWORD timeOut2,
                     int method3, DWORD timeOut3,
                     int method4, DWORD timeOut4,
                     HWND parentWindow);
#endif

// (2) If you want to connect to a madVR instance running on a LAN PC with a
//     known IP address, "madVR_ConnectToIp" lets you do just that.
// "result=true"  means: A madVR instance was found and a connection opened.
// "result=false" means: No madVR instance found, or connection failed.
#ifdef __cplusplus
  BOOL madVR_ConnectToIp(LPCSTR ipAddress, DWORD timeOut = 1000);
#else
  BOOL madVR_ConnectToIp(LPCSTR ipAddress, DWORD timeOut       );
#endif

// ----------------------------------------------------------------------------
// remote controlling the connected madVR instance

// The following functions allow you to read and set the "pressed" state
// of the "stay on top", "use fullscreen" and "disable OSD" buttons.
BOOL madVR_IsStayOnTopButtonPressed();
BOOL madVR_IsUseFullscreenButtonPressed();
BOOL madVR_IsDisableOsdButtonPressed();
BOOL madVR_SetStayOnTopButton(BOOL pressed);
BOOL madVR_SetUseFullscreenButton(BOOL pressed);
BOOL madVR_SetDisableOsdButton(BOOL pressed);

// "madVR_GetBlackAndWhiteLevel" reports the madVR output level setup.
// E.g. if madVR is setup to output TV levels, you'll get "blackLevel = 16" and
// "whiteLevel = 235" reported.
// The purpose of asking this information is that it allows you to avoid
// dithering, if you so prefer. Dithering will be automatically disabled by
// madVR if the final 8bit output value calculated by madVR ends up being a
// simple cardinal without any fractional part.
// E.g. if you use "madVR_ShowRGB(0.5, 0.5, 0.5)" with madVR configured to PC
// levels, the final 8bit value will be 127.5, which means that madVR has to
// use dithering to display the color correctly. If you want to avoid dithering,
// use "x / (whiteLevel - blackLevel)" values.
// Dithering in itself is not bad. It allows madVR to produce test pattern
// colors which would otherwise not be possible to display in 8bit. However,
// calibration quality might be ever so slightly improved if you choose
// measurement colors which don't need dithering to display correctly. It's
// your choice, though. Maybe some part of your calibration might even improve
// if you have the chance to measure colors with a bitdepth higher than 8bit.
BOOL madVR_GetBlackAndWhiteLevel(int *blackLevel, int *whiteLevel);

// "madVR_Disable3dlut" disables 3dlut processing.
// The 3dlut stays disabled until the connection is closed.
// Disable the 3dlut if you want to calibrate/profile the display, or if you
// want to measure the display behaviour prior to calibration.
// Don't disable the 3dlut if you want to measure the final display after full
// calibration.
BOOL madVR_Disable3dlut();

// "madVR_Get/SetDeviceGammaRamp" calls the win32 API "Get/SetDeviceGammaRamp"
// on the target PC / display. A "NULL" ramp sets a linear ramp.
// The original ramp is automatically restored when you close the connection.
#ifdef __cplusplus
  BOOL madVR_GetDeviceGammaRamp(LPVOID ramp);
  BOOL madVR_SetDeviceGammaRamp(LPVOID ramp = NULL);
#else
  BOOL madVR_GetDeviceGammaRamp(LPVOID ramp);
  BOOL madVR_SetDeviceGammaRamp(LPVOID ramp);
#endif

// "madVR_SetOsdText" shows a "text" on the top left of the video image.
BOOL madVR_SetOsdText(LPCWSTR text);

// "madVR_Get/SetPatternConfig" lets you retrieve/define how much percent of
// the madVR rendering window is painted in the test pattern color, and how
// much is painted with a specific background color.
// Using a background color can make sense for plasma measurements.
// patternAreaInPercent:     1-100 (%)
// backgroundLevelInPercent: 0-100 (%)
// backgroundMode:           0 = constant gray; 1 = APL - gamma light; 2 = APL - linear light
// blackBorderWidth:         0-100 (pixels), default: 20
// When calling "madVR_SetPatternConfig", you can set all parameters that you
// don't want to change to "-1".
BOOL madVR_GetPatternConfig(int *patternAreaInPercent, int *backgroundLevelInPercent, int *backgroundMode, int *blackBorderWidth);
BOOL madVR_SetPatternConfig(int  patternAreaInPercent, int  backgroundLevelInPercent, int  backgroundMode, int  blackBorderWidth);

// "madVR_ShowProgressBar" initializes the madVR progress bar.
// It will progress one step with every "madVR_ShowRGB" call (see below).
BOOL madVR_ShowProgressBar(int numberOfRgbMeasurements);

// "madVR_SetProgressBarPos" sets the madVR progress bar to a specific pos.
// After calling this API, the progress bar will not automatically move
// forward after calls to "madVR_ShowRGB", anymore. Calling this API means
// you have to manually move the progress bar.
BOOL madVR_SetProgressBarPos(int currentPos, int maxPos);

// "madVR_ShowRGB" shows a specific RGB color test pattern.
// Values are gamma corrected with "black = 0.0" and "white = 1.0". The values
// are automatically converted to TV or PC output levels by madTPG, depending
// on the end user's display device setup.
// You can go below 0.0 or above 1.0 for BTB/WTW, if you want. Of course a test
// pattern with BTB/WTW will only work if the connected madVR instance is
// configured to TV level output.
// "madVR_ShowRGB" blocks until the GPU has actually output the test pattern to
// the display. How fast the display will actually show the test pattern will
// depend on the display's input latency, which is outside of madVR's control.
BOOL madVR_ShowRGB(double r, double g, double b);

// "madVR_ShowRGBEx" works similar to "ShowRGB", but instead of letting
// madTPG calculate the background color, you can define it yourself.
BOOL madVR_ShowRGBEx(double r, double g, double b, double bgR, double bgG, double bgB);

// ----------------------------------------------------------------------------
// 3dlut conversion & loading

typedef double TEeColor3dlut[ 65][ 65][ 65][3];
typedef WORD     TMadVR3dlut[256][256][256][3];

// "madVR_Convert3dlutFile" converts an existing eeColor 3dlut file to
// the madVR 3dlut file format. The 64^3 3dlut is internally interpolated to
// 256^3 by using a linear Mitchell filter.
// "gamut" 0: BT.709; 1: SMPTE-C; 2: EBU/PAL; 3: BT.2020; 4: DCI-P3
BOOL madVR_Convert3dlutFile(LPWSTR eeColor3dlutFile, LPWSTR madVR3dlutFile, int gamut);

// "madVR_Create3dlutFileFromArray65/255" creates a madVR 3dlut file from
// an array which is sorted in the same way as an eeColor 3dlut text file.
// The 64^3 dlut is internally interpolated to 256^3 by using a linear
// Mitchell filter.
// "gamut" 0: BT.709; 1: SMPTE-C; 2: EBU/PAL; 3: BT.2020; 4: DCI-P3
BOOL madVR_Create3dlutFileFromArray65 (TEeColor3dlut *lutData, LPWSTR madVR3dlutFile, int gamut);
BOOL madVR_Create3dlutFileFromArray256(  TMadVR3dlut *lutData, LPWSTR madVR3dlutFile, int gamut);

// "madVR_Load3dlutFile" loads a 3dlut (can be either eeColor or madVR
// file format) into the connected madTPG instance.
// "saveToSettings=false" means: the 3dlut only stays loaded until madTPG is closed; "gamut" is ignored
// "saveToSettings=true"  means: the 3dlut is permanently written to the madVR settings, to the "gamut" slot
// "gamut" 0: BT.709; 1: SMPTE-C; 2: EBU/PAL; 3: BT.2020; 4: DCI-P3
BOOL madVR_Load3dlutFile(LPWSTR lutFile, BOOL saveToSettings, int gamut);

// "madVR_Load3dlutFromArray65/255" loads a 3dlut into the connected
// madTPG instance.
// "saveToSettings=false" means: the 3dlut only stays loaded until madTPG is closed; "gamut" is ignored
// "saveToSettings=true"  means: the 3dlut is permanently written to the madVR settings, to the "gamut" slot
// "gamut" 0: BT.709; 1: SMPTE-C; 2: EBU/PAL; 3: BT.2020; 4: DCI-P3
BOOL madVR_Load3dlutFromArray65 (TEeColor3dlut *lutData, BOOL saveToSettings, int gamut);
BOOL madVR_Load3dlutFromArray256(  TMadVR3dlut *lutData, BOOL saveToSettings, int gamut);

// ----------------------------------------------------------------------------
// disconnecting from madVR

// "madVR_Disconnect" closes the current connection to madVR.
BOOL madVR_Disconnect();

// "madVR_Quit" closes the madTPG instance we're connected to.
BOOL madVR_Quit();

// ----------------------------------------------------------------------------
// checking madVR availability

// "madVR_IsAvailable" checks whether the madHcNet32.dll can be found.
// It must either be in the current directory, or in the search path.
// Or alternatively it will also work if madVR is installed on the current PC.
BOOL madVR_IsAvailable();

// ----------------------------------------------------------------------------


// ============================================================================
// II. THE HARD WAY
// ============================================================================

// ----------------------------------------------------------------------------
// finding / enumerating madVR instances on the LAN

// The following API lets you automatically locate madVR instances running
// anywhere on either the local PC or remote PCs connected via LAN.
// For every found madVR instance this full information record is returned:

typedef struct _TMadVRInstance
{                           // Example:
  HANDLE    handle;         // 1
  ULONGLONG instance;       // 0x40001000
  LPSTR     ipAddress;      // "192.168.1.1"
  LPWSTR    computerName;   // "HTPC"
  LPWSTR    userName;       // "Walter"
  LPWSTR    os;             // "Windows 8.1"
  ULONG_PTR processId;      // 248
  LPWSTR    processName;    // "madVR Test Pattern Generator"
  LPWSTR    exeFile;        // "madTPG.exe"
  LPWSTR    exeVersion;     // 1.0.0.0
  LPWSTR    madVRVersion;   // 0.87.11.0
  LPWSTR    gpuName;        // "GeForce 750"
  LPWSTR    monitorName;    // "JVC HD-350"
} TMadVRInstance, *PMadVRInstance;

// "madVR_Find_Async" also starts a search for madVR instances, but instead of
// returning information directly, it will send a message to the specified
// "window" for every found madVR instance.
// After the search is complete, "madVR_Find_Async" will keep an eye open for
// newly started and closed down madVR instances and automatically report them
// to your "window", as well.
// In order to stop notification, call "madVR_Find_Async" with NULL parameters.
// wParam: 0 = a new madVR instance was detected
//         1 = a known madVR instance closed down
// lParam: "PMadVRInstance" of the new/closed madVR instance
//         do not free the PMadVRInstance, the memory is managed by madVR
BOOL madVR_Find_Async(HWND window, DWORD msg);

// ----------------------------------------------------------------------------
// connection to a specific madVR instance

// "madVR_Connect" connects you to the specified madVR instance.
// If a previous connection exists, it will be closed automatically.
// The "handle" and "instance" originate from a "madVR_Find_Async" search.
BOOL madVR_ConnectToInstance(HANDLE handle, ULONGLONG instance);

// ----------------------------------------------------------------------------


// ============================================================================
// III. GUI LOCALIZATION
// ============================================================================

// Localize/customize all texts used by madVR_ConnectDialog.
void Localize_ConnectDialog (LPCWSTR title,       // madTPG selection...
                             LPCWSTR text,        // Please make sure that madTPG is running on the target computer and then select it here:
                             LPCWSTR columns,     // ip address|computer|pid|process|gpu|monitor|os
                             LPCWSTR notListed,   // The madTPG instance I'm looking for is not listed
                             LPCWSTR select,      // Select
                             LPCWSTR cancel       // Cancel
                            );

// Localize/customize all texts used by madVR_IpAddressDialog.
// This is a dialog used internally by madVR_ConnectDialog.
void Localize_IpAddressDialog (LPCWSTR title,          // find madTPG instance...
                               LPCWSTR text,           // Please enter the IP address of the computer on which madTPG is running:
                               LPCWSTR connect,        // Connect
                               LPCWSTR cancel,         // Cancel
                               LPCWSTR warningTitle,   // Warning...
                               LPCWSTR warningText1,   // There doesn't seem to be any madTPG instance running on that computer.
                               LPCWSTR warningText2    // The target computer does not react.\n\n
                              );                       // Please check if it's turned on and connected to the LAN.\n
                                                       // You may also want to double check your firewall settings.


// ============================================================================
// deprecated APIs
// ============================================================================

#ifdef __cplusplus
  BOOL madVR_ConnectDialog(BOOL searchLan = TRUE, HWND parentWindow = NULL);
  BOOL madVR_BlindConnect(BOOL searchLan = TRUE, DWORD timeOut = 3000);
#else
  BOOL madVR_ConnectDialog(BOOL searchLan, HWND parentWindow);
  BOOL madVR_BlindConnect(BOOL searchLan, DWORD timeOut);
#endif

// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------

#ifdef __cplusplus
  }
#endif

#endif
