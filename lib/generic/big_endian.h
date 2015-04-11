#ifndef _LIB_GENERIC_BIG_ENDIAN_H
# define _LIB_GENERIC_BIG_ENDIAN_H

# include "../swab.h"

# define HOST_IS_BIGENDIAN 1
# if defined(HAVE_BIT_INSTR) && _GNUC_PREREQ (3,4)
#  define nul_byte_index32(x) ((unsigned)__builtin_clz(x)/BITS_PER_CHAR)
#  define nul_byte_index(x) ((unsigned)__builtin_clzl(x)/BITS_PER_CHAR)
#  define nul_byte_index64(x) ((unsigned)__builtin_clzll(x)/BITS_PER_CHAR)
#  define nul_word_index32(x) ((unsigned)__builtin_clz(x)/(BITS_PER_CHAR * 2))
#  define nul_word_index(x) ((unsigned)__builtin_clzl(x)/(BITS_PER_CHAR * 2))
#  define nul_word_index64(x) ((unsigned)__builtin_clzll(x)/(BITS_PER_CHAR * 2))
#  define nul_byte_index32_last(x) (3u-((unsigned)__builtin_ctz(x)/BITS_PER_CHAR))
#  define nul_byte_index_last(x) ((sizeof(x)-1u)-((unsigned)__builtin_ctzl(x)/BITS_PER_CHAR))
#  define nul_word_index32_last(x) (1u-((unsigned)__builtin_ctz(x)/(BITS_PER_CHAR * 2)))
#  define nul_word_index_last(x) (((sizeof(x)-1u)/2)-((unsigned)__builtin_ctzl(x)/(BITS_PER_CHAR * 2)))
# else
#  define nul_byte_index32(x) nul_byte_index_b32(x)
#  define nul_byte_index(x) (4 >= sizeof(x) ? nul_byte_index_b32(x) : nul_byte_index_b64(x))
#  define nul_byte_index64(x) nul_byte_index_b64(x)
#  define nul_word_index32(x) nul_word_index_b32(x)
#  define nul_word_index(x) (4 >= sizeof(x) ? nul_word_index_b32(x) : nul_word_index_b64(x))
#  define nul_word_index64(x) nul_word_index_b64(x)
#  define nul_byte_index32_last(x) (3u-nul_byte_index_l32(x))
#  define nul_byte_index_last(x) ((sizeof(x)-1u)-(4 >= sizeof(x) ? nul_byte_index_l32(x) : nul_byte_index_l64(x)))
#  define nul_word_index32_last(x) (1u-nul_word_index_l32(x))
#  define nul_word_index_last(x) (((sizeof(x)-1u)/2)-(4 >= sizeof(x) ? nul_word_index_l32(x) : nul_word_index_l64(x)))
# endif

#define cpu_to_le64(x) ((my_le64)__swab64((x)))
#define le64_to_cpu(x) __swab64((uint64_t)(my_le64)(x))
#define cpu_to_le32(x) ((my_le32)__swab32((x)))
#define le32_to_cpu(x) __swab32((uint32_t)(my_le32)(x))
#define cpu_to_le16(x) ((my_le16)__swab16((x)))
#define le16_to_cpu(x) __swab16((uint16_t)(my_le16)(x))
#define cpu_to_be64(x) ((my_be64)(uint64_t)(x))
#define be64_to_cpu(x) ((uint64_t)(my_be64)(x))
#define cpu_to_be32(x) ((my_be32)(uint32_t)(x))
#define be32_to_cpu(x) ((uint32_t)(my_be32)(x))
#define cpu_to_be16(x) ((my_be16)(uint16_t)(x))
#define be16_to_cpu(x) ((uint16_t)(my_be16)(x))

static inline my_le64 cpu_to_le64p(const uint64_t *p)
{
	return (my_le64)__swab64p(p);
}
static inline uint64_t le64_to_cpup(const my_le64 *p)
{
	return __swab64p((const uint64_t *)p);
}
static inline my_le32 cpu_to_le32p(const uint32_t *p)
{
	return (my_le32)__swab32p(p);
}
static inline uint32_t le32_to_cpup(const my_le32 *p)
{
	return __swab32p((const uint32_t *)p);
}
static inline my_le16 cpu_to_le16p(const uint16_t *p)
{
	return (my_le16)__swab16p(p);
}
static inline uint16_t le16_to_cpup(const my_le16 *p)
{
	return __swab16p((const uint16_t *)p);
}
static inline my_be64 cpu_to_be64p(const uint64_t *p)
{
	return (my_be64)*p;
}
static inline uint64_t be64_to_cpup(const my_be64 *p)
{
	return (uint64_t)*p;
}
static inline my_be32 cpu_to_be32p(const uint32_t *p)
{
	return (my_be32)*p;
}
static inline uint32_t be32_to_cpup(const my_be32 *p)
{
	return (uint32_t)*p;
}
static inline my_be16 cpu_to_be16p(const uint16_t *p)
{
	return (my_be16)*p;
}
static inline uint16_t be16_to_cpup(const my_be16 *p)
{
	return (uint16_t)*p;
}

#endif /* _LIB_GENERIC_BIG_ENDIAN_H */
