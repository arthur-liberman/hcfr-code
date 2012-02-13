AUTOMAKE_OPTIONS = dist-bzip2 
ACLOCAL_AMFLAGS = -I m4
DISTCLEANFILES = ChangeLog libusb-1.0A.pc
EXTRA_DIST = TODO PORTING

if BUILD_EXAMPLES
SUBDIRS = libusb doc examples
else
SUBDIRS = libusb doc
endif

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libusb-1.0A.pc

.PHONY: ChangeLog dist-up
ChangeLog:
	git --git-dir $(top_srcdir)/.git log > ChangeLog || touch ChangeLog

dist-hook: ChangeLog

reldir = .release/$(distdir)
dist-up: dist
	rm -rf $(reldir)
	mkdir -p $(reldir)
	cp $(distdir).tar.bz2 $(reldir)
	rsync -rv $(reldir) dsd_,libusb@frs.sourceforge.net:/home/frs/project/l/li/libusb/libusb-1.0A/
	rm -rf $(reldir)

