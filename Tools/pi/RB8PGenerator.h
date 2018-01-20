#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <math.h>
#include <tchar.h>

#ifndef RB8PGenerator_DLL_H
#define RB8PGenerator_DLL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BUILDING_RB8PGenerator_DLL
#define RB8PGenerator_DLL __declspec(dllexport)
#else
#define RB8PGenerator_DLL __declspec(dllimport)
#endif

RB8PGenerator_DLL char * __stdcall RB8PG_discovery();
SOCKET RB8PGenerator_DLL __stdcall RB8PG_connect(const char *server_addr);
int    RB8PGenerator_DLL __stdcall RB8PG_send(SOCKET sock,const char *message);
int    RB8PGenerator_DLL __stdcall RB8PG_close(SOCKET sock);

#ifdef DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

#ifdef __cplusplus
}
#endif

#endif