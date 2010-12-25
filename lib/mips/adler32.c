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
#if (defined(__mips_loongson_vector_rev) || defined(__mips_dsp)) && defined(__GNUC__)
# define HAVE_ADLER32_VEC
static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len);
# define MIN_WORK 16
#endif

#include "../generic/adler32.c"

#if defined(__mips_loongson_vector_rev) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <loongson.h>

# define SOV8 (sizeof(uint8x8_t))
//#define VNMAX (6*NMAX)
# define VNMAX (4*NMAX)

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
		static const int16x4_t vord_lo = {5,6,7,8};
		static const int16x4_t vord_hi = {1,2,3,4};
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
			"dsrl	%0, %0, %5\n\t"
			"dsll	%0, %0, %5\n\t"
			"biadd	%4, %0\n\t"
			"punpcklbh	%3, %0, %6\n\t"
			"paddw	%1, %1, %4\n\t"
			"punpckhbh	%4, %0, %6\n\t"
			"pmaddhw	%3, %3, %7\n\t"
			"pmaddhw	%4, %4, %8\n\t"
			"paddw	%2, %2, %3\n\t"
			"paddw	%2, %2, %4\n\t"
			: /* %0  */ "=&f" (in8),
			  /* %1  */ "=f" (vs1),
			  /* %2  */ "=f" (vs2),
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
			uint32x2_t vs1_r;
			int t;

			/* gcc generates horible loop code... */
			asm volatile (
				".set noreorder\n\t"
				"xor	%3, %3, %3\n\t"
				"1:\n\t"
				"ldc1	%0, (%6)\n\t"
				"addiu	%7, %7, -8\n\t"
				"paddw	%3, %1, %3\n\t"
				"biadd	%5, %0\n\t"
				"punpcklbh	%4, %0, %9\n\t"
				"paddw	%1, %5, %1\n\t"
				"punpckhbh	%5, %0, %9\n\t"
				"pmaddhw	%4, %4, %10\n\t"
				"pmaddhw	%5, %5, %11\n\t"
				"sltiu	%8, %7, 8\n\t"
				"paddw	%2, %2, %4\n\t"
				"paddw	%2, %2, %5\n\t"
				"beqz	%8, 2f\n\t"
				"addiu	%6, %6, 8\n\t"
				"ldc1	%0, (%6)\n\t"
				"addiu	%7, %7, -8\n\t"
				"paddw	%3, %1, %3\n\t"
				"biadd	%5, %0\n\t"
				"punpcklbh	%4, %0, %9\n\t"
				"dsll	%5, %5, %12\n\t"
				"paddw	%1, %5, %1\n\t"
				"punpckhbh	%5, %0, %9\n\t"
				"pmaddhw	%4, %4, %10\n\t"
				"pmaddhw	%5, %5, %11\n\t"
				"sltiu	%8, %7, 8\n\t"
				"paddw	%2, %2, %4\n\t"
				"paddw	%2, %2, %5\n\t"
				"bnez	%8, 1b\n\t"
				"addiu	%6, %6, 8\n\t"
				"2:\n\t"
				".set reorder\n\t"
				: /* %0  */ "=&f" (in8),
				  /* %1  */ "=f" (vs1),
				  /* %2  */ "=f" (vs2),
				  /* %3  */ "=&f" (vs1_r),
				  /* %4  */ "=&f" (in_lo),
				  /* %5  */ "=&f" (in_hi),
				  /* %6  */ "=d" (buf),
				  /* %7  */ "=r" (k),
				  /* %8  */ "=r" (t)
				: /* %9  */ "f" (v0),
				  /* %10 */ "f" (vord_lo),
				  /* %11 */ "f" (vord_hi),
				  /* %12 */ "f" (32),
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
				"dsll	%0, %0, %5\n\t"
				"dsrl	%0, %0, %5\n\t"
				"paddw	%2, %2, %3\n\t"
				"biadd	%4, %0\n\t"
				"punpcklbh	%3, %0, %7\n\t"
				"paddw	%1, %1, %4\n\t"
				"punpckhbh	%4, %0, %7\n\t"
				"pmaddhw	%3, %3, %8\n\t"
				"pmaddhw	%4, %4, %9\n\t"
				"paddw	%2, %2, %3\n\t"
				"paddw	%2, %2, %4\n\t"
				: /* %0  */ "=&f" (in8),
				  /* %1  */ "=f" (vs1),
				  /* %2  */ "=f" (vs2),
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
		vs1 = paddw_u(vs1, (uint32x2_t)pshufh_u((uint16x4_t)v0, (uint16x4_t)vs1, 0xE));
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

static char const rcsid_a32ml[] GCC_ATTR_USED_VAR = "$Id: $";
#elif defined(__mips_dsp) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */

typedef signed char v4i8 __attribute__((vector_size(4)));
typedef long long a64;

# define SOV4 (sizeof(v4i8))
# define VNMAX (4*NMAX)

static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1, s2;

	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;

	if(likely(len >= 2*SOV4))
	{
		static const v4i8 v1 = {1,1,1,1};
		v4i8 vord_lo, vord_hi, vord;
		 /*
		  * yes, our sums should stay within 32 bit, that accs are
		  * 64 bit is to no (real) use to us.
		  */
		uint32_t vs1, vs2, vs1h, vs2h;
		v4i8 in4;
		unsigned f, n;
		unsigned k;
		uint32_t srl;

		if(HOST_IS_BIGENDIAN) {
			vord_lo = (v4i8){8,7,6,5};
			vord_hi = (v4i8){4,3,2,1};
		} else {
			vord_lo = (v4i8){5,6,7,8};
			vord_hi = (v4i8){1,2,3,4};
		}
		vord = vord_hi;
		/*
		 * Add stuff to achieve alignment
		 */
		/* align hard down */
		f = (unsigned) ALIGN_DOWN_DIFF(buf, SOV4);
		n = SOV4 - f;
		buf = (const unsigned char *)ALIGN_DOWN(buf, SOV4);

		/* add n times s1 to s2 for start round */
		s2 += s1 * n;

		k = len < VNMAX ? (unsigned)len : VNMAX;
		len -= k;

		/* insert scalar start somewhere */
		vs1 = s1;
		vs2 = s2;
		vs1h = 0;
		vs2h = 0;

		/* get input data */
		srl = *(const uint32_t *)buf;
		if(HOST_IS_BIGENDIAN) {
			srl <<= f * BITS_PER_CHAR;
			srl >>= f * BITS_PER_CHAR;
		} else {
			srl >>= f * BITS_PER_CHAR;
			srl <<= f * BITS_PER_CHAR;
		}
		in4 = (v4i8)srl;

		/* add all byte horizontal and add to old qword */
		vs1 = __builtin_mips_dpau_h_qbl(vs1, in4, v1);
		vs1 = __builtin_mips_dpau_h_qbr(vs1, in4, v1);

		/* apply order, add 4 byte horizontal and add to old qword */
		vs2 = __builtin_mips_dpau_h_qbl(vs2, in4, vord);
		vs2 = __builtin_mips_dpau_h_qbr(vs2, in4, vord);

		buf += SOV4;
		k -= n;

		if(likely(k >= 2*SOV4)) do
		{
			uint32_t vs1_r, vs1h_r, t1, t2;
			v4i8 in4h;

			/*
			 * gcc and special regs...
			 * Since the mips acc are special, gcc fears to treat.
			 * Esp. gcc thinks it's cool to have the result ready in
			 * the gpr at the end of this main loop, so he hoist a
			 * move out of the acc and back in into the loop.
			 *
			 * So do it by hand...
			 */
			asm (
				".set noreorder\n\t"
				"mflo	%10, %q0\n\t"
				"xor	%6, %6, %6\n\t"
				"xor	%7, %7, %7\n"
				"1:\n\t"
				"lw	%4,(%9)\n\t"
				"lw	%5,4(%9)\n\t"
				"addu	%6, %6, %10\n\t"
				"dpau.h.qbl	%q0, %4, %14\n\t"
				"addiu	%8, %8, -8\n\t"
				"mflo	%10, %q1\n\t"
				"dpau.h.qbr	%q0, %4, %14\n\t"
				"dpau.h.qbl	%q1, %5, %14\n\t"
				"sltu	%11, %8, 8\n\t"
				"dpau.h.qbl	%q3, %4, %13\n\t"
				"addu	%7, %7 ,%10\n\t"
				"dpau.h.qbl	%q2, %5, %12\n\t"
				"addiu	%9, %9, 8\n\t"
				"mflo	%10, %q0\n\t"
				"dpau.h.qbr	%q1, %5, %14\n\t"
				"dpau.h.qbr	%q3, %4, %13\n\t"
				"beqz	%11, 1b\n\t"
				"dpau.h.qbr	%q2, %5, %12\n\t"
				".set reorder"
				: /* %0  */ "=a" (vs1),
				  /* %1  */ "=A" (vs1h),
				  /* %2  */ "=A" (vs2),
				  /* %3  */ "=A" (vs2h),
				  /* %4  */ "=&r" (in4),
				  /* %5  */ "=&r" (in4h),
				  /* %6  */ "=&r" (vs1_r),
				  /* %7  */ "=&r" (vs1h_r),
				  /* %8  */ "=r" (k),
				  /* %9  */ "=d" (buf),
				  /* %10 */ "=r" (t1),
				  /* %11 */ "=r" (t2)
				: /* %12 */ "r" (vord_lo),
				  /* %13 */ "r" (vord_hi),
				  /* %14 */ "r" (v1),
				  /* %  */ "0" (vs1),
				  /* %  */ "1" (vs1h),
				  /* %  */ "2" (vs2),
				  /* %  */ "3" (vs2h),
				  /* %  */ "8" (k),
				  /* %  */ "9" (buf)
			);

			/* reduce vs1 round sum before multiplying by 8 */
			vs1_r  = reduce(vs1_r);
			vs1h_r = reduce(vs1h_r);
			/* add all vs1 for 8 times */
			/* reduce the vectors to something in the range of BASE */
			vs2  = reduce(vs2  + vs1_r  * 8);
			vs2h = reduce(vs2h + vs1h_r * 8);
			vs1  = reduce(vs1);
			vs1h = reduce(vs1h);
			len += k;
			k = len < VNMAX ? (unsigned)len : VNMAX;
			len -= k;
		} while(likely(k >= 2*SOV4));
		vs1 += vs1h;
		vs2 += vs2h;
		if(likely(k >= SOV4))
		{
			/* get input data */
			in4 = *(const v4i8 *)buf;
			vs2 += vs1 * 4;

			/* add all byte horizontal and add to old qword */
			vs1 = __builtin_mips_dpau_h_qbl(vs1, in4, v1);
			vs1 = __builtin_mips_dpau_h_qbr(vs1, in4, v1);

			/* apply order, add 4 byte horizontal and add to old qword */
			vs2 = __builtin_mips_dpau_h_qbl(vs2, in4, vord);
			vs2 = __builtin_mips_dpau_h_qbr(vs2, in4, vord);

			k -= SOV4;
			buf += SOV4;
		}

		if(likely(k))
		{
			uint32_t vk;

			/*
			 * handle trailer
			 */
			f = SOV4 - k;

			/* add k times vs1 for this trailer */
			vk = vs1 * k;

			/* get input data */
			srl = *(const uint32_t *)buf;
			if(HOST_IS_BIGENDIAN) {
				srl >>= f * BITS_PER_CHAR;
				srl <<= f * BITS_PER_CHAR;
			} else {
				srl <<= f * BITS_PER_CHAR;
				srl >>= f * BITS_PER_CHAR;
			}
			in4 = (v4i8)srl;

			/* add all byte horizontal and add to old qword */
			vs1 = __builtin_mips_dpau_h_qbl(vs1, in4, v1);
			vs1 = __builtin_mips_dpau_h_qbr(vs1, in4, v1);

			/* apply order, add 4 byte horizontal and add to old dword */
			vs2 = __builtin_mips_dpau_h_qbl(vs2, in4, vord);
			vs2 = __builtin_mips_dpau_h_qbr(vs2, in4, vord);

			vs2 += vk;

			buf += k;
			k -= k;
		}

		/* shake and roll */
		s1 = (uint32_t)vs1;
		s2 = (uint32_t)vs2;
	}

	if(unlikely(len)) do {
		s1 += *buf++;
		s2 += s1;
	} while (--len);
// TODO: i hate synthetized processors
	/*
	 * If we have a full "high-speed" Multiply/Devide Unit,
	 * the old multiply-by-reciproc should be the best way
	 * (since then we should get a 32x32 mul in 2 cycles?),
	 * but wait, we need 4 muls == 8 + 2 shift + 2 sub + 2 load
	 * imidiate.
	 * If we do not have the "full" MDU, a mul takes 32 cycles
	 * and a div 25 (WTF?).
	 * GCC generates a classic div, prop. needs the right -mtune
	 * for a mul.
	 * Use our hand reduce, 17 simple instructions for both operands.
	 * Maybe we should do the same thing for loongson
	 */
	/* s2 should be something like 24 bit here */
	s1 = reduce_x(s1);
	s2 = reduce_x(s2);

	return s2 << 16 | s1;
}

static char const rcsid_a32md[] GCC_ATTR_USED_VAR = "$Id: $";
#endif
/* EOF */
