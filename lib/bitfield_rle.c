/*
 * bitfield_rle.c
 * run length encoding for bitfields, esp. QHTs
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

#include <string.h>
#include "other.h"
#include "my_bitops.h"
#include "my_bitopsm.h"

/*
 * General note:
 * despite their general name, these functions use a fixed rle sheme
 * optimized for sparsely populated bitfields (like QHTs).
 *
 * - Bytes with one bit cleared are expressed by a prefix (0xE0)
 *   or'ed with the bit index.
 * - Bytes with two bits cleared are expressed by a prefix (0xC0)
 *   or'ed with an index into a code table.
 * - Bytes with 3 or more bits cleared are prefixed by a byte
 *   prefixed (0x80) and or'ed with the run length and copied literetly.
 *
 * 0xFF runs are coded as bytes with the MSB cleared containing the run
 * length when the run is smaller than 2 times 0x7F, or prefixed with a
 * byte prefixed with 0xF0 or'ed with the length of the run length,
 * followed by the length of the run in host endian.
 *
 * This gives a maximum compression of 4 bytes for an empty QHT (1:32768).
 * A mostly empty QHT like after you installed Shareaza (only the
 * .exe is shared) compresses to ~122 bytes (1:1074.3)
 *
 * decoding of a nearly empty QHT needs ~70,000 instructions, encoding
 * 270,000 (also 70,000 for x86 and their string instructions). These
 * values get worse the more the QHT is populated.
 *
 * An QHT with ~3000 files (90% 0xFF, but short runs) compresses to
 * ~24kb (1:5.13) in ~1.2 Mio instructions, decodes in ~700,000.
 *
 * Input may not be compressed by run length encoding, like in the
 * obvious case of many bytes with more than 2 cleared bits.
 *
 * Another property of this rle sheme is a bit can still be looked up
 * in the compressed form, no decompression is needed, only a "walk" up
 * to the right bit index. But when the compressed result gets huge this
 * walk may be inefficient (to much to walk).
 *
 * To prevent this and size grows the user should provide a buffer smaller
 * than the input data to stop these functions early and fallback to
 * uncompressed handling.
 */

/*
 * bitfield_encode - run length encode a bit field
 * res: buffer for result data
 * t_len: size fo the result buffer
 * data: buffer with input data
 * s_len: length of input data
 *
 * return value: length of the result or -length of the result
 *               if not all data could be consumed.
 *
 * Please refer to the general notes about these functions.
 */

/*
 * bitfield_decode - run length decode a bit field
 * res: buffer for result data
 * t_len: size fo the result buffer
 * data: buffer with input data
 * s_len: length of input data
 *
 * return value: length of the result or -length of the result
 *               if not all data could be consumed.
 *
 * Please refer to the general notes about these functions.
 */

static unsigned popcnt_8(unsigned v)
{
	unsigned w = v - ((v >> 1) & 0x55);
	unsigned x = (w & 0x33) + ((w >> 2) & 0x33);
	unsigned y = (x + (x >> 4)) & 0x0F;
	return y;
}

