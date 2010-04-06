/*
 * bitfield_rle.c
 * run length encoding for bitfields, esp. QHTs
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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
 * Despite their general name, these functions use a fixed RLE sheme
 * invented by me (while taking a crap) optimized for sparsely
 * populated bitfields (like QHTs). But otherwise it's a RLE as
 * in other image formats like TIFF, BMP or PCX. The main difference
 * is, we only compress 0xFF, which is our "white"/background color.
 * This way we can stuff more, and bring the RLE packet down to 1 byte.
 *
 * - Bytes with one bit cleared are expressed by a prefix (0xE0)
 *   or'ed with the bit index.
 * - Bytes with two bits cleared are expressed by a prefix (0xC0)
 *   or'ed with an index into a code table.
 * - Bytes with 3 or more bits cleared are prefixed by a byte
 *   prefixed (0x80) and or'ed with the run length minus one and
 *   copied literetly.
 *
 * 0xFF runs are coded as bytes with the MSB cleared containing the run
 * length minus one when the run is smaller than 2 times 0x80, or prefixed
 * with a byte prefixed with 0xF0 or'ed with the length of the run length,
 * followed by the length of the run minus one in host endian.
 *
 * The result:
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
 * Another property of this RLE sheme is a bit can still be looked up
 * in the compressed form, no decompression is needed, only a "walk" up
 * to the right bit index. But when the compressed result gets huge this
 * walk may be inefficient (to much to walk).
 *
 * To prevent this and size growth the user should provide a buffer smaller
 * than the input data to stop these functions early and fallback to
 * uncompressed handling.
 */

/*
 * While this all works quite well, we have a problem:
 * Even bitfield_lookup being cheaper than bitfield_decode
 * (avg. half the cost), it is still, with the call count,
 * our most expensive function.
 * We have for ex. 213,522 calls with 8,742,356,171 instructions
 * executed. (next is memand with 1.6 Mrd instructions)
 *
 * Ohoh...
 *
 * This is made worse by a bad interaction.
 * QHTs which are "full" take more time to walk but also
 * aggrevate more hits.
 * That sometimes hub QHTs are compressed does not help,
 * too. (need to fix this...)
 *
 * Two cheopo graphs to make the problem obvious:
 *
 *                                     Search hit propability
 *                                                ^
 *                                            |   | high
 *                                           /    |
 *                                           |    |
 *                                           |    |
 *                                          /     |
 *                                         /      |
 *                                        /       |
 *                                       /        |
 *                                      /         |
 *                     /---------------/          |
 *             -------/                           | low
 * QHT entries <----------------------------------+
 *              empty                         full
 *
 *                                          lookup time
 *                                                ^
 *                                            |   | high
 *                                           /    |
 *                                           |    |
 *                                           |    |
 *                                          /     |
 *                                         /      |
 *                                        /       |
 *                                       /        |
 *                                      /         |
 *                     /---------------/          |
 *             -------/                           | low
 * QHT entries <----------------------------------+
 *              empty                         full
 *
 * ~50% of the searches consist of a single value (prop.
 * search for a single word or hash, sha1, tiger, etc.),
 * so...
 *
 * Idea:
 * Add a sync block to the RLE'd bitfields.
 * We start the data with a flag byte. If it says
 * "have sync block", we have added a kind of index
 * at the end:
 *
 * num syncs: n
 * 0  : bitfield index a - rle offset u
 * 1  : bitfield index b - rle offset v
 * ...
 * n-1: bitfield index e - rle offset y
 * n  : bitfield index f - rle offset z
 *
 * With "bitfield index" sorted in ascending order (or
 * decending, depends on the way you look at it ;).
 * This way a lookup can first search the index for the
 * last pair smaller than the hash to search, and then
 * start walking from that rle offset.
 * We can generate the index while encoding.
 * For example by generating an index entry every
 * "quarter", four entries total. Even such a low number
 * of entries (and low mem comsumtion) would really
 * help to start the decoding much nearer the target
 * offset.
 *
 * And yeah: i also invented that while taking a crap...
 *
 * Update:
 * This seems to work quite fine, taking the default
 * instruction count needed for lookup down by a factor
 * of 10. The cost on encode is neglegtable.
 *
 * And by the way, this is nothing new, it is known as
 * a scanline index. The only difference is, we do not
 * have fixed scanlines, but compress a far as we get.
 */
