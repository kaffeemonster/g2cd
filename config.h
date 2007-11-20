/*
 * config.h
 * the configurable compatibility-options
 *
 * Copyright (c) 2004, Jan Seiffert
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

/* do we have IPv6 */
#define HAVE_INET6_ADDRSTRLEN
#define HAVE_INET_NTOP
#define HAVE_INET_PTON
/* sometimes we even haven't this */
#define HAVE_INET_ATON

/* need epoll compat? */
#define NEED_EPOLL_COMPAT
/* if we need epoll compat-layer, wich? Only use _ONE_! */
//#define HAVE_POLL
//#define HAVE_KEPOLL	/* Linux with old userland and 2.6 Kernel */
//#define HAVE_DEVPOLL	/* Solaris, TODO */
#define HAVE_KQUEUE	/* BSD, TODO */

/* do we have sighandler_t */
//#define HAVE_SIGHANDLER_T

/* do we have stdint.h */
#define HAVE_STDINT_H
/* do we have alloca.h */
//#define HAVE_ALLOCA_H

/* are we on a GNU/nonGNU-sys */
//#define HAVE_STRNLEN
/* only select one or none of the following */
//#define HAVE_GNU_STRERROR_R
/* Old Solaris does not have strerror_r, instaed strerror
 * is thread save
 */
//#define HAVE_MTSAFE_STRERROR

/* are we on an multi-processor-mashine
 * (even virtual processors)
 */
//#define HAVE_SMP
/* if we have SMP, do we have Spinlocks? Solaris has none! */
//#define HAVE_SPINLOCKS 

/* do we want to use my inline-asm's */
#define I_LIKE_ASM

/* this is callong for trouble, but helpfull */
//#define WANT_BACKTRACES

/* how many bytes must be avail to switch away
 * from byte wise working
 */
#define SYSTEM_MIN_BYTES_WORK 128

/* not needed anymore */
/* It is always a bad thing[tm] to fiddle around with the
 * System-dependend allingment, but sometimes...
 * Meaning of all this:
 * We normaly let the Compiler do the dirty Stuff, it would be
 * insane to fiddle around with it. Also we are normaly
 * accessing all Multibyte values in an byte orientated buffer
 * byte-wise, since mostly we have to regard the endianess, and
 * to be safe.
 * But sometimes you want to simply work on some mem, and doing
 * it byte-wise would waste Performance. Then this comes into
 * play.
 * SYSTEM_ALIGNMENT_NEEDED defines what alingment is needed,
 * if this constraint is not met, no multibyte access will be
 * done.
 * SYSTEM_ALIGNMENT_WANTED describes the value which alignment
 * should be achiefed, before doing multibyte access.
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
 * (eg.: Proccesor always reads 64bit).
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
