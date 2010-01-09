/*
 * aes.c
 * AES routines
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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

#include <string.h>
#include "other.h"
#include "aes.h"

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/aes.c"
# elif defined(__arm__)
#  include "arm/aes.c"
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
