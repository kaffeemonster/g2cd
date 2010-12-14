/*
 * my_bitops.c
 * some nity grity bitops, generic fallback/nop
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

void *test_cpu_feature(const struct test_cpu_feature *t, size_t l)
{
	size_t i;

	for(i = 0; i < l; i++) {
		if(CFF_DEFAULT & t[i].flags)
			return t[i].func;
	}
	return NULL; /* die, sucker, die! */
}

static char const rcsid_mbg[] GCC_ATTR_USED_VAR = "$Id:$";
