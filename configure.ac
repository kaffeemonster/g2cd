#
# configure.ac
# autoconf input for our foo
#
# Copyright (c) 2010-2011 Jan Seiffert
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

AC_PREREQ([2.65])
AC_INIT([g2cd], [0.0.00.11], [kaffeemonster@googlemail.com])
AC_CANONICAL_HOST
AC_CONFIG_SRCDIR([G2MainServer.c])
AC_CONFIG_HEADERS([config_auto.h])

strerror_mtsafe=no
default_hate_cbranch=no
default_enable_unaligned_ok=no
default_disable_bit_instr=yes
default_enable_release=no
disable_dyndispatch=no
disable_backtraces=no
disable_dbus=no
disable_hw_mult=no
disable_smp=no
disable_asm=no
disable_v9=no
disable_vis=no
disable_vis2=no
use_mapfiles=no
disable_warnings=no
my_enable_debug=no
my_enable_release=no

case "$host" in
	*powerpc-*)
		default_disable_bit_instr=no;
		default_hate_cbranch=yes;
		default_enable_unaligned_ok=yes;;
	*powerpc64-*)
		default_disable_bit_instr=no;
		default_hate_cbranch=yes;
		default_enable_unaligned_ok=yes;;
	*i?86-*)
		default_disable_bit_instr=no;
		default_enable_unaligned_ok=yes;;
	*x86_64-*)
		default_disable_bit_instr=no;
		default_enable_unaligned_ok=yes;;
	*arm*)
		default_disable_bit_instr=no;;
	*mips*)
		default_disable_bit_instr=no;;
	*alpha*)
# this is a bit more complicated:
# alpha has the cix extention, a cpu may have it or not...
		default_disable_bit_instr=no;;
	*ia64*)
# ia64 has a popcnt, gcc can build a cntlz or something like
# that. If it's fast...
		default_disable_bit_instr=no;;
	*)
		;;
esac

case "$host" in
	*-solar*)
# Old Solaris does not have strerror_r, instaed strerror is thread save
		strerror_mtsafe=yes;
# sysmbol visability only on later, because the linker can hide the
# symbols with a mapfile
		use_mapfile=yes;;
	*win32*)
		disable_dbus=yes;
		disable_backtraces=yes;;
	*mingw*)
		disable_dbus=yes;
		disable_backtraces=yes;;
	*)
		;;
esac

AC_ARG_WITH([warnings], [AS_HELP_STRING([--without-warnings],
            [disable compile time warnings])],
            [disable_warnings="$witheval"], [])
AC_ARG_WITH([backtraces], [AS_HELP_STRING([--without-backtraces],
            [do you want backtraces on crash? (This is hacky, but is often the only helper on crash)])],
            [disable_backtraces="$witheval"], [])
AC_ARG_WITH([dbus], [AS_HELP_STRING([--without-dbus],
            [do you want a dbus interface])],
            [disable_dbus="$witheval"], [])
AC_ARG_ENABLE([debug], [AS_HELP_STRING([--enable-debug],
              [make a debug build])],
              [my_enable_debug=yes], [])
AC_ARG_ENABLE([unaligned], [AS_HELP_STRING([--enable-unaligned],
              [your arch can do unaligned mem access. Do not enable if complicated software fixups make this possible])],
              [enable_unaligned_ok=yes], [enable_unaligned_ok=$default_enable_unaligned_ok])
AC_ARG_ENABLE([bit-instr], [AS_HELP_STRING([--disable-bit-instr],
              [your arch lacks certain bit manipulation instruction, clz and the like, sometimes other instructions can make up for it, but only if they are fast])],
              [disable_bit_instr=yes], [disable_bit_instr=$default_disable_bit_instr])
AC_ARG_ENABLE([hw-mult], [AS_HELP_STRING([--disable-hw-mult],
              [your arch has no multiply instruction, or the instruction is slow])],
              [disable_hw_mult=yes], [])
AC_ARG_ENABLE([smp], [AS_HELP_STRING([--disable-smp],
              [disable smp safety, only do this if you are 110% sure the code will only run on an UP machine])],
              [disable_smp=yes], [])
AC_ARG_ENABLE([asm], [AS_HELP_STRING([--disable-asm],
              [disable assembler code])],
              [disable_asm=yes], [])
