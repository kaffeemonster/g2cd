/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2004 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/*
 * This is not the original adler32.c from the zlib distribution,
 * but a slightly modified version. If you are looking for the
 * original, please go to zlib.net.
 */

#define ADLER32_C
#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/adler32.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/adler32.c"
# else
#  include "generic/adler32.c"
# endif
#else
# include "generic/adler32.c"
#endif

/* EOF */
