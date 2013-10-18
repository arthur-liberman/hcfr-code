See instlib.txt in the ArgyllCMS source on how to
create the instlib.zip archive.

To build it:

If you are on Linux or OS X, you first need to
build libusb 1.0A, ie::

	cd libusb1
	sh autogen.sh
	make
	cp libusb/libusb-1.0A.a .
	cd ..

(The libraries are pre-built for MSWin)

To build the standalone instrument lib, you
need to edit the Makefile to #include the appropriate
Makefile.XXX for your operating system, and then
run your "make".

