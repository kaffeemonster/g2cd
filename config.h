/*
 * config.h
 * the configurable compatibility-options
 *
 * Copyright (c) 2004-2009 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version
 * 2 published by the Free Software Foundation.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 * $Id: config.h,v 1.15 2005/11/05 11:12:01 redbully Exp redbully $
 */

#ifndef _CONFIG_H
#define _CONFIG_H

/* can we have dyn arrays? */
#define HAVE_C99
/*
 * Is __thread keyword for thread local storage available?
 * __thread should be cheaper since direct CPU-instructions
 * get generated (for example addressing over the %gs segment
 * selector on x86: movl %gs:-4, %eax) instead of a call
 * over the GOT, a runtimecheck for a proper key, a runtime
 * address calculation, and finaly load and return.
 */
#define HAVE___THREAD
/* does the comiler support 128bit vars on this arch */
//#define HAVE_TIMODE
/* oh man, no end in brokenness */
#define HAVE_RESTRICT
#define HAVE_ISBLANK

/* do we have IPv6 and other net stuff */
#define HAVE_SA_FAMILY_T
#define HAVE_IN_PORT_T
#define HAVE_SOCKLEN_T
#define HAVE_IPV6
#define HAVE_INET6_ADDRSTRLEN
#define HAVE_INET_NTOP
#define HAVE_INET_PTON
#define HAVE_IPV6_V6ONLY

/* need epoll compat? */
//#define NEED_EPOLL_COMPAT
/* if we need epoll compat-layer, wich? Only use _ONE_! */
//#define HAVE_POLL	/* BROKEN ? */
//#define HAVE_KEPOLL	/* Linux with old userland and 2.6 Kernel */
//#define HAVE_DEVPOLL	/* Solaris, EXPERIMENTAL */
//#define HAVE_KQUEUE	/* BSD, EXPERIMENTAL */

/* do we have sighandler_t */
#define HAVE_SIGHANDLER_T

/* do we have stdint.h */
#define HAVE_STDINT_H
/* do we have alloca.h */
#define HAVE_ALLOCA_H

/* do we have malloc.h */
#define HAVE_MALLOC_H
/* or malloc_np.h (FreeBSD) */
//#define HAVE_MALLOC_NP_H
/* do we have malloc_usable_size() (GlibC >= ?? && FreeBSD >= 7.0) */
#define HAVE_MALLOC_USABLE_SIZE

/* are we on a GNU/nonGNU-sys */
#define HAVE_STRNLEN
#define HAVE_STRCHRNUL
#define HAVE_MEMPCPY
/* only select one or none of the following */
#define HAVE_GNU_STRERROR_R
/* Old Solaris does not have strerror_r, instaed strerror
 * is thread save
 */
//#define HAVE_MTSAFE_STRERROR

/* do you have db.h or ndbm.h? */
#define HAVE_DB

/*
 * we want to recv the recv addr on udp
 * for Linux we need:
 */
#define HAVE_IP_PKTINFO
/* and on BSD we need: */
//#define HAVE_IP_RECVDSTADDR
//#define HAVE_IP_SENDSRCADDR
//#define HAVE_SYS_UIO_H

/*
 * are we on an multi-processor-mashine
 * (even virtual processors)
 */
#define HAVE_SMP
/* if we have SMP, do we have Spinlocks? Old Solaris has none! */
#define HAVE_SPINLOCKS

/* do we want to use my inline-asm's */
#define I_LIKE_ASM

/* your arch is a little picky 'bout lots of conditional branches? */
#if defined(__powerpc__) || defined(__powerpc64__)
# define I_HATE_CBRANCH
#endif

/* this is a call for trouble, but helpfull */
//TODO: backtrace a little bit broken ATM
#define WANT_BACKTRACES

/*
 * Is unaligned access on this machine ok
 * If this is wrongly set, you may find a SIGBUS
 */
#define UNALIGNED_OK 1
/*
 * Does your arch has bit foo instructions?
 * We sometimes need to find the index of a bit. This can be
 * done fast and cheap if your arch has instructions to scan
 * for a bit (bsf, cntlz). (popcnt is a bad idea if your
 * shifter are slow, sparc, 50 times slower than our generic
 * bitmagic)
 */
#define HAVE_BIT_INSTR

