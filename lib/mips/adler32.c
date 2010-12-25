/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   ppc implementation
 * Copyright (C) 1995-2004 Mark Adler
 * Copyright (C) 2009-2010 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/*
 * This is not the original adler32.c from the zlib distribution,
 * but a heavily modified version. If you are looking for the
 * original, please go to zlib.net.
 */
#if defined(__mips_loongson_vector_rev) && defined(__GNUC__)
# define HAVE_ADLER32_VEC
static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len);
# define MIN_WORK 16
#endif

#include "../generic/adler32.c"

#if defined(__mips_loongson_vector_rev) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <loongson.h>

#define SOV8 (sizeof(uint8x8_t))
//#define VNMAX (6*NMAX)
#define VNMAX (3*NMAX)

static inline uint32x2_t vector_reduce(uint32x2_t x)
{
	uint32x2_t y;

	y = psllw_u(x, 16);
	y = psrlw_u(y, 16);
	x = psrlw_u(x, 16);
	y = psubw_u(y, x);
	x = psllw_u(x, 4);
	x = paddw_u(x, y);
	return x;
}

static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1, s2;

	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;

	if(likely(len >= 2*SOV8))
	{
		/* Loongsons and their ST MMX foo are little endian */
		static const int16x4_t vord_lo = {8,7,6,5};
		static const int16x4_t vord_hi = {4,3,2,1};
		uint32x2_t vs2, vs1;
		int16x4_t in_lo, in_hi;
		uint8x8_t v0 = {0};
		uint8x8_t in8;
		unsigned f, n;
		unsigned k;

		/*
		 * Add stuff to achieve alignment
		 */
		/* align hard down */
		f = (unsigned) ALIGN_DOWN_DIFF(buf, SOV8);
		n = SOV8 - f;
		buf = (const unsigned char *)ALIGN_DOWN(buf, SOV8);

		/* add n times s1 to s2 for start round */
		s2 += s1 * n;

		k = len < VNMAX ? (unsigned)len : VNMAX;
		len -= k;

		/* insert scalar start somewhere */
		vs1 = (uint32x2_t)(uint64_t)s1;
		vs2 = (uint32x2_t)(uint64_t)s2;

		/* get input data */
		/* add all byte horizontal and add to old qword */
		/* apply order, add 4 byte horizontal and add to old dword */
		asm (
			"ldc1	%0, %9\n\t"
			"dsll	%0, %0, %5\n\t"
			"dsrl	%0, %0, %5\n\t"
			"biadd	%3, %0\n\t"
			"paddd	%1, %1, %3\n\t"
			"punpcklbh	%3, %0, %6\n\t"
			"punpckhbh	%4, %0, %6\n\t"
			"pmaddhw	%3, %3, %7\n\t"
			"pmaddhw	%4, %4, %8\n\t"
			"paddw	%3, %3, %4\n\t"
			"paddw	%2, %2, %3\n\t"
			: /* %0  */ "=&f" (in8),
			  /* %1  */ "=&f" (vs1),
			  /* %2  */ "=&f" (vs2),
			  /* %3  */ "=&f" (in_lo),
			  /* %4  */ "=&f" (in_hi)
			: /* %5  */ "f" (f * BITS_PER_CHAR),
			  /* %6  */ "f" (v0),
			  /* %7  */ "f" (vord_lo),
			  /* %8  */ "f" (vord_hi),
			  /* %9  */ "m" (*buf),
			  /* %10 */ "1" (vs1),
			  /* %11 */ "2" (vs2)
		);
		buf += SOV8;
		k -= n;

		if(likely(k >= SOV8)) do
		{
			uint32x2_t vs1_r, extra_bit;
			int t;

			/* gcc generates horible loop code... */
			asm volatile (
				".set noreorder\n\t"
				"xor	%3, %3, %3\n\t"
				"1:\n\t"
				"ldc1	%0, (%6)\n\t"
				"addiu	%7, %7, -8\n\t"
				"paddd	%3, %1, %3\n\t"
				"biadd	%4, %0\n\t"
				"paddd	%1, %4, %1\n\t"
				"punpcklbh	%4, %0, %10\n\t"
				"punpckhbh	%5, %0, %10\n\t"
				"pmaddhw	%4, %4, %11\n\t"
				"pmaddhw	%5, %5, %12\n\t"
				"paddw	%4, %4, %5\n\t"
				"paddw	%2, %2, %4\n\t"
				"sltiu	%8, %7, 8\n\t"
				"beqz	%8, 1b\n\t"
				"addiu	%6, %6, 8\n\t"
				".set reorder\n\t"
				"and	%9, %3, %13\n\t"
				"dsrl	%3, %3, %14\n\t"
				"pshufh	%3, %3, %15\n\t"
				: /* %0  */ "=f" (in8),
				  /* %1  */ "=f" (vs1),
				  /* %2  */ "=f" (vs2),
				  /* %3  */ "=f" (vs1_r),
				  /* %4  */ "=f" (in_lo),
				  /* %5  */ "=f" (in_hi),
				  /* %6  */ "=d" (buf),
				  /* %7  */ "=r" (k),
				  /* %8  */ "=r" (t),
				  /* %9  */ "=f" (extra_bit)
				: /* %10 */ "f" (v0),
				  /* %11 */ "f" (vord_lo),
				  /* %12 */ "f" (vord_hi),
				  /* %13 */ "f" (0x1),
				  /* %14 */ "f" (4),
				  /* %15 */ "f" (0x44),
				  /* %12 */ "1" (vs1),
				  /* %13 */ "2" (vs2),
				  /* %15 */ "6" (buf),
				  /* %16 */ "7" (k)
			);
			/*
			 * and the rest of the generated code also looks awful,
			 * looks like gcc does not know he can shift and and in
			 * the copro regs + is a little lost with reg allocation
			 * in the copro...
			 */

			/* reduce vs1 round sum before multiplying by 8 */
			vs1_r = vector_reduce(vs1_r);
			vs1_r = paddw_u(vs1_r, extra_bit);
			/* add all vs1 for 8 times */
			vs2 = paddw_u(psllw_u(vs1_r, 3), vs2);
			/* reduce the vectors to something in the range of BASE */
			vs2 = vector_reduce(vs2);
			vs1 = vector_reduce(vs1);
			len += k;
			k = len < VNMAX ? (unsigned)len : VNMAX;
			len -= k;
		} while(likely(k >= SOV8));

		if(likely(k))
		{
			uint32x2_t vk;
			/*
			 * handle trailer
			 */
			f = SOV8 - k;

			vk = (uint32x2_t)(uint64_t)k;

			/* get input data */
			/* add all byte horizontal and add to old qword */
			/* add k times vs1 for this trailer */
			/* apply order, add 4 byte horizontal and add to old dword */
			asm (
				"ldc1	%0, %10\n\t"
				"pmuluw	%3, %1, %6\n\t"
				"paddw	%2, %2, %3\n\t"
				"dsll	%0, %0, %5\n\t"
				"dsrl	%0, %0, %5\n\t"
				"biadd	%3, %0\n\t"
				"paddd	%1, %1, %3\n\t"
				"punpcklbh	%3, %0, %7\n\t"
				"punpckhbh	%4, %0, %7\n\t"
				"pmaddhw	%3, %3, %8\n\t"
				"pmaddhw	%4, %4, %9\n\t"
				"paddw	%3, %3, %4\n\t"
				"paddw	%2, %2, %3\n\t"
				: /* %0  */ "=&f" (in8),
				  /* %1  */ "=&f" (vs1),
				  /* %2  */ "=&f" (vs2),
				  /* %3  */ "=&f" (in_lo),
				  /* %4  */ "=&f" (in_hi)
				: /* %5  */ "f" (f * BITS_PER_CHAR),
				  /* %6  */ "f" (vk),
				  /* %7  */ "f" (v0),
				  /* %8  */ "f" (vord_lo),
				  /* %9  */ "f" (vord_hi),
				  /* %10 */ "m" (*buf),
				  /* %11 */ "1" (vs1),
				  /* %12 */ "2" (vs2)
			);

			buf += k;
			k -= k;
		}

		/* add horizontal */
		vs2 = paddw_u(vs2, (uint32x2_t)pshufh_u((uint16x4_t)v0, (uint16x4_t)vs2, 0xE));
		/* shake and roll */
		s1 = (uint32_t)(uint64_t)vs1;
		s2 = (uint32_t)(uint64_t)vs2;
	}

	if(unlikely(len)) do {
		s1 += *buf++;
		s2 += s1;
	} while (--len);
	MOD(s1);
	MOD(s2);

	return s2 << 16 | s1;
}

static char const rcsid_a32m[] GCC_ATTR_USED_VAR = "$Id: $";
#endif
/* EOF */
