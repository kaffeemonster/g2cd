###############################################################
#
# G2CD-Makefile
# uhm, and a nice example how a Makefile can look like, when
# not autogenarated
#
# Copyright (c) 2004, Jan Seiffert
#
# This file is part of g2cd.
#
# g2cd is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2
# of the License, or any later version.
# 
# g2cd is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public
# License along with g2cd; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA  02111-1307  USA
#
# $Id: Makefile,v 1.38 2005/11/05 18:25:48 redbully Exp redbully $
#
# ! USE SPACES AT = AND += OPERATION ! The solaris 5.7 'make'
# is a little ...
#

#
# Variables user might want to change (make it work on his
# system)
#

#
# Name of your Programs
#	compiler
CC = gcc
HOSTCC = gcc
CC_VER_INFO = --version
CC_VER = "$(PORT_PR) \"%02d%02d\n\" $($(PORT_PR) "__GNUC__ __GNUC_MINOR__\n" | $(CC) -E -xc - | tr -c "[:digit:]\n" " " |  tail -n 1)"
#	rcs, and a little silent-magic
CO = @$(PORT_PR) "\tRCS[$@]\n"; co
AR = @./ccdrv -s$(VERBOSE) "AR[$@ $<]" ar
ARFLAGS = cr
RANLIB = @./ccdrv -s$(VERBOSE) "RL[$@]" ranlib
#	ctags, anyone?
CTAGS = ctags
CSCOPE = cscope
OBJCOPY = objcopy
STRIP = strip

# split up host and target
HOSTCFLAGS = -O -Wall

#
# the C-Standart the compiler should work with
#
#	thats what i want
CFLAGS += -std=c99
#	don't know how it is right, but cygwin excludes some
#	essential function if __STRICT_ANSI is defined
#CFLAGS += -std=gnu99
CFLAGS += -pedantic
#	gcc == 2.95.3
#	wuerg-Around just functions due to Gnu extension to the
#	C-Language
#CFLAGS += -std=gnu9x

#
# Warnings (for the Devs)
#
#	most eminent warnings
WARN_FLAGS = -Wall -Wimplicit -Wmissing-prototypes -Wwrite-strings
#	middle
WARN_FLAGS += -Wpointer-arith -Wcast-align
#  ok, if you want to get picky...
WARN_FLAGS += -W -Wcast-qual -Wunused
#	strange warnings under Cygwin, i tripple checked it, to
#	no avail
#CFLAGS += -Wconversion
#	least eminent
#	not on gcc 2.95.3
#CFLAGS += -Wpadded
WARN_FLAGS += -Wshadow
#	interesting to know
#CFLAGS += -Winline
#	not on gcc 2.95.3
WARN_FLAGS += -Wdisabled-optimization
#	gcc 3.4.?
WARN_FLAGS += -Wsequence-point
CFLAGS += $(WARN_FLAGS)

#
# !! mashine-dependent !! non-x86 please see here
#
ARCH = athlon-xp
#ARCH = pentium2
#ARCH = pentium4
#ARCH = G4
#ARCH = ultrasparc
ARCH_FLAGS += -momit-leaf-frame-pointer
ARCH_FLAGS += -minline-all-stringops
ARCH_FLAGS += -maccumulate-outgoing-args
ARCH_FLAGS += -march=$(ARCH)
#ARCH_FLAGS = -mcpu=$(ARCH)
CFLAGS += $(ARCH_FLAGS)

#
# Include paths (addidtional)
#
#ZLIB__SYSTEM_PATH = /opt/zlib-1.1.4
#CFLAGS += -I$(ZLIB_SYSTEM_PATH)/include

#
# Debugging
#
# stuff it here, were -save-temps is
#	this hopefully makes compilation faster, gcc-specific?
CFLAGS += -pipe
#CFLAGS += -save-temps
CFLAGS += -g3 -pg

