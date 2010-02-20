#ifndef _LIB_SWAB_H
# define _LIB_SWAB_H

# include "other.h"

/*
 * casts are necessary for constants, because we never know how for sure
 * how U/UL/ULL map to uint16_t, uint32_t, uint64_t. At least not in a portable way.
 */
# define ___constant_swab16(x) ((uint16_t)(				\
	(((uint16_t)(x) & (uint16_t)0x00ffU) << 8) |			\
	(((uint16_t)(x) & (uint16_t)0xff00U) >> 8)))

# define ___constant_swab32(x) ((uint32_t)(				\
	(((uint32_t)(x) & (uint32_t)0x000000ffUL) << 24) |		\
	(((uint32_t)(x) & (uint32_t)0x0000ff00UL) <<  8) |		\
	(((uint32_t)(x) & (uint32_t)0x00ff0000UL) >>  8) |		\
	(((uint32_t)(x) & (uint32_t)0xff000000UL) >> 24)))

# define ___constant_swab64(x) ((uint64_t)(				\
	(((uint64_t)(x) & (uint64_t)0x00000000000000ffULL) << 56) |	\
	(((uint64_t)(x) & (uint64_t)0x000000000000ff00ULL) << 40) |	\
	(((uint64_t)(x) & (uint64_t)0x0000000000ff0000ULL) << 24) |	\
	(((uint64_t)(x) & (uint64_t)0x00000000ff000000ULL) <<  8) |	\
	(((uint64_t)(x) & (uint64_t)0x000000ff00000000ULL) >>  8) |	\
	(((uint64_t)(x) & (uint64_t)0x0000ff0000000000ULL) >> 24) |	\
	(((uint64_t)(x) & (uint64_t)0x00ff000000000000ULL) >> 40) |	\
	(((uint64_t)(x) & (uint64_t)0xff00000000000000ULL) >> 56)))

/**
 * __swab16 - return a byteswapped 16-bit value
 * @x: value to byteswap
 */
# define __swab16(x)				\
	(GCC_CONSTANT_P((uint16_t)(x)) ?	\
	___constant_swab16(x) :			\
	__fswab16(x))

/**
 * __swab32 - return a byteswapped 32-bit value
 * @x: value to byteswap
 */
# define __swab32(x)				\
	(GCC_CONSTANT_P((uint32_t)(x)) ?	\
	___constant_swab32(x) :			\
	__fswab32(x))

/**
 * __swab64 - return a byteswapped 64-bit value
 * @x: value to byteswap
 */
# define __swab64(x)				\
	(GCC_CONSTANT_P((uint64_t)(x)) ?	\
	___constant_swab64(x) :			\
	__fswab64(x))

# ifdef HAVE_BYTESWAP_H
#  include <byteswap.h>
# endif

/*
 * Implement the following as inlines, but define the interface using
 * macros to allow constant folding when possible:
 * ___swab16, ___swab32, ___swab64, ___swahw32, ___swahb32
 */

static inline GCC_ATTR_CONST uint16_t __fswab16(uint16_t val)
{
# ifdef HAVE_BYTESWAP_H
	return bswap_16(val);
# else
	return ___constant_swab16(val);
# endif
}

static inline GCC_ATTR_CONST uint32_t __fswab32(uint32_t val)
{
# ifdef HAVE_BYTESWAP_H
	return bswap_32(val);
# else
	return ___constant_swab32(val);
# endif
}

static inline GCC_ATTR_CONST uint64_t __fswab64(uint64_t val)
{
# ifdef HAVE_BYTESWAP_H
#  if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ < 8
	uint32_t h = val >> 32;
	uint32_t l = val & ((1ULL << 32) - 1);
	return (((uint64_t)bswap_32(l)) << 32) | ((uint64_t)(bswap_32(h)));
#  else
	return bswap_64(val);
#  endif
# else
#  if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ < 8
	uint32_t h = val >> 32;
	uint32_t l = val & ((1ULL << 32) - 1);
	return (((uint64_t)___constant_swab32(l)) << 32) | ((uint64_t)(___constant_swab32(h)));
#  else
	return ___constant_swab64(val);
#  endif
# endif
}

/**
 * __swab16p - return a byteswapped 16-bit value from a pointer
 * @p: pointer to a naturally-aligned 16-bit value
 */
static inline uint16_t __swab16p(const uint16_t *p)
{
	return __swab16(*p);
}

/**
 * __swab32p - return a byteswapped 32-bit value from a pointer
 * @p: pointer to a naturally-aligned 32-bit value
 */
static inline uint32_t __swab32p(const uint32_t *p)
{
	return __swab32(*p);
}

/**
 * __swab64p - return a byteswapped 64-bit value from a pointer
 * @p: pointer to a naturally-aligned 64-bit value
 */
static inline uint64_t __swab64p(const uint64_t *p)
{
	return __swab64(*p);
}

#endif