// TODO: is this bug free?
// TODO: more than 4 syncblocks?

/*
 * bitfield_encode - run length encode a bit field
 * res: buffer for RLE result data
 * t_len: size of the RLE result buffer
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
 * t_len: size of the result buffer
 * data: buffer with RLE input data
 * s_len: length of RLE input data
 *
 * return value: length of the result or -length of the result
 *               if not all RLE data could be consumed.
 *
 * Please refer to the general notes about these functions.
 */

/*
 * bitfield_and - and a RLE'd bit field with an uncompressed bit field
 * res: input buffer for uncompressed bitfield and output for result
 * t_len: size of the input/output buffer
 * data: buffer with RLE input data
 * s_len: length of RLE input data
 *
 * return value: length of the result or -length of the result
 *               if not all RLE data could be consumed.
 *
 * Please refer to the general notes about these functions.
 */

/*
 * bitfield_lookup - test an array of bit indexes against an encoded bitfield
 * vals: array of bit indexes
 * v_len: number of elements in vals
 * data: encoded bitfield
 * s_len: length of encoded bitfield
 *
 * return value: 0 if no index matched, -1 if one index matched
 *
 * This function works best if the indexes are sorted ascending
 * (or it has to "unpack" the bitfield v_len times worst case)
 *
 * Please refer to the general notes about these functions.
 */

