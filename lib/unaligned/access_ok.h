#ifndef _LIB_GENERIC_UNALIGNED_ACCESS_OK_H
# define _LIB_GENERIC_UNALIGNED_ACCESS_OK_H

#include "../byteorder.h"

static inline uint16_t get_unaligned_le16(const void *p)
{
	return le16_to_cpup((const my_le16 *)p);
}

static inline uint32_t get_unaligned_le32(const void *p)
{
	return le32_to_cpup((const my_le32 *)p);
}

static inline uint64_t get_unaligned_le64(const void *p)
{
	return le64_to_cpup((const my_le64 *)p);
}

static inline uint16_t get_unaligned_be16(const void *p)
{
	return be16_to_cpup((const my_be16 *)p);
}

static inline uint32_t get_unaligned_be32(const void *p)
{
	return be32_to_cpup((const my_be32 *)p);
}

static inline uint64_t get_unaligned_be64(const void *p)
{
	return be64_to_cpup((const my_be64 *)p);
}

static inline void put_unaligned_le16(uint16_t val, void *p)
{
	*((my_le16 *)p) = cpu_to_le16(val);
}

static inline void put_unaligned_le32(uint32_t val, void *p)
{
	*((my_le32 *)p) = cpu_to_le32(val);
}

static inline void put_unaligned_le64(uint64_t val, void *p)
{
	*((my_le64 *)p) = cpu_to_le64(val);
}

static inline void put_unaligned_be16(uint16_t val, void *p)
{
	*((my_be16 *)p) = cpu_to_be16(val);
}

static inline void put_unaligned_be32(uint32_t val, void *p)
{
	*((my_be32 *)p) = cpu_to_be32(val);
}

static inline void put_unaligned_be64(uint64_t val, void *p)
{
	*((my_be64 *)p) = cpu_to_be64(val);
}

#endif /* _LIB_GENERIC_UNALIGNED_ACCESS_OK_H */
