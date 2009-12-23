#ifndef _LIB_GENERIC_UNALIGNED_H
# define _LIB_GENREIC_UNALIGNED_H

/*
 * This is the most generic implementation of unaligned accesses
 * and should work almost anywhere.
 *
 * If an architecture can handle unaligned accesses in hardware,
 * it may want to use the linux/unaligned/access_ok.h implementation
 * instead.
 */

# if __BYTE_ORDER == __LITTLE_ENDIAN
#  include <linux/unaligned/le_struct.h>
#  include <linux/unaligned/be_byteshift.h>
#  include <linux/unaligned/generic.h>
#  define get_unaligned	__get_unaligned_le
#  define put_unaligned	__put_unaligned_le
# elif __BYTE_ORDER == __BIG_ENDIAN
#  include <linux/unaligned/be_struct.h>
#  include <linux/unaligned/le_byteshift.h>
#  include <linux/unaligned/generic.h>
#  define get_unaligned	__get_unaligned_be
#  define put_unaligned	__put_unaligned_be
# else
#  error need to define endianess
# endif

#endif /* _LIB_GENERIC_UNALIGNED_H */
