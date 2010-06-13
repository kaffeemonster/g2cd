/*
 * tstrncmp.c
 * tstrncmp
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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

/*
 * tstrncmp - strncmp for tchars
 * s1: one string you want to compare
 * s2: other string you want to compare
 * n: the maximum length
 *
 * return value: an integer less than, equal to, or greater than zero
 * if s1 (or the first n bytes thereof) is found, respectivly, to be
 * less than, to match, or to be greater than s2.
 *
 * NOTE: we asume that the caller garantees that maxlen bytes
 *       are accessible at s
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"
#include "tchar.h"

#ifdef I_LIKE_ASM
# include "generic/tstrncmp.c"
#else
# include "generic/tstrncmp.c"
#endif
