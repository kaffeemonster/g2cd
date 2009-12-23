/*
 * my_neon.h
 * little arm neon helper
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
 * $Id: $
 */

#ifndef MY_NEON_H
# define MY_NEON_H

# define SOVUCQ sizeof(uint8x16_t)
# define SOVUC sizeof(uint8x8_t)

static inline uint8x16_t neon_simple_alignq(uint8x16_t a, uint8x16_t b, unsigned amount)
{
	switch(amount % SOVUCQ)
	{
	case  0: return a;
	case  1: return vextq_u8(a, b,  1);
	case  2: return vextq_u8(a, b,  2);
	case  3: return vextq_u8(a, b,  3);
	case  4: return vextq_u8(a, b,  4);
	case  5: return vextq_u8(a, b,  5);
	case  6: return vextq_u8(a, b,  6);
	case  7: return vextq_u8(a, b,  7);
	case  8: return vextq_u8(a, b,  8);
	case  9: return vextq_u8(a, b,  9);
	case 10: return vextq_u8(a, b, 10);
	case 11: return vextq_u8(a, b, 11);
	case 12: return vextq_u8(a, b, 12);
	case 13: return vextq_u8(a, b, 13);
	case 14: return vextq_u8(a, b, 14);
	case 15: return vextq_u8(a, b, 15);
	}
	return b;
}

static inline uint8x8_t neon_simple_align(uint8x8_t a, uint8x8_t b, unsigned amount)
{
	switch(amount % SOVUC)
	{
	case  0: return a;
	case  1: return vext_u8(a, b,  1);
	case  2: return vext_u8(a, b,  2);
	case  3: return vext_u8(a, b,  3);
	case  4: return vext_u8(a, b,  4);
	case  5: return vext_u8(a, b,  5);
	case  6: return vext_u8(a, b,  6);
	case  7: return vext_u8(a, b,  7);
	}
	return b;
}

#endif
