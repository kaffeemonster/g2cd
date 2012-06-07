###############################################################
#
# G2CD-Makefile
# uhm, and a nice example how a Makefile can look like, when
# not autogerated
#
# Copyright (c) 2004 - 2012 Jan Seiffert
#
# This file is part of g2cd.
#
# g2cd is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3
# of the License, or (at your option) any later version.
#
# g2cd is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with g2cd.
# If not, see <http://www.gnu.org/licenses/>.
#
# $Id: Makefile,v 1.38 2005/11/05 18:25:48 redbully Exp redbully $
#
# ! USE SPACES AT = AND += OPERATION ! The solaris 5.7 'make'
# is a little ...
#

include config_auto.make

#	version info
# gcc
CC_VER_INFO = --version
# sun studio
#CC_VER_INFO = -V
CC_VER = "$(PORT_PR) \"%02d%02d\n\" $($(PORT_PR) "__GNUC__ __GNUC_MINOR__\n" | $(CC) -E -xc - | tr -c "[:digit:]\n" " " |  tail -n 1)"
#	rcs, and a little silent-magic
CO = @$(PORT_PR) "\tRCS[$@]\n"; co
AR = @./ccdrv -s$(VERBOSE) "AR[$@]" ./arflock $@ $(AR_PROG)
RANLIB = @./ccdrv -s$(VERBOSE) "RL[$@]" ./arflock $@ $(RANLIB_PROG)
#	ctags, anyone?
CTAGS = ctags
CSCOPE = cscope
RM = rm -f

#
# fixed Libs
#
#	switch between profile-generation and final build
#LDLIBS_BASE += -lgcov
LDLIBS_Z = -lz $(LDLIBS_BASE)
#	All libs if we don't use modules
LDLIBS = -lcommon $(LDLIBS_Z)

#
# Linking-Flags
#

#	Lib Paths
LDFLAGS += -L./lib/

#
# Pull in some functions
#
#LDFLAGS += -Wl,-u,strlen
#LDFLAGS += -Wl,-u,strnlen
LDFLAGS += -Wl,-u,adler32

#
# Include Paths
#
# get our modulepath in the ring, if not used does not harm
CFLAGS += -Izlib

#
#
#
###############################################################
# Compatibility Option
#
#

#	If your system does not have a printf (shell-builtin or
#	whatever) replace it with an expression wich interprets
#	backslash-charakters (like \n \t)
#	This is *NOT* just cosmetical, there is one point in the
#	Build where it is needed
PORT_PR = printf
#	just examples of replacements (not needed on these systems,
#	they have a printf), make *sure* your replacement works as
#	desired on your system before using
#PORT_PR = echo -e -n   # Linux
#PORT_PR = echo         # Slolaris 5.7, the build could look a
#                         little bit ugly

#	maybe your RCS doesn't unterstand quite
COFLAGS = -q

DATE = date
DATEFLAGS = -u +%Y%m%d-%H%M

#
#
# End of interesting Variables
###############################################################
# Internal Config
#
#	internal Variables and make-targets
#	only touch if realy needed
#
MAIN = $(PACKAGE_NAME)$(EXEEXT)
VERSION = $(PACKAGE_VERSION)
DISTNAME = $(PACKAGE_NAME)-$(VERSION)
LONGNAME = Go2CDaemon $(VERSION)

#
#	mostly all files depend on these header's
#
MY_STD_DEPS_HEADS = \
	G2MainServer.h \
	lib/other.h \
	config.h
#
#	together with this files
#
MY_STD_DEPS = $(MY_STD_DEPS_HEADS) config_auto.h lib/log_facility.h lib/sec_buffer.h Makefile ccdrv
#
#	All headerfiles
#
HEADS = \
	$(MY_STD_DEPS_HEADS) \
	G2Acceptor.h \
	G2Handler.h \
	G2UDP.h \
	G2UDPPValid.h \
	G2HeaderStrings.h \
	G2Connection.h \
	G2ConHelper.h \
	G2ConRegistry.h \
	G2Packet.h \
	G2PacketTyper.h \
	G2PacketSerializer.h \
	G2PacketSerializerStates.h \
	G2QHT.h \
	G2KHL.h \
	G2GUIDCache.h \
	G2QueryKey.h \
	builtin_defaults.h \
	timeout.h \
	idbus.h \
	gup.h
