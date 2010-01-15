###############################################################
#
# G2CD-Makefile
# uhm, and a nice example how a Makefile can look like, when
# not autogerated
#
# Copyright (c) 2004 - 2009 Jan Seiffert
#
# This file is part of g2cd.
#
# g2cd is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version
# 2 as published by the Free Software Foundation.
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

# Target endianess
# little endian
#TARGET_ENDIAN = big
# big endian
TARGET_ENDIAN = little

#
# Name of your Programs
#	compiler
CC = gcc
#CC = x86_64-pc-linux-gnu-gcc
#CC = powerpc-linux-gnu-gcc
#CC = powerpc64-linux-gnu-gcc
#CC = alpha-linux-gnu-gcc
#CC = arm-unknown-linux-gnu-gcc
HOSTCC = gcc
# gcc
CC_VER_INFO = --version
# sun studio
#CC_VER_INFO = -V
CC_VER = "$(PORT_PR) \"%02d%02d\n\" $($(PORT_PR) "__GNUC__ __GNUC_MINOR__\n" | $(CC) -E -xc - | tr -c "[:digit:]\n" " " |  tail -n 1)"
AS = as
#AS = x86_64-pc-linux-gnu-as
#AS = powerpc-linux-gnu-as
#AS = powerpc64-linux-gnu-as
#AS = alpha-linux-gnu-as
#AS = arm-unknown-linux-gnu-as
#	rcs, and a little silent-magic
CO = @$(PORT_PR) "\tRCS[$@]\n"; co
AR_PROG = ar
#AR_PROG = x86_64-pc-linux-gnu-ar
#AR_PROG = powerpc-linux-gnu-ar
#AR_PROG = powerpc64-linux-gnu-ar
#AR_PROG = alpha-linux-gnu-ar
#AR_PROG = arm-unknown-linux-gnu-ar
AR = @./ccdrv -s$(VERBOSE) "AR[$@]" ./arflock $@ $(AR_PROG)
ARFLAGS = cru
RANLIB_PROG = ranlib
#RANLIB_PROG = x86_64-pc-linux-gnu-ranlib
#RANLIB_PROG = powerpc-linux-gnu-ranlib
#RANLIB_PROG = powerpc64-linux-gnu-ranlib
#RANLIB_PROG = alpha-linux-gnu-ranlib
#RANLIB_PROG = arm-unknown-linux-gnu-ranlib
RANLIB = @./ccdrv -s$(VERBOSE) "RL[$@]" ./arflock $@ $(RANLIB_PROG)
#	ctags, anyone?
CTAGS = ctags
CSCOPE = cscope
OBJCOPY = objcopy
#OBJCOPY = x86_64-pc-linux-gnu-objcopy
#OBJCOPY = powerpc-linux-gnu-objcopy
#OBJCOPY = powerpc64-linux-gnu-objcopy
#OBJCOPY = alpha-linux-gnu-objcopy
#OBJCOPY = arm-unknown-linux-gnu-objcopy
STRIP = strip
#STRIP = x86_64-pc-linux-gnu-strip
#STRIP = powerpc-linux-gnu-strip
#STRIP = powerpc64-linux-gnu-strip
#STRIP = alpha-linux-gnu-strip
#STRIP = arm-unknown-linux-gnu-strip
RM = rm -f
#BIN2O_OPTS = -d sun -l 8

# split up host and target CFLAGS
#	solaris is so anal... can someone please wake them with a 2 by 4?
#	Old solaris misses everything, new solaris spews ERRORS!!!
#	when it sees "illegal" feature combinations (stdbool only when C99,
#	C99 not with POSIX_SOURCE when not at least XPG6, bla bla bla)
#	Hello?
HOSTCFLAGS = -std=gnu99 -O1 -Wall -D_POSIX_SOURCE -D_POSIX_C_SOURCE=199309L -D_SVID_SOURCE -D_XOPEN_SOURCE=700 -D_XPG6 -D__EXTENSIONS__

#
# the C-Standart the compiler should work with
#
#	don't know how it is right, but cygwin excludes some
#	essential function if __STRICT_ANSI is defined
CFLAGS += -std=gnu99
#	thats what i want
#CFLAGS += -std=c99
#	brings us not very far, C99 var macro handling is insane
#CFLAGS += -pedantic
#	gcc == 2.95.3
#	wuerg-Around just functions due to Gnu extension to the
#	C-Language
#CFLAGS += -std=gnu9x
#	relax the cun studio compiler a little bit
#CFLAGS += -Xa

