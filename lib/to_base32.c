/*
 * to_base32.c
 * convert binary string to base 32
 *
 * Copyright (c) 2010 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 * $Id: $
 */

/*
 * to_base32 - convert a binary string to a base32 utf-16 string
 * dst: memory where to write to
 * src: string to convert
 * num: number of bytes to convert
 *
 * return value: pointer behind the converted string
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"
#include "tchar.h"

#ifdef I_LIKE_ASM
//# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
//#  include "x86/to_base32.c"
//# elif defined(__powerpc__) || defined(__powerpc64__)
//#  include "ppc/to_base32.c"
//# elif defined(__alpha__)
//#  include "alpha/to_base32.c"
//# elif defined(__arm__)
//#  include "arm/to_base32.c"
//# elif defined(__ia64__)
//#  include "ia64/to_base32.c"
//# else
#  include "generic/to_base32.c"
//# endif
#else
# include "generic/to_base32.c"
#endif
