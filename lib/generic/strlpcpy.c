/*
 * strlpcpy.c
 * strlpcpy for efficient concatination, generic implementation
 *
 * Copyright (c) 2008-2011 Jan Seiffert
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

#define ALIGN_WANTED SOST

char *strlpcpy(char *dst, const char *src, size_t maxlen)
{
	size_t i;
	size_t r;

	prefetch(src);
	prefetchw(dst);
	if(unlikely(!src || !dst || !maxlen))
		return dst;

	if(unlikely(maxlen < SOST))
		goto OUT;

	if(UNALIGNED_OK)
	{
		/*
		 * We can do unaligned access, busily start doing
		 * something.
		 * But make sure we do not cross a page boundry
		 * on the source side. (we simply assume 4k pages)
		 */
		if(IS_ALIGN(src, SOST))
			goto DO_LARGE;

		/* quickly go to the page boundry */
		i = ALIGN_DIFF(src, 4096);
		i = i < maxlen ? i : maxlen;
		for(; likely(SOSTM1 < i); i -= SOST, maxlen -= SOST, src += SOST, dst += SOST)
		{
			size_t c = get_unaligned((const size_t *)src);
			r = has_nul_byte(c);
			if(r)
				return cpy_rest0(dst, src, nul_byte_index(r));
			put_unaligned(c, (size_t *)dst);
		}

		/* slowly go over the page boundry */
		for(; i && *src; i--, maxlen--)
			*dst++ = *src++;

DO_LARGE:
		if(likely(*src))
		{
			size_t *dst_b = (size_t *)dst;
			const size_t *src_b = (const size_t *)src;

			i = maxlen / SOST;
			for(; likely(i); i--, src_b++, dst_b++)
			{
				size_t c = *src_b;
				r = has_nul_byte(c);
				if(r)
					return cpy_rest0((char *)dst_b, (const char *)src_b, nul_byte_index(r));
				/*
				 * Unaligned writes are "more bad" then unaligned reads, but
				 * the dance with the page boundery sucks, as long as these
				 * strings are not super long...
				 */
				put_unaligned(c, dst_b);
			}

			maxlen = i * SOST + (maxlen % SOST);
			dst = (char *)dst_b;
			src = (const char *)src_b;
		}
	}
	else
	{
		size_t *dst_b;
		const size_t *src_b;

		/*
		 * Unaligned access is not ok.
		 * Blindly try to align src and check the outcome
		 */
		i = ALIGN_DIFF(src, ALIGN_WANTED);
		for(; maxlen && i && *src; maxlen--, i--)
			*dst++ = *src++;
		/* something to copy left? */
		if(unlikely(!(*src && maxlen)))
			goto OUT_SET;
		src_b = (const size_t *)src;
		/* look what alignment we achieved */
		if(ALIGN_DOWN_DIFF(dst, ALIGN_WANTED) ^ ALIGN_DOWN_DIFF(src, ALIGN_WANTED))
		{
			/* dst is still unaligned ... */
			size_t t, te, u;
			unsigned align, shift1, shift2;

			i      = 0;
			align  = SOST - (unsigned)ALIGN_DOWN_DIFF(dst, SOST);
			shift1 = BITS_PER_CHAR * align;
			shift2 = BITS_PER_CHAR * (SOST - align);
			dst_b  = (size_t *)ALIGN_DOWN(dst, SOST);
			te     = *dst_b;
			if(HOST_IS_BIGENDIAN)
				te >>= shift1;
			else
				te <<= shift1;
			do
			{
				t = te;
				te = *src_b++;
				r = has_nul_byte(te);
				if(HOST_IS_BIGENDIAN)
					t = t << shift1 | te >> shift2;
				else
					t = t >> shift1 | te << shift2;
				if(r) {
					r = nul_byte_index(r) + 1;
					break;
				}
				if(i + SOST > maxlen)
					break;
				i += SOST;
				*dst_b++ = t;
			} while(1);
			if(!r)
				u = maxlen - i;
			else {
				u = r;
				r = 1;
			}
			if(u >= align)
			{
				*dst_b++ = t;
				u -= align;
				if(!u)
					return (char *)dst_b - r;
				t = te;
				if(HOST_IS_BIGENDIAN)
					t = t << shift1;
				else
					t = t >> shift1;
				u = u;
			}
			else
				u = u + (SOST - align);
			align = SOST - u;
			te = *dst_b;
			u  = (SOST - u) * BITS_PER_CHAR;
			if(HOST_IS_BIGENDIAN)
				u = MK_C(0xFFFFFFFFUL) << u;
			else
				u = MK_C(0xFFFFFFFFUL) >> u;
			t = (t & u) | (te & ~u);
			*dst_b++ = t;
			return ((char *)dst_b) - align - r;
		}
		else
		{
			i = maxlen / SOST;
			dst_b = (size_t *)dst;
			for(; likely(i); i--, src_b++, dst_b++)
			{
				size_t c = *src_b;
				r = has_nul_byte(c);
				if(r)
					return cpy_rest0((char *)dst_b, (const char *)src_b, nul_byte_index(r));
				*dst_b = c;
			}
			maxlen = i * SOST + (maxlen % SOST);
			dst = (char *)dst_b;
			src = (const char *)src_b;
		}
	}

OUT:
	for(; maxlen && *src; maxlen--)
		*dst++ = *src++;
OUT_SET:
	if(likely(maxlen))
		*dst = '\0';
	return dst;
}

static char const rcsid_slpcg[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
