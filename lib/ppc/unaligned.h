#ifndef _LIB_POWERPC_UNALIGNED_H
# define _LIB_POWERPC_UNALIGNED_H

/* The PowerPC can do unaligned accesses itself in big endian mode. */
#include "../unaligned/access_ok.h"
#include "../unaligned/generic.h"

#define get_unaligned	__get_unaligned_be
#define put_unaligned	__put_unaligned_be

#endif /* _LIB_POWERPC_UNALIGNED_H */