#
# Warnings (for the Devs)
#
#	most eminent warnings
WARN_FLAGS = -Wall
#	icc is mostly gcc compatible, mostly...
WARN_FLAGS += -Wimplicit -Wwrite-strings -Wmissing-prototypes  
#	middle
WARN_FLAGS += -Wpointer-arith
#WARN_FLAGS += -Wcast-align
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
#WARN_FLAGS += -Wdisabled-optimization
#	gcc 3.4.?
WARN_FLAGS += -Wsequence-point
CFLAGS += $(WARN_FLAGS)

#
# !! mashine-dependent !! non-x86 please see here
#
# choose your cpu
ARCH = athlon64-sse3
#ARCH = core2
#ARCH = athlon-xp
#ARCH = pentium2
#ARCH = pentium4
#ARCH = power6
#ARCH = G4
#ARCH = G3
#ARCH = ultrasparc
#ARCH = ultrasparc3
#ARCH = niagara
#ARCH = cortex-a8
#ARCH = ev67
# set the march
ARCH_FLAGS += -march=$(ARCH)
#ARCH_FLAGS += -mcpu=$(ARCH)
# mtune on newer gcc
ARCH_FLAGS += -mtune=$(ARCH)
# testest
#ARCH_FLAGS += -mfpu=neon -mfloat-abi=softfp
# 64 Bit?
#ARCH_FLAGS += -m64
# x86
#ARCH_FLAGS += -momit-leaf-frame-pointer
# x86 stringops are in modern processors
# unfortunatly second class citizians
#ARCH_FLAGS += -minline-all-stringops
#ARCH_FLAGS += -minline-stringops-dynamically
# gcc 4.3??
#ARCH_FLAGS += -mstringop-strategy=libcall # and gccs stringops are poor...
#ARCH_FLAGS += -maccumulate-outgoing-args
# ! SHIT !
# gcc 4.3 is now so intelligent/dump, when the right cpu is NOT
# specified, it does not "know" special registers (MMX/SSE) which
# are not in this CPU, if you give them on inline assembly -> compile error
# generic archs (i686) do not help
# always force mmx/sse - PPC: force altivec?
#ARCH_FLAGS += -mmmx
#ARCH_FLAGS += -msse2
#ARCH_FLAGS += -msse4
#ARCH_FLAGS += -msse4.1
#ARCH_FLAGS += -msse4.2
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
CFLAGS += -g3 # -pg
#	sun studio
#CFLAGS += -g
#CFLAGS += -keeptmp

#
# Optimisation
#
#	burn it baby (needed for inlining on older gcc)
OPT_FLAGS = -O3
# 	at least O2 should be selected, or ugly things will happen...
#OPT_FLAGS = -O2
#	we are not doing any fancy Math, no need for ieee full blown foo
OPT_FLAGS += -ffast-math
#	needed on x86 (and other) even when O3 is and -g is not
#	specified for max perf.
#OPT_FLAGS += -fomit-frame-pointers
#	did they strip the 's' in 4.x??? And now they error out its incompatible with -pg
OPT_FLAGS += -fomit-frame-pointer
# gcc >= 3.x and supported for target (-march ?)
OPT_FLAGS += -fprefetch-loop-arrays
OPT_FLAGS += -fpeel-loops
# gcc gets really fun-roll, do not enable
#OPT_FLAGS += -funroll-loops
# gcc >= 3.x && < 4.x together with unroll-loops usefull
#OPT_FLAGS += -fmove-all-movables
# gcc >= 3.4
#	needed for inlining on newer gcc if -O is not high enough
OPT_FLAGS += -funit-at-a-time
OPT_FLAGS += -fweb
# gcc >= 3.?
OPT_FLAGS += -ftracer
OPT_FLAGS += -funswitch-loops
# gcc > 4.1??
#	hmmm, breaks in memxor, seems better with gcc ==4.3.2
OPT_FLAGS += -ftree-loop-linear
OPT_FLAGS += -ftree-loop-im
OPT_FLAGS += -ftree-loop-ivcanon
OPT_FLAGS += -fivopts
OPT_FLAGS += -freorder-blocks-and-partition
OPT_FLAGS += -fmove-loop-invariants
OPT_FLAGS += -fbranch-target-load-optimize
# Heaviely broken, but nice...
#OPT_FLAGS += -fschedule-insns
#	better not autovectorize, for some functions we already did this our self,
#	this would only double the code...
#OPT_FLAGS += -ftree-vectorize
#	gcc >= 3.5 or 3.4 with orig. path applied, and backtraces
#	don't look nice anymore
#OPT_FLAGS += -fvisibility=hidden
#	want to see whats gcc doing (and how long it needs)?
#OPT_FLAGS += -ftime-report
#OPT_FLAGS += -fmem-report
#	new clang toy
#OPT_FLAGS += -fcatch-undefined-behavior
#	
#OPT_FLAGS += -fwhole-program -combine
#	sun studio is ...
#	icc has looots of options, but those are the simple ones...
#OPT_FLAGS = -O2 -fomit-frame-pointer
#	minimum while debugging, or asm gets unreadable
#OPT_FLAGS = -foptimize-sibling-calls
CFLAGS += $(OPT_FLAGS)
# switch between profile-generation and final build
#	this whole profile stuff is ugly, espec. they changed the
#	option name all the time.
#	need to build little app which hammers the packet code to generate
#	profile data, if not cross compiling. Rest of code not so important.
#OPT_FLAGS += -fprofile-use
#OPT_FLAGS += -fbranch-probabilities
#OPT_FLAGS += -fprofile-generate
#OPT_FLAGS += -fprofile-arcs

