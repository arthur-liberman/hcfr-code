
Snapshot of libusb1 development,
that includes Win2K & libusb0.sys back end MSWindows support. 

Date: 30 July 2010

Changes:
	Add resetep() support, since Spyder2 won't work on Linux
	without it.

Date: 17 April 2010

Changes:
	Patches darwin_usb.[ch] to eliminate GetConfiguration() calls
	since they crash one of my devices, and (according to libusbi.h)
	are undesirable anyway because they do I/O.
	An approach similar to windows_usb is used, caching the
	active configuration value in the private structure.


Date: 20 March 2010

Changes:

	Fixed to work on Win2K

	Fixed DDK compilation for older DDK's

	Added support for libusb0.sys, including working with
	64K packet limit, and libusb0.sys driver source.

	Fixed concurrency bugs.

	Created universal .inf template

