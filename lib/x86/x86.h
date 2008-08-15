/*
 * x86.h
 * some x86 defines
 *
 * Copyright (c) 2006,2007 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
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
 * $Id:$
 */

#undef SSE_PREFETCH
#undef SSE_FENCE
#undef SSE_MOVE
#undef SSE_STORE
#undef SSE_AND
#undef SSE_XOR
#undef MMX_PREFETCH
#undef PREFETCH
#undef MMX_STORE
#undef MMX_FENCE
#undef SIZE_T_BYTE

#ifdef HAVE_SSE
# define SSE_PREFETCH(x)	"prefetcht0	" #x "\n\t"
# define PREFETCH(x)	"prefetcht0	" #x "\n\t"
# define SSE_FENCE	"sfence\n"
# ifdef HAVE_SSE2
#  define SSE_MOVE(x, y)	"movdqa	" #x ", " #y "\n\t"
#  define SSE_STORE(x, y)	"movdqa	" #x ", " #y "\n\t"
#  define SSE_AND(x, y)	"pand	" #x ", " #y "\n\t"
#  define SSE_XOR(x, y)	"pxor	" #x ", " #y "\n\t"
# else
#  define SSE_MOVE(x, y)	"movaps	" #x ", " #y "\n\t"
#  define SSE_STORE(x, y)	"movaps	" #x ", " #y "\n\t"
#  define SSE_AND(x, y)	"andps	" #x ", " #y "\n\t"
#  define SSE_XOR(x, y)	"xorps	" #x ", " #y "\n\t"
# endif
#endif
#ifdef HAVE_MMX
# ifdef HAVE_SSE
#  define MMX_PREFETCH(x)	"prefetcht0	" #x "\n\t"
#  define PREFETCH(x)	"prefetcht0	" #x "\n\t"
#  define MMX_STORE(x, y)	"movq	" #x ", " #y "\n\t"
#  define MMX_FENCE	"sfence\n"
# else
#  define MMX_PREFETCH(x)
#  define PREFETCH(x)
#  define MMX_STORE(x, y)	"movq	" #x ", " #y "\n\t"
#  define MMX_FENCE
# endif
#else
# define PREFETCH(x)
#endif

#ifdef __i386__
# define SIZE_T_BYTE	4
#else
# define SIZE_T_BYTE	8
#endif