#
# Optimisation
#
#	burn it baby (needed for inlining on older gcc)
OPT_FLAGS = -O3
#  at least O should be selected, or ugly things will happen...
#OPT_FLAGS = -O
#	we are not doing any fancy Math, but...
OPT_FLAGS += -ffast-math
#	needed on x86 (and other) even when O3 is and -g is not
#	specified for max perf.
OPT_FLAGS += -fomit-frame-pointers
#	gcc >= 3.x and supported for target (-march ?)
OPT_FLAGS += -fprefetch-loop-arrays
OPT_FLAGS += -funroll-loops
#	gcc >= 3.4? we want this and not the above
#OPT_FLAGS += -fold-unroll-loops
#	gcc >= 3.x together with unroll-loops usefull
OPT_FLAGS += -fmove-all-movables
#	gcc >= 3.4
OPT_FLAGS += -funit-at-a-time
OPT_FLAGS += -fweb
#	gcc >= 3.? *warning* experimental options
OPT_FLAGS += -ftracer
OPT_FLAGS += -fnew-ra
OPT_FLAGS += -funswitch-loops
#	gcc >= 3.5 or 3.4 with orig. path applied, and backtraces
#	don't look nice anymore
#OPT_FLAGS += -fvisibility=hidden
#	want to see whats gcc doing (and how long it needs)?
#OPT_FLAGS += -ftime-report
#	switch between profile-generation and final build
#OPT_FLAGS += -fprofile-generate
CFLAGS += $(OPT_FLAGS) #-fprofile-use
#	while debugging
#CFLAGS += -O1
#OPT_FLAGS += -ftest-coverage # needed?
#OPT_FLAGS += -fprofile-arcs
#CFLAGS += $(OPT_FLAGS) #-fbranch-probabilities
#	gcc 3.? make sure the OS sees our Stack-handling since we
#	are Multithreaded or leave it - seems buggy
#CFLAGS += -fstack-check

#
# Libs
#
# Libraries from the System (which will never be modules)
LDLIBS_BASE = #-lm
#	switch between profile-generation and final build
#LDLIBS_BASE += -lgcov
# either you set this and the appropreate flags below, or you
# use the -pthread shortcut on recent gcc systems
#LDLIBS_BASE += -lpthread
#	on solaris, there is a lot more we need
#LDLIBS_BASE += -lresolv
#LDLIBS_BASE += -lsocket
#LDLIBS_BASE += -lnsl
#LDLIBS_BASE += -ldl
#LDLIBS_BASE += -lrt
#
#	All libs if we don't use modules 
LDLIBS = $(LDLIBS_BASE) -lz

#
#	Linking-Flags
#
#	Hopefully the linker knows somehting about this
LDFLAGS = -g -pg
LDFLAGS += -Wl,-O1
LDFLAGS += -Wl,--sort-common
LDFLAGS += -Wl,--enable-new-dtags
LDFLAGS += -Wl,--as-needed
#LDFLAGS += -Wl,-M
# maybe get patched into binutils
#LDFLAGS += -Wl,-hashvals
#LDFLAGS += -Wl,-zdynsort
#	Some linker can also strip the bin. with this flag 
#LDFLAGS += -s
#	switch between profile-generation and final build
#LDFLAGS += -fprofile-arcs
# pthread shortcut or single Makros and the lib above
LDFLAGS += -pthread
#LDFLAGS += -D_POSIX_PTHREAD_SEMANTICS
#LDFLAGS += -D_REENTRANT
# on solaris we can do a little in respect to symbols
#LDFLAGS += -Wl,-M,.mapfile
# maybe one day also on linux
#LDFLAGS += -Wl,-B,direct
# get some more symbols in the executable, so backktraces
# look better
LDFLAGS += -rdynamic

#
#	Lib Paths (additional)
#
#LDFLAGS += -L$(ZLIB_LPATH)/lib -Wl,R,$(ZLIB_LPATH)/lib


#
# Include Paths
#
# get our modulepath in the ring, if not used does not harm
CFLAGS += -Izlib

