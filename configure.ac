#
# configure.ac
# autoconf input for our foo
#
# Copyright (c) 2010 Jan Seiffert
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
AC_INIT([g2cd], [0.0.0.11], [kaffeemonster@googlemail.com])
AC_CANONICAL_HOST
AC_CONFIG_SRCDIR([G2MainServer.c])
AC_CONFIG_HEADERS([config_auto.h])

strerror_mtsafe=no
default_hate_cbranch=no
default_enable_unaligned_ok=no
default_disable_bit_instr=yes
disable_backtraces=no
disable_hw_mult=no
disable_smp=no
disable_asm=no
disable_v9=no
disable_vis=no
disable_vis2=no
use_mapfiles=no

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
	*)
		;;
esac

AC_ARG_WITH([backtraces], [AS_HELP_STRING([--without-backtraces],
            [do you want backtraces on crash? (This is hacky, but is often the only helper on crash)])],
            [disable_backtraces="$witheval"], [])
AC_ARG_ENABLE([unaligned], [AS_HELP_STRING([--enable-unaligned],
              [your arch can do unaligned mem access. Do not enable if complicated software fixups make this possible])],
              [enable_unaligned_ok="$enableval"], [enable_unaligned_ok=$default_enable_unaligned_ok])
AC_ARG_ENABLE([bit-instr], [AS_HELP_STRING([--disable-bit-instr],
              [your arch lacks certain bit manipulation instruction, clz and the like, sometimes other instructions can make up for it, but only if they are fast])],
              [disable_bit_instr="$enableval"], [disable_bit_instr=$default_disable_bit_instr])
AC_ARG_ENABLE([hw-mult], [AS_HELP_STRING([--disable-hw-mult],
              [your arch has no multiply instruction, or the instruction is slow])],
              [disable_hw_mult="$enableval"], [])
AC_ARG_ENABLE([smp], [AS_HELP_STRING([--disable-smp],
              [disable smp safety, only do this if you are 110% sure the code will only run on an UP machine])],
              [disable_smp="$enableval"], [])
AC_ARG_ENABLE([asm], [AS_HELP_STRING([--disable-asm],
              [disable assembler code])],
              [disable_asm="$enableval"], [])
AC_ARG_ENABLE([sparcv9], [AS_HELP_STRING([--disable-sparcv9],
              [disable sparcv9 code, only do this if you really do _not_ have a v9, which means < UltraSPARC/SPARC64])],
              [disable_v9="$enableval"], [])
AC_ARG_ENABLE([sparcvis], [AS_HELP_STRING([--disable-sparcvis],
              [disable sparc VIS code, if you have an >= UltraSPARC keep enabled, except for single FPU chips (Niagara, etc.)])],
              [disable_vis="$enableval"], [])
AC_ARG_ENABLE([sparcvis2], [AS_HELP_STRING([--disable-sparcvis2],
              [disable sparc VIS2 code, same as for VIS, but >= UltraSPARC III])],
              [disable_vis2="$enableval"], [])

AC_MSG_CHECKING([wheter we should disable backtrace generation])
if test "$disable_backtraces" = yes; then
	AC_MSG_RESULT([yes])
else
# this is a call for trouble, but helpfull
	AC_DEFINE(WANT_BACKTRACES, 1, do we want to generate backtraces)
	AC_MSG_RESULT([no])
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
if test "$enable_unaligned_ok" = yes; then
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
if test "$disable_bit_instr" = yes; then
	AC_MSG_RESULT([yes])
else
# Does your arch has bit foo instructions?
# We sometimes need to find the index of a bit. This can be
# done fast and cheap if your arch has instructions to scan
# for a bit (bsf, cntlz). (popcnt is a bad idea if your
# shifter are slow, sparc, 50 times slower than our generic
# bitmagic, sparclite/sparclet have a scan instruction (ffs)
# they might help)
	AC_DEFINE(HAVE_BIT_INSTR, 1, arch has bit manipulation instruction)
	AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([wheter we should avoid multiplications])
if test "$disable_hw_mult" = yes; then
	AC_MSG_RESULT([yes])
