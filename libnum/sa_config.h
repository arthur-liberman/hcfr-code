
#ifndef __CONFIG_H__
#define __CONFIG_H__

/* General project wide configuration */

/* Version of Argyll release */
/* Bug fix = 4 bits */
/* minor number = 8 bits */
/* major number = 8 bits */

#define ARGYLL_VERSION 0x01083
#define ARGYLL_VERSION_STR "1.8.3"
#define ARGYLL_BUILD_STR "100"

/* Maximum file path length */
#define MAXNAMEL 1024

/* A simpler #define to remove __APPLE__ from non OS X code */
#if defined(UNIX) && !defined(__APPLE__)
# define UNIX_X11       /* Unix like using X11 */
#else
# undef UNIX_X11
#endif

#if defined(UNIX) && !defined(__APPLE__)
#define USE_UCMM		/* Enable the Unix micro CMM */
#endif

#endif /* __CONFIG_H__ */