#
# Macros & Misc
#
#	shortcut for -D_REENTRANT and all other such things on
#	some GCC's
CFLAGS += -pthread
#CFLAGS += -D_POSIX_PTHREAD_SEMANTICS
#CFLAGS += -D_REENTRANT
#	not used (until now)
#CFLAGS += -DFASTDBL2INT 
CFLAGS += -DDEBUG_DEVEL
#CFLAGS += -DDEBUG_DEVEL_OLD
CFLAGS += -DHAVE_CONFIG_H
CFLAGS += -DASSERT_BUFFERS
#	on recent glibc-system to avoid implicit-warnings
#	for strnlen
CFLAGS += -D_GNU_SOURCE
#	on recent glibc-system this brings performance at
#	cost of size
CFLAGS += -D__USE_STRING_INLINES
#	on solaris this may be needed for some non std-things
#CFLAGS += -D__EXTENSIONS__

#
#
# End of configurable options.
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
# Internal Name - Config
#
#	internal Variables and make-targets
#	only touch if realy needed
#
#	Name of Program
MAIN = g2cd
#	Version-ding-dong
VERSION = 0.0.00.08
DISTNAME = $(MAIN)-$(VERSION)
LONGNAME = Go2CHub $(VERSION) programming Experiment

#
#	mostly all files depend on these header's
#
MY_STD_DEPS_HEADS = \
	G2MainServer.h \
	other.h \
	config.h
#
#	together with this files
#
MY_STD_DEPS = $(MY_STD_DEPS_HEADS) lib/log_facility.h lib/sec_buffer.h Makefile ccdrv
#
#	All headerfiles
#
HEADS = \
	$(MY_STD_DEPS_HEADS) \
	G2Acceptor.h \
	G2Handler.h \
	G2UDP.h \
	G2Connection.h \
	G2ConHelper.h \
	G2Packet.h \
	G2PacketSerializer.h \
	builtin_defaults.h \
	timeout.h
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
	G2Packet.c \
	G2PacketSerializer.c \
	timeout.c
SRCS = \
	$(MSRCS) \
	ccdrv.c \
	calltree.c
#	and again: with gmake ... $(wildcard *.c)
#
#	other files
#	like README CHANGELOG
#
AUX = \
	Makefile \
	gen_guid.sh \
	profile.xml \
	.mapfile \
	COPYING \
	README
#
#	files to be included in a package
#
TARED_FILES = $(HEADS) $(SRCS) $(AUX)
# normaly we would set it here, derived vom the mod-dirs, but..
TARED_DIRS =

#
#	what should be made
#
OBJS = \
	G2MainServer.o \
	G2Acceptor.o \
	G2Handler.o \
	G2UDP.o \
	G2Connection.o \
	G2ConHelper.o \
	G2Packet.o \
	G2PacketSerializer.o \
	data.o \
	timeout.o
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
	G2Packet.rtl \
	G2PacketSerializer.rtl \
	timeout.rtl
# Insert all rule high enoug, so it gets the default rule
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
%.o: %.c
	@$(COMPILE.c) $< -o $@

%.rtl: %.c
	@$(RM) $@ && $(COMPILE.c) -dr $< -o `basename $@ .rtl`.o && (mv -f `basename $@ .rtl`.c.*.rtl $@ || touch $@)
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
$(MAIN): $(OBJS) $(LIBCOMMON) .mapfile
	@./ccdrv -s$(VERBOSE) "LD[$@]" $(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBCOMMON) $(LOADLIBES) $(LDLIBS)

once: final
final: .final
.final: version.h $(LIBSRCS) $(MSRCS) $(HEADS) .mapfile
	@./ccdrv -s$(VERBOSE) "CC-LD[$(MAIN)]" $(CC) $(CFLAGS) $(CPPFLAGS) -o $(MAIN) $(MSRCS) $(LIBSRCS) $(LDFLAGS) $(LOADLIBES) $(LDLIBS) && touch $@

withzlib: .withzlib
.withzlib: $(OBJS) $(ZLIB) $(LIBCOMMON) .mapfile
	@./ccdrv -s$(VERBOSE) "LD[$(MAIN)]" $(CC) -o $(MAIN) $(OBJS) $(LDFLAGS) $(ZLIB) $(LIBCOMMON) $(LOADLIBES) $(LDLIBS_BASE) && touch $@
	
