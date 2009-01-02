/*
 * my_bitops.c
 * some nity grity bitops, generic fallback/nop
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
 * $Id:$
 */

void *test_cpu_feature(const struct test_cpu_feature *t, size_t l)
{
	size_t i;
	for(i = 0; i < l; i++)
	{
		if(-1 == t[i].flags_needed) {
			if(t[i].callback && !t[i].callback)
				continue;
			return t[i].func;
		}
	}
	return NULL; /* die, sucker, die! */
}

static char const rcsid_mbg[] GCC_ATTR_USED_VAR = "$Id:$";