case "$host" in
	*i?86-*)
	AC_ARG_ENABLE([dyndispatch], [AS_HELP_STRING([--disable-dyndispatch],
	              [disable dynamic code patching to dispatch to cpu dependent functions])],
	              [disable_dyndispatch=yes], []);;
	*x86_64-*)
	AC_ARG_ENABLE([dyndispatch], [AS_HELP_STRING([--disable-dyndispatch],
	              [disable dynamic code patching to dispatch to cpu dependent functions])],
	              [disable_dyndispatch=yes], []);;
	*sparc*)
	AC_ARG_ENABLE([sparcv9], [AS_HELP_STRING([--disable-sparcv9],
	              [disable sparcv9 code, only do this if you really do _not_ have a v9, which means < UltraSPARC/SPARC64])],
	              [disable_v9=yes], [])
	AC_ARG_ENABLE([sparcvis], [AS_HELP_STRING([--disable-sparcvis],
	              [disable sparc VIS code, if you have an >= UltraSPARC keep enabled, except for single FPU chips (Niagara, etc.)])],
	              [disable_vis=yes], [])
	AC_ARG_ENABLE([sparcvis2], [AS_HELP_STRING([--disable-sparcvis2],
	              [disable sparc VIS2 code, same as for VIS, but >= UltraSPARC III])],
	              [disable_vis2=yes], []);;
	*)
		;;
esac

AC_ARG_ENABLE([release], [AS_HELP_STRING([--enable-release], [compile for real, or for local development])],[
              if test x"$enableval" = xno; then
                  my_enable_release=no
              else
                  my_enable_release=yes
              fi
              ], [my_enable_release=no])

AC_MSG_CHECKING([wheter we are making a release build])
if test x"$my_enable_release" = xyes; then
	AC_DEFINE(WE_ARE_MAKING_A_RELEASE, 1, a release)
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([wheter we should disable backtrace generation])
if test x"$disable_backtraces" = xno; then
# this is a call for trouble, but helpfull
	AC_DEFINE(WANT_BACKTRACES, 1, do we want to generate backtraces)
	AC_MSG_RESULT([no])
else
	AC_MSG_RESULT([yes])
fi

AC_MSG_CHECKING([wheter the arch does hate conditional branches])
if test "$default_hate_cbranch" = yes; then
# your arch is a little picky 'bout lots of conditional branches?
	AC_DEFINE(I_HATE_CBRANCH, 1, arch does not like conditional branches)
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([wheter we can do unaligned mem access])
if test x"$enable_unaligned_ok" = xyes; then
# Is unaligned access on this machine ok.
# OK here is a little bit complicated. Your machine should be
# able to access unaligned memory possitions with "good" speed.
# Either native, or with a special instruction, doesn't matter.
#
# But PLEASE make sure to not set it if:
# - unaligned access is emulated by a exception handler
# - there is "special" unaligned handling, but it is still
#   complicated (ex.: Alpha, ARM, etc.)
# Or it will kill performance, because this turns a profitable
# optimisation into the exact oposite.
#
# Setting this totally wrong may lead to SIGBUS
	AC_DEFINE(UNALIGNED_OK, 1, arch can do unalinged access ok)
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([wheter we should disable bit instructions])
if test x"$disable_bit_instr" = xno; then
# Does your arch has bit foo instructions?
# We sometimes need to find the index of a bit. This can be
# done fast and cheap if your arch has instructions to scan
# for a bit (bsf, cntlz). popcnt would also work, but is a
# bad idea if it is slow like on sparc (50 times slower than
# our generic bitmagic). Sparclite/Sparclet have a scan
# instruction (ffs), it might help.
	AC_DEFINE(HAVE_BIT_INSTR, 1, arch has bit manipulation instruction)
	AC_MSG_RESULT([no])
else
	AC_MSG_RESULT([yes])
fi

AC_MSG_CHECKING([wheter we should avoid multiplications])
if test x"$disable_hw_mult" = xno; then
# Does your arch has a fast multiplication?
# We misuse the multiplication as a fast mixing func for
# hashes. A slow mull or an hardware emulated (old sparc
# and its hw aided mull) does not help in this case and
# another aproach (add, shifts, xor, etc.) can be faster.
	AC_DEFINE(HAVE_HW_MULT, 1, arch has a fast hardware multiplier)
	AC_MSG_RESULT([no])
else
	AC_MSG_RESULT([yes])
fi

AC_MSG_CHECKING([wheter we can disable smp safety])
if test x"$disable_smp" = xno; then
# are we on an multi-processor-mashine
# (even virtual processors)
	AC_DEFINE(HAVE_SMP, 1, arch has smp)
	AC_MSG_RESULT([no])
else
	AC_MSG_RESULT([yes])
fi

AC_MSG_CHECKING([wheter we should disable assembler code])
if test x"$disable_asm" = xno; then
# do we want to use my inline-asm's
	AC_DEFINE(I_LIKE_ASM, 1, build the asm foo)
	AC_MSG_RESULT([no])
else
	AC_MSG_RESULT([yes])
fi

