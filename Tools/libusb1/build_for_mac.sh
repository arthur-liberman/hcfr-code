#!/bin/sh
SDK=/Developer-old/SDKs/MacOSX10.5.sdk
FLAGS="-arch i386 -isysroot $SDK -mmacosx-version-min=10.5"

cp libusb-X.X.pc.in libusb-1.0A.pc.in
sh ./autogen.sh
automake
CFLAGS=$FLAGS CXXFLAGS=$FLAGS LDFLAGS=$FLAGS ./configure

make