/*
 * Does your arch has a fast multiplication?
 * We misuse the multiplication as a fast mixing func for
 * hashes. A slow mull or an hardware emulated (old sparc
 * and its hw aided mull) does not help in this case and
 * another aproach (add, shifts, xor, etc.) can be faster.
 */
#define HAVE_HW_MULT

/*
 * Solaris likes to hide the capabilities of their Sparc
 * v9 behind silly games of 32 bit compat and "user space
 * is better of 32 Bit, 64 Bit is for databases", so if not
 * forcing 64 bit builds, everthing looks like a v8
 * even if you have a v9 (issued with the UltraSPARC in
 * 1995, more than a dekade in IT ago).
 * This way you miss new "real" atomic instr. and can do
 * 64 bit arith. just fine. If only the program knew...
 * So here is a switch, only kill it if you are certain
 * you have an real old Sparc.
 */
#define HAVE_REAL_V9
/*
 * You have an UltraSPARC with VIS?
 * The anwser is prop. yes, see above.
 * Still you may want to disable the VIS code, for example for
 * the Niagara T1. VIS is part of the FPU (now called FGU),
 * and Niagara has 32 cores, which all share a single FPU.
 * So on such CPUs it would be faster if the 32 cores are
 * busy coping and doing stuff, even bytewise, than all waiting
 * on one FPU.
 */
#define HAVE_VIS
/*
 * You have an UltraSparc III or better (~2001)?
 * don't know whats up with the Fujitsu SPARC64 III/IV/V etc.
 */
#define HAVE_VIS2

/*
 * how many bytes must be avail to switch away
 * from byte wise working
 */
#define SYSTEM_MIN_BYTES_WORK 128

/* not needed anymore, simply hang it of sizeof(type) */
/*
 * It is always a bad thing[tm] to fiddle around with the
 * System-dependend allingment, but sometimes...
 * Meaning of all this:
 * We normaly let the Compiler do the dirty Stuff, it would be
 * insane to fiddle around with it. Also we are normaly
 * accessing all Multibyte values in an byte orientated buffer
 * byte-wise, since mostly we have to regard the endianess, and
 * to be safe.
 * But sometimes you want to simply work on some mem, and doing
 * it byte-wise would waste performance. Then this comes into
 * play.
 * SYSTEM_ALIGNMENT_NEEDED defines what alingment is needed,
 * if this constraint is not met, no multibyte access will be
 * done.
 * SYSTEM_ALIGNMENT_WANTED describes the value which alignment
 * should be achieved, before doing multibyte access.
 * Ok, and how do we come to the Numbers?
 * Examples:
 * An x86 can mostly read everything from every address, it
 * just gets realy slow on bad aligned data. So i don't set an
 * needed alignment of one, since such missaligned data is
 * faster handeld bytewise.
 * An 68k for example can read multibyte values only from even
 * addresses, not doing so results in an processor-exeption
 * wich would result in an SIGBUS on Unices. So here would be
 * the needed-value 2. But this is still quite silly.
 * We use at least 32bit multibyte values, so using less than
 * 4 byte alignment as needed is not recommended.
 * On an arch were most types like size_t, int, long etc.
 * elvaluates to 64 bit, the needed-value should be raised to
 * 8, but be warned, raising this value to much could lead to
 * a not alignable situation and provoke bytewise access
 * -> slow.
 * The wanted-value is a part for itself, good values for this
 * is a little gambling.
 * To give an Example: the x86 in newer generations have
 * vectorinstructions, normaly referred to as MMX, 3DNow, SSE
 * and the like. PPC knows AltiVec. They normaly work on larger
 * Datatypes, MMX 64Bit, SSE 128Bit, so to take the greatest
 * advantage of such instructions you better align your data to
 * a multiple of the datatype.
 * The Memoryfetchunit also plays some tricks, like accessing 2
 * consecutive 32Bits is faster on an 8 alignment, due to magic
 * (eg.: Proccesor memory bus always reads 64bit).
 * But this is all Black Magic[tm], good values to orientate is
 * the alignment of pointers return by malloc on your arch.
 * All in all, to get the most out of it, you must know your
 * CPU and the code, where it is used.
 */
/* The minimal allingment needed on the Plattform */
/* #define SYSTEM_ALIGNMENT_NEEDED 4 */
/* An allingment positive for the Performance */
/* #define SYSTEM_ALIGNMENT_WANTED 8 */

#endif