case "$host" in
	*i?86-*)
		AC_MSG_CHECKING([wheter we should disable dynamic code dispatch])
		if test x"$disable_dyndispatch" = xyes; then
			AC_DEFINE(USE_OLD_DISPATCH, 1, do not use dynamic code patching)
			AC_MSG_RESULT([yes])
		else
			AC_MSG_RESULT([no])
		fi
		;;
	*x86_64-*)
		AC_MSG_CHECKING([wheter we should disable dynamic code dispatch])
		if test x"$disable_dyndispatch" = xyes; then
			AC_DEFINE(USE_OLD_DISPATCH, 1, do not use dynamic code patching)
			AC_MSG_RESULT([yes])
		else
			AC_MSG_RESULT([no])
		fi
		;;
	*sparc*)
		AC_MSG_CHECKING([wheter we should disable SPARCv9 code])
		if test x"$disable_v9" = xno; then
# Solaris likes to hide the capabilities of their Sparc
# v9 behind silly games of 32 bit compat and "user space
# is better of 32 Bit, 64 Bit is for databases", so if not
# forcing 64 bit builds, everthing looks like a v8
# even if you have a v9 (issued with the UltraSPARC in
# 1995, more than a dekade in IT ago).
# This way you miss new "real" atomic instr. and can do
# 64 bit arith. just fine. If only the program knew...
# So here is a switch, only kill it if you are certain
# you have a real old Sparc.
			AC_DEFINE(HAVE_REAL_V9, 1, the cpu is act. a v9)
			AC_MSG_RESULT([no])
		else
			AC_MSG_RESULT([yes])
		fi

		AC_MSG_CHECKING([wheter we should disable SPARC VIS code])
		if test x"$disable_vis" = xno; then
# You have an UltraSPARC with VIS?
# The anwser is prop. yes, see above.
# Still you may want to disable the VIS code, for example for
# the Niagara T1. VIS is part of the FPU (now called FGU),
# and Niagara has 32 cores, which all share a single FPU.
# So on such CPUs it would be faster if the 32 cores are
# busy coping and doing stuff, even bytewise, than all waiting
# on one FPU.
			AC_DEFINE(HAVE_VIS, 1, and we have VIS)
			AC_MSG_RESULT([no])
		else
			AC_MSG_RESULT([yes])
		fi

		AC_MSG_CHECKING([wheter we should disable SPARC VIS2 code])
		if test x"$disable_vis2" = xno; then
# You have an UltraSparc III or better (~2001)?
# don't know whats up with the Fujitsu SPARC64 III/IV/V etc.
			AC_DEFINE(HAVE_VIS2, 1, and we have VIS2)
			AC_MSG_RESULT([no])
		else
			AC_MSG_RESULT([yes])
		fi
		;;
	*)
		;;
esac


AC_USE_SYSTEM_EXTENSIONS
# Checks for programs.
AC_PROG_CC
m4_include([m4/acx_pthread.m4])
case "$host" in
	*win32*)
		;;
	*mingw*)
		;;
	*)
		ACX_PTHREAD
		# we are a pthread program, make the result the default
		LIBS="$PTHREAD_LIBS $LIBS"
		CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
		LDFLAGS="$LDFLAGS $PTHREAD_CFLAGS"
		CC="$PTHREAD_CC";;
esac
# now check the rest
AC_PROG_CPP
AC_PROG_CC_C99
# actually we don't wan't real C99, because cygwin excludes some
# essential function if __STRICT_ANSI is defined, luckily autoconf
# enables gnu99 mode
# the sun studio compiler would need a little bit relaxaciton:
# CFLAGS += -Xa
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_CHECK_TOOL([AS], [as], [as])
AC_CHECK_TOOL([AR], [ar], [ar])
AC_CHECK_TOOL([OBJCOPY], [objcopy], [objcopy])
AC_CHECK_TOOL([STRIP], [strip], [strip])

if test x"$build" = x"$host"
then
# hopefully not cross compiling...
	HOSTCC="$CC"
else
	HOSTCC="cc"
fi
AC_SUBST(HOSTCC)

test -n "$ARFLAGS" || ARFLAGS="cru"
AC_SUBST(ARFLAGS)

# TODO: some nice tests instead of a switch...
case "$host" in
	*win32*)
		BIN2O_OPTS="-d coff";;
	*mingw*)
		BIN2O_OPTS="-d coff";;
	*coff*)
		BIN2O_OPTS="-d coff";;
	*-solar*)
		BIN2O_OPTS="-d sun -l 8";
# the USE_SYSTEM_EXTENTIONs is not strong enough
# at least for solars i know some powabunga...
		AC_DEFINE(_POSIX_SOURCE, 1, because...)
		AC_DEFINE(_POSIX_C_SOURCE, 1, because...)
		AC_DEFINE(_SVID_SOURCE, 1, because...)
		AC_DEFINE(_XOPEN_SOURCE, 1, because...)
		AC_DEFINE(_XPG4_2, 1, because...)
		AC_DEFINE(_XPG6, 1, because...);;
	*arm*)
		BIN2O_OPTS="-d sun -l 8";;
	*)
