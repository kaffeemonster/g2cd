/*
 * mempcpy.c
 * mempcpy for non-GNU platforms, genereic impl.
 *
 * Copyright (c) 2008-2019 Jan Seiffert
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
 * let the compiler get fancy at optimizing this.
 * if he doesn't know "restrict": it's not that often used
 * and propaply still better than strcpys and strcats
 * normaly used in this situation.
 */

static noinline GCC_ATTR_FASTCALL void *mempcpy_big(void *restrict dst, const void *restrict src, size_t len)
{
	size_t i;
	char *restrict dst_c = dst;
	const char *restrict src_c = src;

#if !(defined(COMPILER_IS_STUPID) || defined(restrict))
	/* if the compiler is good, he will do the right thing(TM) */
	for(i = (len/16)*16; i; i--)
		*dst_c++ = *src_c++;
	len %= 16;
#else
	i = ALIGN_DIFF(dst_c, SOST);
	switch(i & SOSTM1)
	{
	case 7: dst_c[6] = src_c[6]; GCC_FALL_THROUGH
	case 6: dst_c[5] = src_c[5]; GCC_FALL_THROUGH
	case 5: dst_c[4] = src_c[4]; GCC_FALL_THROUGH
	case 4: dst_c[3] = src_c[3]; GCC_FALL_THROUGH
	case 3: dst_c[2] = src_c[2]; GCC_FALL_THROUGH
	case 2: dst_c[1] = src_c[1]; GCC_FALL_THROUGH
	case 1: dst_c[0] = src_c[0]; GCC_FALL_THROUGH
	case 0: break;
	}
	dst_c += i; src_c += i; len -= i;

	if(!UNALIGNED_OK)
	{
		i = (((intptr_t)dst_c) & ((SOST * 2) - 1)) ^
		    (((intptr_t)src_c) & ((SOST * 2) - 1));
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
		size_t *dst_s = (size_t *)dst_c;
		const size_t *src_s = (const size_t *)src_c;
		size_t small_len = ((len / 16) * 16) / SOST;

		for(i = 0; i < small_len; i += SOST < 8 ? 4 : 2)
		{
			dst_s[i +  0] = src_s[i +  0];
			dst_s[i +  1] = src_s[i +  1];
			if(SOST < 8) {
				dst_s[i +  2] = src_s[i +  2];
				dst_s[i +  3] = src_s[i +  3];
			}
		}
		dst_c = (char *)(dst_s + i);
		src_c = (const char *)(src_s + i);
		len %= 16;
	}
alignment_failed:
	if(!UNALIGNED_OK) {
		for(i = 0; i < (len - 15); i++)
			dst_c[i] = src_c[i];
		dst_c += i; src_c += i; len -= i;
	}
#endif

	return cpy_rest(dst_c, src_c, len);
}

void *my_mempcpy(void *restrict dst, const void *restrict src, size_t len)
{
	if(likely(len < 16))
		return cpy_rest(dst, src, len);

	/* trick gcc to generate lean stack frame and do a tailcail */
	return mempcpy_big(dst, src, len);
}

static char const rcsid_mpcg[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