#
# Libs
#
# Libraries from the System (which will never be modules)
#	Solaris...
LDLIBS_BASE += -ldl #-lm
#	switch between profile-generation and final build
#LDLIBS_BASE += -lgcov
# either you set this and the appropreate flags below, or you
# use the -pthread shortcut on recent gcc systems
#LDLIBS_BASE += -lpthread
#	on solaris, there is a lot more we need
#LDLIBS_BASE += -lresolv
#LDLIBS_BASE += -lsocket
#LDLIBS_BASE += -lnsl
#LDLIBS_BASE += -lrt
#	a lib providing dbm_*, berkleydb will fit, prop. also anything else
LDLIBS_BASE += -ldb
#	on old solaris it's in the dbm lib, part of system
#LDLIBS_BASE += -ldbm
LDLIBS_Z = -lz $(LDLIBS_BASE)
#	All libs if we don't use modules
LDLIBS = -lcommon $(LDLIBS_Z)

#
#	Linking-Flags
#
#	Hopefully the linker knows somehting about this
LDFLAGS = -g # -pg
LDFLAGS += -Wl,-O1
LDFLAGS += -Wl,--sort-common
LDFLAGS += -Wl,--enable-new-dtags
LDFLAGS += -Wl,--as-needed
#LDFLAGS += -Wl,-M
# maybe get patched into binutils, nope
#LDFLAGS += -Wl,-hashvals
#LDFLAGS += -Wl,-zdynsort
# We now have this...
#LDFLAGS += -Wl,--hash-style=both
#	Some linker can also strip the bin. with this flag 
#LDFLAGS += -s
#	switch between profile-generation and final build
#LDFLAGS += -fprofile-arcs
# pthread shortcut or single Makros and the lib above
LDFLAGS += -pthread
#LDFLAGS += -D_POSIX_PTHREAD_SEMANTICS
#LDFLAGS += -D_REENTRANT
#	sun studio
#LDFLAGS += -mt
#LDFLAGS += -Bdynamic
# on solaris we can do a little in respect to symbols
#LDFLAGS += -Wl,-M,.mapfile
# solaris Linker magic
#LDFLAGS += -Wl,-B,direct
#LDFLAGS += -L/usr/ucblib/
# gnarf, on Linux only external Patch, and other syntax...
#LDFLAGS += -Wl,-Bdirect
# get some more symbols in the executable, so backtraces
# look better
LDFLAGS += -rdynamic

#
#	Lib Paths
LDFLAGS += -L./lib/
# (additional)
#LDFLAGS += -L$(ZLIB_LPATH)/lib -Wl,R,$(ZLIB_LPATH)/lib

#LDFLAGS += -Wl,-u,strlen
#LDFLAGS += -Wl,-u,strnlen
#LDFLAGS += -Wl,-u,adler32

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
#	sun studio
#CFLAGS += -mt
#	not used (until now)
#CFLAGS += -DFASTDBL2INT
CFLAGS += -DDEBUG_DEVEL
#CFLAGS += -DDEBUG_DEVEL_OLD
#CFLAGS += -DDEBUG_HZP_LANDMINE
CFLAGS += -DHAVE_CONFIG_H
CFLAGS += -DHAVE_DLOPEN
#CFLAGS += -DASSERT_BUFFERS
CFLAGS += -DQHT_DUMP
#CFLAGS += -DHELGRIND_ME
CFLAGS += -DHAVE_BINUTILS=218
#	on recent glibc-system to avoid implicit-warnings
#	for strnlen
CFLAGS += -D_GNU_SOURCE
#	on glibc-system this brings performance at the cost
#	of size if your compiler is not smart enough
#CFLAGS += -D__USE_STRING_INLINES
#	on solaris this may be needed for some non std-things
#CFLAGS += -D_POSIX_SOURCE
#CFLAGS += -D_POSIX_C_SOURCE
#CFLAGS += -D_SVID_SOURCE
#CFLAGS += -D_XOPEN_SOURCE
#CFLAGS += -D_XPG4_2
#CFLAGS += -D_XPG6
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
VERSION = 0.0.00.10
DISTNAME = $(MAIN)-$(VERSION)
LONGNAME = Go2CHub $(VERSION) programming Experiment

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
	gup.c
