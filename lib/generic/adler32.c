/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   generic implementation
 * Copyright (C) 1995-2004 Mark Adler
 * Copyright (C) 2010-2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/*
 * This is not the original adler32.c from the zlib distribution,
 * but a heavily modified version. If you are looking for the
 * original, please go to zlib.net.
 */

#define BASE 65521UL    /* largest prime smaller than 65536 */
#define NMAX 5552

/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */
#define DO1(buf,i)  {adler += (buf)[i]; sum2 += adler;}
#define DO2(buf,i)  DO1(buf,i); DO1(buf,i+1);
#define DO4(buf,i)  DO2(buf,i); DO2(buf,i+2);
#define DO8(buf,i)  DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)   DO8(buf,0); DO8(buf,8);

/* use NO_DIVIDE if your processor does not do division in hardware */
#ifdef NO_DIVIDE
/* use NO_SHIFT if your processor does shift > 1 by loop */
# ifdef NO_SHIFT
#  define reduce(a) reduce_full(a)
#  define reduce_x(a) \
	do { \
		if (MIN_WORK >= (1 << 6) && a >= (BASE << 6)) a -= (BASE << 6); \
		if (MIN_WORK >= (1 << 5) && a >= (BASE << 5)) a -= (BASE << 5); \
		if (a >= (BASE << 4)) a -= (BASE << 4); \
		if (a >= (BASE << 3)) a -= (BASE << 3); \
		if (a >= (BASE << 2)) a -= (BASE << 2); \
		if (a >= (BASE << 1)) a -= (BASE << 1); \
		if (a >= BASE) a -= BASE; \
	} while (0)
#  define reduce_full(a) \
	do { \
		if (a >= (BASE << 16)) a -= (BASE << 16); \
		if (a >= (BASE << 15)) a -= (BASE << 15); \
		if (a >= (BASE << 14)) a -= (BASE << 14); \
		if (a >= (BASE << 13)) a -= (BASE << 13); \
		if (a >= (BASE << 12)) a -= (BASE << 12); \
		if (a >= (BASE << 11)) a -= (BASE << 11); \
		if (a >= (BASE << 10)) a -= (BASE << 10); \
		if (a >= (BASE << 9)) a -= (BASE << 9); \
		if (a >= (BASE << 8)) a -= (BASE << 8); \
		if (a >= (BASE << 7)) a -= (BASE << 7); \
		if (a >= (BASE << 6)) a -= (BASE << 6); \
		if (a >= (BASE << 5)) a -= (BASE << 5); \
		if (a >= (BASE << 4)) a -= (BASE << 4); \
		if (a >= (BASE << 3)) a -= (BASE << 3); \
		if (a >= (BASE << 2)) a -= (BASE << 2); \
		if (a >= (BASE << 1)) a -= (BASE << 1); \
		if (a >= BASE) a -= BASE; \
	} while (0)
# else
#  define reduce(x) \
	do { \
		uint32_t y = x & 0x0000ffff; \
		x >>= 16; \
		y -= x; \
		x <<= 4; \
		x += y; \
	} while(0)
#  define reduce_x(x) \
	do { \
		uint32_t y = x & 0x0000ffff; \
		x >>= 16; \
		y -= x; \
		x <<= 4; \
		x += y; \
		x = x >= BASE ? x - BASE : x; \
	} while(0)
#  define reduce_full(x) \
	do { \
		uint32_t y = x & 0x0000ffff; \
		x >>= 16; \
		y -= x; \
		x <<= 4; \
		x += y; \
	} while(x >= BASE)
# endif
#else
# define reduce(a) a %= BASE
# define reduce_x(a) a %= BASE
# define reduce_full(a) a %= BASE
#endif

#ifndef MIN_WORK
# define MIN_WORK 16
#endif

/* ========================================================================= */
static noinline uint32_t adler32_1(uint32_t adler, const uint8_t *buf, unsigned len GCC_ATTR_UNUSED_PARAM)
{
	uint32_t sum2;

	/* split Adler-32 into component sums */
	sum2 = (adler >> 16) & 0xffff;
	adler &= 0xffff;

	adler += buf[0];
	if(adler >= BASE)
		adler -= BASE;
	sum2 += adler;
	if(sum2 >= BASE)
		sum2 -= BASE;
	return adler | (sum2 << 16);
}

static noinline uint32_t adler32_common(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t sum2;

	/* split Adler-32 into component sums */
	sum2 = (adler >> 16) & 0xffff;
	adler &= 0xffff;

	while(len--) {
		adler += *buf++;
		sum2 += adler;
	}
	if(adler >= BASE)
		adler -= BASE;
	/* only added so many BASE's */
	reduce_x(sum2);
	return adler | (sum2 << 16);
}

#ifndef HAVE_ADLER32_VEC
static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t sum2;
	unsigned n;

	/* split Adler-32 into component sums */
	sum2 = (adler >> 16) & 0xffff;
	adler &= 0xffff;

	/* do length NMAX blocks -- requires just one modulo operation */
	while(len >= NMAX)
	{
		len -= NMAX;
		n = NMAX / 16;	/* NMAX is divisible by 16 */
		do {
			DO16(buf);	/* 16 sums unrolled */
			buf += 16;
		} while(--n);
		reduce_full(adler);
		reduce_full(sum2);
	}

	/* do remaining bytes (less than NMAX, still just one modulo) */
	if(len)
	{	/* avoid modulos if none remaining */
		while(len >= 16) {
			len -= 16;
			DO16(buf);
			buf += 16;
		}
		while (len--) {
			adler += *buf++;
			sum2 += adler;
		}
		reduce_full(adler);
		reduce_full(sum2);
	}

	/* return recombined sums */
	return adler | (sum2 << 16);
}
#endif

uint32_t adler32(uint32_t adler, const uint8_t *buf, unsigned len)
{
	/* in case user likes doing a byte at a time, keep it fast */
	if(1 == len)
		return adler32_1(adler, buf, len); /* should create a "fast" tailcall */

	/* initial Adler-32 value (deferred check for len == 1 speed) */
	if(buf == NULL)
		return 1L;

	/* in case short lengths are provided, keep it somewhat fast */
	if(unlikely(len < MIN_WORK))
		return adler32_common(adler, buf, len); /* should create a "fast" tailcall */

	return adler32_vec(adler, buf, len);
}

static char const rcsid_a32g[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
