/*
 * tstrchrnul.c
 * tstrchrnul
 *
 * Copyright (c) 2009 Jan Seiffert
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
 * tstrchrnul - strchr which also returns a pointer to '\0'
 * s: the string to search
 * c: the character to search
 *
 * return value: a pointer to the character c or to the '\0'
 *
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"
#include "tchar.h"

#ifdef I_LIKE_ASM
# include "generic/tstrchrnul.c"
#else
# include "generic/tstrchrnul.c"
#endif
