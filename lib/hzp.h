/*
 * hzp.c
 * Header for the hzp interface.
 *
 * Copyright (c) 2006-2010 Jan Seiffert
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

#ifndef LIB_HZP_H
# define LIB_HZP_H

# include <stdbool.h>
# include "other.h"
# include "atomic.h"

# ifndef LIB_HZP_C
#  define LIB_HZP_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#  define LIB_HZP_EXTRNVAR(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define LIB_HZP_EXTRN(x) x GCC_ATTR_VIS("hidden")
#  define LIB_HZP_EXTRNVAR(x) x GCC_ATTR_VIS("hidden")
# endif

enum hzps
{
	HZP_EPOLL,
	HZP_QHT,
	HZP_QHTDAT,
	/* keep this the last entry!! */
	HZP_MAX
};

struct hzp_free
{
	atomicst_t st;
	void *data;
	void (*free_func)(void *);
};

struct hzp
{
	void * volatile ptr[HZP_MAX];
	struct hzp_flags
	{
		int used;
	} flags;
	atomicst_t lst;
};

# ifdef HAVE___THREAD
LIB_HZP_EXTRNVAR(__thread struct hzp local_hzp);
/*
 * MB:
 * User has to provide a full memory barrier, to
 * prevent the read of the new_ref missing updates and
 * the write of the ref getting reodered
 */
static inline void hzp_ref(enum hzps key, void *new_ref)
{
	if(key < HZP_MAX) {
		local_hzp.ptr[key] = new_ref;
	}
	wmb();
}
static inline void hzp_unref(enum hzps key)
{
	hzp_ref(key, NULL);
}
# else
LIB_HZP_EXTRN(void hzp_ref(enum hzps, void *));
LIB_HZP_EXTRN(void hzp_unref(enum hzps));
# endif

LIB_HZP_EXTRN(bool hzp_alloc(void));
LIB_HZP_EXTRN(void hzp_free(void));
# ifdef DEBUG_HZP_LANDMINE
# define hzp_deferfree(x, y, z) _hzp_deferfree(x, y, z, __func__, __LINE__)
LIB_HZP_EXTRN(void _hzp_deferfree(struct hzp_free *, void *, void (*)(void *), const char *, unsigned));
# else
LIB_HZP_EXTRN(void hzp_deferfree(struct hzp_free *, void *, void (*)(void *)) GCC_ATTR_FASTCALL);
# endif
LIB_HZP_EXTRN(int hzp_scan(int));
	
#endif
