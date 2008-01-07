/*
 * other.h
 * some C-header-magic-glue
 *
 * Copyright (c) 2004, 2005 Jan Seiffert
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
 * $Id: other.h,v 1.16 2005/11/04 06:04:43 redbully Exp redbully $
 */

#ifndef _OTHER_H
#define _OTHER_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#ifndef HAVE_C99
/* Non C99 compiler with no extensions don't know about the following keywords */
# ifdef __GNUC__
#  define DYN_ARRAY_LEN 0
# else
#  define DYN_ARRAY_LEN 1
# endif	/* __GNUC__ */
#else
# define DYN_ARRAY_LEN
#endif /* HAVE_C99 */

/*
 * some Magic for determining the attrib-capabilitie of an gcc or just set it
 * to nothing, if we are not capable of it
 */
#if !defined __GNUC__ || __GNUC__ < 2
# define __attribute__(xyz)	/* Ignore */
#endif

#ifdef __GNUC__
# define GCC_ATTRIB(x) __attribute__((x))
#endif /* __GNUC__ */

#if defined __GNUC__ && defined __GNUC_MINOR__
# define _GNUC_PREREQ(maj, min) \
	((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
# define _GNUC_PREREQ(maj, min) 0
#endif  /* defined __GNUC__ && defined __GNUC_MINOR__ */

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

#if _GNUC_PREREQ (3,1)
# define GCC_ATTR_UNUSED_PARAM(type, name) type name GCC_ATTRIB(__unused__)
#else
/* hopefully all compiler know that foo(int) means a unused var, no, seems to be a SUN-Spezial */
# ifdef __SUNCC /* how does the SUN-Compiler calls himself? */
#  define GCC_ATTR_UNUSED_PARAM(type, name) type
# else
#  define GCC_ATTR_UNUSED_PARAM(type, name) type name
# endif /* __SUNCC */
#endif /* _GNUC_PREREQ (3,1) */

/* Cygwin (since it generates PE-files) doens't know about visibility */
#if _GNUC_PREREQ (3,3) && !__CYGWIN__
# define GCC_ATTR_VIS(x) GCC_ATTRIB(__visibility__ (x))
#else
# define GCC_ATTR_VIS(x)
#endif /* _GNUC_PREREQ (3,3) && !__CYGWIN__ */

#if _GNUC_PREREQ (2,3)
# define GCC_ATTR_PRINTF(str_ind, to_check) GCC_ATTRIB(format (__printf__ , (str_ind), (to_check)))
#else
# define GCC_ATTR_PRINTF(str_ind, to_check)
#endif /* _GNUC_PREREQ (2,3) */

#if _GNUC_PREREQ (2,5)
# define GCC_ATTR_CONST GCC_ATTRIB(__const__)
#else
# define GCC_ATTR_CONST
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
# define GCC_ATTR_CONSTRUCT GCC_ATTRIB(__constructor__)
#else
# define GCC_ATTR_CONSTRUCT
#endif

#if _GNUC_PREREQ (2,7)
# define GCC_ATTR_DESTRUCT GCC_ATTRIB(__destructor__)
#else
# define GCC_ATTR_DESTRUCT
#endif

#if _GNUC_PREREQ (2,7)
# define GCC_ATTR_FASTCALL GCC_ATTRIB(__regparm__(3))
#else
# define GCC_ATTR_FASTCALL
#endif

#if _GNUC_PREREQ (3,1)
# define always_inline inline GCC_ATTRIB(__always_inline__)
#else
# define always_inline inline
#endif

#ifndef HAVE_INET6_ADDRSTRLEN
/*
 * This buffersize is needed, but we not have it everywere, even on Systems
 * that claims to be IPv6 capable...
 */
# define INET6_ADDRSTRLEN 46
#endif /* HAVE_INET6_ADDRSTRLEN */

#ifndef	HAVE_INET_NTOP
/*
 * const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt);
 * for cygwin & 5.7 solaris
 * Wuerg-Around
 */
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <string.h>
# define inet_ntop(a, b, c, d) strncpy(c, inet_ntoa((*(b))), d)
#endif /* HAVE_INET_NTOP */

#ifndef HAVE_INET_PTON
/*
 * static __inline__ int inet_pton(int af, const char *src, void *dst);
 * more Wuerg-Around
 */
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# ifndef	HAVE_INET_ATON	/* for really stupid systems */
static __inline__ int inet_pton(int af, const char *src, void *dest)
{
	unsigned long result = inet_addr(src);
	af = af; /* to make compiler happy */

	if((unsigned long)-1 == result)
		return -1;
	
	if(dest)
		((struct in_addr *)dest)->s_addr = result;

	return 1;
}
# else	/* for cygwin and solaris */
#  define inet_pton(a, b, c) inet_aton(b, c)
# endif /* HAVE_INET_ATON */
#endif /* HAVE_INET_PTON */

#ifndef HAVE_STRNLEN
/*
 * According to man-page a GNU-Extension, mumbel mumbel...
 * They are right, not on 5.7 Solaris
 */
#include "lib/my_bitops.h"
#endif /* HAVE_STRNLEN */

/* 
 * if we are on a SMP-System (also HT) take a spinlock for
 * short locks, else a mutex, since our only Processor shouldn't
 * active spin-wait for itself
 */
#if defined HAVE_SMP && defined HAVE_SPINLOCKS
# define shortlock_t	pthread_spinlock_t
# define shortlock_t_init(da_lock)	pthread_spin_init (da_lock, PTHREAD_PROCESS_PRIVATE)
# define shortlock_t_destroy(da_lock)	pthread_spin_destroy(da_lock)
# define shortlock_t_lock(da_lock)	pthread_spin_lock(da_lock)
# define shortlock_t_unlock(da_lock)	pthread_spin_unlock(da_lock)
#else
# define shortlock_t	pthread_mutex_t
# define shortlock_t_init(da_lock)	pthread_mutex_init (da_lock, NULL)
# define shortlock_t_destroy(da_lock)	pthread_mutex_destroy(da_lock)
# define shortlock_t_lock(da_lock)	pthread_mutex_lock(da_lock)
# define shortlock_t_unlock(da_lock)	pthread_mutex_unlock(da_lock)
#endif /* HAVE_SMP */

#ifndef HAVE_SIGHANDLER_T
# ifdef __FreeBSD__
#  define sighandler_t sig_t
# else
typedef void (*sighandler_t)(int);
# endif
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#else
# include <inttypes.h>
#endif /* HAVE_STDINT_H */

/* most things won't work if BITS_PER_CHAR = 8 isn't true, but you never know */
#ifdef __CHAR_BIT__
# if __CHAR_BIT__ > 0
#  define BITS_PER_CHAR __CHAR_BIT__
# else
#  include <limits.h>
#  define BITS_PER_CHAR CHAR_BIT
# endif /* __CHAR_BIT__ > 0 */
#else
# include <limits.h>
# define BITS_PER_CHAR CHAR_BIT
#endif /* __CHAR_BIT__ */

#include <stddef.h>
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#define anum(x) (sizeof((x))/sizeof(*(x)))

/* unaligned access */
#ifdef __GNUC__
# ifdef __linux__
#  include <endian.h>
# else
#  include <sys/endian.h>
# endif
/* let the compiler handle unaligned access */
# define get_unaligned(dest, ptr) \
do { \
	struct { \
		__typeof__(*(ptr)) __v GCC_ATTR_PACKED; \
	} *__p = (void *)(ptr); \
	(dest) = __p->__v; \
} while(0)

# if (__BYTE_ORDER == __LITTLE_ENDIAN)
#  define get_unaligned_endian(dest, ptr, big_end) \
do { \
	if(!big_end) \
		get_unaligned((dest), (ptr)); \
	else { \
		switch(sizeof(*(ptr)) * BITS_PER_CHAR) { \
		case 32: \
			(dest) = (((__typeof__(*(ptr)))(*(char *)(ptr))&0xFF) << 24) | \
				(((__typeof__(*(ptr)))(*(((char *)(ptr))+1))&0xFF) << 16) | \
				(((__typeof__(*(ptr)))(*(((char *)(ptr))+2))&0xFF) << 8) | \
				((__typeof__(*(ptr)))(*(((char *)(ptr))+3))&0xFF); \
			break; \
		case 16: \
			(dest) = (((__typeof__(*(ptr)))(*(char *)(ptr))&0xFF) << 8) | \
				((__typeof__(*(ptr)))(*(((char *)(ptr))+1))&0xFF); \
				break; \
		default: \
			{ \
				unsigned i = 0, mask = (sizeof(*(ptr)) * BITS_PER_CHAR) - 8; \
				for((dest) = 0; i < sizeof(*(ptr)); i++, mask -= 8) \
					(dest) |= ((__typeof__(*(ptr)))(((char *)(ptr))[i]&0xFF)) << mask; \
			} \
		} \
	} \
} while(0)
# elif (__BYTE_ORDER == __BIG_ENDIAN)
#  define get_unaligned_endian(dest, ptr, big_end) \
do { \
	if(big_end) \
		get_unaligned((dest), (ptr)); \
	else { \
		switch(sizeof(*(ptr)) * BITS_PER_CHAR) { \
		case 32: \
			(dest) = (((__typeof__(*(ptr)))(*(((char *)(ptr))+4))&0xFF) << 24) | \
				(((__typeof__(*(ptr)))(*(((char *)(ptr))+3))&0xFF) << 16) | \
				(((__typeof__(*(ptr)))(*(((char *)(ptr))+2))&0xFF) << 8) | \
				((__typeof__(*(ptr)))(*(char *)(ptr))&0xFF); \
			break; \
		case 16: \
			(dest) = (((__typeof__(*(ptr)))(*(((char *)(ptr))+1))&0xFF) << 8) | \
				((__typeof__(*(ptr)))(*(char *)(ptr))&0xFF); \
			break; \
		default: \
			{ \
				unsigned i = 0; \
				for((dest) = 0; i < sizeof(*(ptr)); i++) \
					(dest) |= ((__typeof__(*(ptr)))(((char *)(ptr))[i]&0xFF)) << (i * 8); \
			} \
		} \
	} \
} while(0)
# else
#  error "You're kidding, you don't own a PDP11?"
# endif 

# define put_unaligned(val, ptr) \
do { \
	struct { \
		__typeof__(*(ptr)) __v GCC_ATTR_PACKED; \
	}  *__p = (void *)(ptr); \
	__p->__v = (val); \
} while(0)
#else
# error "No unaligned access macros for your machine/compiler written yet"
#endif /**/

#endif /* _OTHER_H */
/* EOF */
