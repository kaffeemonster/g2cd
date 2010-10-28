#ifndef _LIB_PARISC_UNALIGNED_H
# define _LIB_PARISC_UNALIGNED_H

# include "../unaligned/be_struct.h"
# include "../unaligned/le_byteshift.h"
# include "../unaligned/generic.h"

// TODO: parisc has the e-bit, so can work in either endian mode?
# define get_unaligned	__get_unaligned_be
# define put_unaligned	__put_unaligned_be

#endif /* _LIB_PARISC_UNALIGNED_H */
