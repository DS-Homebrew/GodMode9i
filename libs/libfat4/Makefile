ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro)
endif

export TOPDIR	:=	$(CURDIR)

export LIBFAT_MAJOR	:= 1
export LIBFAT_MINOR	:= 1
export LIBFAT_PATCH	:= 5

export VERSTRING	:=	$(LIBFAT_MAJOR).$(LIBFAT_MINOR).$(LIBFAT_PATCH)

export DESTDIR := $(DESTDIR)

default: release

all: release dist

release: nds-release

nds-release: include/libfatversion.h
	$(MAKE) -C nds BUILD=release

debug: nds-debug

nds-debug: include/libfatversion.h
	$(MAKE) -C nds BUILD=debug

clean: nds-clean

nds-clean:
	$(MAKE) -C nds clean

dist-bin: nds-dist-bin

nds-dist-bin: include/libfatversion.h nds-release distribute/$(VERSTRING)
	$(MAKE) -C nds dist-bin

dist-src: distribute/$(VERSTRING)
	@tar --exclude=.svn --exclude=*CVS* -cvjf distribute/$(VERSTRING)/libfat-src-$(VERSTRING).tar.bz2 \
	source include Makefile \
	nds/Makefile \
	gba/Makefile \
	libogc/Makefile \
	gp2x/Makefile

dist: dist-bin dist-src

distribute/$(VERSTRING):
	@[ -d $@ ] || mkdir -p $@

include/libfatversion.h : Makefile
	@echo "#ifndef __LIBFATVERSION_H__" > $@
	@echo "#define __LIBFATVERSION_H__" >> $@
	@echo >> $@
	@echo "#define _LIBFAT_MAJOR_	$(LIBFAT_MAJOR)" >> $@
	@echo "#define _LIBFAT_MINOR_	$(LIBFAT_MINOR)" >> $@
	@echo "#define _LIBFAT_PATCH_	$(LIBFAT_PATCH)" >> $@
	@echo >> $@
	@echo '#define _LIBFAT_STRING "libFAT Release '$(LIBFAT_MAJOR).$(LIBFAT_MINOR).$(LIBFAT_PATCH)'"' >> $@
	@echo >> $@
	@echo "#endif // __LIBFATVERSION_H__" >> $@

install: nds-install

nds-install: nds-release
	$(MAKE) -C nds install
