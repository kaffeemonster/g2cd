#ifndef _LIB_GENERIC_UNALIGNED_MEMMOVE_H
# define _LIB_GENERIC_UNALIGNED_MEMMOVE_H

#include <string.h>

/* Use memmove here, so gcc does not insert a __builtin_memcpy. */

static inline uint16_t __get_unaligned_memmove16(const void *p)
{
	uint16_t tmp;
	memmove(&tmp, p, 2);
	return tmp;
}

static inline uint32_t __get_unaligned_memmove32(const void *p)
{
	uint32_t tmp;
	memmove(&tmp, p, 4);
	return tmp;
}

static inline uint64_t __get_unaligned_memmove64(const void *p)
{
	uint64_t tmp;
	memmove(&tmp, p, 8);
	return tmp;
}

static inline void __put_unaligned_memmove16(uint16_t val, void *p)
{
	memmove(p, &val, 2);
}

static inline void __put_unaligned_memmove32(uint32_t val, void *p)
{
	memmove(p, &val, 4);
}

static inline void __put_unaligned_memmove64(uint64_t val, void *p)
{
	memmove(p, &val, 8);
}

#endif /* _LIB_GENERIC_UNALIGNED_MEMMOVE_H */
