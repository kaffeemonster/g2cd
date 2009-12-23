/*
 * cpy_rest.c
 * copy a byte trailer, generic implementation
 *
 * Copyright (c) 2008-2009 Jan Seiffert
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

noinline GCC_ATTR_FASTCALL char *cpy_rest(char *dst, const char *src, unsigned i)
{
	if(UNALIGNED_OK)
	{
		uint16_t *dst_w = (uint16_t *)dst, t16;
		const uint16_t *src_w = (const uint16_t *)src;
		uint32_t *dst_dw = (uint32_t *)dst, t32;
		const uint32_t *src_dw = (const uint32_t *)src;
		uint64_t *dst_qw = (uint64_t *)dst, t64;
		const uint64_t *src_qw = (const uint64_t *)src;

		switch(i)
		{
		case 15:
			dst[14] = src[14];
		case 14:
			t16 = get_unaligned(&src_w[6]);
			put_unaligned(t16, &dst_w[6]);
			goto C12;
		case 13:
			dst[12] = src[12];
		case 12:
C12:
			t32 = get_unaligned(&src_dw[2]);
			put_unaligned(t32, &dst_dw[2]);
			goto C8;
		case 11:
			dst[10] = src[10];
		case 10:
			t16 = get_unaligned(&src_w[4]);
			put_unaligned(t16, &dst_w[4]);
			goto C8;
		case  9:
			dst[8] = src[8];
		case  8:
C8:
			t64 = get_unaligned(&src_qw[0]);
			put_unaligned(t64, &dst_qw[0]);
			break;
		case  7:
			dst[6] = src[6];
		case  6:
			t16 = get_unaligned(&src_w[2]);
			put_unaligned(t16, &dst_w[2]);
			goto C4;
		case  5:
			dst[4] = src[4];
		case  4:
C4:
			t32 = get_unaligned(&src_dw[0]);
			put_unaligned(t32, &dst_dw[0]);
			break;
		case  3:
			dst[2] = src[2];
		case  2:
			t16 = get_unaligned(&src_w[0]);
			put_unaligned(t16, &dst_w[0]);
			break;
		case  1:
			dst[0] = src[0];
		case  0:
			break;
		}
	}
	else
	{
		unsigned n;
		for(n = 0; n < i; n++)
			dst[n] = src[n];
	}
	return dst + i;
}

noinline GCC_ATTR_FASTCALL char *cpy_rest_o(char *dst, const char *src, unsigned i)
{
	if(UNALIGNED_OK)
	{
		uint16_t *dst_w = (uint16_t *)dst, t16;
		const uint16_t *src_w = (const uint16_t *)src;
		uint32_t *dst_dw = (uint32_t *)dst, t32;
		const uint32_t *src_dw = (const uint32_t *)src;
		uint64_t *dst_qw = (uint64_t *)dst, t64;
		const uint64_t *src_qw = (const uint64_t *)src;

		switch(i)
		{
		case 15:
			dst[14] = src[14];
		case 14:
			t16 = get_unaligned(&src_w[6]);
			put_unaligned(t16, &dst_w[6]);
			goto C12;
		case 13:
			dst[12] = src[12];
		case 12:
C12:
			t32 = get_unaligned(&src_dw[2]);
			put_unaligned(t32, &dst_dw[2]);
			goto C8;
		case 11:
			dst[10] = src[10];
		case 10:
			t16 = get_unaligned(&src_w[4]);
			put_unaligned(t16, &dst_w[4]);
			goto C8;
		case  9:
			dst[8] = src[8];
		case  8:
C8:
			t64 = get_unaligned(&src_qw[0]);
			put_unaligned(t64, &dst_qw[0]);
			break;
		case  7:
			dst[6] = src[6];
		case  6:
			t16 = get_unaligned(&src_w[2]);
			put_unaligned(t16, &dst_w[2]);
			goto C4;
		case  5:
			dst[4] = src[4];
		case  4:
C4:
			t32 = get_unaligned(&src_dw[0]);
			put_unaligned(t32, &dst_dw[0]);
			break;
		case  3:
			dst[2] = src[2];
		case  2:
			t16 = get_unaligned(&src_w[0]);
			put_unaligned(t16, &dst_w[0]);
			break;
		case  1:
			dst[0] = src[0];
		case  0:
			break;
		}
	}
	else
	{
		unsigned n;
		for(n = 0; n < i; n++)
			dst[n] = src[n];
	}
	return dst;
}

GCC_ATTR_FASTCALL char *cpy_rest0(char *dst, const char *src, unsigned i)
{
	dst[i] = '\0';
	return cpy_rest(dst, src, i);
}

static char const rcsid_cprg[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