#	whith gmake all could be so simple $(wildcard *.h)
#
#	All sourcefiles
#
MSRCS = \
	G2MainServer.c \
	G2Acceptor.c \
	G2Handler.c \
	G2UDP.c \
	G2Connection.c \
	G2ConHelper.c \
	G2ConRegistry.c \
	G2Packet.c \
	G2PacketSerializer.c \
	G2QHT.c \
	G2KHL.c \
	G2GUIDCache.c \
	G2QueryKey.c \
	timeout.c \
	idbus.c \
	gup.c
SRCS = \
	$(MSRCS) \
	G2PacketTyperGenerator.c \
	G2HeaderFieldsSort.c \
	ccdrv.c \
	arflock.c \
	bin2o.c \
	calltree.c
#	and again: with gmake ... $(wildcard *.c)
#
#	other files
#	like README CHANGELOG
#
AUX = \
	Makefile \
	.mapfile \
	g2cd.conf.in \
	g2cd.1 \
	COPYING \
	README \
	autogen.sh \
	configure \
	configure.ac \
	config_auto.h.in \
	config_auto.make.in \
	config.guess \
	config.sub \
	install-sh \
	bpfasm.py \
	G2UDPPValid.bpf \
	m4/acx_pthread.m4 \
	m4/ax_check_linker_flags.m4 \
	m4/ax_check_compiler_flags.m4 \
	m4/ax_check_zlib.m4 \
	m4/ax_check_var.m4 \
	m4/ax_sys_dev_poll.m4 \
	m4/ax_wint_t.m4 \
	m4/test_asm.m4 \
	m4/pkg.m4 \
	obs_build/debian.changelog \
	obs_build/debian.control \
	obs_build/debian.rules \
	obs_build/g2cd.dsc \
	obs_build/g2cd.spec \
	obs_build/g2cd-xUbuntu_6.06.dsc

#
#	files to be included in a package
#
TARED_FILES = $(HEADS) $(SRCS) $(AUX)
# normaly we would set it here, derived vom the mod-dirs, but..
TARED_DIRS = m4 obs_build

AUXS = \
	g2cd.conf \
	.mapfile \
	version.h
#
#	what should be made
#
OBJS = \
	G2Packet.o \
	G2Connection.o \
	data.o \
	G2MainServer.o \
	G2Acceptor.o \
	G2Handler.o \
	G2UDP.o \
	G2ConHelper.o \
	G2ConRegistry.o \
	G2PacketSerializer.o \
	G2QHT.o \
	G2KHL.o \
	G2GUIDCache.o \
	G2QueryKey.o \
	timeout.o \
	idbus.lo \
	gup.o
#	and again: with gmake ... $(patsubst %.c,%.o,$(MSRCS))
#OBJS = $(patsubst %.c,%.o,$(MSRCS))

# Depend. - files, if you want to track the
# Depend. automaticaly
#DEPENDS = $(patsubst %.c,%.d,$(MSRCS))
#DEPENDS =
RTL_DUMPS = \
	G2MainServer.rtl \
	G2Acceptor.rtl \
	G2Handler.rtl \
	G2UDP.rtl \
	G2Connection.rtl \
	G2ConHelper.rtl \
	G2ConRegistry.rtl \
	G2Packet.rtl \
	G2PacketSerializer.rtl \
	G2QHT.rtl \
	G2KHL.rtl \
	G2GUIDCache.rtl \
	G2QueryKey.rtl \
	timeout.rtl \
	gup.rtl
# Insert all rule high enough, so it gets the default rule
#	std. all-taget

all: $(MAIN)

# build our commen lib
include lib/module.make
# support for compiled-in zlib
include zlib/module.make

#
#
# End of Name - Config.
###############################################################
# Additional Make - Gizmos
#
#

#
# We like colors
#
COLRED = "`tput setaf 1`"
COLGREEN = "`tput setaf 2`"
COLYELLOW = "`tput setaf 3`"
COLBLUE = "`tput setaf 4`"
COLNO = "`tput op`"

#
COLS = "`tput cols`"
TOEOL = "$$(tput cuf $(COLS))"


#
# make the compiling silence (this new style as seen on linux 2.6)
#
COMPILE.c = @./ccdrv -s$(VERBOSE) "CC[$<]" $(CC) $(CFLAGS) $(CPPFLAGS) -c
LINK.c = @./ccdrv -s$(VERBOSE) "LD[$<]" $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(LOADLIBES)
#COMPILE.c = @$(PORT_PR) "\tCC[$<]\n"; $(CC) $(CFLAGS) $(CPPFLAGS) -c
#LINK.c = @$(PORT_PR) "\tLD[$<]\n"; $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(LOADLIBES)