# is cygwin also coff?
		;;
esac
AC_SUBST(BIN2O_OPTS)

m4_include([m4/ax_check_linker_flags.m4])
# get some more symbols in the executable, so backtraces
# look better
AX_CHECK_LINKER_FLAGS([-rdynamic],
                      [LDFLAGS="-rdynamic $LDFLAGS"],
                      [AX_CHECK_LINKER_FLAGS([-Bdynamic],
                                             [LDFLAGS="-Bdynamic $LDFLAGS"], [])])
# the solaris linker can optimize the link some more...
AX_CHECK_LINKER_FLAGS([-Wl,-B,direct],
                      [LDFLAGS="-Wl,-B,direct $LDFLAGS"],
                      [])
# check for DEP/ASLR/LAA, yes, i know, they are PE specific...
AX_CHECK_LINKER_FLAGS([-Wl,--dynamicbase],
                      [LDFLAGS="-Wl,--dynamicbase $LDFLAGS"],
                      [])
AX_CHECK_LINKER_FLAGS([-Wl,--nxcompat],
                      [LDFLAGS="-Wl,--nxcompat $LDFLAGS"],
                      [])
AX_CHECK_LINKER_FLAGS([-Wl,--large-address-aware],
                      [LDFLAGS="-Wl,--large-address-aware $LDFLAGS"],
                      [])

if test x"$use_mapfiles" = xyes; then
	t_file_name=`mktemp`
	echo "{};" >> "$t_file_name"
	AX_CHECK_LINKER_FLAGS([-Wl,-M,"$t_file_name"],
	                      [LDFLAGS="-Wl,-M,.mapfile $LDFLAGS"],
	                      [])
	rm -f "$t_file_name"
fi

m4_include([m4/ax_check_compiler_flags.m4])
if test x"$my_enable_debug" = xno; then
# give a switch for whole program opt
# OPT_FLAGS += -fwhole-program -combine
# switch between profile-generation and final build
#	this whole profile stuff is ugly, espec. they changed the
#	option name all the time.
#	need to build little app which hammers the packet code to generate
#	profile data, if not cross compiling. Rest of code not so important
# OPT_FLAGS += -fprofile-use
# OPT_FLAGS += -fbranch-probabilities
# OPT_FLAGS += -fprofile-generate
# OPT_FLAGS += -fprofile-arcs
	check_for_prefetch_loop_array=no
	case "$host" in
		*powerpc-*)
			check_for_prefetch_loop_array=yes;;
		*powerpc64-*)
			check_for_prefetch_loop_array=yes;;
		*x86_64-*)
			check_for_prefetch_loop_array=yes;;
		*mips*)
			check_for_prefetch_loop_array=yes;;
		*ia64-*)
			check_for_prefetch_loop_array=yes;;
		*)
			;;
	esac
	if test x"$check_for_prefetch_loop_array" = xyes; then
		AX_CHECK_COMPILER_FLAGS([-fprefetch-loop-arrays],[CFLAGS="-fprefetch-loop-arrays $CFLAGS"],[])
	fi
	AX_CHECK_COMPILER_FLAGS([-fomit-frame-pointer],[CFLAGS="-fomit-frame-pointer $CFLAGS"],[])
	AX_CHECK_COMPILER_FLAGS([-ffast-math],[CFLAGS="-ffast-math $CFLAGS"],[])
#	AX_CHECK_COMPILER_FLAGS([-O3],[CFLAGS="-O3 $CFLAGS"],[])
	AX_CHECK_COMPILER_FLAGS([-g],[CFLAGS="-g $CFLAGS"],[])
	AX_CHECK_COMPILER_FLAGS([-pipe],[CFLAGS="-pipe $CFLAGS"],[])
#	AX_CHECK_COMPILER_FLAGS([-save-temps],[CFLAGS="-save-temps $CFLAGS"],[])
# else
	# TODO: automatically switch to low CFLAGS, without default -O2 but with user override
fi

