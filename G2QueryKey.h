/*
 * G2QueryKey.h
 * header for the query key stuff
 *
 * Copyright (c) 2008-2009 Jan Seiffert
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
 * $Id:$
 */

#ifndef _G2QUERYKEY_H
# define _G2QUERYKEY_H

# include <stdbool.h>
# include "lib/combo_addr.h"

# ifndef _G2QUERYKEY_C
#  define _G2QUERYKEY_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define _G2QUERYKEY_EXTRN(x) x GCC_ATTR_VIS("hidden")
# endif

_G2QUERYKEY_EXTRN(void g2_qk_init(void));
_G2QUERYKEY_EXTRN(void g2_qk_tick(void));
_G2QUERYKEY_EXTRN(uint32_t g2_qk_generate(const union combo_addr *source));
_G2QUERYKEY_EXTRN(bool g2_qk_check(const union combo_addr *source, uint32_t key));
_G2QUERYKEY_EXTRN(bool g2_qk_lookup(uint32_t *qk, const union combo_addr *addr));

#endif
/* EOF */
