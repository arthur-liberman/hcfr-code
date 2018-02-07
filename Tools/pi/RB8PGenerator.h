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

char * __stdcall RB8PGenerator_DLL RB8PG_discovery();
SOCKET __stdcall RB8PGenerator_DLL RB8PG_connect(const char *server_addr);
int    __stdcall RB8PGenerator_DLL RB8PG_send(SOCKET sock,const char *message);
char * __stdcall RB8PGenerator_DLL RB8PG_get(SOCKET sock,const char *message);
int    __stdcall RB8PGenerator_DLL RB8PG_close(SOCKET sock);

#ifdef DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

#ifdef __cplusplus
}
#endif

#endif