if test x"$disable_warnings" = xno; then
	WFLAGS=""
	AX_CHECK_COMPILER_FLAGS([-Wsequence-point],[WFLAGS="-Wsequence-point $WFLAGS"],[])
	AX_CHECK_COMPILER_FLAGS([-Wshadow],[WFLAGS="-Wshadow $WFLAGS"],[])
	AX_CHECK_COMPILER_FLAGS([-Wcast-qual],[WFLAGS="-Wcast-qual $WFLAGS"],[])
	AX_CHECK_COMPILER_FLAGS([-Wmissing-prototypes],[WFLAGS="-Wmissing-prototypes $WFLAGS"],[])
	AX_CHECK_COMPILER_FLAGS([-Wimplicit],[WFLAGS="-Wimplicit $WFLAGS"],[])
	AX_CHECK_COMPILER_FLAGS([-Wwrite-strings],[WFLAGS="-Wwrite-strings $WFLAGS"],[])
	AX_CHECK_COMPILER_FLAGS([-Wpointer-arith],[WFLAGS="-Wpointer-arith $WFLAGS"],[])
	AX_CHECK_COMPILER_FLAGS([-W],[WFLAGS="-W $WFLAGS"],[])
	AX_CHECK_COMPILER_FLAGS([-Wall],[WFLAGS="-Wall $WFLAGS"],[])

	CFLAGS="$WFLAGS $CFLAGS"
fi

m4_include([m4/test_asm.m4])
# use the compiler as as driver, since he may use some other as
# then the system as. we want to feed inline asm, so it is important
# to go trough the compiler.
susp_binutils=215
my_cv_as="$CC"
as_tst_needed=no

case "$host" in
	*i?86-*)
		as_tst_needed=yes;;
	*x86_64-*)
		AS_CHECK_FEATURE([movq %xmm_, %r_x], as_cv_movq_tst, [-c], [movq %xmm0, %rax])
		if test x"$as_cv_movq_tst" = xyes; then
			AC_DEFINE(HAVE_MOVQ_XMM_GPR, 1, Yes, the assambler is not broken)
		else
			AC_MSG_WARN([Your assembler is broken, prop. because your distro is to old to live. Please, let it die.])
		fi
		as_tst_needed=yes;;
	*)
		;;
esac
if test x"$as_tst_needed" != xno; then
	AS_CHECK_FEATURE(SSSE3, as_cv_ssse3_tst, [-c], [pmaddubsw %xmm5, %xmm0], [susp_binutils=217])
	if test x"$as_cv_ssse3_tst" != xno; then
		AS_CHECK_FEATURE(SSE4.2, as_cv_sse42_tst, [-c], [pcmpestri $0b0010100, (%eax), %xmm1], [susp_binutils=218])
		if test x"$as_cv_sse42_tst" != xno; then
			AS_CHECK_FEATURE(AVX, as_cv_avx_tst, [-c], [vandpd %ymm0, %ymm1, %ymm0], [susp_binutils=219])
		fi
	fi
fi
AC_DEFINE_UNQUOTED([HAVE_BINUTILS], [$susp_binutils], [set to the version of you AS])

# Checks for libraries.
AC_SEARCH_LIBS([dlopen], [dl])
AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([gethostname], [nsl])
AC_SEARCH_LIBS([getaddrinfo], [resolv])
AC_SEARCH_LIBS([sched_yield], [rt])
case "$host" in
	*win32*)
		LIBS="-lws2_32 $LIBS";;
	*mingw*)
		LIBS="-lws2_32 $LIBS";;
esac

# Checks for header files.
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([arpa/inet.h byteswap.h fcntl.h malloc.h malloc_np.h netdb.h netinet/in.h netinet/tcp.h stdint.h strings.h sys/param.h sys/socket.h sys/time.h sys/sysctl.h sys/uio.h sys/mman.h valgrind/valgrind.h syslog.h wchar.h winsock2.h ws2tcpip.h])

AC_CHECK_HEADERS([ctype.h inttypes.h limits.h stddef.h stdlib.h string.h signal.h errno.h unistd.h], [], [AC_MSG_ERROR(an important system header was not found!)])

m4_include([m4/ax_check_zlib.m4])
AX_CHECK_ZLIB

