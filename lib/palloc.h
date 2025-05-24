/*
 * palloc.h
 * Header for pad allocator
 *
 * Copyright (c) 2012-2015 Jan Seiffert
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
 * $Id:$
 */

#ifndef PALLOC_H
# define PALLOC_H

# include "other.h"

/*
 * for the pad we have 1<<19 (512kb) and 1<<16 (64kb) for the window
 */
# define PA_W ((((1u<<19)+(1u<<16))+(sizeof(ssize_t)-1))/sizeof(ssize_t))
# define PA_SMAX 2048u /* power of 2, ~W/100 */
# define PA_SMAX21 ((2*PA_SMAX)-1)

typedef int32_t pa_address;
typedef uint32_t pa_exseg;
typedef uint32_t pa_seg;

struct d_heap
{
	unsigned int wordsperseg;
	pa_seg s; /* segments in use */
	pa_address pa[PA_SMAX+1]; /* pointer array */
	pa_address st[PA_SMAX21+1]; /* segment tree */
	ssize_t v[PA_W+1] GCC_ATTR_ALIGNED(8); /* main DS area */
};

# ifndef PALLOC_C
#  define PALLOC_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define PALLOC_EXTRN(x) x GCC_ATTR_VIS("hidden")
# endif

PALLOC_EXTRN(void pa_init(struct d_heap *d));
PALLOC_EXTRN(void *pa_alloc(void *opaque, unsigned int num, unsigned int size) GCC_ATTR_MALLOC GCC_ATTR_ALLOC_SIZE2(2, 3) GCC_ASSUME_ALIGNED(sizeof(ssize_t)));
PALLOC_EXTRN(void pa_free(void *opaque, void *addr));
#endif
