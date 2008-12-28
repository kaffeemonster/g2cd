/*
 * cpy_rest.c
 * copy a byte trailer
 *
 * Copyright (c) 2008 Jan Seiffert
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
 * cpy_rest - copy a low byte count from src to dst
 * dst: where to copy to
 * src: where to read from
 * i: how much bytes to copy
 *
 * return value: dst + i
 *
 * NOTE: handles at most 15 bytes!!
 *
 * This function is a little bit wired. On most copy routines
 * (memcpy, strcpy) at some point a trailer emerges. You know
 * the length, but it is odd and does not fit into the main
 * copy loop. Before repeating the trailer handling over and
 * over and over, put it together.
 */
/*
 * cpy_rest0 - copy a low byte count from src to dst and zero term
 * dst: where to copy to
 * src: where to read from
 * i: how much bytes to copy
 *
 * return value: dst + i
 *
 * NOTE: handles at most 15 bytes!!
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/cpy_rest.c"
# else
#  include "generic/cpy_rest.c"
# endif
#else
# include "generic/cpy_rest.c"
#endif