#if 1
struct sync_block
{
	uint32_t idx;
	uint32_t off;
};
/* steal first 2 bit for the number of sync blocks */
#define NUM_SYNC_MASK (0x07)
#define NUM_SYNC_MAX  4

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
	struct sync_block synb[NUM_SYNC_MAX];
	uint8_t *r_wptr = res, *flags = NULL;
	const uint8_t *o_data;
	size_t idx, idx_step, os_len;
	ssize_t c_len;
	unsigned cnt, o_row, num_syn = 0, syn_len;

	if(likely(t_len))
	{
		/* add a flag byte */
		t_len--;
		flags = r_wptr++;
	}
	/* reserve space for sync block */
	syn_len = NUM_SYNC_MAX * sizeof(struct sync_block);
	syn_len = syn_len < t_len ? syn_len : t_len;
	syn_len = (syn_len / sizeof(struct sync_block)) * sizeof(struct sync_block);
	t_len -= syn_len;

	/* prepare sync blocks */
	idx_step = s_len / (NUM_SYNC_MAX + 1); /* equally distributed */
	for(cnt = 0, idx = idx_step; cnt < NUM_SYNC_MAX; cnt++, idx += idx_step) {
		synb[cnt].idx = idx;
		synb[cnt].off = 0;
	}
	os_len = s_len;

	while(likely(t_len) && likely(s_len))
	{
		uint8_t z = *data;

		/* is it time to emit a sync block? */
		if(unlikely(os_len - s_len >= synb[num_syn].idx))
		{
			synb[num_syn].idx = os_len - s_len;
			synb[num_syn].off = r_wptr - (res + 1);
			num_syn++;
			/* did we compress better than expected? */
			if(likely(num_syn < NUM_SYNC_MAX) && unlikely(synb[num_syn - 1].idx >= synb[num_syn].idx))
			{
				/* reindex */
				idx_step = s_len / ((NUM_SYNC_MAX - num_syn) + 1);
				for(cnt = num_syn, idx = synb[num_syn - 1].idx + idx_step;
				    cnt < NUM_SYNC_MAX; cnt++, idx += idx_step)
					synb[cnt].idx = idx;
			}
		}

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
					"repe scasl\n\t" /* (0x042329E3) execution */
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
			if(s_len >= SO32)
			{
				size_t shift = SO32 - ALIGN_DOWN_DIFF(data, SO32);
				const uint32_t *dt_32 = (const uint32_t *)ALIGN_DOWN(data, SO32);
				uint32_t x;

				x = *dt_32++;
				if(HOST_IS_BIGENDIAN)
					x |= 0xFFFFFFFFU << shift * BITS_PER_CHAR;
				else
					x |= 0xFFFFFFFFU >> shift * BITS_PER_CHAR;
				if(likely(0xFFFFFFFFU == x))
				{
					s_len -= shift;
					f_row += shift;

					for(; likely(s_len >= SO32); s_len -= SO32,
					    dt_32++, f_row += SO32) {
						if(unlikely(0xFFFFFFFF != *dt_32))
							break;
					}
					data = (const uint8_t *)dt_32;
				}
			}
			for(; s_len; s_len--, data++, f_row++) {
				if(unlikely(0xFF != *data))
					break;
			}
#endif
			if(f_row > 2 * 0x80)
			{
				f_row--;
#if defined(I_LIKE_ASM) && (defined(__i386__) || defined(__x86_64__))
				{
					size_t v;
					asm ("bsr	%1, %0" : "=r" (v) : "rm" (f_row));
					cnt = (v / BITS_PER_CHAR) + 1;
				}
#else
				cnt = ((flsst(f_row) - 1) / BITS_PER_CHAR) + 1;
#endif
				*r_wptr++ = cnt | 0xF0;
				t_len--;
				if(unlikely(cnt > t_len)) {
					s_len = 1;
					break;
				}
				switch(cnt)
				{
				case 1:
					*r_wptr++ = f_row;
					t_len--;
					break;
				case 2:
					put_unaligned(f_row, (uint16_t *)r_wptr);
					r_wptr += sizeof(uint16_t);
					t_len  -= sizeof(uint16_t);
					break;
				case 3:
					if(HOST_IS_BIGENDIAN) {
						*r_wptr++ = (f_row >> (2 * BITS_PER_CHAR)) & 0xFF;
						put_unaligned(f_row & 0xFFFF, (uint16_t *)(r_wptr));
					} else {
						*r_wptr++ = f_row & 0xFF;
						put_unaligned((f_row >> BITS_PER_CHAR) & 0xFFFF, (uint16_t *)r_wptr);
					}
					r_wptr += sizeof(uint16_t);
					t_len  -= sizeof(uint16_t) + sizeof(uint8_t);
					break;
				case 4:
					put_unaligned(f_row, (uint32_t *)r_wptr);
					r_wptr += SO32;
					t_len  -= SO32;
					break;
				default:
					if(HOST_IS_BIGENDIAN) { /* write big endian data */
						unsigned i = cnt - 1;
						for(; t_len && cnt; t_len--, cnt--, i--)
							*r_wptr++ = (f_row >> (i * BITS_PER_CHAR)) & 0xFF;
					} else { /* write little endian data */
						for(; t_len && cnt; t_len--, cnt--) {
							*r_wptr++ = f_row & 0xFF;
							f_row >>= BITS_PER_CHAR;
						}
					}
				}
			}
			else
			{
				if(f_row > 0x80) {
					*r_wptr++ = 0x7F;
					f_row -= 0x80;
					t_len--;
				}
				if(f_row && t_len) {
					*r_wptr++ = f_row - 1;
					t_len--;
				}
			}
			continue;
		}

		/* check for single, dual or other bits */
		if(s_len < 2 || 0xFF == data[1])
		{
			cnt = popcnt_8(~z);
			if(likely(1 == cnt || 2 == cnt))
			{
				if(likely(1 == cnt))
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
		if(!IS_ALIGN(data, sizeof(uint16_t))) {
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
			cnt = o_row > 0x40 ? 0x40 : o_row;
			cnt = cnt <= t_len ? cnt : t_len;
			*r_wptr++ = (cnt - 1) | 0x80;
			o_row -= cnt;
			r_wptr = my_mempcpy(r_wptr, o_data, cnt);
			o_data += cnt;
			t_len -= cnt;
		}
	}

	if(flags)
	{
		*flags = 0;
		if(num_syn)
		{
			unsigned n_syn_len = num_syn * sizeof(struct sync_block);
			syn_len = n_syn_len < syn_len ? n_syn_len : syn_len;
			r_wptr  = my_mempcpy(r_wptr, synb, syn_len);
			*flags |= syn_len / sizeof(struct sync_block);
		}
	}
	c_len = r_wptr - res;
	if(s_len)
		c_len = -c_len;
	return c_len;
}

