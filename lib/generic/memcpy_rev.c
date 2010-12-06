/*
 * memcpy_rev.c
 * memcpy_rev
 *
 * Copyright (c) 2010 Jan Seiffert
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
 * a generic reverse memcpy.
 * It is important this thing copies in reverse! No matter whats
 * better for your HW!
 */
void *my_memcpy_rev(void *dst, const void *src, size_t len)
{
	size_t i;
	char *dst_c = (char *)dst + len;
	const char *src_c = (const char *)src + len;

	/*
	 * when we are here, we know dst is above src
	 * and thanks to len they overlap
	 *
	 * align write side
	 */
	if(len < SOST + 15)
		goto alignment_failed;

	i = ALIGN_DIFF(dst_c, SOST);
	dst_c -= i; src_c -= i; len -= i;
	switch(i & SOSTM1)
	{
	case 7: dst_c[6] = src_c[6];
	case 6: dst_c[5] = src_c[5];
	case 5: dst_c[4] = src_c[4];
	case 4: dst_c[3] = src_c[3];
	case 3: dst_c[2] = src_c[2];
	case 2: dst_c[1] = src_c[1];
	case 1: dst_c[0] = src_c[0];
	case 0: break;
	}

	if(!UNALIGNED_OK)
	{
		i = ALIGN_DOWN_DIFF(dst_c, SOST * 2) ^
		    ALIGN_DOWN_DIFF(src_c, SOST * 2);
		if(unlikely(i &  1))
			goto alignment_failed;
		if(unlikely(i &  2))
			goto alignment_failed;
		if(unlikely(i &  4)) {
			if(SOST < 8)
				goto alignment_size_t;
			else
				goto alignment_failed;
		}
		if(i & 8)
			goto alignment_size_t;
	}

alignment_size_t:
	if(likely(len > 15))
	{
		size_t *dst_s = (size_t *)dst_c, *end_p;
		const size_t *src_s = (const size_t *)src_c;
		size_t small_len = ((len / 16) * 16) / SOST;

		end_p = dst_s - small_len;
		do
		{
			if(SOST < 8) {
				*--dst_s = *--src_s;
				*--dst_s = *--src_s;
			}
			*--dst_s = *--src_s;
			*--dst_s = *--src_s;
		} while(dst_s > end_p);
		dst_c = (char *)dst_s;
		src_c = (const char *)src_s;
		len %= 16;
	}

alignment_failed:
	for(i = len; i; i--)
		*--dst_c = *--src_c;
	return dst;
}

static char const rcsid_mcrg[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