#
# since SUN-make doens't set -o
#
.SUFFIXES: .c .o .lo .a .rtl .gch .h
.c.o:
	@$(COMPILE.c) $< -o $@

.c.lo:
	@$(COMPILE.c) $< $(PICFLAGS) -o $@

.c.rtl:
	@$(RM) $@ && $(COMPILE.c) -dr $< -o `basename $@ .rtl`.o && (mv -f `basename $@ .rtl`.c.*.rtl $@ || touch $@)

.h.gch.h:
	@$(COMPILE.c) $< -o $@

.o.a:
	@$(AR) $(ARFLAGS) $@ $*.o

#%.o: %.c
#	@$(COMPILE.c) $< -o $@
#
#%.rtl: %.c
#	@$(RM) $@ && $(COMPILE.c) -dr $< -o `basename $@ .rtl`.o && (mv -f `basename $@ .rtl`.c.*.rtl $@ || touch $@)
#
#%.h.gch: %.h
#	@$(COMPILE.c) $< -o $@
#
# add impliciet rules for dependencies (not used until now, managed from hand)
#
#%.o: %.d

#	Adjust to your Compiler, gcc genarates depend. with "-MM"
#%.d: %.c
#	@$(PORT_PR) "\tDEP[$@]\n"; $(CC) -MM -MG $(CFLAGS) $(CPPFLAGS) $< | sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@;

#
#
# End of Make - Gizmos.
###############################################################
# Fianlly the Main-target-rules
#
#
#	make clean-target always work
.PHONY: $(MAKECMDGOALS) all eclean clean distclean zlibclean libclean alll allz help love hardcopy print print_pretty scope todo strip-all strip install once final withzlib oncewithzlib686 finalwithzlib686 oncewithzlib finalwithzlib

#	what are the all-targets derived from
#	this command is EXTRA written out since it allows us to
#	define a diffrent main-name and the solaris 5.7 'make'
#	is a little...
$(MAIN): $(AUXS) $(OBJS) $(LIBCOMMON)
	@./ccdrv -s$(VERBOSE) "LD[$@]" $(CC) -o $@ $(OBJS) $(LDFLAGS) $(LOADLIBES) $(LDLIBS)

once: final
final: .final
.final: $(AUXS) $(LIBSRCS) $(LIBBINOBJS) $(MSRCS) $(HEADS)
	@./ccdrv -s$(VERBOSE) "CC-LD[$(MAIN)]" $(CC) $(CFLAGS) $(CPPFLAGS) -o $(MAIN) $(MSRCS) $(LIBSRCS) $(LIBBINOBJS) $(LDFLAGS) $(LOADLIBES) $(LDLIBS_Z) && touch $@

withzlib: .withzlib
.withzlib: $(AUXS) $(OBJS) $(ZLIB) $(LIBCOMMON)
	@./ccdrv -s$(VERBOSE) "LD[$(MAIN)]" $(CC) -o $(MAIN) $(OBJS) $(LDFLAGS) $(ZLIB) $(LIBCOMMON) $(LOADLIBES) $(LDLIBS_BASE) && touch $@
	
oncewithzlib686: finalwithzlib686
finalwithzlib686: .finalwithzlib686
.finalwithzlib686: $(AUXS) $(LIBSRCS) $(LIBBINOBJS) $(ZSRCS) $(ZSRCS_686) $(MSRCS) $(HEADS)
	@./ccdrv -s$(VERBOSE) "CC-LD[$(MAIN)]" $(CC) $(CFLAGS) -DASMV -DNO_UNDERLINE $(CPPFLAGS) -o $(MAIN) $(MSRCS) $(LIBSRCS) $(ZSRCS) $(ZSRCS_686) $(LIBBINOBJS) $(LDFLAGS) $(LOADLIBES) $(LDLIBS_BASE) && touch $@

oncewithzlib: finalwithzlib
finalwithzlib: .finalwithzlib
.finalwithzlib: $(AUXS) $(LIBSRCS) $(LIBBINOBJS) $(ZSRCS) $(MSRCS) $(HEADS)
	@./ccdrv -s$(VERBOSE) "CC-LD[$(MAIN)]" $(CC) $(CFLAGS) $(CPPFLAGS) -o $(MAIN) $(MSRCS) $(LIBSRCS) $(ZSRCS) $(LIBBINOBJS) $(LDFLAGS) $(LOADLIBES) $(LDLIBS_BASE) && touch $@

# substitude config file defaults
g2cd.conf: g2cd.conf.in builtin_defaults.h Makefile ccdrv
	@./ccdrv -s$(VERBOSE) "CPP[$@]" sh -c "$(CPP) $(CPPFLAGS) -P -I. - < g2cd.conf.in > $@"