static const uint8_t twos_index[] =
{
	 0,  0,  0,  0,  0,  1,  2,  0,  0,  3,  4,  0,  5,  0,  0,  0,
	 0,  6,  7,  0,  8,  0,  0,  0,  9,  0,  0,  0,  0,  0,  0,  0,
	 0, 10, 11,  0, 12,  0,  0,  0, 13,  0,  0,  0,  0,  0,  0,  0,
	14,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0, 15, 16,  0, 17,  0,  0,  0, 18,  0,  0,  0,  0,  0,  0,  0,
	19,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	20,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0, 21, 22,  0, 23,  0,  0,  0, 24,  0,  0,  0,  0,  0,  0,  0,
	25,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	26,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	27,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static const uint8_t index_twos[] =
{
	0x03, 0x05, 0x06, 0x09, 0x0A, 0x0C, 0x11,
	0x12, 0x14, 0x18, 0x21, 0x22, 0x24, 0x28,
	0x30, 0x41, 0x42, 0x44, 0x48, 0x50, 0x60,
	0x81, 0x82, 0x84, 0x88, 0x90, 0xA0, 0xC0,
};

static const struct
{
	uint8_t a, b;
} index_twos_bits[] =
{
	{0, 1}, {0, 2}, {1, 2}, {0, 3}, {1, 3}, {2, 3}, {0, 4},
	{1, 4}, {2, 4}, {3, 4}, {0, 5}, {1, 5}, {2, 5}, {3, 5},
	{4, 5}, {0, 6}, {1, 6}, {2, 6}, {3, 6}, {4, 6}, {5, 6},
	{0, 7}, {1, 7}, {2, 7}, {3, 7}, {4, 7}, {5, 7}, {6, 7},
};

ssize_t bitfield_encode(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len)
{
	uint8_t *r_wptr = res;
	const uint8_t *o_data;
	ssize_t c_len;
	unsigned cnt, o_row;

	while(likely(t_len) && likely(s_len))
	{
		uint8_t z = *data;

		if(likely(0xFF == z))
		{
			size_t f_row;
#if defined(I_LIKE_ASM) && (defined(__i386__) || defined(__x86_64__))
			size_t t, u;
			asm (
					"or	$-1, %2\n\t"
					"xor	%3, %3\n\t"
					"mov	%4, %0\n\t"
					"shr	$2, %0\n\t"
					"jz	1f\n\t"
					"mov	%0, %3\n\t"
					"repe scasl\n\t"
					"sub	$4, %1\n\t"
					"inc	%0\n\t"
					"sub	%0, %3\n\t"
					"shl	$2, %3\n\t"
					"sub	%3, %4\n"
					"1:\n\t"
					"test	%4, %4\n\t"
					"jz	2f\n\t"
					"mov	%4, %0\n\t"
					"repe scasb\n\t"
					"je	3f\n\t"
					"dec	%1\n\t"
					"inc	%0\n"
					"3:\n\t"
					"mov	%4, %2\n\t"
					"sub	%0, %2\n\t"
					"sub	%2, %4\n\t"
					"add	%2, %3\n"
					"2:\n\t"
				: /* %0 */ "=c" (t),
				  /* %1 */ "=D" (data),
				  /* %2 */ "=a" (u),
				  /* %3 */ "=r" (f_row),
				  /* %4 */ "=r" (s_len)
				: /* %5 */ "4" (s_len),
				  /* %6 */ "1" (data)
			);
#else
			f_row = 0;
			if(s_len >= sizeof(uint32_t))
			{
				if(!UNALIGNED_OK)
				{
					size_t t = ALIGN_DIFF(data, sizeof(uint32_t));
					for(s_len -= t; t; t--, data++, f_row++) {
						if(unlikely(0xFF != *data))
							break;
					}
				}
				for(; likely(s_len >= sizeof(uint32_t)); s_len -= sizeof(uint32_t),
				    data += sizeof(uint32_t), f_row += sizeof(uint32_t)) {
					if(unlikely(0xFFFFFFFF != *(const uint32_t *)data))
						break;
				}
			}
			for(; s_len; s_len--, data++, f_row++) {
				if(unlikely(0xFF != *data))
					break;
			}
#endif
			if(f_row > 2 * 0x7F)
			{
#if defined(I_LIKE_ASM) && (defined(__i386__) || defined(__x86_64__))
				size_t v;
				asm ("bsr	%1, %0" : "=r" (v) : "rm" (f_row));
				cnt = (v / 8) + 1;
#else
				cnt = (((sizeof(f_row) * 8) - __builtin_clzl(f_row)) / 8) + 1;
#endif
				*r_wptr++ = cnt | 0xF0;
				t_len--;
				if(unlikely(cnt > t_len)) {
					s_len = 1;
					break;
				}
				if(UNALIGNED_OK)
				{
					switch(cnt)
					{
					case 1:
						*r_wptr++ = f_row;
						t_len--;
						break;
					case 2:
						*(uint16_t *)r_wptr = f_row;
						r_wptr += sizeof(uint16_t);
						t_len -= sizeof(uint16_t);
						break;
					case 3:
						if(HOST_IS_BIGENDIAN) {
							*r_wptr++ = (f_row >> (2 * 8)) & 0xFF;
							*(uint16_t *)r_wptr = f_row & 0xFFFF;
						} else {
							*r_wptr++ = f_row & 0xFF;
							*(uint16_t *)r_wptr = (f_row >> 8) & 0xFFFF;
						}
						r_wptr += sizeof(uint16_t);
						t_len -= sizeof(uint16_t) + sizeof(uint8_t);
						break;
					case 4:
						*(uint32_t *)r_wptr = f_row;
						r_wptr += sizeof(uint32_t);
						t_len -= sizeof(uint32_t);
						break;
					default:
						goto write_out_manual;
					}
				}
				else
				{
write_out_manual:
					if(HOST_IS_BIGENDIAN) { /* write big endian data */
						unsigned i = cnt - 1;
						for(; t_len && cnt; t_len--, cnt--, i--)
							*r_wptr++ = (f_row >> (i * 8)) & 0xFF;
					} else { /* write little endian data */
						for(; t_len && cnt; t_len--, cnt--) {
							*r_wptr++ = f_row & 0xFF;
							f_row >>= 8;
						}
					}
				}
			}
			else
			{
				if(f_row > 0x7F) {
					*r_wptr++ = 0x7F;
					f_row -= 0x7F;
					t_len--;
				}
				if(f_row && t_len) {
					*r_wptr++ = f_row;
					t_len--;
				}
			}
			continue;
		}

		/* check for single, dual or other bits */
		if(s_len < 1 || 0xFF == data[1])
		{
			cnt = popcnt_8(~z);
			if(likely(1 == cnt || 2 == cnt))
			{
				if(1 == cnt)
					*r_wptr++ = (__builtin_ffs(~z) - 1) | 0xE0;
				else
					*r_wptr++ = twos_index[(~z) & 0xFF] | 0xC0;
				data++;
				s_len--;
				t_len--;
				continue;
			}
		}

		/* ok, looks like we have to code it literly */
		o_data = data;
		o_row = 0;
		/* find length */
#if defined(I_LIKE_ASM) && (defined(__i386__) || defined(__x86_64__))
		if(s_len >= sizeof(uint16_t))
		{
			size_t t;
			asm (
				"repne scasw\n\t"
				"sub	$2, %0\n\t"
				"inc	%1\n\t"
			: /* %0 */ "=D" (data),
			  /* %1 */ "=c" (t)
			: /* %2 */ "0" (data),
			  /* %3 */ "1" (s_len / 2),
			  /* %4 */ "a" (0xFFFFFFFF)
			);
			o_row += ((s_len / 2) - t) * 2;
			s_len -= ((s_len / 2) - t) * 2;
		}
#else
		/* align to 2 */
		if(!UNALIGNED_OK && (((uintptr_t)data) & 1) && *data != 0xFF) {
			o_row++;
			data++;
			s_len--;
		}
		for(; s_len >= sizeof(uint16_t); s_len -= sizeof(uint16_t),
		    data += sizeof(uint16_t), o_row += sizeof(uint16_t)) {
			if(0xFFFF == *(const uint16_t *)data)
				break;
		}
#endif
		/* overshoot one byte? */
		if(data != o_data && 0xFF == *(data - 1)) {
			s_len++;
			data--;
			o_row--;
		}
		/* odd byte at end? */
		if(s_len && *data != 0xFF) {
			s_len--;
			data++;
			o_row++;
		}
		/* copy */
		while(t_len && o_row)
		{
			t_len--;
			cnt = o_row > 0x3F ? 0x3F : o_row;
			cnt = cnt <= t_len ? cnt : t_len;
			*r_wptr++ = cnt | 0x80;
			o_row -= cnt;
			r_wptr = mempcpy(r_wptr, o_data, cnt);
			o_data += cnt;
			t_len -= cnt;
		}
	}

	c_len = r_wptr - res;
	if(s_len)
		c_len = -c_len;
	return c_len;
}

ssize_t bitfield_decode(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len)
{
	uint8_t *r_wptr = res;
	ssize_t c_len;
	size_t cnt = 0;

	while(likely(t_len) && likely(s_len))
	{
		unsigned t;
		uint8_t c = *data;

		if(unlikely(!(c & 0x80)))
		{
			data++;
			s_len--;
			cnt = c;
			goto write_out_ff;
		}

		if(unlikely(!(c & 0x40)))
		{
			/* plain data */
			cnt = c & ~0x80;
			cnt = cnt <= s_len ? cnt : s_len;
			cnt = cnt <= t_len ? cnt : t_len;
			r_wptr = mempcpy(r_wptr, data + 1, cnt);
			t_len -= cnt;
			cnt++;
			data += cnt;
			s_len -= cnt;
			continue;
		}

		if(unlikely(!(c & 0x20)))
		{
			/* two bit */
			*r_wptr++ = ~index_twos[c & ~0xC0];
			t_len--;
			data++;
			s_len--;
			continue;
		}

		if(unlikely(!(c & 0x10)))
		{
		/* one bit */
			*r_wptr++ =  ~(1 << (c & ~0xE0));
			t_len--;
			data++;
			s_len--;
			continue;
		}

		t = c & ~0xF0;
		s_len--;
		data++;
		if(t > s_len)
			break;
		if(UNALIGNED_OK )
		{
			switch(cnt)
			{
			case 1:
				cnt = *data++;
				s_len--;
				break;
			case 2:
				cnt    = *(const uint16_t *)data;
				data  += sizeof(uint16_t);
				s_len -= sizeof(uint16_t);
				break;
			case 3:
				if(HOST_IS_BIGENDIAN) {
					cnt  = ((size_t)(*data++)) << (2 * 8);
					cnt |= *(const uint16_t *)data;
				} else {
					cnt  = *data++;
					cnt |= ((size_t)(*(const uint16_t *)data)) << 8;
				}
				data  += sizeof(uint16_t);
				s_len -= sizeof(uint16_t) + sizeof(uint8_t);
				break;
			case 4:
				cnt    = *(const uint32_t *)data;
				data  += sizeof(uint32_t);
				s_len -= sizeof(uint32_t);
				break;
			default:
				goto read_in_manual;
			}
		}
		else
		{
read_in_manual:
			if(HOST_IS_BIGENDIAN) {
				for(cnt = 0; likely(s_len) && likely(t); data++, s_len--, t--) {
					cnt <<= 8;
					cnt |= *data;
				}
			} else {
				unsigned i = 0;
				for(cnt = 0; likely(s_len) && likely(t); data++, s_len--, t--, i++)
					cnt |= ((size_t)*data) << (i * 8);
			}
		}
write_out_ff:
		cnt = cnt <= t_len ? cnt : t_len;
#if defined(I_LIKE_ASM) && (defined(__i386__) || defined(__x86_64__))
		asm (
				"rep	stosb\n\t"
				"mov	%3, %0\n\t"
				"rep	stosl\n\t"
			: /* %0 */ "=c" (t),
			  /* %1 */ "=D" (r_wptr)
			: /* %2 */ "a" (0xFFFFFFFF),
			  /* %3 */ "rm" (cnt / sizeof(uint32_t)),
			  /* %4 */ "0" (cnt % sizeof(uint32_t)),
			  /* %5 */ "1" (r_wptr)
		);
#else
		memset(r_wptr, 0xFF, cnt);
		r_wptr += cnt;
#endif
		t_len -= cnt;
	}

	c_len = r_wptr - res;
	if(s_len)
		c_len = -c_len;
	return c_len;
}