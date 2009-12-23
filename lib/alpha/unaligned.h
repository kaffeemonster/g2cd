#ifndef _LIB_ALPHA_UNALIGNED_H
# define _LIB_ALPHA_UNALIGNED_H

/* the compiler does the fix up quite fine */
# include "../unaligned/le_struct.h"
# include "../unaligned/be_byteshift.h"
# include "../unaligned/generic.h"

# define get_unaligned __get_unaligned_le
# define put_unaligned __put_unaligned_le

#endif /* _LIB_ALPHA_UNALIGNED_H */
