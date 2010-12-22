#ifndef _LIB_GENERIC_UNALIGNED_PACKED_STRUCT_H
# define _LIB_GENERIC_UNALIGNED_PACKED_STRUCT_H

struct __una_u16 { uint16_t x; } __attribute__((packed));
struct __una_u32 { uint32_t x; } __attribute__((packed));
struct __una_u64 { uint64_t x; } __attribute__((packed));

static inline uint16_t __get_unaligned_cpu16(const void *p)
{
	const struct __una_u16 *ptr = (const struct __una_u16 *)p;
	return ptr->x;
}

static inline uint32_t __get_unaligned_cpu32(const void *p)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *)p;
	return ptr->x;
}

static inline uint64_t __get_unaligned_cpu64(const void *p)
{
	const struct __una_u64 *ptr = (const struct __una_u64 *)p;
	return ptr->x;
}

static inline void __put_unaligned_cpu16(uint16_t val, void *p)
{
	struct __una_u16 *ptr = (struct __una_u16 *)p;
	ptr->x = val;
}

static inline void __put_unaligned_cpu32(uint32_t val, void *p)
{
	struct __una_u32 *ptr = (struct __una_u32 *)p;
	ptr->x = val;
}

static inline void __put_unaligned_cpu64(uint64_t val, void *p)
{
	struct __una_u64 *ptr = (struct __una_u64 *)p;
	ptr->x = val;
}

#endif /* _LIB_GENERIC_UNALIGNED_PACKED_STRUCT_H */
