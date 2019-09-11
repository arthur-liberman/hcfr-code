
# Boilerplate Makefile for compiling standalone instrumement driver

# Copyright 2000 - 2007 Graeme W. Gill
# This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
# see the License2.txt file for licencing details.

# "include" the right environment for your system,
# by uncommenting the appropriate line:

# Microsoft VC++, WinNT setup
include Makefile.WNT

# Generic UNIX setup
#include Makefile.UNIX

# Apple OS X
#include Makefile.OSX

###############################


CCDEFINES = $(DEFFLAG)SALONEINSTLIB $(DEFFLAG)ENABLE_SERIAL $(DEFFLAG)ENABLE_FAST_SERIAL $(DEFFLAG)ENABLE_USB

#Set optimisation on
CCFLAGS = $(CCFLAGSDEF) $(CCOPTFLAG) $(CCDEFINES) $(BCONFIG)

# Set debugging on
#CCFLAGS = $(CCFLAGSDEF) $(CCDEBUGFLAG) $(CCDEFINES) $(BCONFIG)
# Debugging and debugging #define
#CCFLAGS = $(CCFLAGSDEF) $(CCDEBUGFLAG) $(CCDEFINES) $(DEFFLAG)DEBUG

LINKFLAGS = $(LINKFLAGSDEF) $(LINKDEBUGFLAG)

# Where headers come from
STDHDRS = $(INCFLAG)$(STDHDRSDEF)
WIN_STDHDRS = $(INCFLAG)usb$(SLASH)driver

all:: libinst$(SUFLIB) libinstappsup$(SUFLIB) spotread$(SUFEXE) oeminst$(SUFEXE) 

INSTHEADERS = dtp20.h dtp22.h dtp41.h dtp51.h dtp92.h ss.h ss_imp.h i1disp.h i1d3.h i1pro.h i1pro_imp.h munki.h munki_imp.h hcfr.h huey.h colorhug.h spyd2.h specbos.h kleink10.h
INSOBJS = dtp20$(SUFOBJ) dtp22$(SUFOBJ) dtp41$(SUFOBJ) dtp51$(SUFOBJ) dtp92$(SUFOBJ) ss$(SUFOBJ) ss_imp$(SUFOBJ) i1disp$(SUFOBJ) i1d3$(SUFOBJ) i1pro$(SUFOBJ) i1pro_imp$(SUFOBJ) munki$(SUFOBJ) munki_imp$(SUFOBJ) hcfr$(SUFOBJ) huey$(SUFOBJ) colorhug$(SUFOBJ) spyd2$(SUFOBJ) specbos$(SUFOBJ) kleink10$(SUFOBJ) ex1$(SUFOBJ) smcube$(SUFOBJ)

HEADERS = pollem.h conv.h sa_conv.h aglob.h hidio.h icoms.h inst.c inst.h insttypeinst.h insttypes.h disptechs.h rspec.h xrga.h $(INSTHEADERS) usbio.h xspect.h rspl1.h sort.h xdg_bds.h ccss.h ccmx.h pars.h cgats.h instappsup.h usb$(SLASH)driver$(SLASH)driver_api.h

# libinst objects
OBJS = conv$(SUFOBJ) sa_conv$(SUFOBJ) aglob$(SUFOBJ) inst$(SUFOBJ) numsup$(SUFOBJ) rspl1$(SUFOBJ) icoms$(SUFOBJ) usbio$(SUFOBJ) hidio$(SUFOBJ) insttypes$(SUFOBJ) disptechs$(SUFOBJ) rspec$(SUFOBJ) xrga$(SUFOBJ) pollem$(SUFOBJ) xspect$(SUFOBJ) xdg_bds$(SUFOBJ) ccss$(SUFOBJ) ccmx$(SUFOBJ) pars$(SUFOBJ) cgats$(SUFOBJ) $(INSOBJS)


# instrument library

conv$(SUFOBJ): conv.c $(HEADERS)
	$(CC) conv.c

sa_conv$(SUFOBJ): sa_conv.c $(HEADERS)
	$(CC) sa_conv.c

aglob$(SUFOBJ): aglob.c $(HEADERS)
	$(CC) aglob.c

inst$(SUFOBJ): inst.c $(HEADERS)
	$(CC) inst.c

numsup$(SUFOBJ): numsup.c $(HEADERS)
	$(CC) numsup.c

rspl1$(SUFOBJ): rspl1.c $(HEADERS)
	$(CC) rspl1.c

icoms$(SUFOBJ): icoms.c $(HEADERS)
	$(CC) icoms.c

usbio$(SUFOBJ): usbio.c $(HEADERS)
	$(CC) usbio.c

hidio$(SUFOBJ): hidio.c $(HEADERS)
	$(CC) hidio.c

insttypes$(SUFOBJ): insttypes.c $(HEADERS)
	$(CC) insttypes.c

rspec$(SUFOBJ): rspec.c $(HEADERS)
	$(CC) rspec.c

xrga$(SUFOBJ): xrga.c $(HEADERS)
	$(CC) xrga.c

