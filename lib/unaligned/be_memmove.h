#ifndef _LIB_GENERIC_UNALIGNED_BE_MEMMOVE_H
# define _LIB_GENERIC_UNALIGNED_BE_MEMMOVE_H

#include "memmove.h"

static inline uint16_t get_unaligned_be16(const void *p)
{
	return __get_unaligned_memmove16((const uint8_t *)p);
}

static inline uint32_t get_unaligned_be32(const void *p)
{
	return __get_unaligned_memmove32((const uint8_t *)p);
}

static inline uint64_t get_unaligned_be64(const void *p)
{
	return __get_unaligned_memmove64((const uint8_t *)p);
}

static inline void put_unaligned_be16(uint16_t val, void *p)
{
	__put_unaligned_memmove16(val, p);
}

static inline void put_unaligned_be32(uint32_t val, void *p)
{
	__put_unaligned_memmove32(val, p);
}

static inline void put_unaligned_be64(uint64_t val, void *p)
{
	__put_unaligned_memmove64(val, p);
}

#endif /* _LIB_GENERIC_UNALIGNED_LE_MEMMOVE_H */
