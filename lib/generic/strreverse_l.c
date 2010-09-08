/*
 * strreverse_l.c
 * strreverse_l, generic implementation
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

#ifndef ARCH_NAME_SUFFIX
# define F_NAME(z, x, y) z x
#else
# define F_NAME(z, x, y) static noinline z x##y
#endif

F_NAME(void, strreverse_l, _generic)(char *begin, char *end)
{
	char tchar;

	if(sizeof(size_t) > 4)
	{
		for(; unlikely(end - 15 > begin); end -= 8, begin += 8)
		{
			uint64_t tll1 = get_unaligned((uint64_t *)(end - 7));
			uint64_t tll2 = get_unaligned((uint64_t *)begin);
			tll1 = __swab64(tll1);
			tll2 = __swab64(tll2);
			put_unaligned(tll1, (uint64_t *)begin);
			put_unaligned(tll2, (uint64_t *)(end - 7));
		}
	}
	for(; end - 7 > begin; end -= 4, begin += 4)
	{
		uint32_t tl1 = get_unaligned((uint32_t *)(end - 3));
		uint32_t tl2 = get_unaligned((uint32_t *)begin);
		tl1 = __swab32(tl1);
		tl2 = __swab32(tl2);
		put_unaligned(tl1, (uint32_t *)begin);
		put_unaligned(tl2, (uint32_t *)(end - 3));
	}
	if(end - 3 > begin)
	{
		uint16_t t1 = get_unaligned((uint16_t *)(end - 1));
		uint16_t t2 = get_unaligned((uint16_t *)begin);
		t1 = __swab16(t1);
		t2 = __swab16(t2);
		put_unaligned(t1, (uint16_t *)begin);
		put_unaligned(t2, (uint16_t *)(end - 1));
		end   -= 2;
		begin += 2;
	}

	while(end > begin)
		tchar = *end, *end-- = *begin, *begin++ = tchar;
}

static char const rcsid_srlg[] GCC_ATTR_USED_VAR = "$Id: $";