ssize_t bitfield_decode(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len)
{
	uint8_t *r_wptr = res, flags;
	ssize_t c_len;
	size_t cnt = 0;

	if(likely(s_len))
	{
		/* read flag byte */
		s_len--;
		flags = *data++;
		/* skip syncs, if any */
		if(likely(flags & NUM_SYNC_MASK)) {
			unsigned sync_size = (flags & NUM_SYNC_MASK) * sizeof(struct sync_block);
			sync_size = s_len > sync_size ? sync_size : s_len;
			s_len -= sync_size;
		}
	}

	while(likely(t_len) && likely(s_len))
	{
		size_t t;
		uint8_t c = *data;

		if(unlikely(!(c & 0x80)))
		{
			data++;
			s_len--;
			cnt = c & 0x7F;
			goto write_out_ff;
		}

		if(unlikely(!(c & 0x40)))
		{
			/* plain data */
			cnt = (c & 0x3F) + 1;
			cnt = cnt <= s_len ? cnt : s_len;
			cnt = cnt <= t_len ? cnt : t_len;
			r_wptr = my_mempcpy(r_wptr, data + 1, cnt);
			t_len -= cnt;
			cnt++;
			data += cnt;
			s_len -= cnt;
			continue;
		}

		if(unlikely(!(c & 0x20)))
		{
			/* two bit */
			*r_wptr++ = ~index_twos[c & 0x1F];
			t_len--;
			data++;
			s_len--;
			continue;
		}

		if(likely(!(c & 0x10)))
		{
		/* one bit */
			*r_wptr++ = ~(1 << (c & 0x0F));
			t_len--;
			data++;
			s_len--;
			continue;
		}

		cnt = c & 0x0F;
		s_len--;
		data++;
		if(cnt > s_len)
			break;
		switch(cnt)
		{
		case 1:
			cnt = *data++;
			s_len--;
			break;
		case 2:
			cnt = get_unaligned((const uint16_t *)data);
			data  += sizeof(uint16_t);
			s_len -= sizeof(uint16_t);
			break;
		case 3:
			cnt = get_unaligned((const uint16_t *)(data + 1));
			if(HOST_IS_BIGENDIAN)
				cnt |= ((size_t)(data[0])) << (2 * BITS_PER_CHAR);
			else
				cnt = (cnt << BITS_PER_CHAR) | data[0];
			data  += sizeof(uint16_t) + sizeof(uint8_t);
			s_len -= sizeof(uint16_t) + sizeof(uint8_t);
			break;
		case 4:
			cnt = get_unaligned((const uint32_t *)data);
			data  += SO32;
			s_len -= SO32;
			break;
		default:
			t = cnt;
			if(HOST_IS_BIGENDIAN) {
				for(cnt = 0; likely(s_len) && likely(t); data++, s_len--, t--) {
					cnt <<= BITS_PER_CHAR;
					cnt |= *data;
				}
			} else {
				unsigned i = 0;
				for(cnt = 0; likely(s_len) && likely(t); data++, s_len--, t--, i++)
					cnt |= ((size_t)*data) << (i * BITS_PER_CHAR);
			}
		}
write_out_ff:
		cnt++;
		cnt = cnt <= t_len ? cnt : t_len;
#if defined(I_LIKE_ASM) && (defined(__i386__) || defined(__x86_64__))
		asm (
				"rep	stosb\n\t"
				"mov	%3, %0\n\t"
				"rep	stosl\n\t"
			: /* %0 */ "=c" (t),
			  /* %1 */ "=D" (r_wptr)
			: /* %2 */ "a" (0xFFFFFFFF),
			  /* %3 */ "rm" (cnt / SO32),
			  /* %4 */ "0" (cnt % SO32),
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

ssize_t bitfield_and(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len)
{
	uint8_t *r_wptr = res, flags;
	ssize_t c_len;
	size_t cnt = 0;

	if(likely(s_len))
	{
		/* read flag byte */
		s_len--;
		flags = *data++;
		/* skip syncs, if any */
		if(flags & NUM_SYNC_MASK) {
			unsigned sync_size = (flags & NUM_SYNC_MASK) * sizeof(struct sync_block);
			sync_size = s_len > sync_size ? sync_size : s_len;
			s_len -= sync_size;
		}
	}

	while(likely(t_len) && likely(s_len))
	{
		size_t t;
		uint8_t c = *data;

		if(likely(!(c & 0x80)))
		{
			data++;
			s_len--;
			cnt = c & 0x7F;
			goto write_out_ff;
		}

		if(unlikely(!(c & 0x40)))
		{
			/* plain data */
			cnt = (c & 0x3F) + 1;
			cnt = cnt <= s_len ? cnt : s_len;
			cnt = cnt <= t_len ? cnt : t_len;
			memand(r_wptr, data + 1, cnt);
			r_wptr += cnt;
			t_len -= cnt;
			cnt++;
			data += cnt;
			s_len -= cnt;
			continue;
		}

		if(unlikely(!(c & 0x20)))
		{
			/* two bit */
			*r_wptr++ &= ~index_twos[c & 0x1F];
			t_len--;
			data++;
			s_len--;
			continue;
		}

		if(likely(!(c & 0x10)))
		{
		/* one bit */
			*r_wptr++ &= ~(1 << (c & 0x0F));
			t_len--;
			data++;
			s_len--;
			continue;
		}

		cnt = c & 0x0F;
		s_len--;
		data++;
		if(cnt > s_len)
			break;
		switch(cnt)
		{
		case 1:
			cnt = *data++;
			s_len--;
			break;
		case 2:
			cnt = get_unaligned((const uint16_t *)data);
			data  += sizeof(uint16_t);
			s_len -= sizeof(uint16_t);
			break;
		case 3:
			cnt = get_unaligned((const uint16_t *)(data + 1));
			if(HOST_IS_BIGENDIAN)
				cnt |= ((size_t)(data[0])) << (2 * BITS_PER_CHAR);
			else
				cnt = (cnt << BITS_PER_CHAR) | data[0];
			data  += sizeof(uint16_t) + sizeof(uint8_t);
			s_len -= sizeof(uint16_t) + sizeof(uint8_t);
			break;
		case 4:
			cnt = get_unaligned((const uint32_t *)data);
			data  += SO32;
			s_len -= SO32;
			break;
		default:
			t = cnt;
			if(HOST_IS_BIGENDIAN) {
				for(cnt = 0; likely(s_len) && likely(t); data++, s_len--, t--) {
					cnt <<= BITS_PER_CHAR;
					cnt |= *data;
				}
			} else {
				unsigned i = 0;
				for(cnt = 0; likely(s_len) && likely(t); data++, s_len--, t--, i++)
					cnt |= ((size_t)*data) << (i * BITS_PER_CHAR);
			}
		}
write_out_ff:
		cnt++;
		/* besides of moving the pointer, nothing to do in the 0xff case */
		cnt = cnt <= t_len ? cnt : t_len;
		r_wptr += cnt;
		/*
		 * our decode came to a intermidiat step, so next is a
		 * break in the 0xff's, which means we will write -> prefetch
		 */
		prefetchw(r_wptr);
		t_len -= cnt;
	}

	c_len = r_wptr - res;
	if(s_len)
		c_len = -c_len;
	return c_len;
}

int bitfield_lookup(const uint32_t *vals, size_t v_len, const uint8_t *data, size_t s_len)
{
	struct sync_block synb[NUM_SYNC_MAX];
	const uint8_t *dwptr;
	size_t i, rem_len, synpos;
	unsigned num_syn = 0, flags;
	uint32_t bpos;

	if(likely(s_len))
	{
		/* read flag byte */
		s_len--;
		flags = *data++;
		/* extract syncs, if any */
		num_syn = flags & NUM_SYNC_MASK;
		if(likely(num_syn))
		{
			unsigned sync_size = num_syn * sizeof(struct sync_block);
			if(unlikely(sync_size >= s_len || num_syn > NUM_SYNC_MAX))
				return 0; /* somethings broken here */
			my_memcpy(synb, data + s_len - sync_size, sync_size);
			s_len -= sync_size;
		}
	}

	for(i = 0, synpos = 0, bpos = 0, rem_len = s_len, dwptr = data;
	    i < v_len; i++)
	{
		/*
		 * when the act. bit index is below the bit pos, we have to
		 * restart.
		 * Sort your numbers, kids. Look next door, introsort_u32.
		 */
		if(unlikely(vals[i] < bpos)) {
			dwptr   = data;
			bpos    = 0;
			rem_len = s_len;
			synpos  = 0;
		}
		/* check if we can fast forward with the sync block */
		if(likely(num_syn))
		{
			unsigned last_fit = synpos;
			while(synpos < num_syn && vals[i] / BITS_PER_CHAR >= synb[synpos].idx)
				last_fit = synpos++;
			synpos = last_fit;
			if(vals[i] / BITS_PER_CHAR >= synb[synpos].idx && synb[synpos].idx > bpos / BITS_PER_CHAR) {
				dwptr   = data + synb[synpos].off;
				bpos    = synb[synpos].idx * BITS_PER_CHAR;
				rem_len = s_len - synb[synpos].off;
			}
		}

		while(likely(rem_len))
		{
			size_t cnt, t;
			uint32_t dist = vals[i] - bpos;
			uint8_t c = *dwptr;

			/* a simple ff run? */
			if(likely(!(c & 0x80)))
			{
				cnt = (c + 1) * BITS_PER_CHAR;
				if(dist < cnt) /* match within run? */
					break; /* cannot match, next val */
				dwptr++;
				rem_len--;
				bpos += cnt;
				continue;
			}

			/* plain data run? */
			if(unlikely(!(c & 0x40)))
			{
				cnt = (c & ~0x80) + 1;
				if(dist < (cnt * BITS_PER_CHAR)) { /* match within run? */
					if(!(dwptr[1 + (dist / 8)] & (1 << (dist % 8))))
						return -1; /* match */
					break; /* no match, next val */
				}
				cnt      = cnt < rem_len ? cnt : rem_len - 1;
				bpos    += cnt * BITS_PER_CHAR;
				dwptr   += cnt + 1;
				rem_len -= cnt + 1;
				continue;
			}

			/* two bit */
			if(unlikely(!(c & 0x20)))
			{
				if(dist < 8) { /* dist in this byte? */
					if(index_twos[c & ~0xC0] & (1 << dist))
						return -1; /* match */
					break; /* no match, next val */
				}
				dwptr++;
				rem_len--;
				bpos += 8;
				continue;
			}

			/* one bit */
			if(likely(!(c & 0x10)))
			{
				if(dist < 8) { /* dist in this byte? */
					if((unsigned)(c & ~0xE0) == dist)
						return -1; /* match */
					break; /* no match, next val */
				}
				dwptr++;
				rem_len--;
				bpos += 8;
				continue;
			}

			/* long ff run */
			t = c & ~0xF0;
			if(t >= rem_len)
				break; /* somethings fishy here... */
			switch(t)
			{
				unsigned j;
			case 1:
				cnt = dwptr[1];
				break;
			case 2:
				cnt = get_unaligned((const uint16_t *)(dwptr + 1));
				break;
			case 3:
				cnt = get_unaligned((const uint16_t *)(dwptr + 2));
				if(HOST_IS_BIGENDIAN)
					cnt |= ((size_t)(dwptr[1])) << (2 * BITS_PER_CHAR);
				else
					cnt = (cnt << BITS_PER_CHAR) | dwptr[1];
				break;
			case 4:
				cnt = get_unaligned((const uint32_t *)(dwptr + 1));
				break;
			default:
				if(HOST_IS_BIGENDIAN) {
					for(cnt = 0, j = 0; likely(j < t); j++) {
						cnt <<= BITS_PER_CHAR;
						cnt |= dwptr[1 + j];
					}
				} else {
					for(cnt = 0, j = 0; likely(j < t); j++)
						cnt |= ((size_t)dwptr[1 + j]) << (j * BITS_PER_CHAR);
				}
			}
			cnt++;

			if(dist < (cnt * BITS_PER_CHAR)) /* match within run? */
				break; /* cannot match, next val */
			dwptr   += 1 + t;
			rem_len -= 1 + t;
			bpos    += cnt * BITS_PER_CHAR;
		}
	}
	return 0;
}

#else
# include <stdbool.h>

struct enc_state
{
	uint8_t *res;
	size_t res_len;
	size_t run_len;
	int b_idx;
	uint8_t b;
	bool last_bit;
};

static bool add_bit_enc(struct enc_state *st, bool bit)
{
	if(likely(st->last_bit == bit)) {
		st->run_len++;
		return true;
	} else
		return false;
}

static bool write_bit(struct enc_state *st, bool bit)
{
	uint8_t x = (uint8_t)bit << st->b_idx;
	st->b |= x;
	st->b_idx--;
	if(st->b_idx < 0)
	{
		if(st->res_len) {
			*st->res++ = st->b;
			st->res_len--;
		} else
			return false;
		st->b_idx = 7;
		st->b = 0;
	}
	return true;
}

static bool elias_delta_enc(struct enc_state *st)
{
	size_t run_len = st->run_len;
	size_t n = flsst(run_len);
	size_t z = flsst(n) - 1;
	size_t t;
	size_t i;

#if 0
	if(st->res_len > 4 * sizeof(size_t))
	{
		size_t *t = (size_t *)st->res;
		*t++ = z;
		*t++ = n;
		*t++ = run_len;
		*t++ = st->run_len;
		st->res = (uint8_t *)t;
		st->res_len -= 4 * sizeof(size_t);
	}
	else
		return false;
#else
	for(i = 0; i < z; i++) {
		if(!write_bit(st, false))
			return false;
	}
	t = n << ((sizeof(size_t) * BITS_PER_CHAR) - (z + 1));
	for(i = 0; i < (z + 1); i++, t <<= 1) {
		if(!write_bit(st, !!(t & (~((~((size_t)0))>>1)))))
			return false;
	}
	t = run_len << ((sizeof(size_t) * BITS_PER_CHAR) - (n - 1));
	for(i = 0; i < (n - 1); i++, t <<= 1) {
		if(!write_bit(st, !!(t & (~((~((size_t)0))>>1)))))
			return false;
	}
#endif
	return true;
}

ssize_t bitfield_encode(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len)
{
	size_t c_len;
	struct enc_state st;
	uint8_t control, *o_res = res;
	bool start_bit, last_add_bit;

	prefetch(data);
	prefetchw(res);
	if(unlikely(!s_len) || unlikely(!t_len))
		return 0;
	start_bit = *data & 0x80;

	control = (uint8_t)start_bit << 4;
	*res++  = control;
	t_len--;

	st.res      = res;
	st.res_len  = t_len;
	st.run_len  = 0;
	st.b_idx    = 7;
	st.b        = 0;
	st.last_bit = start_bit;
	last_add_bit = false;
	for(; likely(st.res_len) && likely(s_len); s_len--, data++)
	{
		unsigned count;
		uint8_t z = *data, mask;
		prefetch(data + 64);

		for(count = BITS_PER_CHAR, mask = 0x80; count; count--, mask >>= 1)
		{
			if(!(last_add_bit = add_bit_enc(&st, !!(z & mask))))
			{
				if(!elias_delta_enc(&st))
					break;
				st.run_len  = 1;
				st.last_bit = !!(z & mask);
			}
		}
	}
	/* flush last data */
	if(last_add_bit) {
		if(!elias_delta_enc(&st))
			s_len++;
	}
	if(st.res_len) {
		if(st.b_idx < 7) {
			*st.res++ = st.b;
			st.res_len--;
		}
	} else if(st.b_idx < 7) {
		/* data left but we can not write it... */
		s_len++;
	}

	c_len = st.res - o_res;
	if(s_len)
		c_len = -c_len;
	return c_len;
}

enum decoding_states
{
	DEC_START,
	DEC_MORE,
	DEC_TRAIL_PREP,
	DEC_TRAIL,
};

struct dec_state
{
	uint8_t *res;
	size_t res_len;
	unsigned l;
	size_t n;
	size_t n_n;
	size_t rem;
	enum decoding_states state;
	int b_idx;
	uint8_t b;
	bool polarity;
};

static bool elias_delta_dec(struct dec_state *st, bool bit)
{
	switch(st->state)
	{
	case DEC_START:
		if(!bit) {
			st->l++;
			break;
		}
		st->state = DEC_MORE;
	case DEC_MORE:
		st->n = (st->n << 1) | bit;
		if(st->l) {
			st->l--;
			break;
		}
		st->state = DEC_TRAIL_PREP;
	case DEC_TRAIL_PREP:
		if(0 == st->n - 1) {
			st->rem += 1 << (st->n - 1);
			return false;
		}
		st->state = DEC_TRAIL;
		break;
	case DEC_TRAIL:
		st->rem = (st->rem << 1) | bit;
		st->n_n++;
		if(st->n_n >= st->n - 1) {
			st->rem += 1 << (st->n - 1);
			return false;
		}
		break;
	}
	return true;
}

static bool write_bits(struct dec_state *st, unsigned level)
{
	size_t n;

#if 0
	if(st->res_len > 2 * sizeof(size_t))
	{
		size_t *t = (size_t *)st->res;
		*t++ = 0;
		*t++ = st->n;
		*t++ = 0;
		*t++ = st->rem;
		st->res = (uint8_t *)t;
		st->res_len -= 2 * sizeof(size_t);
	}
	else
		return false;
#else
	for(n = 0; n < st->rem && st->b_idx >= 0; n++, st->b_idx--)
		st->b |= (uint8_t)st->polarity << st->b_idx;

	if(st->b_idx < 0)
	{
		if(st->res_len) {
			*st->res++ = st->b;
			st->res_len--;
		} else
			return false;
		st->b_idx = 7;
		st->b = 0;
	}

	if(n < st->rem)
	{
		size_t rem_by;
		st->rem -= n;
		rem_by = st->rem / BITS_PER_CHAR;
		if(rem_by) {
			if(rem_by > st->res_len)
				return false;
			memset(st->res, st->polarity ? 0xFF : 0x00, rem_by);
			st->res += rem_by;
			st->res_len -= rem_by;
		}
		st->rem %= BITS_PER_CHAR;
		if(st->rem)
			return write_bits(st, level + 1);
	}
#endif
	return true;
}

ssize_t bitfield_decode(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len)
{
	size_t c_len;
	struct dec_state st;
	uint8_t control;
	bool start_bit;

	prefetch(data);
	prefetchw(res);
	if(unlikely(!s_len) || unlikely(!t_len))
		return 0;
	control   = *data++;
	s_len--;
	start_bit = !!(control & 0x10);

	st.res      = res;
	st.res_len  = t_len;
	st.l        = 0;
	st.n        = 0;
	st.n_n      = 0;
	st.rem      = 0;
	st.b_idx    = 7;
	st.b        = 0;
	st.state    = DEC_START;
	st.polarity = start_bit;
	for(; likely(st.res_len) && likely(s_len); s_len--, data++)
	{
		unsigned count;
		uint8_t z = *data, mask;
		prefetch(data + 64);

		for(count = BITS_PER_CHAR, mask = 0x80; count; count--, mask >>= 1)
		{
			if(!elias_delta_dec(&st, !!(z & mask)))
			{
				if(!write_bits(&st, 0))
					break;
				prefetchw(st.res);
				st.l = 0;
				st.n = 0;
				st.n_n = 0;
				st.rem = 0;
				st.state = DEC_START;
				st.polarity = !st.polarity;
			}
		}
	}
	if(st.res_len) {
		if(st.rem) {
			if(!write_bits(&st, 0))
				s_len++;
		}
	} else if(st.rem) {
		s_len++;
	}

	c_len = st.res - res;
	if(s_len)
		c_len = -c_len;
	return c_len;
}

ssize_t bitfield_and(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len)
{
	return -1;
}

int bitfield_lookup(const uint32_t *vals, size_t v_len, const uint8_t *data, size_t s_len)
{
	return 0;
}
#endif