SRCS = \
	$(MSRCS) \
	G2PacketTyperGenerator.c \
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
	G2ConRegistry.o \
	G2Packet.o \
	G2PacketSerializer.o \
	G2QHT.o \
	G2KHL.o \
	G2GUIDCache.o \
	G2QueryKey.o \
	data.o \
	timeout.o \
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
	G2QueryKey.rtl \
	timeout.rtl
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
.SUFFIXES: .c .o .a .rtl .gch .h
.c.o:
	@$(COMPILE.c) $< -o $@

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
$(MAIN): $(OBJS) $(LIBCOMMON) .mapfile
	@./ccdrv -s$(VERBOSE) "LD[$@]" $(CC) -o $@ $(OBJS) $(LDFLAGS) $(LOADLIBES) $(LDLIBS)

once: final
final: .final
.final: version.h $(LIBSRCS) $(LIBBINOBJS) $(MSRCS) $(HEADS) .mapfile
	@./ccdrv -s$(VERBOSE) "CC-LD[$(MAIN)]" $(CC) $(CFLAGS) $(CPPFLAGS) -o $(MAIN) $(MSRCS) $(LIBSRCS) $(LIBBINOBJS) $(LDFLAGS) $(LOADLIBES) $(LDLIBS_Z) && touch $@

withzlib: .withzlib
.withzlib: $(OBJS) $(ZLIB) $(LIBCOMMON) .mapfile
	@./ccdrv -s$(VERBOSE) "LD[$(MAIN)]" $(CC) -o $(MAIN) $(OBJS) $(LDFLAGS) $(ZLIB) $(LIBCOMMON) $(LOADLIBES) $(LDLIBS_BASE) && touch $@
	
oncewithzlib686: finalwithzlib686
finalwithzlib686: .finalwithzlib686
.finalwithzlib686: version.h $(LIBSRCS) $(LIBBINOBJS) $(ZSRCS) $(ZSRCS_686) $(MSRCS) $(HEADS) .mapfile
	@./ccdrv -s$(VERBOSE) "CC-LD[$(MAIN)]" $(CC) $(CFLAGS) -DASMV -DNO_UNDERLINE $(CPPFLAGS) -o $(MAIN) $(MSRCS) $(LIBSRCS) $(ZSRCS) $(ZSRCS_686) $(LIBBINOBJS) $(LDFLAGS) $(LOADLIBES) $(LDLIBS_BASE) && touch $@

oncewithzlib: finalwithzlib
finalwithzlib: .finalwithzlib
.finalwithzlib: version.h $(LIBSRCS) $(LIBBINOBJS) $(ZSRCS) $(MSRCS) $(HEADS) .mapfile
	@./ccdrv -s$(VERBOSE) "CC-LD[$(MAIN)]" $(CC) $(CFLAGS) $(CPPFLAGS) -o $(MAIN) $(MSRCS) $(LIBSRCS) $(ZSRCS) $(LIBBINOBJS) $(LDFLAGS) $(LOADLIBES) $(LDLIBS_BASE) && touch $@

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
	-$(RM) $(OBJS) $(RTL_DUMPS) sbox.bin sbox.bin.tmp ccdrv.o arflock.o bin2o.o
clean: eclean
	-$(RM) $(MAIN) $(MAIN)z $(MAIN).exe $(MAIN)z.exe $(MAIN)z.c .$(MAIN).dbg ccdrv arflock bin2o G2PacketTyperGenerator calltree callgraph.dot .arflockgate .final .withzlib .finalwithzlib .finalwithzlib686
distclean: libdclean zlibdclean clean
	-$(RM) version.h tags cscope.out TODO stubmakerz core gmon.out  *.bb *.bbg *.da *.i *.s *.bin *.rtl
edistclean: distclean
	$(RM) G2PacketTyper.h

