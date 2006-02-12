/*
 * flsst.c
 * find last set in size_t, generic implementation
 *
 * Copyright (c) 2004,2005,2006 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2
 * of the License, or any later version.
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

inline size_t flsst(size_t find)
{
	size_t found;

	if(!find)
		return 0;
# if 1
	/*
	 * Do some magic and then use the popcount to calculate the
	 * flsst.
	 * 
	 * The upside of this is, there are /no/ conditional jumps in
	 * it. The downside, we need a (fast)popcount function. IA64
	 * and SPARCv9 seem to have a OP for that, and the generic
	 * implementation also has no conditional jumps, but...
	 */
	find |= find >> L2P(6);
	find |= find >> L2P(5);
	find |= find >> L2P(4);
	find |= find >> L2P(3);
	find |= find >> L2P(2);
	find |= find >> L2P(1);
	found = popcountst(find);
# else
	/* 
	 * ... who ever needs it:
	 * Generic Implenetations of devide and conquer approach.
	 */
#  define magic_mask(shift)	(((size_t) -1)<<(shift))
	found = SIZE_T_BITS;
	if(SIZE_T_BITS <= 128)
	{
		/* 
		 * this should evaluate to a bunch of clean constant
		 * tests, exatly the number needed for 32 or 64 Bit,
		 * even wenn gcc is not highly optimizing
		 */
		if(L2P(1) && !(find & magic_mask(L2P(1))))
			find <<= L2P(1), found -= L2P(1);
		if(L2P(2) && !(find & magic_mask(L2P(1)+L2P(2))))
			find <<= L2P(2), found -= L2P(2);
		if(L2P(3) && !(find & magic_mask(L2P(1)+L2P(2)+L2P(3))))
			find <<= L2P(3), found -= L2P(3);
		if(L2P(4) && !(find & magic_mask(L2P(1)+L2P(2)+L2P(3)+L2P(4))))
			find <<= L2P(4), found -= L2P(4);
		if(L2P(5) && !(find & magic_mask(L2P(1)+L2P(2)+L2P(3)+L2P(4)+L2P(5))))
			find <<= L2P(5), found -= L2P(5);
		if(L2P(6) && !(find & magic_mask(L2P(1)+L2P(2)+L2P(3)+L2P(4)+L2P(5)+L2P(6))))
			find <<= L2P(6), found -= L2P(6);
		if(L2P(7) && !(find & magic_mask(L2P(1)+L2P(2)+L2P(3)+L2P(4)+L2P(5)+L2P(6)+L2P(7))))
			find <<= L2P(7), found -= L2P(7);
	}
	else
	{
		/* Totaly generic */
		size_t mask, shift;
		mask  = magic_mask(s(1));
		shift = SIZE_T_BITS / 2;
		do
		{
			if(!(find & mask))
			{
				find <<= shift;
				found -= shift;
			}
			shift /= 2;
			mask <<= shift;
		} while(shift);
	}
# endif
	return found;
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id:$";
