/*
 * str_spn_space.c
 * count white space span length
 *
 * Copyright (c) 2012 Jan Seiffert
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
 * str_spn_space - count the span length of a white space run
 * str: string to search
 *
 * White space is according to the C POSIX locale space, tab, 
 * newline, carriage return, form feed and vertical tab.
 *
 * return value: the number of consecutive white space characters.
 */
#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifdef I_LIKE_ASM
# if 0
# elif defined(__alpha__)
#  include "alpha/str_spn_space.c"
# elif 0 && defined(__hppa__) || defined(__hppa64__)
#  include "parisc/str_spn_space.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/str_spn_space.c"
# elif defined(__tile__)
#  include "tile/str_spn_space.c"
# else
#  include "generic/str_spn_space.c"
# endif
#else
# include "generic/str_spn_space.c"
#endif

size_t str_spn_space(const char *str)
{
#ifndef STR_SPN_NO_PRETEST
	/*
	 * since most strings will have no whitspace,
	 * split a first test out and only go into the
	 * full mumbo jumbo if there is whitespace
	 */
	if(likely(!isspace_a(*(const unsigned char *)str)))
		return 0;
#endif
	return str_spn_space_slow(str);
}

/*
 * str_skip_space - skip the white spaec at the beginning of a string
 * str: string to search
 *
 * return value: a pointer on the first non white space character
 *               in the string.
 */
char *str_skip_space(char *str)
{
	return str + str_spn_space(str);
}

/* EOF */
