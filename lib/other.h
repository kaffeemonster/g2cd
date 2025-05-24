/*
 * other.h
 * some C-header-magic-glue
 *
 * Copyright (c) 2004 - 2019 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id: other.h,v 1.16 2005/11/04 06:04:43 redbully Exp redbully $
 */

#ifndef _OTHER_H
#define _OTHER_H

#ifdef HAVE_CONFIG_H
# include "../config.h"
#endif /* HAVE_CONFIG_H */

#ifndef UNALIGNED_OK
# define UNALIGNED_OK 0
#endif

#ifndef FLEXIBLE_ARRAY_MEMBER
/* Non C99 compiler with no extensions don't know about the following keywords */
# ifdef __GNUC__
#  define DYN_ARRAY_LEN 0
# else
#  define DYN_ARRAY_LEN 1
# endif	/* __GNUC__ */
# define restrict
#else
# define DYN_ARRAY_LEN FLEXIBLE_ARRAY_MEMBER
#endif

/*
 * some Magic for determining the attrib-capabilitie of an gcc or just set it
 * to nothing, if we are not capable of it
 */
#if defined(__GNUC__) && __GNUC__ >= 2
# define HAVE_GCC_ATTRIBUTE
#elif defined(__SUNPRO_C) && __SUNPRO_C >= 0x590
# define HAVE_GCC_ATTRIBUTE
#else
# undef  HAVE_GCC_ATTRIBUTE
# define __attribute__(xyz)	/* Ignore */
#endif

#ifdef HAVE_GCC_ATTRIBUTE
# define GCC_ATTRIB(x) __attribute__((x))
#endif /* HAVE_GCC_ATTRIBUTE */

