#ifndef _LIB_SPARC_UNALIGNED_H
# define _LIB_SPARC_UNALIGNED_H

# include <../unaligned/be_struct.h>
# include <../unaligned/le_byteshift.h>
# include <../unaligned/generic.h>
# define get_unaligned	__get_unaligned_be
# define put_unaligned	__put_unaligned_be

#endif /* _LIB_SPARC_UNALIGNED_H */
