/*
 * memmove.c
 * memmove
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
 * memmove - memmove handling overlaping copys
 * dst: where to copy to
 * src: from where to copy
 * len: how much to copy
 *
 * return value: a pointer to dst
 */

#define IN_STRWHATEVER
#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

/* memmove as a macro... */
#undef memmove
#ifndef MEMMOVE_DEFINED
void *memmove(void *dst, const void *src, size_t len);
#define MEMMOVE_DEFINED
#endif

#ifdef I_LIKE_ASM
#  include "generic/memmove.c"
#else
# include "generic/memmove.c"
#endif
/* EOF */