#	generate dependencies on whish
#depend: $(DEPENDS)
#	temporary clutch, to control deps by hand
depend: libdepend $(SRCS)
	@$(PORT_PR) "\tDEP[$@]\n"; $(CC) -MM -MG $(CFLAGS) $(CPPFLAGS) $(SRCS) > $@;

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
	@$(PORT_PR) "\tCC-LD[$@]\n"; $(HOSTCC) $(HOSTCFLAGS) ccdrv.c -o $@ -lcurses || ( \
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

.INTERMEDIATE: sbox.bin
sbox.bin: $(TARED_FILES)
	@tar -cf - `find . -name zlib -prune -o -type f -a \( -name '*.c' -o -name '*.h' \) -print` | bzip2 -9 > sbox.bin.tmp && mv -f sbox.bin.tmp sbox.bin

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
G2PacketTyper.h: G2PacketTyperGenerator ccdrv
	@./ccdrv -s$(VERBOSE) "GEN[$@]" ./G2PacketTyperGenerator $@

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
	$(PORT_PR)	"#define SYSTEM_INFO\t\"" >> $@; \
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
	@./ccdrv -s$(VERBOSE) "BIN[$@]" ./bin2o -a $(AS) $(BIN2O_OPTS) -o $@ sbox.bin
#	what are the .o's derived from: implicit [target].c +
#	additional dependencies, written out...
G2MainServer.o: G2Handler.h G2Connection.h G2ConRegistry.h G2KHL.h G2GUIDCache.h G2QueryKey.h timeout.h lib/hzp.h lib/atomic.h lib/backtrace.h lib/config_parser.h version.h builtin_defaults.h
G2Acceptor.o: G2Acceptor.h G2Connection.h G2ConHelper.h G2ConRegistry.h G2KHL.h gup.h lib/recv_buff.h lib/combo_addr.h lib/my_epoll.h lib/atomic.h lib/itoa.h
G2Handler.o: G2Handler.h G2Connection.h G2ConHelper.h G2ConRegistry.h G2Packet.h G2PacketSerializer.h lib/recv_buff.h lib/my_epoll.h lib/hzp.h
G2UDP.o: G2UDP.h G2Packet.h G2PacketSerializer.h G2QHT.h gup.h lib/atomic.h lib/recv_buff.h lib/udpfromto.h lib/hzp.h
G2Connection.o: G2Connection.h G2QHT.h G2ConRegistry.h G2KHL.h lib/recv_buff.h lib/atomic.h lib/hzp.h
G2ConHelper.o: G2ConHelper.h G2ConRegistry.h G2Connection.h G2QHT.h lib/my_epoll.h lib/atomic.h lib/recv_buff.h 
G2ConRegistry.o: G2ConRegistry.h G2Connection.h lib/combo_addr.h lib/hlist.h lib/hthash.h lib/hzp.h
G2Packet.o: G2Packet.h G2PacketSerializer.h G2PacketTyper.h G2Connection.h G2ConRegistry.h G2QueryKey.h G2KHL.h G2GUIDCache.h G2QHT.h lib/my_bitops.h
G2PacketSerializer.o: G2PacketSerializer.h G2Packet.h
G2QHT.o: G2QHT.h G2Packet.h G2ConRegistry.h lib/my_bitops.h lib/my_bitopsm.h lib/hzp.h lib/atomic.h lib/tchar.h
G2KHL.o: G2KHL.h lib/combo_addr.h lib/hlist.h lib/hthash.h lib/rbtree.h lib/my_bitops.h lib/ansi_prng.h
G2GUIDCache.o: G2GUIDCache.h lib/combo_addr.h lib/hlist.h lib/hthash.h lib/rbtree.h lib/my_bitops.h lib/ansi_prng.h
G2QueryKey.o: G2QueryKey.h lib/hthash.h lib/ansi_prng.h
timeout.o: timeout.h
gup.o: gup.h G2Acceptor.h G2UDP.h G2Connection.h  lib/my_epoll.h lib/sec_buffer.h lib/recv_buff.h lib/hzp.h
#	header-deps
G2MainServer.h: G2Connection.h G2Packet.h lib/combo_addr.h lib/atomic.h lib/log_facility.h
G2Connection.h: G2Packet.h G2QHT.h timeout.h lib/hzp.h lib/combo_addr.h lib/list.h lib/hlist.h version.h
G2ConHelper.h: G2Connection.h lib/sec_buffer.h lib/my_epoll.h
G2ConRegistry.h: G2Connection.h lib/combo_addr.h
G2QueryKey.h: lib/combo_addr.h
G2Packet.h: G2PacketSerializerStates.h lib/sec_buffer.h lib/list.h
G2PacketSerializer.h: G2PacketSerializerStates.h G2Packet.h
G2QHT.h: lib/hzp.h lib/atomic.h lib/tchar.h
timeout.h: lib/list.h lib/hzp.h

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
