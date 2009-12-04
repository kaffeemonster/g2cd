/*
 * tchar.c
 * util funcs for tchars
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
 * $Id:$
 */

#include <stddef.h>
#include "tchar.h"

static int utf8totc(tchar_t **r, size_t *rl, const char *src, size_t sl)
{
	const unsigned char *s_c = (const unsigned char *)src;
	uint32_t result;
	int len;

	if(unlikely(sl < 1 || *rl < 1))
		return -1;

	if(0x00 == (s_c[0] & 0x80)) { /* plain ascii */
		result = s_c[0];
		len = 1;
		goto out;
	}

	if(sl < 2)
		return -1;

	if(0xc0 == (s_c[0] & 0xe0)) {
		result = ((s_c[0] & 0x1f) << 6) + (s_c[1] & 0x3f);
		len = 2;
		goto out;
	}

	if(sl < 3)
		return -1;

	if(0xe0 == (s_c[0] & 0xf0)) {
		result = ((s_c[0] & 0x0f) << 12) + ((s_c[1] & 0x3f) << 6) + (s_c[2] & 0x3f);
		len = 3;
		goto out;
	}

	if(sl < 4)
		return -1;
	if(s_c[0] > 244) /* overlong mb sequence, codepoint above 0x10ffff */
		return -1;

	result = ((s_c[0] & 0x07) << 18) + ((s_c[1] & 0x3f) << 12) + ((s_c[2] & 0x3f) << 6) + (s_c[3] & 0x3f);
	len = 4;
out:
	if(unlikely(result > 0xffff))
	{
		/* surrogate */
		if(*rl < 2)
			return -1;
		result -= 0x10000;
		*(*r++) = (result >> 10) | 0xD800;
		(*rl)--;
		result = (result & 0x3FF) | 0xDC00;
	}
	*(*r++) = result;
	(*rl)--;

	return len;
}

size_t utf8totcs(tchar_t *dst, size_t dl, const char *src, size_t *sl)
{
	size_t cnt = dl, sll = *sl;

	while(likely(cnt && sll))
	{
		int len = utf8totc(&dst, &cnt, src, sll);
		if(len < 0)
			break;

		src += len;
		sll -= len;
	}

	*sl = *sl - sll;
	return dl - cnt;
}

tchar_t *tstrptolower(tchar_t *s)
{
	tchar_t *w = s;

	for(; *w; w++)
	{
// TODO: this is obsolete with Shareaza 2.5.0.0
		/*
		 * commit says:
		 * Greek Capital Sigma and Small Final Sigma always
		 * converted to Small Sigma
		 */
		if(unlikely(0x03a3 == *w) && w != s)
		{
			/*
			 * lowercase greek final sigmas in words first in a special way
			 * s/(\w+)\x3a3(\W|$)/$1\x3c2$2/g
			 */
			if(tctype1(*(w - 1)) & TC_C1_ALPHA)
			{
				if(!w[1] ||
				   tctype1(w[1]) & (TC_C1_SPACE|TC_C1_BLANK)) {
					*w = 0x03c2;
					continue;
				}
			}
		}
		*w = tchar_tolower_base_data[*w];
	}

	return w;
}

void tistypemix(const tchar_t *s, size_t len, bool *word, bool *digit, bool *mix)
{
	*digit = false;

	for(; *s && len; s++, len--)
	{
		if(tisdigit(*s))
			*digit = true;
		else if(tischaracter(*s))
			*word = true;
	}

	*mix = *word && *digit;
	if(*mix)
	{
		*word = false;
		*digit = false;
	}
}

/*@unused@*/
static char const rcsid_tc[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
