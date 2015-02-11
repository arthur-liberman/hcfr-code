// ***************************************************************
//  madTPG.cpp                version: 1.2.0  ·  date: 2014-12-01
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

#include "stdafx.h"
#include "madTPG.h"

// ----------------------------------------------------------------------------

static HMODULE HcNetDll = NULL;
static BOOL InitDone = false;
static BOOL InitSuccess = false;

static BOOL (WINAPI *ConnectDialog)(BOOL searchLan, HWND parentWindow) = NULL;
static BOOL (WINAPI *BlindConnect)(BOOL searchLan, DWORD timeOut) = NULL;
static BOOL (WINAPI *ConnectToIp)(LPCSTR ipAddress, DWORD timeOut) = NULL;
static BOOL (WINAPI *ConnectEx)(int method1, DWORD timeOut1, int method2, DWORD timeOut2, int method3, DWORD timeOut3, int method4, DWORD timeOut4, HWND parentWindow) = NULL;
static BOOL (WINAPI *IsStayOnTopButtonPressed)() = NULL;
static BOOL (WINAPI *IsUseFullscreenButtonPressed)() = NULL;
static BOOL (WINAPI *IsDisableOsdButtonPressed)() = NULL;
static BOOL (WINAPI *SetStayOnTopButton)(BOOL pressed) = NULL;
static BOOL (WINAPI *SetUseFullscreenButton)(BOOL pressed) = NULL;
static BOOL (WINAPI *SetDisableOsdButton)(BOOL pressed) = NULL;
static BOOL (WINAPI *GetBlackAndWhiteLevel)(int *blackLevel, int *whiteLevel) = NULL;
static BOOL (WINAPI *Disable3dlut)() = NULL;
static BOOL (WINAPI *GetDeviceGammaRamp_)(LPVOID ramp) = NULL;
static BOOL (WINAPI *SetDeviceGammaRamp_)(LPVOID ramp) = NULL;
static BOOL (WINAPI *SetOsdText)(LPCWSTR text) = NULL;
static BOOL (WINAPI *GetPatternConfig)(int *patternAreaInPercent, int *backgroundLevelInPercent, int *backgroundMode, int *blackBorderWidth) = NULL;
static BOOL (WINAPI *SetPatternConfig)(int  patternAreaInPercent, int  backgroundLevelInPercent, int  backgroundMode, int  blackBorderWidth) = NULL;
static BOOL (WINAPI *ShowProgressBar)(int numberOfRgbMeasurements) = NULL;
static BOOL (WINAPI *SetProgressBarPos)(int currentPos, int maxPos) = NULL;
static BOOL (WINAPI *ShowRGB)(double r, double g, double b) = NULL;
static BOOL (WINAPI *ShowRGBEx)(double r, double g, double b, double bgR, double bgG, double bgB) = NULL;
static BOOL (WINAPI *Convert3dlutFile)(LPWSTR eeColor3dlutFile, LPWSTR madVR3dlutFile, int gamut) = NULL;
static BOOL (WINAPI *Create3dlutFileFromArray65)(TEeColor3dlut *lutData, LPWSTR madVR3dlutFile, int gamut) = NULL;
static BOOL (WINAPI *Create3dlutFileFromArray256)(TMadVR3dlut *lutData, LPWSTR madVR3dlutFile, int gamut) = NULL;
static BOOL (WINAPI *Load3dlutFile)(LPWSTR lutFile, BOOL saveToSettings, int gamut) = NULL;
static BOOL (WINAPI *Load3dlutFromArray65)(TEeColor3dlut *lutData, BOOL saveToSettings, int gamut) = NULL;
static BOOL (WINAPI *Load3dlutFromArray256)(TMadVR3dlut *lutData, BOOL saveToSettings, int gamut) = NULL;
static BOOL (WINAPI *Disconnect)() = NULL;
static BOOL (WINAPI *Quit)() = NULL;
static BOOL (WINAPI *IsAvailable)() = NULL;
static BOOL (WINAPI *Find_Async)(HWND window, DWORD msg) = NULL;
static BOOL (WINAPI *ConnectToInstance)(HANDLE handle, ULONGLONG instance) = NULL;
static void (WINAPI *LocConnectDialog)(LPCWSTR title, LPCWSTR text, LPCWSTR columns, LPCWSTR notListed, LPCWSTR select, LPCWSTR cancel) = NULL;
static void (WINAPI *LocIpAddressDialog)(LPCWSTR title, LPCWSTR text, LPCWSTR connect, LPCWSTR cancel, LPCWSTR warningTitle, LPCWSTR warningText1, LPCWSTR warningText2) = NULL;