# keep config up to date
configure: configure.ac install-sh config.guess config.sub
	-autoconf -f
config_auto.h.in: configure.ac
	-autoheader -f
config_auto.h: config_auto.h.in configure
	./configure && touch -c "$@" config_auto.make
config_auto.make: config_auto.make.in configure
	./configure && touch -c "$@" config_auto.h
config.guess:
	-automake -c -f -a
config.sub:
	-automake -c -f -a
install-sh:
	-automake -c -f -a

#
#	Install
#
# paths
$(DESTDIR):
	mkdir -p "$(DESTDIR)"
$(BINPATH): $(DESTDIR)
	mkdir -p "$(BINPATH)"
$(CACHEPATH): $(DESTDIR)
	mkdir -p "$(CACHEPATH)"
$(CONFPATH): $(DESTDIR)
	mkdir -p "$(CONFPATH)"
$(DATAROOTPATH): $(DESTDIR)
	mkdir -p "$(DATAROOTPATH)"
$(DOCPATH): $(DESTDIR)
	mkdir -p "$(DOCPATH)"
$(MYMANPATH): $(DESTDIR)
	mkdir -p "$(MYMANPATH)"

MYMANPATH1 = $(MYMANPATH)/man1
MYMANPATH5 = $(MYMANPATH)/man5
$(MYMANPATH1): $(MYMANPATH)
	mkdir -p "$(MYMANPATH1)"
$(MYMANPATH5): $(MYMANPATH)
	mkdir -p "$(MYMANPATH5)"

# program
install_$(MAIN): $(MAIN) $(BINPATH)
	$(INSTALL_PRG) $(MAIN) $(BINPATH)/
install_program: install_$(MAIN)

# data
install_g2cd.conf: g2cd.conf $(CONFPATH)
	$(INSTALL_DAT) g2cd.conf $(CONFPATH)/
install_g2cd.1: g2cd.1 $(MYMANPATH1)
	$(INSTALL_DAT) g2cd.1 $(MYMANPATH1)/
install_data: install_g2cd.conf install_g2cd.1

install: install_program install_data $(CACHEPATH)

install_striped: install
	$(STRIP) $(BINPATH)/$(MAIN)

#	strip target, the new way by seperating the dbg-info in a new
#	file, not discarding them
#objcopy --set-section-flags .dtors=alloc,load,readonly,data --set-section-flags .dynamic=alloc,load,readonly,data --set-section-flags .got.plt=alloc,load,readonly,data g2cd g2cd.r
strip-all: strip
	$(STRIP) -R .comment -R .jcr $(MAIN)
strip: .$(MAIN).dbg
.$(MAIN).dbg: $(MAIN)
	-@./ccdrv -s$(VERBOSE) "OBJC[$@]" $(OBJCOPY) --only-keep-debug $^ $@ && \
	./ccdrv -s$(VERBOSE) "OBJC[$^]" $(OBJCOPY) --strip-debug $^ && \
	./ccdrv -s$(VERBOSE) "OBJC[$@]" $(OBJCOPY) --add-gnu-debuglink=$@ $^

#
#	clean
#
#	std-cleaning target
eclean: libclean zlibclean
	-$(RM) $(OBJS) $(RTL_DUMPS) sbox.bin sbox.bin.tmp ccdrv.o arflock.o bin2o.o
clean: eclean
	-$(RM) $(MAIN) $(MAIN)z $(MAIN).exe $(MAIN)z.exe $(MAIN)z.c .$(MAIN).dbg ccdrv arflock bin2o G2PacketTyperGenerator G2HeaderFieldsSort calltree callgraph.dot .arflockgate .final .withzlib .finalwithzlib .finalwithzlib686
distclean: libdclean zlibdclean clean
	-$(RM) config.status config.log config_auto.h config_auto.make version.h tags cscope.out TODO stubmakerz core gmon.out  *.bb *.bbg *.da *.i *.s *.bin *.rtl
edistclean: distclean
	$(RM) G2PacketTyper.h G2HeaderFields.h G2UDPPValid.h

#	generate dependencies on whish
#depend: $(DEPENDS)
#	temporary clutch, to control deps by hand
depend: libdepend $(SRCS)
	@$(PORT_PR) "\tDEP[$@]\n"; $(CC) -MM -MG $(CFLAGS) $(CPPFLAGS) $(SRCS) > $@;