disptechs$(SUFOBJ): disptechs.c $(HEADERS)
	$(CC) disptechs.c

pollem$(SUFOBJ): pollem.c $(HEADERS)
	$(CC) pollem.c

xspect$(SUFOBJ): xspect.c $(HEADERS)
	$(CC) xspect.c

pars$(SUFOBJ): pars.c parsstd.c $(HEADERS)
	$(CC) pars.c

cgats$(SUFOBJ): cgats.c cgatsstd.c $(HEADERS)
	$(CC) cgats.c

dtp20$(SUFOBJ): dtp20.c $(HEADERS)
	$(CC) dtp20.c

dtp22$(SUFOBJ): dtp22.c $(HEADERS)
	$(CC) dtp22.c

dtp41$(SUFOBJ): dtp41.c $(HEADERS)
	$(CC) dtp41.c

dtp51$(SUFOBJ): dtp51.c $(HEADERS)
	$(CC) dtp51.c

dtp92$(SUFOBJ): dtp92.c $(HEADERS)
	$(CC) dtp92.c

ss$(SUFOBJ): ss.c $(HEADERS)
	$(CC) ss.c

ss_imp$(SUFOBJ): ss_imp.c $(HEADERS)
	$(CC) ss_imp.c

i1disp$(SUFOBJ): i1disp.c $(HEADERS)
	$(CC) i1disp.c

i1d3$(SUFOBJ): i1d3.c $(HEADERS)
	$(CC) i1d3.c

i1pro$(SUFOBJ): i1pro.c $(HEADERS)
	$(CC) i1pro.c

i1pro_imp$(SUFOBJ): i1pro_imp.c $(HEADERS)
	$(CC) i1pro_imp.c

munki$(SUFOBJ): munki.c $(HEADERS)
	$(CC) munki.c

munki_imp$(SUFOBJ): munki_imp.c $(HEADERS)
	$(CC) munki_imp.c

hcfr$(SUFOBJ): hcfr.c $(HEADERS)
	$(CC) hcfr.c

huey$(SUFOBJ): huey.c $(HEADERS)
	$(CC) huey.c

colorhug$(SUFOBJ): colorhug.c $(HEADERS)
	$(CC) colorhug.c

spyd2$(SUFOBJ): spyd2.c $(HEADERS)
	$(CC) spyd2.c

specbos$(SUFOBJ): specbos.c $(HEADERS)
	$(CC) specbos.c

kleink10$(SUFOBJ): kleink10.c $(HEADERS)
	$(CC) kleink10.c

ex1$(SUFOBJ): ex1.c $(HEADERS)
	$(CC) ex1.c

smcube$(SUFOBJ): smcube.c $(HEADERS)
	$(CC) smcube.c

oemarch$(SUFOBJ): oemarch.c $(HEADERS)
	$(CC) oemarch.c

oeminst$(SUFOBJ): oeminst.c $(HEADERS)
	$(CC) oeminst.c

vinflate$(SUFOBJ): vinflate.c $(HEADERS)
	$(CC) vinflate.c

inflate$(SUFOBJ): inflate.c $(HEADERS)
	$(CC) inflate.c

LzmaDec$(SUFOBJ): LzmaDec.c LzmaDec.h LzmaTypes.h $(HEADERS)
	$(CC) LzmaDec.c

libinst$(SUFLIB): $(OBJS)
	$(LIBU) $(LIBOF)$@ $(OBJS)
	$(RANLIB) libinst$(SUFLIB)

# instappsup objects
SUPOBJS = instappsup$(SUFOBJ)

# instappsup library

instappsup$(SUFOBJ): instappsup.c $(HEADERS)
	$(CC) instappsup.c

libinstappsup$(SUFLIB): $(SUPOBJS)
	$(LIBU) $(LIBOF)$@ $(SUPOBJS)
	$(RANLIB) libinstappsup$(SUFLIB)

# test/example code

spotread$(SUFEXE): spotread$(SUFOBJ) libinst$(SUFLIB) inst.h
	$(LINK) $(LINKOF)spotread$(SUFEXE) spotread$(SUFOBJ) libinst$(SUFLIB) libinstappsup$(SUFLIB) $(LINKLIBS) 

oeminst$(SUFEXE): oeminst$(SUFOBJ) oemarch$(SUFOBJ) inflate$(SUFOBJ) vinflate$(SUFOBJ) LzmaDec$(SUFOBJ) libinst$(SUFLIB) libinstappsup$(SUFLIB)
	$(LINK) $(LINKOF)oeminst$(SUFEXE) oeminst$(SUFOBJ) oemarch$(SUFOBJ) inflate$(SUFOBJ) vinflate$(SUFOBJ) LzmaDec$(SUFOBJ) libinst$(SUFLIB) libinstappsup$(SUFLIB) $(LINKLIBS) 

clean:
	$(RM) *$(SUFOBJ) *$(SUFLIB) spotread$(SUFEXE) i1d3css$(SUFEXE)

