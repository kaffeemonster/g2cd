#ifndef _LIB_POWERPC_UNALIGNED_H
# define _LIB_POWERPC_UNALIGNED_H

/* The PowerPC can do unaligned accesses itself in big endian mode. */
#include "../unaligned/access_ok.h"
#include "../unaligned/generic.h"

// TODO: PPC can do LE, and customers like it...
/*
 * There is demand by customers for LE because they have code for LE,
 * either they own messy shit they don't want to fix or prop. code
 * bought (codecs, GFX-Drivers, foo) from others they can not influence.
 * This is also true for other archs if they are not already LE.
 * Linux has no real support for PPC LE, but this may change...
 */
#define get_unaligned	__get_unaligned_be
#define put_unaligned	__put_unaligned_be

#endif /* _LIB_POWERPC_UNALIGNED_H */
