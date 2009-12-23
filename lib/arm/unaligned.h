#ifndef _LIB_ARM_UNALIGNED_H
# define _LIB_ARM_UNALIGNED_H

# include "../unaligned/le_byteshift.h"
# include "../unaligned/be_byteshift.h"
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
