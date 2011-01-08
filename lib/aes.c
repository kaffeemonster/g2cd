/*
 * aes.c
 * AES routines
 *
 * Copyright (c) 2009-2011 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id: $
 */

#include <string.h>
#include "other.h"
#include "aes.h"

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/aes.c"
# elif defined(__arm__)
#  include "arm/aes.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/aes.c"
# else
	/*
	 * we could create a altivec version, but it would be slower most
	 * of the time and is patented by Microsoft (Galois Field opertion
	 * with SIMD)
	 */
#  include "generic/aes.c"
# endif
#else
# include "generic/aes.c"
#endif
