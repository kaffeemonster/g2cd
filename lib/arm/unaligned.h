#ifndef _LIB_ARM_UNALIGNED_H
# define _LIB_ARM_UNALIGNED_H

# ifdef __ARM_FEATURE_UNALIGNED
/*
 * >= ARMv6 (but not -M) can do unaligned access
 * (if the bits are set right...).
 * They also have the rev instruction (like the
 * x86 bswap).
 * access_ok.h should be fine, but to give the
 * compiler a heads up, we use the packed struct
 */
#  include "../unaligned/le_struct.h"
#  include "../unaligned/be_struct.h"
# else
#  include "../unaligned/le_byteshift.h"
#  include "../unaligned/be_byteshift.h"
# endif
# include "../unaligned/generic.h"

/* Select endianness */
# ifndef __ARMEB__
#  define get_unaligned	__get_unaligned_le
#  define put_unaligned	__put_unaligned_le
# else
#  define get_unaligned	__get_unaligned_be
#  define put_unaligned	__put_unaligned_be
# endif

#endif /* _LIB_ARM_UNALIGNED_H */