# keep this above the dbm test, so when cross compiling and
# pkg-config takes the host libs, things brake
m4_include([m4/pkg.m4])
if test x"$disable_dbus" = xno; then
	dbus_on_machine=no
	PKG_CHECK_EXISTS([dbus-1], [dbus_on_machine=yes], [])

	if test x"$dbus_on_machine" = xyes; then
		dbus_header=no
		PKG_CHECK_MODULES([DBUS], [dbus-1])
		oCPPFLAGS="$CPPFLAGS"
		oCFLAGS="$CFLAGS"
		oLIBS="$LIBS"
		CPPFLAGS="$DBUS_CPPFLAGS $CPPFLAGS"
		CFLAGS="$DBUS_CFLAGS $CFLAGS"
		LIBS="$DBUS_LIBS $LIBS"
		AC_CHECK_HEADER(dbus/dbus.h, [dbus_header=yes], [], [#define DBUS_API_SUBJECT_TO_CHANGE])
		if test x"$dbus_header" = xyes; then
			dbus_working=no
			AC_MSG_CHECKING([if dbus is new enough])
			AC_LINK_IFELSE([AC_LANG_PROGRAM(
                [
#define DBUS_API_SUBJECT_TO_CHANGE
#include <stdlib.h>
#include <dbus/dbus.h>
                ],
                [
                      DBusConnection *d;
                      DBusError de;
                      d = dbus_bus_get(DBUS_BUS_SYSTEM, &de);
                      dbus_bus_request_name(d, "/foo/bla/tralala", 0, &de);
                ]
               )],
               [AC_MSG_RESULT([yes])
                dbus_working=yes],
               [AC_MSG_RESULT([no])])
			if test x"$dbus_working" = xyes; then
				AC_DEFINE(HAVE_DBUS, 1, Define to 1 if we have dbus)
			else
				CFLAGS="$oCFLAGS"
				LIBS="$oLIBS"
			fi
		else
			CFLAGS="$oCFLAGS"
			LIBS="$oLIBS"
		fi
		CPPFLAGS="$oCPPFLAGS"
	fi
fi

# do you have db.h or ndbm.h?
AC_CHECK_HEADER([db.h], [have_db_h=yes],[have_db_h=no][])
if test x"$have_db_h" = xyes; then
	AC_MSG_CHECKING([for ndbm interface in db.h])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	                   [
#define DB_DBM_HSEARCH 1
#include <db.h>
	                   ],
	                   [
	                      datum key, value;
	                      static DBM *s;
	                      s = dbm_open("bla", 0, 0);
	                      return 0;
	                   ]
	                  )],
	                  [AC_MSG_RESULT([yes])],
	                  [have_db_h=no
	                   AC_MSG_RESULT([no])])
	
	if test x"$have_db_h" = xyes; then
		AC_MSG_CHECKING([for ndbm interface in -ldb])
		backup_LIBS="$LIBS"
		LIBS="-ldb $LIBS"
		AC_LINK_IFELSE([AC_LANG_PROGRAM(
		                [
#define DB_DBM_HSEARCH 1
#include <db.h>
		                ],
		                [
		                   datum key, value;
		                   static DBM *s;
		                   s = dbm_open("bla", 0, 0);
		                   return 0;
		                ]
		               )],
		               [AC_DEFINE(HAVE_DB, 1,
	                             Define to 1 if we have a ndbm in db.h)
		                AC_MSG_RESULT([yes])],
		               [have_db_h=no
		                AC_MSG_RESULT([no])])
		if test x"$have_db_h" = xno; then
			LIBS="$backup_LIBS"
		fi
	fi
fi

if test x"$have_db_h" = xno; then
	AC_CHECK_HEADER([ndbm.h], [],[][AC_MSG_ERROR(can not find an ndbm provider!)])
	AC_SEARCH_LIBS([dbm_open], [ndbm dbm], [], [AC_MSG_ERROR(can not find ndbm!)])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
AC_C_RESTRICT
AC_C_BIGENDIAN([AC_SUBST(TARGET_ENDIAN, big)], [AC_SUBST(TARGET_ENDIAN, little)], [], AC_MSG_ERROR(mixed endian binary are not supported))
AC_C_FLEXIBLE_ARRAY_MEMBER
# Is the __thread keyword for thread local storage available?
# __thread should be cheaper since direct CPU-instructions
# get generated (for example addressing over the %gs segment
# selector on x86: movl %gs:-4, %eax) instead of a call
# over the GOT, a runtimecheck for a proper key, a runtime
# address calculation, and finaly load and return.
AC_MSG_CHECKING([for __thread])
AC_LINK_IFELSE([AC_LANG_PROGRAM([], [static __thread int p = 0])],
               [AC_DEFINE(HAVE___THREAD, 1,
                      Define to 1 if compiler supports __thread)
                AC_MSG_RESULT([yes])],
               [AC_MSG_RESULT([no])])
AC_TYPE_UID_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_MSG_CHECKING([for __uint128])
AC_LINK_IFELSE([AC_LANG_PROGRAM([typedef unsigned __uint128 __attribute__((mode(TI)));],[return 0;])],
               [AC_MSG_RESULT([yes])
AC_MSG_CHECKING([if __uint128 works])
AC_LINK_IFELSE([AC_LANG_PROGRAM([typedef unsigned __uint128 __attribute__((mode(TI)));
typedef unsigned long long u64;
void MUL_BIG(u64 x, u64 y, u64 *z, u64 *k)
{
	__uint128 a_res = (__uint128)x * y + *k;
	*k = a_res >> 64;
	*z = a_res;
}
],[return 0;])],
               [AC_MSG_RESULT([yes])
                AC_DEFINE(HAVE_TIMODE, 1, Define to 1 if your compiler has a 128 bit type)],
               [AC_MSG_RESULT([no])])
AC_MSG_CHECKING([if __uint128 in va_list works])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <stdarg.h>
typedef unsigned __uint128 __attribute__((mode(TI)));
struct foo {
	va_list ap;
};
const char *vnDDtoa(const char *buf, struct foo *foo)
{
	__uint128 n = va_arg(foo->ap, __uint128);
	return buf;
}
],[return 0;])],
               [AC_MSG_RESULT([yes])
                AC_DEFINE(COMPILER_CAN_VAARG_TIMODE, 1, Define to 1 if your compiler can handle 128 bit in a va_list)],
               [AC_MSG_RESULT([no])])
               ],
               [AC_MSG_RESULT([no])])
