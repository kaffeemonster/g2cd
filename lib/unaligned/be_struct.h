#ifndef _LIB_GENRIC_UNALIGNED_BE_STRUCT_H
# define _LIB_GENERIC_UNALIGNED_BE_STRUCT_H

# include "packed_struct.h"

static inline uint16_t get_unaligned_be16(const void *p)
{
	return __get_unaligned_cpu16((const uint8_t *)p);
}

static inline uint32_t get_unaligned_be32(const void *p)
{
	return __get_unaligned_cpu32((const uint8_t *)p);
}

static inline uint64_t get_unaligned_be64(const void *p)
{
	return __get_unaligned_cpu64((const uint8_t *)p);
}

static inline void put_unaligned_be16(uint16_t val, void *p)
{
	__put_unaligned_cpu16(val, p);
}

static inline void put_unaligned_be32(uint32_t val, void *p)
{
	__put_unaligned_cpu32(val, p);
}

static inline void put_unaligned_be64(uint64_t val, void *p)
{
	__put_unaligned_cpu64(val, p);
}

#endif /* _LIB_GENERIC_UNALIGNED_BE_STRUCT_H */
