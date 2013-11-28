// ***************************************************************
//  madVRTestPattern.cpp      version: 1.1.0  ·  date: 2013-11-27
//  -------------------------------------------------------------
//  madVR test pattern / calibration remote controlling
//  -------------------------------------------------------------
//  Copyright (C) 2013 - 2013 www.madshi.net, BSD license
// ***************************************************************

// 2013-11-27 1.1.0 added madVR_GetBlackAndWhiteLevel
// 2013-06-15 1.0.0 initial version

// ----------------------------------------------------------------------------
#include "stdafx.h"
#include "madVRTestPattern.h"

// ----------------------------------------------------------------------------

static HMODULE HcNetDll = NULL;
static BOOL InitDone = false;
static BOOL InitSuccess = false;

static BOOL (WINAPI *ConnectDialog)(BOOL searchLan, HWND parentWindow) = NULL;
static BOOL (WINAPI *BlindConnect)(BOOL searchLan, DWORD timeOut) = NULL;
static BOOL (WINAPI *ConnectToIp)(LPCSTR ipAddress, DWORD timeOut) = NULL;
static BOOL (WINAPI *GetBlackAndWhiteLevel)(int *blackLevel, int *whiteLevel) = NULL;
static BOOL (WINAPI *Disable3dlut)() = NULL;
static BOOL (WINAPI *GetDeviceGammaRamp_)(LPVOID ramp) = NULL;
static BOOL (WINAPI *SetDeviceGammaRamp_)(LPVOID ramp) = NULL;
static BOOL (WINAPI *SetOsdText)(LPCWSTR text) = NULL;
static BOOL (WINAPI *SetBackground)(int patternAreaInPercent, COLORREF backgroundColor) = NULL;
static BOOL (WINAPI *ShowProgressBar)(int numberOfRgbMeasurements) = NULL;
static BOOL (WINAPI *SetProgressBarPos)(int currentPos, int maxPos) = NULL;
static BOOL (WINAPI *ShowRGB)(double r, double g, double b) = NULL;
static BOOL (WINAPI *Disconnect)() = NULL;
static BOOL (WINAPI *IsAvailable)() = NULL;
static BOOL (WINAPI *Find_Async)(HWND window, DWORD msg) = NULL;
static BOOL (WINAPI *Connect)(HANDLE handle, ULONGLONG instance) = NULL;
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
    *(FARPROC*)&ConnectDialog         = GetProcAddress(HcNetDll, "madVR_ConnectDialog"        );
    *(FARPROC*)&BlindConnect          = GetProcAddress(HcNetDll, "madVR_BlindConnect"         );
    *(FARPROC*)&ConnectToIp           = GetProcAddress(HcNetDll, "madVR_ConnectToIp"          );
    *(FARPROC*)&GetBlackAndWhiteLevel = GetProcAddress(HcNetDll, "madVR_GetBlackAndWhiteLevel");
    *(FARPROC*)&Disable3dlut          = GetProcAddress(HcNetDll, "madVR_Disable3dlut"         );
    *(FARPROC*)&GetDeviceGammaRamp_   = GetProcAddress(HcNetDll, "madVR_GetDeviceGammaRamp"   );
    *(FARPROC*)&SetDeviceGammaRamp_   = GetProcAddress(HcNetDll, "madVR_SetDeviceGammaRamp"   );
    *(FARPROC*)&SetOsdText            = GetProcAddress(HcNetDll, "madVR_SetOsdText"           );
    *(FARPROC*)&SetBackground         = GetProcAddress(HcNetDll, "madVR_SetBackground"        );
    *(FARPROC*)&ShowProgressBar       = GetProcAddress(HcNetDll, "madVR_ShowProgressBar"      );
    *(FARPROC*)&SetProgressBarPos     = GetProcAddress(HcNetDll, "madVR_SetProgressBarPos"    );
    *(FARPROC*)&ShowRGB               = GetProcAddress(HcNetDll, "madVR_ShowRGB"              );
    *(FARPROC*)&Disconnect            = GetProcAddress(HcNetDll, "madVR_Disconnect"           );
    *(FARPROC*)&Find_Async            = GetProcAddress(HcNetDll, "madVR_Find_Async"           );
    *(FARPROC*)&Connect               = GetProcAddress(HcNetDll, "madVR_Connect"              );
    *(FARPROC*)&LocConnectDialog      = GetProcAddress(HcNetDll, "Localize_ConnectDialog"     );
    *(FARPROC*)&LocIpAddressDialog    = GetProcAddress(HcNetDll, "Localize_IpAddressDialog"   );
    InitSuccess = (ConnectDialog      ) &&
                  (BlindConnect       ) &&
                  (ConnectToIp        ) &&
                  (Disable3dlut       ) &&
                  (SetDeviceGammaRamp_) &&
                  (SetOsdText         ) &&
                  (SetBackground      ) &&
                  (ShowProgressBar    ) &&
                  (ShowRGB            ) &&
                  (Disconnect         ) &&
                  (Find_Async         ) &&
                  (Connect            ) &&
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

BOOL madVR_GetBlackAndWhiteLevel(int* blackLevel, int* whiteLevel)
{
  return Init() && (GetBlackAndWhiteLevel) && GetBlackAndWhiteLevel(blackLevel, whiteLevel);
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

BOOL madVR_SetBackground(int patternAreaInPercent, COLORREF backgroundColor)
{
  return Init() && SetBackground(patternAreaInPercent, backgroundColor);
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

BOOL madVR_Disconnect()
{
  return Init() && Disconnect();
}

BOOL madVR_Find_Async(HWND window, DWORD msg)
{
  return Init() && Find_Async(window, msg);
}

BOOL madVR_Connect(HANDLE handle, ULONGLONG instance)
{
  return Init() && Connect(handle, instance);
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