BOOL Init()
{
  HKEY hk1;
  DWORD size;
  LPWSTR us1;
  int i1;

  if (!InitDone)
  {
    #ifdef _WIN64
      HcNetDll = LoadLibraryW(L"madHcNet64.dll");
    #else
      HcNetDll = LoadLibraryW(L"madHcNet32.dll");
    #endif
    if ((!HcNetDll) && (RegOpenKeyExW(HKEY_CLASSES_ROOT, L"CLSID\\{E1A8B82A-32CE-4B0D-BE0D-AA68C772E423}\\InprocServer32", 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &hk1) == ERROR_SUCCESS))
    {
      size = MAX_PATH * 2 + 2;
      us1 = (LPWSTR) LocalAlloc(LPTR, size + 20);
      i1 = RegQueryValueExW(hk1, NULL, NULL, NULL, (LPBYTE) us1, &size);
      if (i1 == ERROR_MORE_DATA)
      {
        LocalFree(us1);
        us1 = (LPWSTR) LocalAlloc(LPTR, size + 20);
        i1 = RegQueryValueExW(hk1, NULL, NULL, NULL, (LPBYTE) us1, &size);
      }
      if (i1 == ERROR_SUCCESS)
      {
        for (i1 = lstrlenW(us1) - 2; i1 > 0; i1--)
          if (us1[i1] == L'\\')
          {
            us1[i1 + 1] = 0;
            break;
          }
        #ifdef _WIN64
          wcscat_s(us1, size + 10, L"madHcNet64.dll");
        #else
          wcscat_s(us1, size + 10, L"madHcNet32.dll");
        #endif
        HcNetDll = LoadLibraryW(us1);
      }
      LocalFree(us1);
      RegCloseKey(hk1);
    }
    *(FARPROC*)&ConnectDialog                = GetProcAddress(HcNetDll, "madVR_ConnectDialog"               );
    *(FARPROC*)&BlindConnect                 = GetProcAddress(HcNetDll, "madVR_BlindConnect"                );
    *(FARPROC*)&ConnectToIp                  = GetProcAddress(HcNetDll, "madVR_ConnectToIp"                 );
    *(FARPROC*)&ConnectEx                    = GetProcAddress(HcNetDll, "madVR_ConnectEx"                   );
    *(FARPROC*)&IsStayOnTopButtonPressed     = GetProcAddress(HcNetDll, "madVR_IsStayOnTopButtonPressed"    );
    *(FARPROC*)&IsUseFullscreenButtonPressed = GetProcAddress(HcNetDll, "madVR_IsUseFullscreenButtonPressed");
    *(FARPROC*)&IsDisableOsdButtonPressed    = GetProcAddress(HcNetDll, "madVR_IsDisableOsdButtonPressed"   );
    *(FARPROC*)&SetStayOnTopButton           = GetProcAddress(HcNetDll, "madVR_SetStayOnTopButton"          );
    *(FARPROC*)&SetUseFullscreenButton       = GetProcAddress(HcNetDll, "madVR_SetUseFullscreenButton"      );
    *(FARPROC*)&SetDisableOsdButton          = GetProcAddress(HcNetDll, "madVR_SetDisableOsdButton"         );
    *(FARPROC*)&GetBlackAndWhiteLevel        = GetProcAddress(HcNetDll, "madVR_GetBlackAndWhiteLevel"       );
    *(FARPROC*)&Disable3dlut                 = GetProcAddress(HcNetDll, "madVR_Disable3dlut"                );
    *(FARPROC*)&GetDeviceGammaRamp_          = GetProcAddress(HcNetDll, "madVR_GetDeviceGammaRamp"          );
    *(FARPROC*)&SetDeviceGammaRamp_          = GetProcAddress(HcNetDll, "madVR_SetDeviceGammaRamp"          );
    *(FARPROC*)&SetOsdText                   = GetProcAddress(HcNetDll, "madVR_SetOsdText"                  );
    *(FARPROC*)&GetPatternConfig             = GetProcAddress(HcNetDll, "madVR_GetPatternConfig"            );
    *(FARPROC*)&SetPatternConfig             = GetProcAddress(HcNetDll, "madVR_SetPatternConfig"            );
    *(FARPROC*)&ShowProgressBar              = GetProcAddress(HcNetDll, "madVR_ShowProgressBar"             );
    *(FARPROC*)&SetProgressBarPos            = GetProcAddress(HcNetDll, "madVR_SetProgressBarPos"           );
    *(FARPROC*)&ShowRGB                      = GetProcAddress(HcNetDll, "madVR_ShowRGB"                     );
    *(FARPROC*)&ShowRGBEx                    = GetProcAddress(HcNetDll, "madVR_ShowRGBEx"                   );
    *(FARPROC*)&Convert3dlutFile             = GetProcAddress(HcNetDll, "madVR_Convert3dlutFile"            );
    *(FARPROC*)&Create3dlutFileFromArray65   = GetProcAddress(HcNetDll, "madVR_Create3dlutFileFromArray65"  );
    *(FARPROC*)&Create3dlutFileFromArray256  = GetProcAddress(HcNetDll, "madVR_Create3dlutFileFromArray256" );
    *(FARPROC*)&Load3dlutFile                = GetProcAddress(HcNetDll, "madVR_Load3dlutFile"               );
    *(FARPROC*)&Load3dlutFromArray65         = GetProcAddress(HcNetDll, "madVR_Load3dlutFromArray65"        );
    *(FARPROC*)&Load3dlutFromArray256        = GetProcAddress(HcNetDll, "madVR_Load3dlutFromArray256"       );
    *(FARPROC*)&Disconnect                   = GetProcAddress(HcNetDll, "madVR_Disconnect"                  );
    *(FARPROC*)&Quit                         = GetProcAddress(HcNetDll, "madVR_Quit"                        );
    *(FARPROC*)&Find_Async                   = GetProcAddress(HcNetDll, "madVR_Find_Async"                  );
    *(FARPROC*)&ConnectToInstance            = GetProcAddress(HcNetDll, "madVR_ConnectToInstance"           );
    *(FARPROC*)&LocConnectDialog             = GetProcAddress(HcNetDll, "Localize_ConnectDialog"            );
    *(FARPROC*)&LocIpAddressDialog           = GetProcAddress(HcNetDll, "Localize_IpAddressDialog"          );
    InitSuccess = (ConnectDialog      ) &&
                  (BlindConnect       ) &&
                  (ConnectToIp        ) &&
                  (Disable3dlut       ) &&
                  (SetDeviceGammaRamp_) &&
                  (SetOsdText         ) &&
                  (ShowProgressBar    ) &&
                  (ShowRGB            ) &&
                  (Disconnect         ) &&
                  (Find_Async         ) &&
                  (LocConnectDialog   ) &&
                  (LocIpAddressDialog );
    InitDone = true;
  }
  return InitSuccess;
}