oncewithzlib686: finalwithzlib686
finalwithzlib686: .finalwithzlib686
.finalwithzlib686: version.h $(LIBSRCS) $(ZSRCS) $(ZSRCS_686) $(MSRCS) $(HEADS) .mapfile
	@./ccdrv -s$(VERBOSE) "CC-LD[$(MAIN)]" $(CC) $(CFLAGS) -DASMV -DNO_UNDERLINE $(CPPFLAGS) -o $(MAIN) $(MSRCS) $(LIBSRCS) $(ZSRCS) $(ZSRCS_686) $(LDFLAGS) $(LOADLIBES) $(LDLIBS_BASE) && touch $@

oncewithzlib: finalwithzlib
finalwithzlib: .finalwithzlib
.finalwithzlib: version.h $(LIBSRCS) $(ZSRCS) $(MSRCS) $(HEADS) .mapfile
	@./ccdrv -s$(VERBOSE) "CC-LD[$(MAIN)]" $(CC) $(CFLAGS) $(CPPFLAGS) -o $(MAIN) $(MSRCS) $(LIBSRCS) $(ZSRCS) $(LDFLAGS) $(LOADLIBES) $(LDLIBS_BASE) && touch $@

#	someday we need this, dependend on all or $(MAIN)
install:
	@$(PORT_PR) "No install at the moment!\n" && false

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

#	std-cleaning target
eclean: libclean zlibclean
	-$(RM) $(OBJS) $(RTL_DUMPS) ccdrv.o
clean: eclean
	-$(RM) $(MAIN) $(MAIN)z $(MAIN).exe $(MAIN)z.exe $(MAIN)z.c .$(MAIN).dbg ccdrv calltree callgraph.dot .final .withzlib .finalwithzlib .finalwithzlib686
distclean: libdclean zlibdclean clean
	-$(RM) version.h tags cscope.out TODO stubmakerz core gmon.out  *.bb *.bbg *.da *.i *.s *.bin *.rtl

#	generate dependencies on whish
#depend: $(DEPENDS)

#	package-building for ...
#	compression in extra step since the solaris 5.7 'tar'
#	dosn't unstand ANY compression (no, not even -Z)
#	hopefully bzip or gzip fallback funktions
dist: $(TARED_FILES)
	@$(PORT_PR) $(DISTNAME)-`$(DATE) $(DATEFLAGS)` > .fname
	@-$(RM) -rf `cat .fname`
	@$(PORT_PR) "\tPREP[`cat .fname`]\n"
	@mkdir `cat .fname`
	@for da_dir in $(TARED_DIRS) ; do mkdir `cat .fname`/$$da_dir ; done
	@for da_file in $(TARED_FILES) ; do ln $$da_file `cat .fname`/$$da_file ; done
	@$(PORT_PR) "\tTAR[$(DISTNAME)]\n"
	@tar chf `cat .fname`.tar `cat .fname`
	@-(($(PORT_PR) "\tBZIP2[$(DISTNAME).tar]\n"; bzip2 -f9 `cat .fname`.tar) || ($(PORT_PR) "\tGZIP[$(DISTNAME).tar]\n"; gzip -f9 `cat .fname`.tar) || ($(PORT_PR) "\tCOMPR[$(DISTNAME).tar]\n"; compress `cat .fname`.tar))
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
	$(PORT_PR) "\tmake install\tto install the programm (not implementet yet)\n" ; \
	$(PORT_PR) "\nPlease make sure to read the README and *manualy* configure the package,\n" ; \
	$(PORT_PR) "before compilation! (there is no autoconf at the moment)\n" ; \
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
	@$(PORT_PR) "\tCC[$@]\n"; $(HOSTCC) $(HOSTCFLAGS) ccdrv.c -o $@ -lcurses || ( \
		$(PORT_PR) "\n *****************************************************\n" ; \
		$(PORT_PR) " * compiling cc-driver failed, using fallback script *\n" ; \
		$(PORT_PR) " *****************************************************\n\n" ; \
		$(PORT_PR) "#! /bin/sh\n" > $@ ; \
		$(PORT_PR) "shift 2\n" >> $@ ; \
		$(PORT_PR) "echo \$${@}\n" >> $@ ; \
		$(PORT_PR) "CC=\$${1}\n" >> $@; \
		$(PORT_PR) "shift\n" >> $@; \
		$(PORT_PR) "\$${CC} \$${@}\n" >> $@; \
		chmod a+x $@ )

