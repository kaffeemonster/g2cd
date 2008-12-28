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

#undef AVX_MOVE
#undef AVX_XOR
#undef AVX_STORE
#undef SSE_PREFETCH
#undef SSE_PREFETCHW
#undef SSE_FENCE
#undef SSE_MOVE
#undef SSE_LOAD
#undef SSE_LOAD8
#undef SSE_STORE
#undef SSE_AND
#undef SSE_XOR
#undef MMX_PREFETCH
#undef MMX_PREFETCHW
#undef PREFETCH
#undef PREFETCHW
#undef MMX_STORE
#undef MMX_FENCE
#undef SIZE_T_BYTE
#undef MAKE_PREFETCH

#define MAKE_PREFETCH(x, y) x #y "\n\t"

#ifdef HAVE_AVX
# define AVX_MOVE(s, d)	"vmovdqa	" #s ", " #d "\n\t"
# define AVX_STORE(s, d)	"vmovdqa	" #s ", " #d "\n\t"
# define AVX_AND(sa, sb, d)	"vandpd	" #sa ", " #sb ", " #d "\n\t"
# define AVX_XOR(sa, sb, d)	"vxorpd	" #sa ", " #sb ", " #d "\n\t"
#endif
#ifdef HAVE_SSE
# define SSE_PREFETCH(x)	MAKE_PREFETCH("prefetcht0	", x)
# define PREFETCH(x)	MAKE_PREFETCH("prefetcht0	", x)
# ifdef HAVE_3DNOW
#  define SSE_PREFETCHW(x)	MAKE_PREFETCH("prefetchw	", x)
#  define PREFETCHW(x)	MAKE_PREFETCH("prefetchw	", x)
# else
#  define SSE_PREFETCHW(x)	MAKE_PREFETCH("prefetcht0	", x)
#  define PREFETCHW(x)	MAKE_PREFETCH("prefetcht0	", x)
# endif
# define SSE_FENCE	"sfence\n"
# ifdef HAVE_SSE2
#  define SSE_MOVE(x, y)	"movdqa	" #x ", " #y "\n\t"
#  define SSE_STORE(x, y)	"movdqa	" #x ", " #y "\n\t"
#  define SSE_AND(x, y)	"pand	" #x ", " #y "\n\t"
#  define SSE_XOR(x, y)	"pxor	" #x ", " #y "\n\t"
#  define SSE_LOAD8(x, y)	"movlpd	" #x ", " #y "\n\tmovhpd	8+" #x ", " #y "\n\t"
#  ifdef HAVE_SSE3
#   define SSE_LOAD(x, y)	"lddqu	" #x ", " #y "\n\t"
#  else
#   define SSE_LOAD(x, y)	"movdqu	" #x ", " #y "\n\t"
#  endif
# else
#  define SSE_LOAD8(x, y)	"movlps	" #x ", " #y "\n\tmovhps	8+" #x ", " #y "\n\t"
#  define SSE_MOVE(x, y)	"movaps	" #x ", " #y "\n\t"
#  define SSE_STORE(x, y)	"movaps	" #x ", " #y "\n\t"
#  define SSE_AND(x, y)	"andps	" #x ", " #y "\n\t"
#  define SSE_XOR(x, y)	"xorps	" #x ", " #y "\n\t"
# endif
#endif
#ifdef HAVE_MMX
# ifdef HAVE_SSE
#  define MMX_PREFETCH(x)	MAKE_PREFETCH("prefetcht0	", x)
#  define PREFETCH(x)	MAKE_PREFETCH("prefetcht0	", x)
#  ifdef HAVE_3DNOW
#   define MMX_PREFETCHW(x)	MAKE_PREFETCH("prefetchw	", x)
#   define PREFETCHW(x)	MAKE_PREFETCH("prefetchw	", x)
#  else
#   define MMX_PREFETCHW(x)	MAKE_PREFETCH("prefetcht0	", x)
#   define PREFETCHW(x)	MAKE_PREFETCH("prefetcht0	", x)
#  endif
#  define MMX_STORE(x, y)	"movq	" #x ", " #y "\n\t"
#  define MMX_FENCE	"sfence\n"
# else
#  ifdef HAVE_3DNOW
#   define MMX_PREFETCH(x)	MAKE_PREFETCH("prefetch	", x)
#   define PREFETCH(x)	MAKE_PREFETCH("prefetch	", x)
#   define MMX_PREFETCHW(x)	MAKE_PREFETCH("prefetchw	", x)
#   define PREFETCHW(x)	MAKE_PREFETCH("prefetchw	", x)
#  else
#   define MMX_PREFETCH(x)
#   define PREFETCH(x)
#   define MMX_PREFETCHW(x)
#   define PREFETCHW(x)
#  endif
#  define MMX_STORE(x, y)	"movq	" #x ", " #y "\n\t"
#  define MMX_FENCE
# endif
#else
# define PREFETCH(x)
# define PREFETCHW(x)
#endif

#ifdef __i386__
# define SIZE_T_BYTE	4
# define SIZE_T_SHIFT	2
# define NOT_MEM	"notl	"
# define PICREG_R %%ebx
# define PICREG "%%ebx"
#else
# define SIZE_T_BYTE	8
# define SIZE_T_SHIFT	3
# define NOT_MEM	"notq	"
# define PICREG_R %%rbx
# define PICREG "%%rbx"
#endif

#undef ALIGNMENT_WANTED
#ifdef HAVE_AVX
#define ALIGNMENT_WANTED 32
#elif defined(HAVE_SSE)
#define ALIGNMENT_WANTED 16
#elif defined(HAVE_MMX)
#define ALIGNMENT_WANTED 8
#else
#define ALIGNMENT_WANTED SOST
#endif
