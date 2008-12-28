/*
 * mem_searchrn.c
 * search mem for a \r\n, generic implementation
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
 * $Id: $
 */

void *mem_searchrn(void *s, size_t len)
{
	char *p;
	size_t rr, rn, last_rr = 0;
	ssize_t f, k;
	prefetch(s);

	if(unlikely(!s || !len))
		return NULL;
	/*
	 * Sometimes you need a new perspective, like the altivec
	 * way of handling things.
	 * Lower address bits? Totaly overestimated.
	 *
	 * We don't precheck for alignment, 8 or 4 is very unlikely
	 * instead we "align hard", do one load "under the address",
	 * mask the excess info out and afterwards we are fine to go.
	 *
	 * Even this beeing a mem* function, the len can be seen as a
	 * "hint". We can overread and underread, but should cut the
	 * result (and not pass a page boundery, but we cannot because
	 * we are aligned).
	 */
	f = ALIGN_DOWN_DIFF(s, SOST);
	k = SOST - f - (ssize_t)len;

	p  = (char *)ALIGN_DOWN(s, SOST);
	rn = (*(size_t *)p);
	rr = rn ^ MK_C(0x0D0D0D0D); /* \r\r\r\r */
	rr = has_nul_byte(rr);
	if(!HOST_IS_BIGENDIAN) {
		rr >>= f * BITS_PER_CHAR;
		rr <<= f * BITS_PER_CHAR;
	} else {
		rr <<= f * BITS_PER_CHAR;
		rr >>= f * BITS_PER_CHAR;
	}
	if(unlikely(k > 0))
		goto K_SHIFT_CORRECT;

	k = -k;
	goto START_LOOP;

	do
	{
		p += SOST;
		rn = *(size_t *)p;
		rr = rn ^ MK_C(0x0D0D0D0D); /* \r\r\r\r */
		rr = has_nul_byte(rr);
		k -= SOST;
		if(k > 0)
		{
START_LOOP:
			if(rr || last_rr)
			{
				rn ^= MK_C(0x0A0A0A0A); /* \n\n\n\n */
				rn = has_nul_byte(rn);
				last_rr &= rn;
				if(last_rr)
					return p - 1;
				if(!HOST_IS_BIGENDIAN)
					rr &= rn >> BITS_PER_CHAR;
				else
					rr &= rn << BITS_PER_CHAR;
				if(rr)
					return p + nul_byte_index(rr);
			}
		}
	} while(k > 0);
	if(unlikely(k < 0))
	{
		k = -k;
K_SHIFT_CORRECT:
		if(!HOST_IS_BIGENDIAN) {
			rr <<= k * BITS_PER_CHAR;
			rr >>= k * BITS_PER_CHAR;
		} else {
			rr >>= k * BITS_PER_CHAR;
			rr <<= k * BITS_PER_CHAR;
		}
	}

	if(rr || last_rr)
	{
		rn ^= MK_C(0x0A0A0A0A); /* \n\n\n\n */
		rn = has_nul_byte(rn);
		last_rr &= rn;
		if(last_rr)
			return p - 1;
		if(!HOST_IS_BIGENDIAN)
			rr &= rn >> BITS_PER_CHAR;
		else
			rr &= rn << BITS_PER_CHAR;
		if(rr)
			return p + nul_byte_index(rr);
	}

	return NULL;

}

static char const rcsid_msrn[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
