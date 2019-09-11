
#ifndef __CONFIG_H__
#define __CONFIG_H__

/* General project wide configuration */

/*
 * Author: Graeme W. Gill
 *
 * Copyright 2006 - 2018, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#ifdef __cplusplus
	extern "C" {
#endif

/* Version of Argyll release */
/* Bug fix = 4 bits */
/* minor number = 8 bits */
/* major number = 8 bits */

#ifndef USE_NG_VERSION 
# define ARGYLL_VERSION 0x02011
# define ARGYLL_VERSION_STR "2.1.1"
#else
# define ARGYLL_VERSION NG_VERSION
# define ARGYLL_VERSION_STR "NG_VERSION_STR"
#endif

#if defined(NT)
# if defined(_WIN64)
#  define ARGYLL_BUILD_STR "MSWin 64 bit" 
# else
#  define ARGYLL_BUILD_STR "MSWin 32 bit" 
# endif
#endif
#if defined(UNIX)
# if defined(__APPLE__)
#  if defined(__LP64__) || defined(__ILP64__) || defined(__LLP64__)
#   define ARGYLL_BUILD_STR "OS X 64 bit" 
#  else
#   define ARGYLL_BUILD_STR "OS X 32 bit" 
#  endif
# else
#  if defined(__LP64__) || defined(__ILP64__) || defined(__LLP64__)
#   define ARGYLL_BUILD_STR "Linux 64 bit" 
#  else
#   define ARGYLL_BUILD_STR "Linux 32 bit" 
#  endif
# endif
#endif

/* Maximum number of graphs supported by plot */
#define MXGPHS 12

/* Maximum file path length */
#define MAXNAMEL 1024

/* Maximum number of entries to setup for calibration */
#define MAX_CAL_ENT 16384

/* A simpler #define to remove __APPLE__ from non OS X code */
#if defined(UNIX)
# if defined(__APPLE__)
#  define UNIX_APPLE
#  undef  UNIX_X11
# else
#  define UNIX_X11
#  undef  UNIX_APPLE
# endif
#endif

#ifdef UNIX_X11
# define USE_UCMM		/* Enable the Unix micro CMM */
#endif

#ifdef __cplusplus
	}
#endif

#endif /* __CONFIG_H__ */