AC_CHECK_TYPES([ptrdiff_t])
AC_CHECK_TYPES([sighandler_t], [],[], [[#include <signal.h>]])
AC_CHECK_TYPES([sa_family_t, socklen_t], [], [], [[#ifndef WIN32
# include <sys/types.h>
# include <sys/socket.h>
#else
# include <winsock2.h>
# include <ws2tcpip.h>
#endif]])
AC_CHECK_TYPES([in_port_t, in_addr_t], [], [], [[#ifndef WIN32
# include <netinet/in.h>
#else
# include <winsock2.h>
# include <ws2tcpip.h>
#endif]])
AC_CHECK_TYPE([pthread_spinlock_t], [AC_DEFINE(HAVE_SPINLOCKS, 1, do we have Spinlocks? Old Solaris has none!)],[], [[#include <pthread.h>]])
AC_CHECK_TYPE([struct in_pktinfo], [AC_DEFINE(HAVE_IP_PKTINFO, 1, do we have pktinfo)],[], [[#ifndef WIN32
# include <netinet/in.h>
#else
# include <winsock2.h>
# include <ws2tcpip.h>
#endif]])
AC_CHECK_TYPE([struct in6_pktinfo], [AC_DEFINE(HAVE_IP6_PKTINFO, 1, do we have pktinfo v6)],[], [[#ifndef WIN32
# include <netinet/in.h>
#else
# include <winsock2.h>
# include <ws2tcpip.h>
#endif]])
AC_CHECK_MEMBER([struct sockaddr.sa_len], [AC_DEFINE(HAVE_SA_LEN, 1, so we have a sa_len member)], [], [[#ifndef WIN32
# include <sys/types.h>
# include <sys/socket.h>
#else
# include <winsock2.h>
# include <ws2tcpip.h>
#endif]])
AC_CHECK_MEMBER([struct in_pktinfo.ipi_spec_dst], [AC_DEFINE(HAVE_IP_PKTINFO_DST, 1, do we have pktinfo)],[], [[#ifndef WIN32
# include <netinet/in.h>
#else
# include <winsock2.h>
# include <ws2tcpip.h>
#endif]])

m4_include([m4/ax_wint_t.m4])
AX_WINT_T

# do we have IPv6 ?
AC_MSG_CHECKING([for IPv6])
AC_LINK_IFELSE([AC_LANG_PROGRAM(
                [
#ifndef WIN32
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
#else
# include <winsock2.h>
# include <ws2tcpip.h>
#endif
                ],
                [
                   struct sockaddr_in6 s; struct in6_addr t=in6addr_any; int i=AF_INET6;
                   t.s6_addr[[0]] = 0;
                   return 0;
                ]
               )],
               [AC_DEFINE(HAVE_IPV6, 1,
                      Define to 1 if we have IPv6)
                AC_MSG_RESULT([yes])],
               [AC_MSG_RESULT([no])])

AC_CHECK_DECLS([INET6_ADDRSTRLEN, IPV6_V6ONLY, IP_RECVIF, IP_RECVDSTADDR, IP_SENDSRCADDR], [], [], [[#ifndef WIN32
# include <netinet/in.h>
#else
# include <winsock2.h>
# include <ws2tcpip.h>
#endif]])
AC_CHECK_DECLS([TCP_CORK, TCP_THIN_LINEAR_TIMEOUTS, TCP_THIN_DUPACK, TCP_USER_TIMEOUT], [], [], [[#ifndef WIN32
# include <netinet/tcp.h>
#else
# include <winsock2.h>
# include <ws2tcpip.h>
#endif]])
AC_CHECK_DECLS([PR_SET_NAME], [], [], [[#include <sys/prctl.h>]])

# do we need epoll compat?
need_epoll_compat=no
epoll_kepoll=no
epoll_eport=no
epoll_kqueue=no
epoll_devpoll=no
epoll_pollset=no
epoll_poll=no

AC_MSG_CHECKING([for epoll])
AC_LINK_IFELSE([AC_LANG_PROGRAM(
                [
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/poll.h>
                ],
                [
                   int foo = epoll_create(1);
                   return 0;
                ]
               )],
               [AC_MSG_RESULT([yes])],
               [AC_DEFINE(NEED_EPOLL_COMPAT, 1, Define to 1 if we need an epoll emulation)
                need_epoll_compat=yes
                AC_MSG_RESULT([no])])

if test x"$need_epoll_compat" = xyes; then

# Linux with old userland and 2.6 Kernel
	AC_CHECK_DECL([__NR_epoll_create], [epoll_kepoll=yes], [], [[#include <sys/syscall.h>]])

# Solaris 10 event ports, EXPERIMENTAL
	AC_MSG_CHECKING([for event ports])
	AC_LINK_IFELSE([AC_LANG_PROGRAM(
	                [
#include <sys/types.h>
#include <port.h>
	                ],
	                [
	                   int foo = port_create();
	                   return 0;
	                ]
	               )],
	               [epoll_eport=yes
	                AC_MSG_RESULT([yes])],
	               [AC_MSG_RESULT([no])])

# *BSD + MacOS-X kqueue, EXPERIMENTAL
	AC_MSG_CHECKING([for kqueue])
	AC_LINK_IFELSE([AC_LANG_PROGRAM(
	                [
#include <sys/types.h>
#include <sys/event.h>
	                ],
	                [
	                   int foo = kqueue();
	                   return 0;
	                ]
	               )],
	               [epoll_kqueue=yes
	                AC_MSG_RESULT([yes])],
	               [AC_MSG_RESULT([no])])

# Solaris + last HP-UX, maybe IBM AIX /dev/poll support
	m4_include([m4/ax_sys_dev_poll.m4])
	AX_SYS_DEV_POLL([epoll_devpoll=yes], [])

	AC_MSG_CHECKING([for pollset])
	AC_LINK_IFELSE([AC_LANG_PROGRAM(
	                [
#include <sys/poll.h>
#include <sys/pollset.h>
#include <fcntl.h>
	                ],
	                [
	                   int foo = pollset_create(30);
	                   return 0;
	                ]
	               )],
	               [epoll_pollset=yes
	                AC_MSG_RESULT([yes])],
	               [AC_MSG_RESULT([no])])

# try to find the "best"
	if test x"$epoll_kepoll" = xyes; then
		AC_DEFINE(HAVE_KEPOLL, 1, Define to 1 if the kernel has epoll)
	else
		if test x"$epoll_eport" = xyes; then
			AC_DEFINE(HAVE_EPORT, 1, Define to 1 if the kernel has event ports)
		else
			if test x"$epoll_kqueue" = xyes; then
				AC_DEFINE(HAVE_KQUEUE, 1, Define to 1 if the kernel has kqueue)
			else
				if test x"$epoll_devpoll" = xyes; then
					AC_DEFINE(HAVE_DEVPOLL, 1, Define to 1 if the kernel /dev/poll)
				else
					if test x"$epoll_pollset" = xyes; then
						AC_DEFINE(HAVE_POLLSET, 1, Define to 1 if the kernel pollset)
					else
						AC_DEFINE(HAVE_POLL, 1, Define to 1 if the kernel has poll)
					fi
				fi
			fi
		fi
	fi
else
	AC_CHECK_DECL([EPOLLONESHOT], [], [AC_MSG_ERROR(your epoll is to old and does not support oneshot)], [[#include <sys/epoll.h>]])
fi

# The Windows "Secure API" (suckers... it's thread safe, how about impl.
# strerror_r...) is better checked by the decl in string.h, because it
# may be a macro, and check_funcs may not find it as exactly this
# symbol in a lib.
AC_CHECK_DECLS([strerror_s])

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MMAP
# these checks are nice in intend, but suck, since they
# redefine basic funcs. Every hosted system should have
# a malloc/realloc, and the behaivios at 0 is not my
# concern
# AC_FUNC_MALLOC
# AC_FUNC_REALLOC
# the other special tests may also suck
AC_FUNC_STRERROR_R
AC_FUNC_STRNLEN
AC_FUNC_STRTOD
AC_CHECK_FUNCS([isblank dup2 dlopen ftruncate gethostname getpagesize gettimeofday localtime_r memchr memmove mempcpy memset munmap mprotect malloc_usable_size recvmmsg socket strchr strchrnul strcspn strerror strrchr strstr strtoul sysctlbyname inet_ntop inet_pton dbus_connection_try_register_object_path dbus_threads_init_default dbus_watch_get_unix_fd])

if test x"$strerror_mtsafe" = xyes; then
	AC_DEFINE(HAVE_MTSAFE_STRERROR, 1, strerror is thread save)
fi

AC_MSG_CHECKING([wheter we can safely swap on /dev/fd0])
AC_MSG_RESULT([yes])


AC_DEFINE(SYSTEM_MIN_BYTES_WORK, 64, how many bytes must be avail to switch away from byte wise working)
AC_CONFIG_FILES([config_auto.make])
AC_OUTPUT