// ----------------------------------------------------------------------------

BOOL madVR_ConnectDialog(BOOL searchLan, HWND parentWindow)
{
  return Init() && ConnectDialog(searchLan, parentWindow);
}

BOOL madVR_BlindConnect(BOOL searchLan, DWORD timeOut)
{
  return Init() && BlindConnect(searchLan, timeOut);
}

BOOL madVR_ConnectToIp(LPCSTR ipAddress, DWORD timeOut)
{
  return Init() && ConnectToIp(ipAddress, timeOut);
}

BOOL madVR_Connect(int method1, DWORD timeOut1, int method2, DWORD timeOut2, int method3, DWORD timeOut3, int method4, DWORD timeOut4, HWND parentWindow)
{
  return Init() && (ConnectEx) && ConnectEx(method1, timeOut1, method2, timeOut2, method3, timeOut3, method4, timeOut4, parentWindow);
}

BOOL madVR_IsStayOnTopButtonPressed()
{
  return Init() && (IsStayOnTopButtonPressed) && IsStayOnTopButtonPressed();
}

BOOL madVR_IsUseFullscreenButtonPressed()
{
  return Init() && (IsUseFullscreenButtonPressed) && IsUseFullscreenButtonPressed();
}

BOOL madVR_IsDisableOsdButtonPressed()
{
  return Init() && (IsDisableOsdButtonPressed) && IsDisableOsdButtonPressed();
}

BOOL madVR_SetStayOnTopButton(BOOL pressed)
{
  return Init() && (SetStayOnTopButton) && SetStayOnTopButton(pressed);
}

BOOL madVR_SetUseFullscreenButton(BOOL pressed)
{
  return Init() && (SetUseFullscreenButton) && SetUseFullscreenButton(pressed);
}

BOOL madVR_SetDisableOsdButton(BOOL pressed)
{
  return Init() && (SetDisableOsdButton) && SetDisableOsdButton(pressed);
}

BOOL madVR_GetBlackAndWhiteLevel(int* blackLevel, int* whiteLevel)
{
  BOOL result = Init() && (GetBlackAndWhiteLevel) && GetBlackAndWhiteLevel(blackLevel, whiteLevel);
  if ((!result) || (*blackLevel >= *whiteLevel))
  {
    *blackLevel = 0;
    *whiteLevel = 255;
  }
  return result;
}

BOOL madVR_Disable3dlut()
{
  return Init() && Disable3dlut();
}

BOOL madVR_GetDeviceGammaRamp(LPVOID ramp)
{
  return Init() && (GetDeviceGammaRamp_) && GetDeviceGammaRamp_(ramp);
}

