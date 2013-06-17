// ***************************************************************
//  madVRTestPattern.h        version: 1.0.0  ·  date: 2013-06-15
//  -------------------------------------------------------------
//  madVR test pattern / calibration remote controlling
//  -------------------------------------------------------------
//  Copyright (C) 2013 - 2013 www.madshi.net, All Rights Reserved
// ***************************************************************

// 2013-06-15 1.0.0 initial version

// ----------------------------------------------------------------------------

#if !defined(MADVR_H)
#define MADVR_H

#ifdef __cplusplus
  extern "C" {
#endif

#include <windows.h>

#pragma pack(1)

// ============================================================================
// I. THE EASY WAY
// ============================================================================

// ----------------------------------------------------------------------------
// connecting to madVR - three options

// (1) "madVR_ConnectDialog" searches for madVR instances on the local PC and
//     optionally also the LAN, then shows all found instances in a dialog and
//     lets the user choose which one to connect to.
//     Searching the LAN will only work if the user has enabled network access
//     in the madVR tray icon configuration. Also, searching the LAN might
//     popup a Firewall confirmation window.
// "result=true"  means: Connection to the selected madVR instance was opened.
// "result=false" means: The user aborted the connection process.
#ifdef __cplusplus
  BOOL madVR_ConnectDialog(BOOL searchLan = TRUE, HWND parentWindow = NULL);
#else
  BOOL madVR_ConnectDialog(BOOL searchLan       , HWND parentWindow       );
#endif

// (2) "madVR_BlindConnect" searches for madVR instances on the local PC and
//     optionally also the LAN, then simply connects to the first found
//     instance. Local PC instances are preferred over LAN instances.
//     Searching the LAN will only work if the user has enabled network access
//     in the madVR tray icon configuration. Also, searching the LAN might
//     popup a Firewall confirmation window.
// "result=true"  means: A madVR instance was found and a connection opened.
// "result=false" means: No madVR instance found, or connection failed.
#ifdef __cplusplus
  BOOL madVR_BlindConnect(BOOL searchLan = TRUE, DWORD timeOut = 3000);
#else
  BOOL madVR_BlindConnect(BOOL searchLan       , DWORD timeOut       );
#endif

// (3) If you want to connect to a madVR instance running on a LAN PC with a
//     known IP address, "madVR_ConnectToIp" lets you do just that.
//     Connecting to a madVR LAN instance will only work if the user has
//     enabled network access in the madVR tray icon configuration. Also,
//     using "madVR_ConnectToIp" might popup a Firewall confirmation window.
// "result=true"  means: A madVR instance was found and a connection opened.
// "result=false" means: No madVR instance found, or connection failed.
#ifdef __cplusplus
  BOOL madVR_ConnectToIp(LPCSTR ipAddress, DWORD timeOut = 1000);
#else
  BOOL madVR_ConnectToIp(LPCSTR ipAddress, DWORD timeOut       );
#endif

// ----------------------------------------------------------------------------
// remote controlling the connected madVR instance

// "madVR_Disable3dlut" disables 3dlut processing.
// The 3dlut stays disabled until the connection is closed.
// Disable the 3dlut if you want to calibrate/profile the display, or if you
// want to measure the display behaviour prior to calibration.
// Don't disable the 3dlut if you want to measure the final display after full
// calibration.
BOOL madVR_Disable3dlut();

// "madVR_SetDeviceGammaRamp" sets the madVR internal VideoLUTs.
// Parameter is identical to the win32 API "SetDeviceGammaRamp".
BOOL madVR_SetDeviceGammaRamp(LPVOID ramp);

// "madVR_SetOsdText" shows a "text" on the top left of the video image.
BOOL madVR_SetOsdText(LPCWSTR text);

// "madVR_ShowProgressBar" initializes the madVR progress bar.
// It will progress one step with every "madVR_ShowRGB" call (see below).
BOOL madVR_ShowProgressBar(int numberOfRgbMeasurements);

// "madVR_ShowRGB" shows a specific RGB color test pattern.
// Values are gamma corrected with "black = 0.0" and "white = 1.0".
// You can go below 0.0 or above 1.0 for BTB/WTW, if you want. Of course a test
// pattern with BTB/WTW will only work if the connected madVR instance is
// configured to TV level output.
// "madVR_ShowRGB" blocks until the GPU has actually output the test pattern to
// the display. How fast the display will actually show the test pattern will
// depend on the display's input latency, which is outside of madVR's control.
BOOL madVR_ShowRGB(double r, double g, double b);

// ----------------------------------------------------------------------------
// disconnecting from madVR

// "madVR_Disconnect" closes the current connection to madVR.
BOOL madVR_Disconnect();

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

// The following APIs let you automatically locate madVR instances running
// anywhere on either the local PC or remote PCs connected via LAN.
// For every found madVR instance this full information record is returned:

typedef struct _TMadVRInstance
{                           // Example 1:       // Example 2:
  HANDLE    handle;         // 1                // 2
  ULONGLONG instance;       // 0x40001000       // 0x40001000
  LPSTR     ipAddress;      // "192.168.1.1"    // "192.168.1.1"
  LPWSTR    computerName;   // "HTPC"           // "HTPC"
  LPWSTR    userName;       // "Walter"         // "Walter"
  LPWSTR    os;             // "Windows XP"     // "Windows 8"
  ULONG_PTR processId;      // 220              // 248
  LPWSTR    processName;    // "MPC-HC"         // "madVR test pattern generator"
  LPWSTR    exeFile;        // "mplayerc.exe"   // "madTPG.exe"
  LPWSTR    exeVersion;     // 1.3.2066.0       // 1.0.0.0
  LPWSTR    madVRVersion;   // 0.86.4.0         // 0.86.4.0
  LPWSTR    gpuName;        // "Radeon 3850"    // "GeForce 650"
  LPWSTR    monitorName;    // "JVC HD-350"     // "JVC HD-350"
} TMadVRInstance, *PMadVRInstance;

typedef struct _TMadVRInstances
{
  LONG_PTR count;
  TMadVRInstance items[1];
} TMadVRInstances, *PMadVRInstances;

// Normally, a network search returns all running madVR instances in less than
// a second. But under specific circumstances, the search can take several
// seconds. Because of that there are different ways to perform a search:

// (1) synchronous search
// Calling "madVR_Find" (see below) with a timeout means that "madVR_Find" will
// perform a network search and only return when the search is complete, or
// when the timeout was reached.

// (2) asynchronous search I
// You can call "madVR_Find" with a timeout value of "0" to start the search.
// "madVR_Find" will return at once, but starts a search in the background.
// Later, when you see fit, you can call "madVR_Find" another time (with or
// without a timeout value) to pick up the search results.

// (3) asynchronous search II
// Call "madVR_Find_Async" (see below) to start a background network search.
// Whenever a new madVR instance is found (and also when a madVR instance is
// closed), a message will be sent to a window of your choice.
// When that message arrives, you can call "madVR_Find" with a 0 timeout value
// to fetch the updated list of found madVR instances.

// Here are the mentioned APIs:

// "madVR_Find" starts a search for madVR instances running anywhere on the LAN
// and returns information records about all found instances.
// If the search isn't complete within the timeOut interval, "madVR_Find" will
// return and report the instances found until then. The search continues in a
// background thread until it's complete.
// The memory is allocated by madVR, don't allocate nor free it.
// The memory stays valid until the next "madVR_Find" call.
#ifdef __cplusplus
  PMadVRInstances madVR_Find(DWORD timeOut = 3000);
#else
  PMadVRInstances madVR_Find(DWORD timeOut       );
#endif

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
BOOL madVR_Find_Async(HWND window, DWORD msg);

// ----------------------------------------------------------------------------
// connection to a specific madVR instance

// "madVR_Connect" connects you to the specified madVR instance.
// If a previous connection exists, it will be closed automatically.
// The "handle" and "instance" originate from a "madVR_Find(_Async)" search.
BOOL madVR_Connect(HANDLE handle, ULONGLONG instance);

// ----------------------------------------------------------------------------


// ============================================================================
// III. GUI LOCALIZATION
// ============================================================================

// Localize/customize all texts used by madVR_ConnectDialog.
void Localize_ConnectDialog (LPCWSTR title,       // madVR selection...
                             LPCWSTR text,        // Please make sure that madVR is running on the target computer and then select it here:
                             LPCWSTR columns,     // ip address|computer|pid|process|gpu|monitor|os
                             LPCWSTR notListed,   // The madVR instance I'm looking for is not listed
                             LPCWSTR select,      // Select
                             LPCWSTR cancel       // Cancel
                            );

// Localize/customize all texts used by madVR_IpAddressDialog.
// This is a dialog used internally by madVR_ConnectDialog.
void Localize_IpAddressDialog (LPCWSTR title,          // find mad* instance...
                               LPCWSTR text,           // Please enter the IP address of the computer on which mad* is running:
                               LPCWSTR connect,        // Connect
                               LPCWSTR cancel,         // Cancel
                               LPCWSTR warningTitle,   // Warning...
                               LPCWSTR warningText1,   // There doesn't seem to be any madVR instance running on that computer.
                               LPCWSTR warningText2    // The target computer does not react.\n\n
                              );                       // Please check if it's turned on and connected to the LAN.\n
                                                       // You may also want to double check your firewall settings.

// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------

#ifdef __cplusplus
  }
#endif

#endif
