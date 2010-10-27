/*
 * strreverse_l.c
 * strreverse_l, arm implementation
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

#ifdef __ARM_NEON__
# include <arm_neon.h>
# include "my_neon.h"
#endif

void strreverse_l(char *begin, char *end)
{
	char tchar;

#ifdef __ARM__NEON__
	for(; end - ((2*SOVUC)-1) > begin; begin += SOVUC)
	{
		uint8x8_t te, tb;
		end -= SOVUC;
		tb = vld1_u8(begin);
		te = vld1_u8(end);
		tb = vrev64_u8(tb);
		te = vrev64_u8(te);
		vst1_u8(begin, te);
		vst1_u8(end, tb);
	}
#endif
	/*
	 * ARM is a little special challanged, by being "the" RISC
	 * arch and having the "magic" shifter.
	 * Being RISC, unaligned access was not on the ARM radar. This
	 * is an understandable decision, but, since it has the "one
	 * operand gets the shift for free"-shifter, it does not have
	 * any unaligned assist instructions. OK, missing instructions
	 * is not uncommon, at least for real RISC.
	 * The problem is, GCC is tuned for these HW "features".
	 * This leads to arkward code generation on unaligned access.
	 * Together with an endian swap (which also has no instruction,
	 * we have a magic shifter...) even more arkward code generation
	 * and an unaligned write to top it off.
	 *
	 * So we are better off by moving by bytes and only help by
	 * unrolling.
	 *
	 * The sad part is, modern ARM (>= v7?) can do unaligned access,
	 * but you can not use it (controlled by a config bit, default set
	 * to "unaligned forbidden", backward compatible ftw.) in practice.
	 * Esp. there are to many cheap v5 & v6 in the market to make a mind
	 * shift that unaligned access is now ok (and desirable) on ARM.
	 *
	 * Yes, once i also had the opinion that the m68k way is the only
	 * right way (bytes on 1, everything else > 1), anyone who wants
	 * unaligned access must be an wintel slacker, but pratice shows
	 * there are good reasons for unaligned access, they are not pretty,
	 * but practical, not only in the "your data structure is broken"
	 * case, but byte processing, like strings.
	 */
	for(; end - 7 > begin; begin += 4)
	{
		char t1, t2, t3, t4;
		end -= 4;
		t4 = end[0];
		t3 = end[1];
		t2 = end[2];
		t1 = end[3];
		end[3] = begin[0];
		end[2] = begin[1];
		end[1] = begin[2];
		end[0] = begin[3];
		begin[0] = t1;
		begin[1] = t2;
		begin[2] = t3;
		begin[3] = t4;
	}

	while(end > begin)
		tchar = *end, *end-- = *begin, *begin++ = tchar;
}

static char const rcsid_srla[] GCC_ATTR_USED_VAR = "$Id: $";