#	package-building for ...
#	compression in extra step since the solaris 5.7 'tar'
#	dosn't unstand ANY compression (no, not even -Z)
#	hopefully bzip or gzip fallback funktions
#	($(PORT_PR) "\tLZMA[$(DISTNAME).tar]\n"; lzma -ef9 `cat .fname`.tar) || 
#	Debian is anal again... no bzip2 for Debian
#	($(PORT_PR) "\tBZIP2[$(DISTNAME).tar]\n"; bzip2 -f9 `cat .fname`.tar) || 
dist: $(TARED_FILES)
	@$(PORT_PR) $(DISTNAME)-`$(DATE) $(DATEFLAGS)` > .fname
	@-$(RM) -rf `cat .fname`
	@$(PORT_PR) "\tPREP[`cat .fname`]\n"
	@mkdir `cat .fname`
	@for da_dir in $(TARED_DIRS) ; do mkdir `cat .fname`/$$da_dir ; done
	@for da_file in $(TARED_FILES) ; do ln $$da_file `cat .fname`/$$da_file ; done
	@$(PORT_PR) "\tTAR[`cat .fname`]\n"
	@tar chf `cat .fname`.tar `cat .fname`
	@-(($(PORT_PR) "\tGZIP[`cat .fname`.tar]\n"; gzip -f9 `cat .fname`.tar) || ($(PORT_PR) "\tCOMPR[`cat .fname`.tar]\n"; compress `cat .fname`.tar))
	@$(PORT_PR) "please check if packet is correct!\n\n"
	@-$(RM) -rf `cat .fname` .fname

distrevno: $(TARED_FILES)
	@$(PORT_PR) $(DISTNAME)-r`bzr revno` > .fname
	@-$(RM) -rf `cat .fname`
	@$(PORT_PR) "\tPREP[`cat .fname`]\n"
	@mkdir `cat .fname`
	@for da_dir in $(TARED_DIRS) ; do mkdir `cat .fname`/$$da_dir ; done
	@for da_file in $(TARED_FILES) ; do ln $$da_file `cat .fname`/$$da_file ; done
	@$(PORT_PR) "\tTAR[`cat .fname`]\n"
	@tar chf `cat .fname`.tar `cat .fname`
	@-(($(PORT_PR) "\tGZIP[`cat .fname`.tar]\n"; gzip -f9 `cat .fname`.tar) || ($(PORT_PR) "\tCOMPR[`cat .fname`.tar]\n"; compress `cat .fname`.tar))
	@$(PORT_PR) "please check if packet is correct!\n\n"
	@-$(RM) -rf `cat .fname` .fname


#	genarte tags-file for vi(m)
tags: $(LIBHEADS) $(LIBASRCS) $(SRCS) $(HEADS) Makefile
	@-$(RM) -f $@.tmp $@
	@for i in $(LIBHEADS) $(LIBASRCS) $(SRCS) $(HEADS) ; do $(PORT_PR) "\tTAG[$$i]\n" ; $(CTAGS) -f $@.tmp -a "$$i" ; done && mv -f $@.tmp $@

#	another fun, generate cscope
scope: cscope.out
cscope.out: $(LIBHEADS) $(LIBASRCS) $(SRCS) $(HEADS) Makefile
	@-$(RM) -f $@.tmp $@
	@for i in $(LIBHEADS) $(LIBASRCS) $(SRCS) $(HEADS) ; do $(PORT_PR) "\tCSCOPE[$$i]\n" 1>&2 ; $(PORT_PR) "%s " $$i ; done | $(CSCOPE) -b -f$@.tmp -i- && mv -f $@.tmp $@

#	Ok, whats left to do. Please regard the todo line format!
todo: TODO
TODO: $(LIBHEADS) $(LIBASRCS) $(SRCS) $(HEADS) Makefile
	@-$(RM) -f $@.tmp $@
	@for i in $(LIBHEADS) $(LIBASRCS) $(SRCS) $(HEADS) ; do $(PORT_PR) "\tTODO[$$i]\n" 1>&2 ; $(PORT_PR) "%s:\n" $$i ; grep -n "^// TODO:" $$i | sed -e 's-// TODO:--g' | sed -e 's:^:	:g' ; $(PORT_PR) "\n" ; done 1> $@.tmp && mv -f $@.tmp $@

callgraph: callgraph.dot
callgraph.dot: $(RTL_DUMPS) calltree
	@$(RM) $@ $@.tmp && ./ccdrv -s$(VERBOSE) "CT[$@]" ./calltree -o logg_more,logg -f $@.tmp $(RTL_DUMPS) && mv -f $@.tmp $@