BOOL madVR_SetDeviceGammaRamp(LPVOID ramp)
{
  return Init() && SetDeviceGammaRamp_(ramp);
}

BOOL madVR_SetOsdText(LPCWSTR text)
{
  return Init() && SetOsdText(text);
}

BOOL madVR_GetPatternConfig(int *patternAreaInPercent, int *backgroundLevelInPercent, int *backgroundMode, int *blackBorderWidth)
{
  return Init() && (GetPatternConfig) && GetPatternConfig(patternAreaInPercent, backgroundLevelInPercent, backgroundMode, blackBorderWidth);
}

BOOL madVR_SetPatternConfig(int  patternAreaInPercent, int  backgroundLevelInPercent, int  backgroundMode, int  blackBorderWidth)
{
  return Init() && (SetPatternConfig) && SetPatternConfig(patternAreaInPercent, backgroundLevelInPercent, backgroundMode, blackBorderWidth);
}

BOOL madVR_ShowProgressBar(int numberOfRgbMeasurements)
{
  return Init() && ShowProgressBar(numberOfRgbMeasurements);
}

BOOL madVR_SetProgressBarPos(int currentPos, int maxPos)
{
  return Init() && (SetProgressBarPos) && SetProgressBarPos(currentPos, maxPos);
}

BOOL madVR_ShowRGB(double r, double g, double b)
{
  return Init() && ShowRGB(r, g, b);
}

BOOL madVR_ShowRGBEx(double r, double g, double b, double bgR, double bgG, double bgB)
{
  return Init() &&
            ( ((ShowRGBEx != NULL) && ShowRGBEx(r, g, b, bgR, bgG, bgB)) ||
              ((ShowRGBEx == NULL) && ShowRGB  (r, g, b)) );
}

BOOL madVR_Convert3dlutFile(LPWSTR eeColor3dlutFile, LPWSTR madVR3dlutFile, int gamut)
{
  return Init() && (Convert3dlutFile) && Convert3dlutFile(eeColor3dlutFile, madVR3dlutFile, gamut);
}

BOOL madVR_Create3dlutFileFromArray65(TEeColor3dlut *lutData, LPWSTR madVR3dlutFile, int gamut)
{
  return Init() && (Create3dlutFileFromArray65) && Create3dlutFileFromArray65(lutData, madVR3dlutFile, gamut);
}

BOOL madVR_Create3dlutFileFromArray256(TMadVR3dlut *lutData, LPWSTR madVR3dlutFile, int gamut)
{
  return Init() && (Create3dlutFileFromArray256) && Create3dlutFileFromArray256(lutData, madVR3dlutFile, gamut);
}

BOOL madVR_Load3dlutFile(LPWSTR lutFile, BOOL saveToSettings, int gamut)
{
  return Init() && (Load3dlutFile) && Load3dlutFile(lutFile, saveToSettings, gamut);
}

BOOL madVR_Load3dlutFromArray65(TEeColor3dlut *lutData, BOOL saveToSettings, int gamut)
{
  return Init() && (Load3dlutFromArray65) && Load3dlutFromArray65(lutData, saveToSettings, gamut);
}

BOOL madVR_Load3dlutFromArray256(TMadVR3dlut *lutData, BOOL saveToSettings, int gamut)
{
  return Init() && (Load3dlutFromArray256) && Load3dlutFromArray256(lutData, saveToSettings, gamut);
}

BOOL madVR_Disconnect()
{
  return Init() && Disconnect();
}

BOOL madVR_Quit()
{
  BOOL result = Init() && (Quit) && Quit();
  if (!result)
    madVR_Disconnect();
  return result;
}

BOOL madVR_Find_Async(HWND window, DWORD msg)
{
  return Init() && Find_Async(window, msg);
}

BOOL madVR_ConnectToInstance(HANDLE handle, ULONGLONG instance)
{
  return Init() && ConnectToInstance(handle, instance);
}

BOOL madVR_IsAvailable()
{
  return Init();
}

void Localize_ConnectDialog(LPCWSTR title, LPCWSTR text, LPCWSTR columns, LPCWSTR notListed, LPCWSTR select, LPCWSTR cancel)
{
  if (Init())
    LocConnectDialog(title, text, columns, notListed, select, cancel);
}

void Localize_IpAddressDialog(LPCWSTR title, LPCWSTR text, LPCWSTR connect, LPCWSTR cancel, LPCWSTR warningTitle, LPCWSTR warningText1, LPCWSTR warningText2)
{
  if (Init())
    LocIpAddressDialog(title, text, connect, cancel, warningTitle, warningText1, warningText2);
}

// ----------------------------------------------------------------------------