else
# Does your arch has a fast multiplication?
# We misuse the multiplication as a fast mixing func for
# hashes. A slow mull or an hardware emulated (old sparc
# and its hw aided mull) does not help in this case and
# another aproach (add, shifts, xor, etc.) can be faster.
	AC_DEFINE(HAVE_HW_MULT, 1, arch has a fast hardware multiplier)
	AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([wheter we can disable smp safety])
if test "$disable_smp" = yes; then
	AC_MSG_RESULT([yes])
else
# are we on an multi-processor-mashine
# (even virtual processors)
	AC_DEFINE(HAVE_SMP, 1, arch has smp)
	AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([wheter we should disable assembler code])
if test "$disable_asm" = yes; then
	AC_MSG_RESULT([yes])
else
# do we want to use my inline-asm's
	AC_DEFINE(I_LIKE_ASM, 1, build the asm foo)
	AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([wheter we should disable SPARCv9 code])
if test "$disable_v9" = yes; then
	AC_MSG_RESULT([yes])
else
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
fi

AC_MSG_CHECKING([wheter we should disable SPARC VIS code])
if test "$disable_vis" = yes; then
	AC_MSG_RESULT([yes])
else
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
fi

AC_MSG_CHECKING([wheter we should disable SPARC VIS2 code])
if test "$disable_vis2" = yes; then
	AC_MSG_RESULT([yes])
else
# You have an UltraSparc III or better (~2001)?
# don't know whats up with the Fujitsu SPARC64 III/IV/V etc.
	AC_DEFINE(HAVE_VIS2, 1, and we have VIS2)
	AC_MSG_RESULT([no])
fi


AC_USE_SYSTEM_EXTENSIONS
# Checks for programs.
AC_PROG_CC
m4_include([m4/acx_pthread.m4])
ACX_PTHREAD
# we are a pthread program, make the result the default
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
LDFLAGS="$LDFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"
# now check the rest
AC_PROG_CPP
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_CHECK_TOOL([AS], [as], [as])
AC_CHECK_TOOL([AR], [ar], [ar])
AC_CHECK_TOOL([OBJCOPY], [objcopy], [objcopy])
AC_CHECK_TOOL([STRIP], [strip], [strip])

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

if test "$use_mapfiles" = yes; then
	t_file_name=`mktemp`
	echo "{};" >> "$t_file_name"
	AX_CHECK_LINKER_FLAGS([-Wl,-M,"$t_file_name"],
	                      [LDFLAGS="-Wl,-M,.mapfile $LDFLAGS"],
	                      [])
	rm -f "$t_file_name"
fi

# pmaddubsw %xmm5, %xmm0 == gas 2.17
# pcmpestri	$0b0010100, (%eax), %xmm1 == gas 2.18
# vandpd	%ymm0, %ymm1, %ymm0 == gas 2.19

# Checks for libraries.
AC_SEARCH_LIBS([dlopen], [dl])
AC_SEARCH_LIBS([socket], [socket ws2_32])
AC_SEARCH_LIBS([gethostname], [nsl])
AC_SEARCH_LIBS([getaddrinfo], [resolv])
AC_SEARCH_LIBS([sched_yield], [rt])

# Checks for header files.
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([arpa/inet.h byteswap.h fcntl.h malloc.h malloc_np.h netdb.h netinet/in.h netinet/tcp.h stdint.h strings.h sys/param.h sys/socket.h sys/time.h sys/sysctl.h sys/uio.h syslog.h wchar.h])

AC_CHECK_HEADERS([ctype.h pthread.h inttypes.h limits.h stddef.h stdlib.h string.h signal.h errno.h unistd.h], [], [AC_MSG_ERROR(an important system header was not found!)])

m4_include([m4/ax_check_zlib.m4])
AX_CHECK_ZLIB

# do you have db.h or ndbm.h?
AC_CHECK_HEADER([db.h], [have_db_h=yes],[have_db_h=no][])
if test "$have_db_h" = yes; then
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
	
	if test "$have_db_h" = yes; then
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
		if test "$have_db_h" = no; then
			LIBS="$backup_LIBS"
		fi
	fi
fi

if test "$have_db_h" = no; then
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
               [AC_DEFINE(HAVE_TIMODE, 1, Define to 1 if your compiler has a 128 bit type)
                AC_MSG_RESULT([yes])],
               [AC_MSG_RESULT([no])])
