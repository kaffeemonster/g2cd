/*
 * G2ConRegistry.h
 * header for the connection registry
 *
 * Copyright (c) 2008-2010 Jan Seiffert
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

#ifndef _G2CONREGISTRY_H
# define _G2CONREGISTRY_H

# include <stdbool.h>
# include "G2Connection.h"
# include "lib/combo_addr.h"

#define CONREG_LEVEL_COUNT g2_conreg_level_count

# ifndef _G2CONREGISTRY_C
#  define _G2CONREGISTRY_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#  define _G2CONREGISTRY_EXTRNVAR(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define _G2CONREGISTRY_EXTRN(x) x GCC_ATTR_VIS("hidden")
#  define _G2CONREGISTRY_EXTRNVAR(x) x GCC_ATTR_VIS("hidden")
# endif

_G2CONREGISTRY_EXTRNVAR(unsigned g2_conreg_level_count);
_G2CONREGISTRY_EXTRN(void g2_conreg_mark_dirty(g2_connection_t *));
_G2CONREGISTRY_EXTRN(bool g2_conreg_add(g2_connection_t *));
_G2CONREGISTRY_EXTRN(bool g2_conreg_remove(g2_connection_t *));
_G2CONREGISTRY_EXTRN(bool g2_conreg_promote_hub(g2_connection_t *));
_G2CONREGISTRY_EXTRN(void g2_conreg_demote_hub(g2_connection_t *));
_G2CONREGISTRY_EXTRN(intptr_t g2_conreg_all_hub(const union combo_addr *, intptr_t (*)(g2_connection_t *, void *), void *));
_G2CONREGISTRY_EXTRN(intptr_t g2_conreg_all_con(intptr_t (*)(g2_connection_t *, void *), void *));
_G2CONREGISTRY_EXTRN(intptr_t g2_conreg_random_hub(const union combo_addr *, intptr_t (*)(g2_connection_t *, void *), void *));
_G2CONREGISTRY_EXTRN(bool g2_conreg_is_neighbour_hub(const union combo_addr *addr));
_G2CONREGISTRY_EXTRN(intptr_t g2_conreg_for_addr(const union combo_addr *, intptr_t (*)(g2_connection_t *, void *), void *));
_G2CONREGISTRY_EXTRN(intptr_t g2_conreg_for_ip(const union combo_addr *, intptr_t (*)(g2_connection_t *, void *), void *));
_G2CONREGISTRY_EXTRN(bool g2_conreg_have_ip(const union combo_addr *));
_G2CONREGISTRY_EXTRN(void g2_conreg_cleanup(void));
_G2CONREGISTRY_EXTRN(void g2_conreg_init(void));

#endif
/* EOF */
