#!/bin/sh

# Package up the source and other files for the standalone GPLv2 Instrument library
# libusb 1.0 is needed as well though.

# Copyright 2007 - 2011 Graeme W. Gill
# This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
# see the License2.txt file for licencing details.

echo "Making standalone GPLv2 instrument archive instlib.zip "

FILES="License2.txt Makefile.OSX  Makefile.UNIX  Makefile.WNT pollem.h pollem.c conv.h conv.c hidio.h hidio.c icoms.h inst.h inst.c insttypes.c insttypes.h insttypeinst.h dtp20.c dtp20.h dtp22.c dtp22.h dtp41.c dtp41.h dtp51.c dtp51.h dtp92.c dtp92.h ss.h ss.c ss_imp.h ss_imp.c i1disp.c i1disp.h i1d3.h i1d3.c i1pro.h i1pro.c i1pro_imp.h i1pro_imp.c munki.h munki.c munki_imp.h munki_imp.c hcfr.c hcfr.h huey.c huey.h spyd2.c spyd2.h spyd2setup.h spyd2PLD.h spyd2en.c i1d3ccss.c vinflate.c inflate.c LzmaDec.c  LzmaDec.h LzmaTypes.h ntio.c unixio.c usbio.h usbio.c xdg_bds.c xdg_bds.h ../xicc/xspect.h ../xicc/xspect.c ../xicc/ccss.h ../xicc/ccss.c ../rspl/rspl1.h ../rspl/rspl1.c ../h/sort.h ../numlib/numsup.h ../numlib/numsup.c ../cgats/pars.h ../cgats/pars.c ../cgats/parsstd.c ../cgats/cgats.h ../cgats/cgats.c ../cgats/cgatsstd.c  spotread.c"

rm -f instlib.zip
rm -rf _zipdir
rm -f _ziplist
mkdir _zipdir
mkdir _zipdir/instlib


# Archive the Argyll files needed
for j in $FILES
do
	if [ ! -e ${j} ] ; then
		echo "!!! Can't find file " ${j}
	else 
		cp ${j} _zipdir/instlib/${j##*/}
		echo instlib/${j##*/} >> _ziplist
	fi
done

# Plus renamed files
cp IntsLib_Readme.txt _zipdir/instlib/Readme.txt
echo instlib/Readme.txt >> _ziplist
cp Makefile.SA _zipdir/instlib/Makefile
echo instlib/Makefile >> _ziplist
cp ../h/aconfig.h _zipdir/instlib/sa_config.h
echo instlib/sa_config.h >> _ziplist

# Create libusb 1 source and binary archive

if [ X$OS = "XWindows_NT" ] ; then
    echo "We're on MSWindows!"
	WINFILES="libusb0.sys libusb0_x64.sys ptlibusb0.sys ptlibusb0_x64.sys libusb-1.0A.lib libusb-1.0A_x64.lib libusb-1.0A.dll libusb-1.0A_x64.dll libusb/libusb-1.0A.rc"
else 
    echo "We're on a UNIX like system!"
fi

cp ../libusb1/libusb-X.X.pc.in ../libusb1/libusb-1.0A.pc.in
cp ../libusb1/libusb/libusb-X.X.rc ../libusb1/libusb/libusb-1.0A.rc

for j in `cat ../libusb1/afiles | grep -v \\\.inf | grep -v \\\.cat` ColorMunki.inf ColorMunki.cat libusb-1.0A.pc.in ${WINFILES}
do
	#echo "File ${j}"

	# Create any needed temporary directories

	tt=libusb1/${j}
	path=${tt%/*}		# extract path without filename
		
	#echo "path ${path}"

	if [ ! -e _zipdir/instlib/${path} ] ; then                     # if not been created
		echo "Creating directory _zipdir/instlib/${path}"
		mkdir -p _zipdir/instlib/${path}
	fi

	tt=../${tt}

	if [ ! -e ${tt} ] ; then
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!! Can't find file ${tt} !!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
		NOTFOUND="$NOTFOUND ${tt}"
	else 
		cp ${tt} _zipdir/instlib/libusb1/${j}
		echo instlib/libusb1/${j} >> _ziplist
	fi
done
rm ../libusb1/libusb/libusb-1.0A.rc
rm ../libusb1/libusb-1.0A.pc.in

# Don't ask.....
mkdir _zipdir/instlib/libusb1/m4
echo instlib/libusb1/m4 >> _ziplist

cd _zipdir
zip -9 -m ../instlib.zip `cat ../_ziplist`
cd ..
rm -rf _zipdir
rm -f _ziplist

#rm ColorMunki.inf ColorMunki.cat ColorMunki_x64.cat

if [ "X$NOTFOUND" != "X" ] ; then
	echo "!!!!!! Didn't find $NOTFOUND !!!!!!"
fi