AC_CHECK_TYPES([ptrdiff_t])
AC_CHECK_TYPES([sighandler_t], [],[], [[#include <signal.h>]])
AC_CHECK_TYPES([sa_family_t, socklen_t], [], [], [[#include <sys/socket.h>]])
AC_CHECK_TYPES([in_port_t, in_addr_t], [], [], [[#include <netinet/in.h>]])
AC_CHECK_TYPE([pthread_spinlock_t], [AC_DEFINE(HAVE_SPINLOCKS, 1, do we have Spinlocks? Old Solaris has none!)],[], [[#include <pthread.h>]])
AC_CHECK_TYPE([struct in_pktinfo], [AC_DEFINE(HAVE_IP_PKTINFO, 1, do we have pktinfo)],[], [[#include <netinet/in.h>]])

# do we have IPv6 ?
AC_MSG_CHECKING([for IPv6])
AC_LINK_IFELSE([AC_LANG_PROGRAM(
                [
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
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

AC_CHECK_DECLS([INET6_ADDRSTRLEN, IPV6_V6ONLY, IP_RECVIF, IP_RECVDSTADDR, IP_SENDSRCADDR], [], [], [[#include <netinet/in.h>]])
AC_CHECK_DECLS([TCP_CORK], [], [], [[#include <netinet/tcp.h>]])

# do we need epoll compat?
need_epoll_compat=no
epoll_kepoll=no
epoll_eport=no
epoll_kqueue=no
epoll_devpoll=no
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

if test "$need_epoll_compat" = yes; then

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
	AC_MSG_CHECKING([for /dev/poll])
	AC_LINK_IFELSE([AC_LANG_PROGRAM(
	                [
#include <sys/poll.h>
#include <sys/devpoll.h>
#include <fcntl.h>
#include <unistd.h>
	                ],
	                [
	                   int foo = 1;
	                   struct dvpoll do_poll;
	                   int result;
	                   do_poll.dp_timeout = 1000; do_poll.dp_nfds = 10;
	                   result = ioctl(foo, DP_POLL, &do_poll);
	                   return result;
	                ]
	               )],
	               [epoll_devpoll=yes
	                AC_MSG_RESULT([yes])],
	               [AC_MSG_RESULT([no])])

# try to find the "best"
	if test "$epoll_kepoll" = yes; then
		AC_DEFINE(HAVE_KEPOLL, 1, Define to 1 if the kernel has epoll)
	else
		if test "$epoll_eport" = yes; then
			AC_DEFINE(HAVE_EPORT, 1, Define to 1 if the kernel has event ports)
		else
			if test "$epoll_kqueue" = yes; then
				AC_DEFINE(HAVE_KQUEUE, 1, Define to 1 if the kernel has kqueue)
			else
				if test "$epoll_devpoll" = yes; then
					AC_DEFINE(HAVE_DEVPOLL, 1, Define to 1 if the kernel /dev/poll)
				else
					AC_DEFINE(HAVE_POLL, 1, Define to 1 if the kernel has poll)
				fi
			fi
		fi
	fi
else
	AC_CHECK_DECL([EPOLLONESHOT], [], [AC_MSG_ERROR(your epoll is to old and does not support oneshot)], [[#include <sys/epoll.h>]])
fi

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_FUNC_STRNLEN
AC_FUNC_STRTOD
AC_CHECK_FUNCS([isblank dup2 dlopen ftruncate gethostname getpagesize gettimeofday localtime_r memchr memmove mempcpy memset munmap malloc_usable_size socket strchr strchrnul strcspn strerror strrchr strstr strtoul sysctlbyname inet_ntop inet_pton])

if test "$strerror_mtsafe" = yes; then
	AC_DEFINE(HAVE_MTSAFE_STRERROR, 1, strerror is thread save)
fi

AC_MSG_CHECKING([wheter we can safely swap on /dev/fd0])
AC_MSG_RESULT([yes])


AC_DEFINE(SYSTEM_MIN_BYTES_WORK, 64, how many bytes must be avail to switch away from byte wise working)
AC_CONFIG_FILES([config_auto.make])
AC_OUTPUT
