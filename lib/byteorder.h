#ifndef _GENERIC_BYTEORDER_H
# define _GENERIC_BYTEORDER_H

# include "other.h"

# ifdef __linux__
#  include <endian.h>
# elif defined(__NetBSD__) || defined(__OpenBSD__)
// TODO: put FreeBSD here too?
#  include <sys/endian.h>
# elif defined(__FreeBSD__) || defined(BSD)
#  include <machine/endian.h>
# elif defined(__sun__)
#  include <sys/isa_defs.h>
#  define __LITTLE_ENDIAN 1234
#  define __BIG_ENDIAN 4321
#  ifdef _LITTLE_ENDIAN
#   define __BYTE_ORDER __LITTLE_ENDIAN
#  elif defined(_BIG_ENDIAN)
#   define __BYTE_ORDER __BIG_ENDIAN
#  else
#   error "Oh solaris, your header are ..."
#  endif
# elif defined(WIN32)
/*
 * maybe the NT kernel can be ported to big endian,
 * but i think we will never see a "Windows" which
 * runs on big endian...
 */
#  define __LITTLE_ENDIAN 1234
#  define __BIG_ENDIAN    4321
#  define __BYTE_ORDER __LITTLE_ENDIAN
# endif

/* maybe we picked it up by accident, otherwise give up */
# if !(defined(__BYTE_ORDER) || defined(BYTE_ORDER) || defined(_BYTE_ORDER))
#  error "no byte order info for your plattform, giving up!"
# endif
/* BSDs sometimes calls it BYTE_ORDER, fix up */
# ifndef __BYTE_ORDER
#  ifdef _BYTE_ORDER
#   define __BYTE_ORDER  _BYTE_ORDER
#  else
#   define __BYTE_ORDER  BYTE_ORDER
#  endif
# endif
# ifndef __LITTLE_ENDIAN
#  ifdef _LITTLE_ENDIAN
#   define __LITTLE_ENDIAN _LITTLE_ENDIAN
#  else
#   define __LITTLE_ENDIAN  LITTLE_ENDIAN
#  endif
# endif
# ifndef __BIG_ENDIAN
#  ifdef _BIG_ENDIAN
#   define __BIG_ENDIAN _BIG_ENDIAN
#  else
#   define __BIG_ENDIAN  BIG_ENDIAN
#  endif
# endif

# define __le16 uint16_t
# define __le32 uint32_t
# define __le64 uint64_t
# define __be16 uint16_t
# define __be32 uint32_t
# define __be64 uint64_t

# if __BYTE_ORDER == __LITTLE_ENDIAN
#  include "generic/little_endian.h"
# elif __BYTE_ORDER == __BIG_ENDIAN
#  include "generic/big_endian.h"
# else
#  error "You're kidding, you don't own a PDP11?"
# endif

#endif
