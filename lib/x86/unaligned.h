#ifndef _LIB_X86_UNALIGNED_H
# define _LIB_X86_UNALIGNED_H

/* The x86 can do unaligned accesses itself. */
# include "../unaligned/access_ok.h"
# include "../unaligned/generic.h"

# define get_unaligned __get_unaligned_le
# define put_unaligned __put_unaligned_le

#endif /* _LIB_X86_UNALIGNED_H */