.INTERMEDIATE: data.o
data.o: $(TARED_FILES)
	@tar -cf - `find . -name zlib -prune -o -type f -a \( -name '*.c' -o -name '*.h' \) -print` | bzip2 -9 | \
	od -v -A n -t x1 | tr -s [:space:] ':' | \
	awk 'BEGIN { RS = ":"; ORS = " "; print "const char src_ctrl[]= {\n"} /[0-9a-fA-F][0-9a-fA-F]/{print "0x" $$1 ","; if ( NR % 14  == 13) printf "\n"; } END { print "\n};\nconst unsigned long src_ctrl_len = sizeof(src_ctrl);\n\n"}' | \
	$(CC) -x c - -c -o $@ 2> /dev/null || touch $@

calltree: calltree.c Makefile ccdrv
	@./ccdrv -s$(VERBOSE) "LD[$@]" $(HOSTCFLAGS) calltree.c -o $@ $(LDFLAGS)

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

#	Batch-creation of version-info
version.h: Makefile
	@$(PORT_PR)	"\tCREATE[$@]\n"; \
	$(PORT_PR)	"#ifndef _VERSION_H\n" > $@; \
	$(PORT_PR)	"#define _VERSION_H\n" >> $@; \
	$(PORT_PR)	"\n" >> $@; \
	$(PORT_PR)	"#define DIST\t\"$(DISTNAME)\"\n" >> $@; \
	$(PORT_PR)	"#define OUR_UA\t\"$(LONGNAME)\"\n" >> $@; \
	$(PORT_PR)	"#define SYSTEM_INFO\t\"" >> $@; \
	uname -a | tr -d "\n" >> $@; \
	$(PORT_PR)	"\"\n" >> $@; \
	$(PORT_PR)	"#ifdef __VERSION__\n" >> $@; \
	$(PORT_PR)	"# define COMPILER_INFO\t\"$(CC) \" __VERSION__\n" >> $@; \
	$(PORT_PR)	"#else\n" >> $@; \
	$(PORT_PR)	"# define COMPILER_INFO\t\"" >> $@; \
	$(CC) $(CC_VER_INFO) | head -n 1 | tr -d "\n" >> $@; \
	$(PORT_PR)	"\"\n" >> $@; \
	$(PORT_PR)	"#endif\n" >> $@; \
	$(PORT_PR)	"\n" >> $@; \
	$(PORT_PR)	"#endif\n" >> $@; \
	$(PORT_PR)	"//EOF\n" >> $@
#	$(PORT_PR)	"$(TOEOL)\b\b\b[OK]\n"

#	what are the .o's derived from: implicit [target].c +
#	additional dependencies, written out...
G2MainServer.o: G2Acceptor.h G2Handler.h G2UDP.h G2Connection.h builtin_defaults.h version.h lib/hzp.h lib/atomic.h
G2Acceptor.o: G2Acceptor.h G2Connection.h G2ConHelper.h lib/my_epoll.h lib/atomic.h
G2Handler.o: G2Handler.h G2Connection.h G2ConHelper.h G2Packet.h G2PacketSerializer.h lib/my_epoll.h
G2UDP.o: G2UDP.h
G2Connection.o: G2Connection.h
G2ConHelper.o: G2ConHelper.h lib/my_epoll.h lib/atomic.h G2Connection.h
G2Packet.o: G2Packet.h G2Connection.h
G2PacketSerializer.o: G2Packet.h
timeout.o: timeout.h
#	header-deps
G2MainServer.h: G2Connection.h lib/atomic.h
G2Connection.h: G2Packet.h version.h
G2ConHelper.h: G2Connection.h lib/sec_buffer.h lib/my_epoll.h
G2Packet.h: lib/sec_buffer.h

#
#	add std.-dep. to .o's, is this gmake only?
#
$(OBJS): $(MY_STD_DEPS)
$(RTL_DUMPS): $(MY_STD_DEPS)
Makefile: lib/module.make zlib/module.make

#$(DEPENDS): Makefile

# 	include all dependencies
#include $(DEPENDS)

#
#
# End of rules.
###############################################################
# EOF