#	give help
help: Makefile
	@$(PORT_PR) "\nThis is $(LONGNAME),\n" ; \
	$(PORT_PR) "a Gnutella2 Server only impementation\n" ; \
	$(PORT_PR) "\nYou can run:\n" ; \
	$(PORT_PR) "\tmake\t\tor\n\tmake all\tor\n\tmake final\tto compile the programm\n" ; \
	$(PORT_PR) "\tmake strip\tto strip the binaries before install (needs recent objcopy)\n"; \
	$(PORT_PR) "\tmake install\tto install the programm\n" ; \
	$(PORT_PR) "\nPlease make sure to read the README and configure the package,\n" ; \
	$(PORT_PR) "before compilation!\n" ; \
	$(PORT_PR) "\nFor the developer:\n" ; \
	$(PORT_PR) "\tmake clean\tto clean the build directory\n" ; \
	$(PORT_PR) "\tmake distclean\tto clean the build directory even more\n" ; \
	$(PORT_PR) "\tmake dist\tto build a tar.{bz2|gz|z} snapshot of the project\n" ; \
	$(PORT_PR) "\tmake tags\tto build a tags file for your $$(if [[ -n $$EDITOR ]] ; then $(PORT_PR) $$(basename $$EDITOR) ; else $(PORT_PR) '\$$EDITOR' ; fi)\n" ; \
	$(PORT_PR) "\tmake scope\tor\n" ; \
	$(PORT_PR) "\tmake cscope\tto build a cscope.out file for your $$(if [[ -n $$EDITOR ]] ; then $(PORT_PR) $$(basename $$EDITOR) ; else $(PORT_PR) '\$$EDITOR' ; fi)\n" ; \
	$(PORT_PR) "\tmake callgraph\tbuild a callgraph in graphviz format, needs gcc & graphviz\n\t\t\tvery basic, functons called via lookup tables are missing,\n\t\t\tlogging-calls left out for clarity\n" ; \
	$(PORT_PR) "\tmake todo\tor\n" ; \
	$(PORT_PR) "\tmake TODO\tto build a TODO list\n" ; \
	$(PORT_PR) "\nSee COPYING for the license\n\n" ;

love:
	@$(PORT_PR) "Don't know how to make love\n"

ccdrv: ccdrv.c Makefile
	@$(PORT_PR) "\tCC-LD[$@]\n"; $(HOSTCC) $(HOSTCFLAGS) ccdrv.c -o $@ -lncurses || ( \
		$(PORT_PR) "\tCC-LD[$@]\n"; $(HOSTCC) $(HOSTCFLAGS) ccdrv.c -o $@ -lcurses || ( \
			$(PORT_PR) "\n *****************************************************\n" ; \
			$(PORT_PR) " * compiling cc-driver failed, using fallback script *\n" ; \
			$(PORT_PR) " *****************************************************\n\n" ; \
			$(PORT_PR) "#! /bin/sh\n" > $@ ; \
			$(PORT_PR) "shift 2\n" >> $@ ; \
			$(PORT_PR) "echo \$${@}\n" >> $@ ; \
			$(PORT_PR) "CC=\$${1}\n" >> $@; \
			$(PORT_PR) "shift\n" >> $@; \
			$(PORT_PR) "\$${CC} \"\$${@}\"\n" >> $@; \
			chmod a+x $@ ) )

arflock: arflock.c ccdrv Makefile
	@./ccdrv -s$(VERBOSE) "CC-LD[$@]" $(HOSTCC) $(HOSTCFLAGS) arflock.c -o $@ || ( \
		$(PORT_PR) "\n *****************************************************\n" ; \
		$(PORT_PR) " * compiling ar-proxy failed,  using fallback script *\n" ; \
		$(PORT_PR) " *         !!! parralel builds may fail !!!          *\n" ; \
		$(PORT_PR) " *****************************************************\n\n" ; \
		$(PORT_PR) "#! /bin/sh\n" > $@ ; \
		$(PORT_PR) "shift 2\n" >> $@ ; \
		$(PORT_PR) "echo \$${@}\n" >> $@ ; \
		$(PORT_PR) "AR=\$${1}\n" >> $@; \
		$(PORT_PR) "shift\n" >> $@; \
		$(PORT_PR) "echo `which flock`" >> $@; \
		$(PORT_PR) "\$${AR} \$${@}\n" >> $@; \
		chmod a+x $@ )

bin2o: bin2o.c ccdrv Makefile
	@./ccdrv -s$(VERBOSE) "CC-LD[$@]" $(HOSTCC) $(HOSTCFLAGS) bin2o.c -o $@

