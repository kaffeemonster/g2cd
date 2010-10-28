#ifndef _LIB_UNALIGNED_H
# define _LIB_UNALIGNED_H

# include "byteorder.h"

# if !(defined(__GNUC__) || _SUNC_PREREQ(0x5100))
#  error your compiler will not compile unaligned access helper...
/*
 * all it needs is a little cleanup (don't use packed struct on non GNU
 * compatible compiler) and the __builtin_choose_expression removed from
 * __get_unaligned_*
 */
# endif

# if defined(__i386__) || defined(__x86_64__)
#  include "x86/unaligned.h"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/unaligned.h"
# elif defined(__sparc__) || defined(__sparc)
#  include "sparc/unaligned.h"
# elif defined(__mips)
#  include "mips/unaligned.h"
# elif defined(__arm__)
#  include "arm/unaligned.h"
# elif defined(__ia64__)
#  include "ia64/unaligned.h"
# elif defined(__hppa__) || defined(__hppa64__)
#  include "parisc/unaligned.h"
# elif defined(__alpha__)
#  include "alpha/unaligned.h"
# else
#  include "generic/unaligned.h"
# endif

#  define get_unaligned_endian(dest, ptr, big_end) \
do { if(!big_end) (dest) = __get_unaligned_le(ptr); else (dest) = __get_unaligned_be(ptr); } while(0)

#endif /* _LIB_UNALIGNED_H */
