/*
 * mem_spn_ff.c
 * count 0xff span length, generic implementation
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

size_t mem_spn_ff(const void *s, size_t len)
{
	const unsigned char *p = s;

	if(len >= SOST)
	{
		size_t shift = SOST - ALIGN_DOWN_DIFF(p, SOST);
		const size_t *ps = (const size_t *)ALIGN_DOWN(p, SOST);
		size_t x;

		x = *ps++;
		if(HOST_IS_BIGENDIAN)
			x |= (~(size_t)0) << shift * BITS_PER_CHAR;
		else
			x |= (~(size_t)0) >> shift * BITS_PER_CHAR;
		if(likely((~(size_t)0) == x))
		{
			len     -= shift;

			for(; likely(len >= SOST); len -= SOST, ps++) {
				if(unlikely((~(size_t)0) != *ps))
					break;
			}
			if(SOST != SO32)
			{
				if(0xFFFFFFFFu == *((const uint32_t *)ps)) {
					ps = (const size_t *)(((const uint32_t *)ps)+1);
					len -= SO32;
				}
			}
			p = (const unsigned char *)ps;
		}
	}
	for(; len; len--, p++) {
		if(unlikely(0xFF != *p))
			break;
	}

	return (size_t)(p - (const unsigned char *)s);
}

static char const rcsid_msf[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