G2PacketTyperGenerator: G2PacketTyperGenerator.c ccdrv Makefile G2Packet.h lib/list.h
	@./ccdrv -s$(VERBOSE) "CC-LD[$@]" $(HOSTCC) $(HOSTCFLAGS) G2PacketTyperGenerator.c -o $@

G2HeaderFieldsSort: G2HeaderFieldsSort.c ccdrv Makefile G2HeaderStrings.h
	@./ccdrv -s$(VERBOSE) "CC-LD[$@]" $(HOSTCC) $(HOSTCFLAGS) G2HeaderFieldsSort.c -o $@



.INTERMEDIATE: sbox.bin
sbox.bin: $(TARED_FILES)
	@tar -cf - `find . -name zlib -prune -o -type f -a \( -name '*.c' -o -name '*.h' \) -print` | bzip2 -c9 - > sbox.bin.tmp && mv -f sbox.bin.tmp sbox.bin

calltree: calltree.c Makefile ccdrv
	@./ccdrv -s$(VERBOSE) "LD[$@]" $(HOSTCFLAGS) calltree.c -o $@

hardcopy: $(TARED_FILES)
	for file in $(TARED_FILES) ; do expand -t 3 $$file | fold -s | lpr ; done;
print: $(TARED_FILES)
	for file in $(TARED_FILES) ; do expand -t 3 $$file | fold -s -w 64 | pr -n -F -h "$$file" | lpr ; done;
print_pretty: $(TARED_FILES)
	for file in $(TARED_FILES) ; do expand -t 3 $$file | fold -s | nl -ba | lpr -J "$$file" -o prettyprint ; done;
#
#
# End of Main-target-rules.
###############################################################
# Depend.-tracking
#
#
G2PacketTyper.h: G2PacketTyperGenerator ccdrv
	@if [ "$(TARGET_ENDIAN)" = "little" ] ; then \
		./ccdrv -s$(VERBOSE) "GEN[$@]" ./G2PacketTyperGenerator -l $@ ; \
	else \
		./ccdrv -s$(VERBOSE) "GEN[$@]" ./G2PacketTyperGenerator -b $@ ; \
	fi

G2HeaderFields.h: G2HeaderFieldsSort ccdrv
	@./ccdrv -s$(VERBOSE) "GEN[$@]" ./G2HeaderFieldsSort $@

G2UDPPValid.h: G2UDPPValid.bpf bpfasm.py ccdrv
	@./ccdrv -s$(VERBOSE) "GEN[$@]" ./bpfasm.py -sc -o "$@" G2UDPPValid.bpf

#	Batch-creation of version-info
version.h: Makefile
	@$(PORT_PR)	"\tCREATE[$@]\n"; \
	$(PORT_PR)	"#ifndef _VERSION_H\n" > $@; \
	$(PORT_PR)	"#define _VERSION_H\n" >> $@; \
	$(PORT_PR)	"\n" >> $@; \
	$(PORT_PR)	"#define DIST\t\"$(DISTNAME)\"\n" >> $@; \
	$(PORT_PR)	"#define OUR_PROC\t\"$(MAIN)\"\n" >> $@; \
	$(PORT_PR)	"#define OUR_UA\t\"$(LONGNAME)\"\n" >> $@; \
	$(PORT_PR)	"#define OUR_VERSION\t\"$(VERSION)\"\n" >> $@; \
	$(PORT_PR)	"#define OUR_SYSTEM_INFO\t\"" >> $@; \
	uname -a | tr -d "\n" >> $@; \
	$(PORT_PR)	"\"\n" >> $@; \
	$(PORT_PR)	"#ifdef __VERSION__\n" >> $@; \
	$(PORT_PR)	"# define COMPILER_INFO\t\"$(CC) \" __VERSION__\n" >> $@; \
	$(PORT_PR)	"#else\n" >> $@; \
	$(PORT_PR)	"# define COMPILER_INFO\t\"" >> $@; \
	$(CC) $(CC_VER_INFO) 2>&1 | head -n 1 | tr -d "\n" >> $@; \
	$(PORT_PR)	"\"\n" >> $@; \
	$(PORT_PR)	"#endif\n" >> $@; \
	$(PORT_PR)	"\n" >> $@; \
	$(PORT_PR)	"#endif\n" >> $@; \
	$(PORT_PR)	"//EOF\n" >> $@
#	$(PORT_PR)	"$(TOEOL)\b\b\b[OK]\n"

data.o: sbox.bin bin2o
	@./ccdrv -s$(VERBOSE) "BIN[$@]" ./bin2o -a $(AS) -u -r $(BIN2O_OPTS) -o $@ sbox.bin
