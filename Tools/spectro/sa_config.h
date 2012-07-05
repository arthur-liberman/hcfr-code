
#ifndef __CONFIG_H__
#define __CONFIG_H__

/* General project wide configuration */

/* Version of Argyll release */
/* Bug fix = 4 bits */
/* minor number = 8 bits */
/* major number = 8 bits */

#define ARGYLL_VERSION 0x01040
#define ARGYLL_VERSION_STR "1.4.0"

/* Maximum file path length */
#define MAXNAMEL 1024

#if defined(UNIX) && !defined(__APPLE__)
#define USE_UCMM		/* Enable the Unix micro CMM */
#endif

#ifndef SALONEINSTLIB
#define ENABLE_USB		/* Enable access to USB instruments using libusb */
#endif /* !SALONEINSTLIB */

#endif /* __CONFIG_H__ */