#if defined __GNUC__ && defined __GNUC_MINOR__
# define _GNUC_PREREQ(maj, min) \
	((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
# define _GNUC_PREREQ(maj, min) 0
#endif  /* defined __GNUC__ && defined __GNUC_MINOR__ */

#if defined __SUNPRO_C && __SUNPRO_C >= 0x590
# define _SUNC_PREREQ(x) (__SUNPRO_C >= (x))
#else
# define _SUNC_PREREQ(x) 0
#endif /* defined __SUNPRO_C && __SUNPRO_C >= 0x590 */

#if _GNUC_PREREQ (3,1)
# define GCC_ATTR_USED	GCC_ATTRIB(__used__)
#else
# define GCC_ATTR_USED
#endif /* _GNUC_PREREQ (3,1) */

#if _GNUC_PREREQ (3,3)
# define GCC_ATTR_USED_VAR GCC_ATTRIB(__used__)
#else
# define GCC_ATTR_USED_VAR
#endif /* _GNUC_PREREQ (3,3) */

#if _GNUC_PREREQ (4,3)
# define GCC_ATTR_COLD GCC_ATTRIB(__cold__)
#else
# define GCC_ATTR_COLD
#endif /* _GNUC_PREREQ (4,3) */

#if _GNUC_PREREQ (4,3)
# define GCC_ATTR_HOT GCC_ATTRIB(__hot__)
#else
# define GCC_ATTR_HOT
#endif /* _GNUC_PREREQ (4,3) */

#if _GNUC_PREREQ (4,4)
# define GCC_ATTR_OPTIMIZE(x) GCC_ATTRIB(__optimize__ (x))
#else
# define GCC_ATTR_OPTIMIZE(x)
#endif /* _GNUC_PREREQ (3,3) */

#if _GNUC_PREREQ (3,1)
# define GCC_ATTRIB_UNUSED GCC_ATTRIB(__unused__)
#else
# define GCC_ATTRUB_UNUSED
#endif /* _GNUC_PREREQ (3,1) */

#if _GNUC_PREREQ (3,1)
# define GCC_ATTR_UNUSED_PARAM GCC_ATTRIB(__unused__)
#else
# define GCC_ATTR_UNUSED_PARAM
#endif /* _GNUC_PREREQ (3,1) */

/* Cygwin (since it generates PE-files) doens't know about visibility */
#if _GNUC_PREREQ (3,3) && !(defined(__CYGWIN__) || defined(WIN32))
# define GCC_ATTR_VIS(x) GCC_ATTRIB(__visibility__ (x))
#else
# define GCC_ATTR_VIS(x)
#endif /* _GNUC_PREREQ (3,3) && !__CYGWIN__ */

#if _GNUC_PREREQ (2,8) && (defined(__CYGWIN__) || defined(WIN32))
# define GCC_ATTR_DLLEXPORT __declspec(dllexport)
# define GCC_ATTR_STDCALL GCC_ATTRIB(__stdcall__)
#else
# define GCC_ATTR_DLLEXPORT
# define GCC_ATTR_STDCALL
#endif

#if _GNUC_PREREQ (3,0)
# define GCC_ATTR_MALLOC GCC_ATTRIB(__malloc__)
#else
# define GCC_ATTR_MALLOC
#endif

#if _GNUC_PREREQ (4,3)
# define GCC_ATTR_ALLOC_SIZE(x) GCC_ATTRIB(__alloc_size__(x))
# define GCC_ATTR_ALLOC_SIZE2(x, y) GCC_ATTRIB(__alloc_size__(x, y))
#else
# define GCC_ATTR_ALLOC_SIZE(x)
# define GCC_ATTR_ALLOC_SIZE2(x, y)
#endif

#if _GNUC_PREREQ (2,3) && !defined(WIN32)
# define GCC_ATTR_PRINTF(str_ind, to_check) GCC_ATTRIB(format (__printf__ , (str_ind), (to_check)))
#else
# define GCC_ATTR_PRINTF(str_ind, to_check)
#endif /* _GNUC_PREREQ (2,3) */

#if _GNUC_PREREQ (2,5)
# define GCC_ATTR_CONST GCC_ATTRIB(__const__)
#else
# define GCC_ATTR_CONST
#endif /* _GNUC_PREREQ (2,5) */

#if _GNUC_PREREQ (2,6)
# define GCC_ATTR_SECTION(x) GCC_ATTRIB(__section__(x))
#else
# define GCC_ATTR_SECTION(x)
#endif /* _GNUC_PREREQ (2,5) */

#if _GNUC_PREREQ (2,96)
# define GCC_ATTR_PURE GCC_ATTRIB(__pure__)
#else
# define GCC_ATTR_PURE
#endif /* _GNUC_PREREQ (2,96) */

#if _GNUC_PREREQ (2,7)
# define GCC_ATTR_PACKED GCC_ATTRIB(__packed__)
#else
# define GCC_ATTR_PACKED
#endif

#if _GNUC_PREREQ (2,3)
# define GCC_ATTR_ALIGNED(x) GCC_ATTRIB(aligned(x))
#else
# define GCC_ATTR_ALIGNED(x)
#endif

#if _GNUC_PREREQ (2,7)
# define GCC_ATTR_ALIAS(x) GCC_ATTRIB(alias(x))
#else
# define GCC_ATTR_ALIAS(x)
#endif

#if _GNUC_PREREQ (2,7) || _SUNC_PREREQ(0x5100)
# define GCC_ATTR_CONSTRUCT GCC_ATTRIB(__constructor__)
# define GCC_ATTR_DESTRUCT GCC_ATTRIB(__destructor__)
#else
# error "this will not run with your compiler"
# define GCC_ATTR_CONSTRUCT
# define GCC_ATTR_DESTRUCT
#endif

#if _GNUC_PREREQ (2,7)
# ifdef __i386__
/* only really needed for f***ed x86 */
#  define GCC_ATTR_FASTCALL GCC_ATTRIB(__regparm__(3))
# else
#  define GCC_ATTR_FASTCALL
# endif
#else
# define GCC_ATTR_FASTCALL
#endif

#if _GNUC_PREREQ (2,96)
# define likely(x)	__builtin_expect(!!(x), 1)
# define unlikely(x)	__builtin_expect(!!(x), 0)
#else
# define likely(x)	(x)
# define unlikely(x)	(x)
#endif

#if _GNUC_PREREQ (3,1) || _SUNC_PREREQ(0x5100)
# define always_inline inline GCC_ATTRIB(__always_inline__)
#else
# define always_inline inline
#endif

#if _GNUC_PREREQ (3,1)
# undef noinline
# define noinline GCC_ATTRIB(__noinline__)
#else
# ifndef noinline
#  define noinline
# endif
#endif

#if _GNUC_PREREQ (3,1)
# define prefetch(x)	__builtin_prefetch(x)
# define prefetchw(x)	__builtin_prefetch(x, 1)
# define prefetch_nt(x)	__builtin_prefetch(x, 0, 0)
# define prefetchw_nt(x)	__builtin_prefetch(x, 1, 0)
#else
# define prefetch(x) (x)
# define prefetchw(x) (x)
# define prefetch_nt(x) (x)
# define prefetchw_nt(x) (x)
#endif

#if _GNUC_PREREQ (3,0)
# define GCC_CONSTANT_P(x) __builtin_constant_p(x)
#else
# define GCC_CONSTANT_P(x) (0)
#endif

#if _GNUC_PREREQ (4,9)
# define GCC_ASSUME_ALIGNED(x) GCC_ATTRIB(__assume_aligned__(x))
#else
# define GCC_ASSUME_ALIGNED(x)
#endif

/* clang could join the party */
#if _GNUC_PREREQ (5,0)
# define GCC_OVERFLOW_UMUL(a, b, res)  __builtin_umul_overflow(a, b, res)
#else
# define GCC_OVERFLOW_UMUL(a, b, res)  ({bool r = a <= (UINT_MAX / b) ? false : true; *res = !r ? a * b : 0; r;})
#endif

/* gcc now warns on implicit falltrough (better diagnostics in
 * re goto fail). We use that feature
 * unfortunatly the classical fallthrough-in-comment marker only works
 * if comments are passed to cc1 with -C
 */
#if _GNUC_PREREQ (7,0)
# define GCC_FALL_THROUGH  GCC_ATTRIB(fallthrough);
#else
# define GCC_FALL_THROUGH
#endif

#ifdef GOT_GOT
# define SECTION_GOT GCC_ATTR_SECTION(".got")
#else
# define SECTION_GOT
#endif

#ifndef USE_SIMPLE_DISPATCH
# if defined(HAVE_SYS_MMAN_H) && defined(HAVE_MPROTECT) && defined(I_LIKE_ASM)
# else
#  define USE_SIMPLE_DISPATCH
# endif
#endif

/*
 * maybe we can unmap our init code
 * But for now, wimply mark it cold, because it is run exactly
 * one time
 */
#define __init GCC_ATTR_COLD
#define __init_data
#define __init_cdata

#ifndef HAVE_SIGHANDLER_T
# ifdef __FreeBSD__
#  define sighandler_t sig_t
# else
typedef void (*sighandler_t)(int);
# endif
#endif

static inline int isspace_a(unsigned int c)
{
	if(c > ' ')
		return 0;
	if(likely(c == ' '))
		return 1;
	if(c >= '\t' && c <= '\r')
		return 1;
	return 0;
}

static inline int isblank_a(unsigned int c)
{
	return ' ' == c || '\t' == c;
}

static inline int isdigit_a(unsigned int c)
{
	return c >= '0' && c <= '9';
}

static inline int isalpha_a(unsigned int c)
{
	return (c >= 'a' && c <= 'z') ||
	       (c >= 'A' && c <= 'Z');
}

static inline int isalnum_a(unsigned int c)
{
	return isalpha_a(c) || isdigit_a(c);
}

static inline int isgraph_a(unsigned int c)
{
	return c > ' ' && c < 0x7f;
}

#ifndef HAVE_ISBLANK
# define isblank(x) isblank_a(x);
#endif


/* (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) */
#if defined(HAVE_STDINT_H)
# include <stdint.h>
#else
# include <inttypes.h>
#endif /* HAVE_STDINT_H */

/* most things won't work if BITS_PER_CHAR = 8 isn't true, but you never know */
#if __CHAR_BIT__-0 > 0
#  define BITS_PER_CHAR __CHAR_BIT__
#else
# include <limits.h>
# define BITS_PER_CHAR CHAR_BIT
#endif /* __CHAR_BIT__ */

#if __SIZEOF_POINTER__-0 > 0
#  define BITS_PER_POINTER (__SIZEOF_POINTER__ * BITS_PER_CHAR)
#elif SIZEOF_VOID_P-0 > 0
#  define BITS_PER_POINTER (SIZEOF_VOID_P * BITS_PER_CHAR)
#else
# include <limits.h>
# if __WORDSIZE-0 > 0
#  define BITS_PER_POINTER (__WORDSIZE * BITS_PER_CHAR)
# else
#  error "couldn't find the size of pointers"
# endif
#endif

#define SOTC (sizeof(uint16_t))
#define BITS_PER_TCHAR (SOTC*BITS_PER_CHAR)

#include <stddef.h>
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#define anum(x) (sizeof((x))/sizeof(*(x)))

/* compiler LART */
#ifdef __GNUC__
# define barrier()	asm volatile ("")
# define mbarrier()	asm volatile ("" ::: "memory")
# define mem_barrier(x)	asm volatile ("": "=m" (*(x)))
#else
# define barrier()	do {} while (0)
# define mbarrier(x)	do {} while (0)
# define mem_barrier(x)	do {} while (0)
#endif

# ifdef I_LIKE_ASM
#  if defined(__i386__) || defined(__x86_64__)
#   define CPU_RELAX_CONTENT asm volatile("pause");
#  elif defined(__powerpc64__)
#   define CPU_RELAX_CONTENT asm volatile("or 1,1,1\n\tor 2,2,2");
#  elif defined(__ia64__)
#   define CPU_RELAX_CONTENT asm volatile ("hint @pause" ::: "memory");
#  elif defined(__sparc) || defined(__sparc__)
#   define CPU_RELAX_CONTENT asm volatile ("rd %%ccr, %%g0" : : : "memory");
#  else
#   define CPU_RELAX_CONTENT barrier();
#  endif
# else
#  define CPU_RELAX_CONTENT barrier();
# endif
static inline void cpu_relax(void)
{
	CPU_RELAX_CONTENT
}

#if !defined(HAVE_STRNLEN) || !defined(HAVE_MEMPCPY)
/*
 * According to man-page a GNU-Extension, mumbel mumbel...
 * They are right, not on 5.7 Solaris
 */
# include "my_bitops.h"
#endif /* HAVE_STRNLEN || HAVE_MEMPCPY */

# include "byteorder.h"
# include "unaligned.h"

#define str_it2(x)	#x
#define str_it(x)	str_it2(x)

#define DFUNC_NAME2(fname, add) fname##add
#define DFUNC_NAME(fname, add) DFUNC_NAME2(fname, add)
#define DVAR_NAME(fname, add) DFUNC_NAME2(fname, add)

#ifdef __linux__
# if HAVE_DECL_TCP_THIN_LINEAR_TIMEOUTS == 0
#  define TCP_THIN_LINEAR_TIMEOUTS 16
#  undef  HAVE_DECL_TCP_THIN_LINEAR_TIMEOUTS
#  define HAVE_DECL_TCP_THIN_LINEAR_TIMEOUTS 1
# endif
# if HAVE_DECL_TCP_THIN_DUPACK == 0
#  define TCP_THIN_DUPACK 17
#  undef  HAVE_DECL_TCP_THIN_DUPACK
#  define HAVE_DECL_TCP_THIN_DUPACK 1
# endif
# if HAVE_DECL_TCP_USER_TIMEOUT == 0
#  define TCP_USER_TIMEOUT 18
#  undef  HAVE_DECL_TCP_USER_TIMEOUT
#  define HAVE_DECL_TCP_USER_TIMEOUT 1
# endif
#endif

#endif /* _OTHER_H */
/* EOF */