#	what are the .o's derived from: implicit [target].c +
#	additional dependencies, written out...
G2MainServer.o: G2Handler.h G2Connection.h G2ConRegistry.h G2KHL.h G2GUIDCache.h G2QueryKey.h timeout.h idbus.h lib/hzp.h lib/atomic.h lib/backtrace.h lib/config_parser.h lib/my_bitopsm.h version.h builtin_defaults.h
G2Acceptor.o: G2Acceptor.h G2Connection.h G2ConHelper.h G2ConRegistry.h G2KHL.h gup.h lib/recv_buff.h lib/combo_addr.h lib/my_epoll.h lib/atomic.h lib/itoa.h
G2Handler.o: G2Handler.h G2Connection.h G2ConHelper.h G2ConRegistry.h G2Packet.h G2PacketSerializer.h lib/recv_buff.h lib/my_epoll.h lib/hzp.h
G2UDP.o: G2UDP.h G2UDPPValid.h G2Packet.h G2PacketSerializer.h G2QHT.h gup.h lib/my_bitopsm.h lib/atomic.h lib/recv_buff.h lib/udpfromto.h lib/hzp.h
G2Connection.o: G2Connection.h G2QHT.h G2ConRegistry.h G2KHL.h G2HeaderFields.h lib/recv_buff.h lib/atomic.h lib/hzp.h
G2ConHelper.o: G2ConHelper.h G2ConRegistry.h G2Connection.h G2QHT.h lib/my_epoll.h lib/atomic.h lib/recv_buff.h 
G2ConRegistry.o: G2ConRegistry.h G2Connection.h G2QHT.h lib/combo_addr.h lib/hlist.h lib/hthash.h lib/hzp.h
G2Packet.o: G2Packet.h G2PacketSerializer.h G2PacketTyper.h G2Connection.h G2ConRegistry.h G2QueryKey.h G2KHL.h G2GUIDCache.h G2QHT.h lib/my_bitops.h lib/guid.h
G2PacketSerializer.o: G2PacketSerializer.h G2Packet.h
G2QHT.o: G2QHT.h G2Packet.h G2ConRegistry.h lib/my_bitops.h lib/my_bitopsm.h lib/hzp.h lib/atomic.h lib/tchar.h
G2KHL.o: G2KHL.h lib/my_epoll.h lib/combo_addr.h lib/hlist.h lib/hthash.h lib/rbtree.h lib/my_bitops.h lib/ansi_prng.h
G2GUIDCache.o: G2GUIDCache.h lib/combo_addr.h lib/hlist.h lib/hthash.h lib/rbtree.h lib/my_bitops.h lib/ansi_prng.h
G2QueryKey.o: G2QueryKey.h lib/hthash.h lib/ansi_prng.h lib/guid.h
timeout.o: timeout.h
idbus.lo: idbus.h G2MainServer.h G2ConRegistry.h timeout.h lib/my_epoll.h lib/my_bitopsm.h
gup.o: gup.h G2Acceptor.h G2UDP.h G2Connection.h G2ConHelper.h lib/my_epoll.h lib/sec_buffer.h lib/recv_buff.h lib/hzp.h
#	header-deps
G2MainServer.h: G2Connection.h G2Packet.h lib/combo_addr.h lib/atomic.h lib/log_facility.h
G2Connection.h: G2Packet.h G2QHT.h G2HeaderStrings.h timeout.h lib/hzp.h lib/combo_addr.h lib/list.h lib/hlist.h version.h
G2ConHelper.h: G2Connection.h lib/sec_buffer.h lib/my_epoll.h
G2ConRegistry.h: G2Connection.h lib/combo_addr.h
G2QueryKey.h: lib/combo_addr.h
G2Packet.h: G2PacketSerializerStates.h lib/sec_buffer.h lib/list.h
G2PacketSerializer.h: G2PacketSerializerStates.h G2Packet.h
G2QHT.h: lib/hzp.h lib/atomic.h lib/tchar.h lib/palloc.h
G2GUIDCache.h: lib/guid.h
timeout.h: lib/list.h lib/hzp.h

#
#	add std.-dep. to .o's, is this gmake only?
#
$(OBJS): $(MY_STD_DEPS)
$(RTL_DUMPS): $(MY_STD_DEPS)
Makefile: config_auto.make lib/module.make zlib/module.make

#$(DEPENDS): Makefile

# 	include all dependencies
#include $(DEPENDS)

#
#
# End of rules.
###############################################################
# EOF
