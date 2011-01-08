/*
 * vsnprintf.c
 * {v}snprintf with extensions
 *
 * Copyright (c) 2008-2011 Jan Seiffert
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
 *
 * Floating Point conversion code is some one else clever foo,
 * but i forgot who...
 *
 * $Id:$
 */

/*
 * FP-Code was retrieved as a C file without any attribution.
 *
 * All i remember is a paper about efficient floating point
 * conversion. And after digging a reference impl. from one
 * of the paper authors showed up, or something like that.
 *
 * Who ever it was:
 * "For those about to rock: (fire!) We salute you"
 *
 * And i think it's not that efficient, but avoids the fpu
 * quite succefully ;-)
 */

#include "other.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#ifdef HAVE_WCHAR_H
# include <wchar.h>
#endif
#include "my_bitops.h"
#include "itoa.h"
#include "log_facility.h"
#include "combo_addr.h"

/*
 * When times get desperate...
 *
 * No, its not that bad ^^
 *
 * Since all those *printf-familiy functions are hell to implement,
 * mere mortals fear to treat, and libc's go the way of stdio-boxing.
 *
 * Write vfprintf one time, all other just feed into this function.
 * All you need most of the time is a little argument swizzle,
 * but sometimes...
 * ... the string printing printf's have to generate a special
 * FILE-stream, poking deep inside the internals of this opaque
 * libc internal, redirecting the output, checking the length.
 *
 * Unfornatly this it not very efficient if you want to generate
 * strings (you want to /format/ one time, but /print/ several
 * times), all those layers of abstraction clog your poor little
 * CPU.
 *
 * So lets do it ourself.
 * Yes, this is madness, implementing every little fancy detail
 * of whats possible in a format string really is something for
 * the brave at heart. But since we don't want to replace libc
 * fully we only need to implement a needed subset.
 *
 * This way we can also add our own formats, like printing an
 * IP, so we can sweep away a lot of temp printing to stack
 * buffers throughout the program.
 *
 * Known limitations:
 * - Does not always return the correct needed length when truncating
 * - Can barely print floating point
 * - the maxlen is not always obeyed...
 * - Very fancy modifier/conversion options are /broken/
 * - positional argument do not work
 * - And surely not std. conform
 */

#ifdef HAVE_TIMODE
typedef unsigned __uint128 __attribute__((mode(TI)));
#endif

union all_types
{
	signed char c;
	unsigned char uc;
	short s;
	unsigned short us;
	int i;
	unsigned u;
	long l;
	unsigned long ul;
	long long ll;
	unsigned long long ull;
	double e;
	long double L;
	int64_t q;
	uint64_t uq;
	intmax_t j;
	uintmax_t uj;
	ssize_t z;
	size_t uz;
	ptrdiff_t t;
};

enum length_mod
{
	MOD_NONE = 0,
	MOD_CHAR,
	MOD_SHORT,
	MOD_LONG,
	MOD_LONGLONG,
	MOD_LONGDOUBLE,
	MOD_QUAD,
	MOD_INTMAX_T,
	MOD_SIZE_T,
	MOD_PTRDIFF_T,
	MOD_DECIMAL32,
	MOD_DECIMAL64,
	MOD_DECIMAL128,
	MOD_MAX_NUM
} GCC_ATTR_PACKED;

#define type_log10_aprox(x) (((((sizeof(x) * BITS_PER_CHAR)+1)*1233)>>12)+1)

struct format_spec
{
	va_list ap;
	const char *fmt_start;
	char *wptr;
	size_t len;
	size_t maxlen;
	unsigned precision;
	unsigned width;
	union {unsigned u; unsigned long long ull; void *v; } t_store;
	union
	{
		struct
		{
			bool sign:1;
			bool zero:1;
			bool space:1;
			bool left:1;
			bool alternate:1;
			bool ip:1;
			bool guid:1;
			bool negative:1;
			bool th_group:1;
		} flags;
		int xyz;
	} u;
	enum length_mod mod;
	char conv_buf[(type_log10_aprox(intmax_t) * 2) + 4];
};

typedef const char *(*fmt_func)(char *buf, const char *fmt, struct format_spec *);

static const char HEXUC_STRING[] = "0123456789ABCDEFGHIJKL";
static const char HEXLC_STRING[] = "0123456789abcdefghijkl";

#define format_dispatch(buf, fmt, f) format_table[*(const unsigned char *)(fmt)]((buf), ((fmt)+1), (f))
static const fmt_func format_table[256];

/*
 * Helper
 */
static noinline const char *end_format(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->wptr = buf;
	return fmt;
}

static inline char *strncpyrev(char *dst, const char *end, const char *start, size_t n)
{
	char *r = dst + ((end + 1) - start);
	while(n-- && end >= start)
		*dst++ = *end--;
	return r;
}

static GCC_ATTR_FASTCALL char *put_dec_full5(char *buf, unsigned q)
{
	unsigned r;
	char a = '0';

#if 0
	if(q >= 50000) {
		a = '5';
		q -= 50000;
	}

	r      = (q * 0xcccd) >> 19;
	*buf++ = (q - 10 * r) + '0';
#else
	r      = (q * (uint64_t)0xcccd) >> 19;
	*buf++ = (q - 10 * r) + '0';
#endif

	q      = (r * 0x199a) >> 16;
	*buf++ = (r - 10 * q)  + '0';

	r      = (q * 0xcd) >> 11;
	*buf++ = (q - 10 * r)  + '0';

	q      = (r * 0xd) >> 7;
	*buf++ = (r - 10 * q) + '0';

	*buf++ = q + a;

	return buf;
}

static GCC_ATTR_FASTCALL char *put_dec_full4(char *buf, unsigned q)
{
	unsigned r;

	r      = (q * 0xcccd) >> 19;
	*buf++ = (q - 10 * r) + '0';

	q      = (r * 0x199a) >> 16;
	*buf++ = (r - 10 * q)  + '0';

	r      = (q * 0xcd) >> 11;
	*buf++ = (q - 10 * r)  + '0';

	*buf++ = r + '0';

	return buf;
}


char GCC_ATTR_FASTCALL *put_dec_trunc(char *buf, unsigned q)
{
	unsigned r;

	/* if we have to print more than 5 digit, use the full version */
	if(q > 9999)
		return put_dec_full5(buf, q);

	r      = (q * 0x199a) >> 16;
	*buf++ = (q - 10 * r) + '0';

	if(r)
	{
		q      = (r * 0xcd) >> 11;
		*buf++ = (r - 10 * q)  + '0';
	
		if(q)
		{
			r      = (q * 0xcd) >> 11;
			*buf++ = (q - 10 * r)  + '0';

			if(r)
				*buf++ = r + '0';
		}
	}

	return buf;
}

static noinline char *put_dec(char *buf, unsigned num)
{
	/* larger numbers are a more unlikely, or: small numbers are common */
	while(unlikely(num >= 100000)) {
		unsigned rem = num % 100000;
		num /= 100000;
		buf  = put_dec_full5(buf, rem);
	}
	return put_dec_trunc(buf, num);
}

static noinline char *put_dec_ll(char *buf, unsigned long long num)
{
	if(sizeof(size_t) == sizeof(num))
	{
		while(unlikely(num >= 100000)) {
			unsigned rem = num % 100000;
			num /= 100000;
			buf  = put_dec_full5(buf, rem);
		}
		return put_dec_trunc(buf, num);
	}
	else
	{
		unsigned d3, d2, d1, q;

		if(num < 10) {
			*buf++ = '0' + (unsigned)num;
			return buf;
		}

		d1  = (num >> 16) & 0xFFFF;
		d2  = (num >> 32) & 0xFFFF;
		d3  = (num >> 48) & 0xFFFF;

		q   = 656 * d3 + 7296 * d2 + 5536 * d1 + (num & 0xFFFF);

		buf = put_dec_full4(buf, q % 10000);
		q   = q / 10000;

		d1  = q + 7671 * d3 + 9496 * d2 + 6 * d1;
		buf = put_dec_full4(buf, d1 % 10000);
		q   = d1 / 10000;

		d2  = q + 4749 * d3 + 42 * d2;
		buf = put_dec_full4(buf, d2 % 10000);
		q   = d2 / 10000;

		d3  = q + 281 * d3;
		buf = put_dec_full4(buf, d3 % 10000);
		q   = d3 / 10000;

		buf = put_dec_full4(buf, q);

		while(buf[-1] == '0')
			--buf;
		return buf;
	}
}

/*****************************************************************************************
 *
 * Conversions
 *
 *****************************************************************************************/

static noinline const char *decimal_finish(char *buf, const char *fmt, struct format_spec *spec)
{
	size_t len = spec->wptr - spec->conv_buf;
	size_t sav = likely(spec->len < spec->maxlen) ? spec->maxlen - spec->len : 0;
	if(spec->width && spec->width > len)
	{
		size_t i;

		i = spec->width - len;
		i = i < spec->maxlen ? i : spec->maxlen;
		if(spec->u.flags.zero && !spec->u.flags.left)
		{
			if(spec->precision)
				goto OUT_CPY;
			while(i--)
				*buf++ = '0';
		}
		else
		{
			char *ins;
			if(!spec->u.flags.left) {
				ins = buf;
				buf += i;
			} else
				ins = buf + i;
			while(i--)
				*ins++ = ' ';
		}
		len = spec->width;
	}
OUT_CPY:
	buf = strncpyrev(buf, --spec->wptr, spec->conv_buf, sav);
	spec->len += len;
	return end_format(buf, fmt, spec);
}

/*
 * SIGNED
 */
static noinline const char *signed_finish(char *buf, const char *fmt, struct format_spec *spec)
{
	char *wptr = put_dec(spec->conv_buf, spec->t_store.u);
	if(spec->u.flags.negative)
		*wptr++ = '-';
	else if(spec->u.flags.sign && !spec->u.flags.zero)
		*wptr++ = '+';
	else if(spec->u.flags.space)
		*wptr++ = ' ';
	spec->wptr = wptr;
	return decimal_finish(buf, fmt, spec);
}

static noinline const char *signed_finish_ll(char *buf, const char *fmt, struct format_spec *spec)
{
	char *wptr = put_dec_ll(spec->conv_buf, spec->t_store.ull);
	if(spec->u.flags.negative)
		*wptr++ = '-';
	else if(spec->u.flags.sign && !spec->u.flags.zero)
		*wptr++ = '+';
	else if(spec->u.flags.space)
		*wptr++ = ' ';
	spec->wptr = wptr;
	return decimal_finish(buf, fmt, spec);
}

#define MAKE_SFUNC(prfx, type) \
static const char *v##prfx##toa(char *buf, const char *fmt, struct format_spec *spec) \
{ \
	type n = va_arg(spec->ap, type); \
	spec->u.flags.negative = n < 0 ? true : false; \
	n = n < 0 ? -n : n; \
	if(sizeof(unsigned) == sizeof(n)) { \
		spec->t_store.u = n; \
		return signed_finish(buf, fmt, spec); \
	} else if(sizeof(unsigned long long) == sizeof(n)) { \
		spec->t_store.ull = n; \
		return signed_finish_ll(buf, fmt, spec); \
	} else { \
		char *wptr = spec->conv_buf; \
		do { *wptr++ = (n % 10) + '0'; n /= 10; } while(n); \
		if(spec->u.flags.negative) \
			*wptr++ = '-'; \
		else if(spec->u.flags.sign && !spec->u.flags.zero) \
			*wptr++ = '+'; \
		else if(spec->u.flags.space) \
			*wptr++ = ' '; \
		spec->wptr = wptr; \
		return decimal_finish(buf, fmt, spec); \
	} \
}

MAKE_SFUNC( i, int)
MAKE_SFUNC( l, long)
MAKE_SFUNC(ll, long long)
MAKE_SFUNC( j, intmax_t)
MAKE_SFUNC( z, ssize_t)
MAKE_SFUNC( t, ptrdiff_t)

/*
 * UNSIGNED
 */
static noinline const char *unsigned_finish(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->wptr = put_dec(spec->conv_buf, spec->t_store.u);
	return decimal_finish(buf, fmt, spec);
}

static noinline const char *unsigned_finish_ull(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->wptr = put_dec_ll(spec->conv_buf, spec->t_store.ull);
	return decimal_finish(buf, fmt, spec);
}

#define MAKE_UFUNC(prfx, type) \
static const char *v##prfx##toa(char *buf, const char *fmt, struct format_spec *spec) \
{ \
	type n = va_arg(spec->ap, type); \
	if(sizeof(unsigned) == sizeof(n)) { \
		spec->t_store.u = n; \
		return unsigned_finish(buf, fmt, spec); \
	} else if(sizeof(unsigned long long) == sizeof(n)) { \
		spec->t_store.ull = n; \
		return unsigned_finish_ull(buf, fmt, spec); \
	} else { \
		char *wptr = spec->conv_buf; \
		do { *wptr++ = (n % 10) + '0'; n /= 10; } while(n); \
		spec->wptr = wptr; \
		return decimal_finish(buf, fmt, spec); \
	} \
}

MAKE_UFUNC(  u, unsigned)
MAKE_UFUNC( ul, unsigned long)
MAKE_UFUNC(ull, unsigned long long)
MAKE_UFUNC( uj, uintmax_t)
MAKE_UFUNC( uz, size_t)
MAKE_UFUNC( ut, ptrdiff_t)

/*
 * HEX
 */
static inline char *hex_insert_alternate(char *buf, size_t count)
{
	if(count--) {
		*buf++ = '0';
		if(count)
			*buf++ = 'x';
	}
	return buf;
}

static noinline const char *hex_finish_real(char *buf, const char *fmt, struct format_spec *spec)
{
	size_t len = spec->wptr - spec->conv_buf;
	size_t sav = likely(spec->len < spec->maxlen) ? spec->maxlen - spec->len : 0;
	len += !spec->u.flags.alternate ? 0 : 2;
	if(spec->width && spec->width > len)
	{
		size_t i, ch_count = !spec->u.flags.alternate ? 0 :
			sav > 2 ? 2 : sav;

		i = spec->width - len;
		i = i < spec->maxlen - ch_count ? i : spec->maxlen - ch_count;
		if(spec->u.flags.zero && !spec->u.flags.left)
		{
			if(spec->u.flags.alternate)
				buf = hex_insert_alternate(buf, sav);
			if(spec->precision)
				goto OUT_CPY;
			while(i--)
				*buf++ = '0';
		}
		else
		{
			char *ins;
			if(!spec->u.flags.left) {
				ins = buf;
				buf += i;
			} else
				ins = buf + i;
			while(i--)
				*ins++ = ' ';
			if(spec->u.flags.alternate)
				buf = hex_insert_alternate(buf, sav);
		}
		len = spec->width;
	} else if(spec->u.flags.alternate)
		buf = hex_insert_alternate(buf, sav);
OUT_CPY:
	buf = strncpyrev(buf, --spec->wptr, spec->conv_buf, sav);
	spec->len += len;
	return end_format(buf, fmt, spec);
}

static char *put_hex(char *buf, unsigned num, const char *hexchar)
{
	do {
		*buf++ = hexchar[num % 16];
		num /= 16;
	} while(num);
	return buf;
}

static char *put_hex_ll(char *buf, unsigned long long num, const char *hexchar)
{
	if(sizeof(size_t) == sizeof(num))
	{
		do {
			*buf++ = hexchar[num % 16];
			num /= 16;
		} while(num);
	}
	else
	{
		do {
			unsigned t = num & 0xFFFFFFFF;
			num >>= 32;
			buf = put_hex(buf, t, hexchar);
		} while(num);
	}
	return buf;
}

static noinline const char *hex_finish(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->wptr = put_hex(spec->conv_buf, spec->t_store.u, 'x' == *(fmt-1) ? HEXLC_STRING : HEXUC_STRING);
	return hex_finish_real(buf, fmt, spec);
}

static noinline const char *hex_finish_ull(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->wptr = put_hex_ll(spec->conv_buf, spec->t_store.ull, 'x' == *(fmt-1) ? HEXLC_STRING : HEXUC_STRING);
	return hex_finish_real(buf, fmt, spec);
}

#define MAKE_XFUNC(prfx, type) \
static const char *v##prfx##toxa(char *buf, const char *fmt, struct format_spec *spec) \
{ \
	type n = va_arg(spec->ap, type); \
	if(sizeof(unsigned) == sizeof(n)) { \
		spec->t_store.u = n; \
		return hex_finish(buf, fmt, spec); \
	} else if(sizeof(unsigned long long) == sizeof(n)) { \
		spec->t_store.ull = n; \
		return hex_finish_ull(buf, fmt, spec); \
	} else { \
		const char *hexchar = 'x' == *(fmt-1) ? HEXLC_STRING : HEXUC_STRING; \
		char *wptr = spec->conv_buf; \
		do { *wptr++ = hexchar[n % 16]; n /= 16; } while(n); \
		spec->wptr = wptr; \
		return hex_finish_real(buf, fmt, spec); \
	} \
}

MAKE_XFUNC(  u, unsigned)
MAKE_XFUNC( ul, unsigned long)
MAKE_XFUNC(ull, unsigned long long)
MAKE_XFUNC( uj, uintmax_t)
MAKE_XFUNC( uz, size_t)
MAKE_XFUNC( ut, ptrdiff_t)

/*
 * OCTAL
 */
static char *put_octal(char *buf, unsigned num)
{
	do {
		*buf++ = (num % 8) + '0';
		num /= 8;
	} while(num);
	return buf;
}

static char *put_octal_ll(char *buf, unsigned long long num)
{
	if(sizeof(size_t) == sizeof(num))
	{
		do {
			*buf++ = (num % 8) + '0';
			num /= 8;
		} while(num);
	}
	else
	{
		do {
			unsigned t = num & 0xFFFFFFFF;
			num >>= 32;
			buf = put_octal(buf, t);
		} while(num);
	}
	return buf;
}

static noinline const char *octal_finish(char *buf, const char *fmt, struct format_spec *spec)
{
	char *wptr = put_octal(spec->conv_buf, spec->t_store.u);
	if(spec->u.flags.alternate && '0' != *(wptr-1))
		*wptr++ = '0';
	spec->wptr = wptr;
	return decimal_finish(buf, fmt, spec);
}

static noinline const char *octal_finish_ull(char *buf, const char *fmt, struct format_spec *spec)
{
	char *wptr = put_octal_ll(spec->conv_buf, spec->t_store.ull);
	if(spec->u.flags.alternate && '0' != *(wptr-1))
		*wptr++ = '0';
	spec->wptr = wptr;
	return decimal_finish(buf, fmt, spec);
}

#define MAKE_OFUNC(prfx, type) \
static const char *v##prfx##tooa(char *buf, const char *fmt, struct format_spec *spec) \
{ \
	type n = va_arg(spec->ap, type); \
	if(sizeof(unsigned) == sizeof(n)) { \
		spec->t_store.u = n; \
		return octal_finish(buf, fmt, spec); \
	} else if(sizeof(unsigned long long) == sizeof(n)) { \
		spec->t_store.ull = n; \
		return octal_finish_ull(buf, fmt, spec); \
	} else { \
		char *wptr = spec->conv_buf; \
		do { *wptr++ = (n % 8) + '0'; n /= 8; } while(n); \
		if(spec->u.flags.alternate && '0' != *(wptr-1)) \
			*wptr++ = '0'; \
		spec->wptr = wptr; \
		return decimal_finish(buf, fmt, spec); \
	} \
} \

MAKE_OFUNC(  u, unsigned)
MAKE_OFUNC( ul, unsigned long)
MAKE_OFUNC(ull, unsigned long long)
MAKE_OFUNC( uj, uintmax_t)
MAKE_OFUNC( uz, size_t)
MAKE_OFUNC( ut, ptrdiff_t)

/*
 * NOP
 */
/*
 * we don't understand the format, but at least we
 * must remove it from the arglist, or we will crash
 * if we search a pointer later on.
 * Assume an int, because std-promotion makes an int
 * out of everything, if its a (long) long, only
 * the modifier can help or ups...
 */
static noinline const char *nop_finish(char *buf, const char *fmt, struct format_spec *spec)
{
	size_t fmt_len = fmt - spec->fmt_start;
	/* whatever the user wants, don't know, print literatly */
	my_memcpy(buf, spec->fmt_start, spec->len + fmt_len < spec->maxlen ? fmt_len : spec->maxlen - spec->len);
	spec->len += fmt_len;
	return end_format(buf+ fmt_len, fmt, spec);
}

#define MAKE_NFUNC(prfx, type) \
static const char *v##prfx##toa(char *buf, const char *fmt, struct format_spec *spec) \
{ \
	type n GCC_ATTRIB_UNUSED = va_arg(spec->ap, type); \
	return nop_finish(buf, fmt, spec); \
}

MAKE_NFUNC(  n, unsigned)
MAKE_NFUNC( nl, unsigned long)
MAKE_NFUNC(nll, unsigned long long)
MAKE_NFUNC( nj, uintmax_t)
MAKE_NFUNC( nz, size_t)
MAKE_NFUNC( nt, ptrdiff_t)
MAKE_NFUNC( nH, unsigned)
MAKE_NFUNC( nD, unsigned long long)

static const char *vnDDtoa(char *buf, const char *fmt, struct format_spec *spec)
{
#ifdef HAVE_TIMODE
	/* this will let the compiler barf... */
	__uint128 n GCC_ATTRIB_UNUSED = va_arg(spec->ap, __uint128);
#else
	/*
	 * _Decimal128 is 16 byte, lets remove two times 8. That's plain _WRONG_,
	 * but better than nothing, who knows what will be the calling convention
	 * for that...
	 */
	unsigned long long n GCC_ATTRIB_UNUSED = va_arg(spec->ap, unsigned long long);
	n = va_arg(spec->ap, unsigned long long);
#endif
	return nop_finish(buf, fmt, spec);
}

/*
 * FLOTING POINT - ROUGHLY (UN)IMPLEMENTED
 *
 * no long double
 * no hex
 * does not obay any formating
 * no fancy anything
 * no exponential display
 * no nothing
 *
 * but: brings a basic floating point number to the screen
 * and mainly: to pop the right arg size
 */

/* exponent stored + 1024, hidden bit to left of decimal point */
#define BIAS 1023
#define MAN_BITS2RIGHT 52
#define HIDDEN_BIT 0x0010000000000000ULL

#define BIGSIZE 24
#define MIN_E -1074
#define MAX_FIVE 325
#define B_P1 (1ULL << MAN_BITS2RIGHT)

typedef uint64_t big_digit;

struct big_num
{
	big_digit d[BIGSIZE];
	int l;
};

#define five_tab five_tab_base_data
extern const struct big_num five_tab[MAX_FIVE];

#define ADD_BIG(x, y, z, k) \
{ \
	big_digit x_add, z_add; \
	x_add = (x);\
	if((k)) \
		z_add = x_add + (y) + 1, (k) = (z_add <= x_add); \
	else \
		z_add = x_add + (y), (k) = (z_add < x_add); \
	(z) = z_add; \
}

#define SUB_BIG(x, y, z, b) \
{ \
	big_digit x_sub, y_sub; \
	x_sub = (x); y_sub = (y); \
	if((b)) \
		(z) = x_sub - y_sub - 1, b = (y_sub >= x_sub); \
	else \
		(z) = x_sub - y_sub, b = (y_sub > x_sub); \
}

#ifndef HAVE_TIMODE
# define MUL_BIG(x, y, z, k) \
{ \
	big_digit x_mul, low, high; \
	x_mul = (x); \
	low = (x_mul & 0xffffffff) * (y) + (k); \
	high = (x_mul >> 32) * (y) + (low >> 32); \
	(k) = high >> 32; \
	(z) = (low & 0xffffffff) | (high << 32); \
}
#else
# define MUL_BIG(x, y, z, k) \
{ \
	__uint128 a_res = (__uint128)(x) * (y) + (k); \
	(k) = a_res >> 64; \
	(z) = a_res; \
}
#endif

#ifndef HAVE_TIMODE
# define SLL_BIG(x, y, z, k) \
{ \
	big_digit x_sll = (x); \
	(z) = (x_sll << (y)) | (k); \
	(k) = x_sll >> (64 - (y)); \
}
#else
# define SLL_BIG(x, y, z, k) \
{ \
	__uint128 x_sll = ((__uint128)(x) << (y)) | (k); \
	(k) = x_sll >> 64; \
	(z) = x_sll; \
}
#endif

static void mul10(struct big_num *x)
{
	int i, l;
	big_digit *p, k;

	l = x->l;
	for(i = l, p = &x->d[0], k = 0; i >= 0; i--)
		MUL_BIG(*p, 10, *p++, k);
	if(k != 0)
		*p = k, x->l = l+1;
}

#if 0
static void mul16(struct big_num *x)
{
	int i, l;
	big_digit *p, k;

	l = x->l;
	for(i = l, p = &x->d[0], k = 0; i >= 0; i--)
		SLL_BIG(*p, 4, *p++, k);
	if(k != 0)
		*p = k, x->l = l+1;
}
#endif

static void big_short_mul(const struct big_num *x, big_digit y, struct big_num *z)
{
	int i, xl, zl;
	const big_digit *xp;
	big_digit *zp, k;
	uint32_t high, low;

	xl = x->l;
	xp = &x->d[0];
	zl = xl;
	zp = &z->d[0];
	high = y >> 32;
	low = y & 0xffffffff;

	for(i = xl, k = 0; i >= 0; i--, xp++, zp++)
	{
		big_digit xlow, xhigh, z0, t, c, z1;
		xlow = *xp & 0xffffffff;
		xhigh = *xp >> 32;
		z0 = (xlow * low) + k; /* Cout is (z0 < k) */
		t = xhigh * low;
		z1 = (xlow * high) + t;
		c = (z1 < t);
		t = z0 >> 32;
		z1 += t;
		c += (z1 < t);
		*zp = (z1 << 32) | (z0 & 0xffffffff);
		k = (xhigh * high) + (c << 32) + (z1 >> 32) + (z0 < k);
	}
	if(k != 0)
		*zp = k, zl++;
	z->l = zl;
}

static inline void print_big(struct big_num *x)
{
	int i;

	printf("#x");
	i = x->l;

	for(; i >= 0; i--)
		printf("%016llx", (unsigned long long)x->d[i]);
}

static int estimate(int n)
{
	if(n < 0)
		return (int)(n * 0.3010299956639812);
	else
		return 1 + (int)(n * 0.3010299956639811);
}

static void one_shift_left(int y, struct big_num *z)
{
	int n, m, i;

	n = y / 64;
	m = y % 64;

	for(i = 0; i < n; i++)
		z->d[i] = 0;
	z->d[i] = 1ULL << m;
	z->l = n;
}

static void short_shift_left(big_digit x, int y, struct big_num *z)
{
	int n, m, i, zl;
	big_digit *zp;

	n = y / 64;
	m = y % 64;
	zl = n;
	zp = &(z->d[0]);

	for(i = 0; i < n; i++)
		z->d[i] = 0;

	if(0 == m)
		z->d[i] = x;
	else
	{
		big_digit high = x >> (64 - m);
		z->d[i] = x << m;
		if(high != 0)
			z->d[i + 1] = high, zl++;
	}
	z->l = zl;
}

static void big_shift_left(const struct big_num *x, int y, struct big_num *z)
{
	int n, m, i, xl, zl;
	const big_digit *xp;
	big_digit *zp, k;

	n = y / 64;
	m = y % 64;
	xl = x->l;
	xp = &(x->d[0]);
	zl = xl + n;
	zp = &(z->d[0]);

	for(i = n; i > 0; i--)
		*zp++ = 0;

	if(0 == m)
		for(i = xl; i >= 0; i--)
			*zp++ = *xp++;
	else
	{
		for(i = xl, k = 0; i >= 0; i--)
			SLL_BIG(*xp++, m, *zp++, k);
		if(k != 0)
			*zp = k, zl++;
	}
	z->l = zl;
}

static int big_comp(struct big_num *x, struct big_num *y)
{
	int i;

	if(x->l > y->l)
		return 1;
	if(x->l < y->l)
		return -1;

	for(i = x->l; i >= 0; i--)
	{
		big_digit a = x->d[i], b = y->d[i];

		if(a > b)
			return 1;
		else if(a < b)
			return -1;
	}
	return 0;
}

static int sub_big(struct big_num *x, struct big_num *y, struct big_num *z)
{
	int xl, yl, zl, b, i;
	big_digit *xp, *yp, *zp;

	xl = x->l;
	yl = y->l;

	if(yl > xl)
		return 1;

	xp = &x->d[0];
	yp = &y->d[0];
	zp = &z->d[0];

	for(i = yl, b = 0; i >= 0; i--)
		SUB_BIG(*xp++, *yp++, *zp++, b);

	for(i = xl-yl; b && i > 0; i--)
	{
		big_digit x_sub;
		x_sub = *xp++;
		*zp++ = x_sub - 1;
		b = (x_sub == 0);
	}

	for(; i > 0; i--)
		*zp++ = *xp++;

	if(b)
		return 1;

	zl = xl;
	while(zl && *--zp == 0)
		zl--;
	z->l = zl;
	return 0;
}

static void add_big(struct big_num *x, struct big_num *y, struct big_num *z)
{
	int xl, yl, k, i;
	big_digit *xp, *yp, *zp;

	xl = x->l;
	yl = y->l;
	if(yl > xl)
	{
		int tl;
		struct big_num *tn;
		tl = xl; xl = yl; yl = tl;
		tn = x; x = y; y = tn;
	}

	xp = &x->d[0];
	yp = &y->d[0];
	zp = &z->d[0];

	for(i = yl, k = 0; i >= 0; i--)
		ADD_BIG(*xp++, *yp++, *zp++, k);

	for(i = xl-yl; k && i > 0; i--)
	{
		big_digit z_add;
		z_add = *xp++ + 1;
		k = (z_add == 0);
		*zp++ = z_add;
	}

	for(; i > 0; i--)
		*zp++ = *xp++;

	if(k)
		*zp = 1, z->l = xl+1;
	else
		z->l = xl;
}

static int add_cmp(struct big_num *R, struct big_num *S, struct big_num *MP, struct big_num *MM, bool use_mp)
{
	int rl, ml, sl, suml;
	struct big_num sum;

	rl = R->l;
	ml = (use_mp ? MP->l : MM->l);
	sl = S->l;

	suml = rl >= ml ? rl : ml;
	if((sl > suml+1) || ((sl == suml+1) && (S->d[sl] > 1)))
		return -1;
	if(sl < suml)
		return 1;

	add_big(R, (use_mp ? MP : MM), &sum);
	return big_comp(&sum, S);
}

static int qr(struct big_num *R, struct big_num Sbox[9])
{
	if(big_comp(R, &Sbox[4]) < 0)
	{
		if(big_comp(R, &Sbox[1]) < 0)
		{
			if(big_comp(R, &Sbox[0]) < 0)
				return 0;
			else {
				sub_big(R, &Sbox[0], R);
				return 1;
			}
		}
		if(big_comp(R, &Sbox[2]) < 0) {
			sub_big(R, &Sbox[1], R);
			return 2;
		}
		if(big_comp(R, &Sbox[3]) < 0) {
			sub_big(R, &Sbox[2], R);
			return 3;
		}
		sub_big(R, &Sbox[3], R);
		return 4;
	}
	if(big_comp(R, &Sbox[6]) < 0)
	{
		if(big_comp(R, &Sbox[5]) < 0) {
			sub_big(R, &Sbox[4], R);
			return 5;
		}
		sub_big(R, &Sbox[5], R);
		return 6;
	}
	if(big_comp(R, &Sbox[8]) < 0)
	{
		if(big_comp(R, &Sbox[7]) < 0) {
			sub_big(R, &Sbox[6], R);
			return 7;
		}
		sub_big(R, &Sbox[7], R);
		return 8;
	}
	sub_big(R, &Sbox[8], R);
	return 9;
}

union dblou64
{
	uint64_t u;
	double d;
};

static const char *vdtoa(char *buf, const char *fmt, struct format_spec *spec)
{
	/* this eats stack big time! ~4k */
	struct big_num Sbox[9], R, MP, MM;
	big_digit f;
	union dblou64 v;
	size_t sav = likely(spec->len < spec->maxlen) ? spec->maxlen - spec->len : 0;
	int sign, e, f_n, m_n, i, d, tc1, tc2;
	int k, qr_shift, ruf, s_n, sl = 0, slr = 0, dig_i;
	bool use_mp;

	emit_emms();
	v.d = va_arg(spec->ap, double);

	/* decompose float into sign, mantissa & exponent */
	sign = !!(v.u & 0x8000000000000000ULL);
	e    =   (v.u & 0x7FF0000000000000ULL) >> MAN_BITS2RIGHT;
	f    =    v.u & 0x000FFFFFFFFFFFFFULL;
/*	printf("%i %i %llu\n", sign, e, f); */

	if(e != 0) {
		e  = e - BIAS - MAN_BITS2RIGHT;
		f |= HIDDEN_BIT;
	} else if(f != 0) /* denormalized */
		e = 1 - BIAS - MAN_BITS2RIGHT;
/*	printf("%i %i %llu\n", sign, e, f); */

#define ADD_CHAR_TO_BUF(ch) \
do \
{ \
	if(sav) { sav--; *buf = (ch); } \
	buf++; \
	spec->len++; \
} while(0)

	if(sign)
		ADD_CHAR_TO_BUF('-');

	if(unlikely(0 == f)) {
		ADD_CHAR_TO_BUF('0');
		return end_format(buf, fmt, spec);
	}

	ruf = !(f & 1); /* ruf = (even? f) */

	/* Compute the scaling factor estimate, k */
	if(e > MIN_E)
		k = estimate(e + MAN_BITS2RIGHT);
	else
	{
		int n;
		big_digit y;

		for(n = e + MAN_BITS2RIGHT, y = B_P1; f < y; n--)
			y >>= 1;
		k = estimate(n);
	}

	if(e >= 0)
	{
		if(f != B_P1)
			use_mp = false, f_n = e + 1, s_n = 1, m_n = e;
		else
			use_mp = true, f_n = e + 2, s_n = 2, m_n = e;
	}
	else
	{
		if((e == MIN_E) || (f != B_P1))
			use_mp = false, f_n = 1, s_n = 1 - e, m_n = 0;
		else
			use_mp = true, f_n = 2, s_n = 2 - e, m_n = 0;
	}

	/* Scale it! */
	if(k == 0)
	{
		short_shift_left(f, f_n, &R);
		one_shift_left(s_n, &Sbox[0]);
		one_shift_left(m_n, &MM);
		if(use_mp)
			one_shift_left(m_n + 1, &MP);
		qr_shift = 1;
	}
	else if(k > 0)
	{
		s_n += k;
		if(m_n >= s_n)
			f_n -= s_n, m_n -= s_n, s_n = 0;
		else
			f_n -= m_n, s_n -= m_n, m_n = 0;
		short_shift_left(f, f_n, &R);
		big_shift_left(&five_tab[k - 1], s_n, &Sbox[0]);
		one_shift_left(m_n, &MM);
		if(use_mp)
			one_shift_left(m_n + 1, &MP);
		qr_shift = 0;
	}
	else
	{
		const struct big_num *power = &five_tab[-k - 1];

		s_n += k;
		big_short_mul(power, f, &Sbox[0]);
		big_shift_left(&Sbox[0], f_n, &R);
		one_shift_left(s_n, &Sbox[0]);
		big_shift_left(power, m_n, &MM);
		if(use_mp)
			big_shift_left(power, m_n + 1, &MP);
		qr_shift = 1;
	}

	/* fixup */
	tc1 = add_cmp(&R, &Sbox[0], &MP, &MM, use_mp);
	if(tc1 <= -ruf)
	{
		if(tc1 < -ruf)
			k--;
		mul10(&R);
		mul10(&MM);
		if(use_mp)
			mul10(&MP);
	} else if(tc1 == ruf)
		k++;

/*	printf("k = %d\n", k);
	printf("R = "); print_big(&R);
	printf("\nS = "); print_big(&Sbox[0]);
	printf("\nM- = "); print_big(&MM);
	if(use_mp)
		printf("\nM+ = "), print_big(&MP);
	putchar('\n');
	fflush(0); */

	if(qr_shift) {
		sl = s_n / 64;
		slr = s_n % 64;
	}
	else
	{
		big_shift_left(&Sbox[0], 1, &Sbox[1]);
		add_big(&Sbox[1], &Sbox[0], &Sbox[2]);
		big_shift_left(&Sbox[1], 1, &Sbox[3]);
		add_big(&Sbox[3], &Sbox[0], &Sbox[4]);
		add_big(&Sbox[3], &Sbox[1], &Sbox[5]);
		add_big(&Sbox[3], &Sbox[2], &Sbox[6]);
		big_shift_left(&Sbox[3], 1, &Sbox[7]);
		add_big(&Sbox[7], &Sbox[0], &Sbox[8]);
	}

	dig_i = k + 1;
	if(dig_i < 0) {
		int dot_i = 1;
		for(; dot_i >= dig_i; dot_i--)
			ADD_CHAR_TO_BUF(dot_i ? '0' : '.');
	} else if(!dig_i)
		ADD_CHAR_TO_BUF('0');

again:
	if(!--dig_i)
		ADD_CHAR_TO_BUF('.');
	if(qr_shift)
	{ /* Take advantage of the fact that Sbox[0] = (ash 1 s_n) */
		if(R.l < sl)
			d = 0;
		else if(R.l == sl)
		{
			big_digit *p;

			p = &R.d[sl];
			d = *p >> slr;
			*p &= (1ULL << slr) - 1;
			for(i = sl; (i > 0) && (*p == 0); i--)
				p--;
			R.l = i;
		}
		else
		{
			big_digit *p;

			p = &R.d[sl + 1];
			d = *p << (64 - slr) | *(p - 1) >> slr;
			p--;
			*p &= (1ULL << slr) - 1;
			for(i = sl; (i > 0) && (*p == 0); i--)
				p--;
			R.l = i;
		}
	}
	else /* We need to do quotient-remainder */
		d = qr(&R, Sbox);

#define OUTDIG(d) { ADD_CHAR_TO_BUF((d) + '0'); goto out_of_fp; }
//	printf("\ndig_i: %i\td: %i\nR = ", dig_i, d); print_big(&R);
//	printf("\nM- = "); print_big(&MM);
	tc1 = big_comp(&R, &MM);
//	printf("\ncmp: %i\truf: %i\n", tc1, ruf);
	tc1 = tc1 < ruf;
	tc2 = add_cmp(&R, &Sbox[0], &MP, &MM, use_mp) > -ruf;
//	printf("tc1: %i\ttc2: %i\n", tc1, tc2);
	if(!tc1)
	{
		if(!tc2)
		{
			mul10(&R);
			mul10(&MM);
			if(use_mp)
				mul10(&MP);
			ADD_CHAR_TO_BUF(d + '0');
			goto again;
		}
		else
			OUTDIG(d+1)
	}
	else
	{
		if(!tc2)
			OUTDIG(d)
		else
		{
			big_shift_left(&R, 1, &MM);
			if(-1 == big_comp(&MM, &Sbox[0]))
				OUTDIG(d)
			else
				OUTDIG(d+1)
		}
	}
out_of_fp:
	while(dig_i-- > 1)
		ADD_CHAR_TO_BUF('0');
//	putchar('\n');
	return end_format(buf, fmt, spec);
}
#undef OUTDIG
#undef ADD_CHAR_TO_BUF

static noinline const char *fp_finish(char *buf, const char *fmt, struct format_spec *spec)
{
	size_t fmt_len = fmt - spec->fmt_start;
	printf("[FLOAT] unimplemented format \"%.*s\": '%c'\n",
	       (int)fmt_len, spec->fmt_start, *(fmt-1));
	return nop_finish(buf, fmt, spec);
}

static const char *vldtoa(char *buf, const char *fmt, struct format_spec *spec)
{
	long double n GCC_ATTRIB_UNUSED = va_arg(spec->ap, long double);
	return fp_finish(buf, fmt, spec);
}
static const char *vdtoxa(char *buf, const char *fmt, struct format_spec *spec)
{
	double n GCC_ATTRIB_UNUSED = va_arg(spec->ap, double);
	return fp_finish(buf, fmt, spec);
}
static const char *vldtoxa(char *buf, const char *fmt, struct format_spec *spec)
{
	long double n GCC_ATTRIB_UNUSED = va_arg(spec->ap, long double);
	return fp_finish(buf, fmt, spec);
}


/*
 * DECIMAL
 */
static noinline const char *decifp_finish(char *buf, const char *fmt, struct format_spec *spec)
{
	size_t fmt_len = fmt - spec->fmt_start;
	printf("[DECIMAL] unimplemented format \"%.*s\": '%c'\n",
	       (int)fmt_len, spec->fmt_start, *(fmt-1));
	return nop_finish(buf, fmt, spec);
}

static const char *vHtoa(char *buf, const char *fmt, struct format_spec *spec)
{
	unsigned n GCC_ATTRIB_UNUSED = va_arg(spec->ap, unsigned); /* _Decimal32 */
	return decifp_finish(buf, fmt, spec);
}
static const char *vDtoa(char *buf, const char *fmt, struct format_spec *spec)
{
	unsigned long long n GCC_ATTRIB_UNUSED = va_arg(spec->ap, unsigned long long); /* _Decimal64 */
	return decifp_finish(buf, fmt, spec);
}
static const char *vDDtoa(char *buf, const char *fmt, struct format_spec *spec)
{
#ifdef HAVE_TIMODE
	/* this will let the compiler barf... */
	__uint128 n GCC_ATTRIB_UNUSED = va_arg(spec->ap, __uint128);
#else
	/*
	 * _Decimal128 is 16 byte, lets remove two times 8. That's plain _WRONG_,
	 * but better than nothing, who knows what will be the calling convention
	 * for that...
	 */
	unsigned long long n GCC_ATTRIB_UNUSED = va_arg(spec->ap, unsigned long long); /* _Decimal128 */
	n = va_arg(spec->ap, unsigned long long);
#endif
	return decifp_finish(buf, fmt, spec);
}
/* Hex */
static const char *vHtoxa(char *buf, const char *fmt, struct format_spec *spec)
{
	unsigned n GCC_ATTRIB_UNUSED = va_arg(spec->ap, unsigned); /* _Decimal32 */
	return decifp_finish(buf, fmt, spec);
}
static const char *vDtoxa(char *buf, const char *fmt, struct format_spec *spec)
{
	unsigned long long n GCC_ATTRIB_UNUSED = va_arg(spec->ap, unsigned long long); /* _Decimal64 */
	return decifp_finish(buf, fmt, spec);
}
static const char *vDDtoxa(char *buf, const char *fmt, struct format_spec *spec)
{
#ifdef HAVE_TIMODE
	/* this will let the compiler barf... */
	__uint128 n GCC_ATTRIB_UNUSED = va_arg(spec->ap, __uint128);
#else
	/*
	 * _Decimal128 is 16 byte, lets remove two times 8. That's plain _WRONG_,
	 * but better than nothing, who knows what will be the calling convention
	 * for that...
	 */
	unsigned long long n GCC_ATTRIB_UNUSED = va_arg(spec->ap, unsigned long long); /* _Decimal128 */
	n = va_arg(spec->ap, unsigned long long);
#endif
	return decifp_finish(buf, fmt, spec);
}


/*
 * decider...
 */
/*
 * printing all this over one large array of function
 * pointers would be cool...
 */
static const fmt_func num_format_table[][MOD_MAX_NUM] =
{	/*                                                     l                                                             _
	 *                                                      o                                            _       _        D
	 *                                             l         n                                    p       D       D        e
	 *                                              o         g                  i                 t       e       e        c
	 *                                               n                            n                 r       c       c        i
	 *                                                g         d                  t        s        d       i       i        m
	 *                              s                            o                  m        i        i       m       m        a
	 *               n       c       h        l         l         u         q        a        z        f       a       a        l
	 *                o       h       o        o         o         b         u        x        e        f       l       l        1
	 *                 n       a       r        n         n         l         a        _        _        _       3       6        2
	 *                  e       r       t        g         g         e         d        t        t        t       2       4        8
	 *     HEXL */ { vutoxa, vutoxa, vutoxa, vultoxa, vulltoxa, vulltoxa, vulltoxa, vujtoxa, vuztoxa, vuttoxa, vnHtoa, vnDtoa, vnDDtoa}, /* 0 */
	/*     HEXU */ { vutoxa, vutoxa, vutoxa, vultoxa, vulltoxa, vulltoxa, vulltoxa, vujtoxa, vuztoxa, vuttoxa, vnHtoa, vnDtoa, vnDDtoa}, /* 1 */
	/* UNSIGNED */ {  vutoa,  vutoa,  vutoa,  vultoa,  vulltoa,  vulltoa,  vulltoa,  vujtoa,  vuztoa,  vuttoa, vnHtoa, vnDtoa, vnDDtoa}, /* 2 */
	/*   SIGNED */ {  vitoa,  vitoa,  vitoa,   vltoa,   vlltoa,   vlltoa,   vlltoa,   vjtoa,   vztoa,   vttoa, vnHtoa, vnDtoa, vnDDtoa}, /* 3 */
	/*    OCTAL */ { vutooa, vutooa, vutooa, vultooa, vulltooa, vulltooa, vulltooa, vujtooa, vuztooa, vuttooa, vnHtoa, vnDtoa, vnDDtoa}, /* 4 */
	/*       FP */ {  vdtoa,  vdtoa,  vdtoa,   vdtoa,    vdtoa,   vldtoa,    vdtoa,   vdtoa,   vdtoa,   vdtoa,  vHtoa,  vDtoa,  vDDtoa}, /* 5 */
	/*    FPHEX */ { vdtoxa, vdtoxa, vdtoxa,  vdtoxa,   vdtoxa,  vldtoxa,   vdtoxa,  vdtoxa,  vdtoxa,  vdtoxa, vHtoxa, vDtoxa, vDDtoxa}, /* 6 */
	/*      NOP */ {  vntoa,  vntoa,  vntoa,  vnltoa,  vnlltoa,  vnlltoa,  vnlltoa,  vnjtoa,  vnztoa,  vnttoa, vnHtoa, vnDtoa, vnDDtoa}  /* 7 */
};

static const char *f_x(char *buf, const char *fmt, struct format_spec *spec)
{
	return num_format_table[0][spec->mod](buf, fmt, spec);
}
static const char *f_X(char *buf, const char *fmt, struct format_spec *spec)
{
	return num_format_table[1][spec->mod](buf, fmt, spec);
}
static const char *f_u(char *buf, const char *fmt, struct format_spec *spec)
{
	return num_format_table[2][spec->mod](buf, fmt, spec);
}
static const char *f_i(char *buf, const char *fmt, struct format_spec *spec)
{
	return num_format_table[3][spec->mod](buf, fmt, spec);
}
static const char *f_o(char *buf, const char *fmt, struct format_spec *spec)
{
	return num_format_table[4][spec->mod](buf, fmt, spec);
}
static const char *f_fp(char *buf, const char *fmt, struct format_spec *spec)
{
	return num_format_table[5][spec->mod](buf, fmt, spec);
}
static const char *f_fpx(char *buf, const char *fmt, struct format_spec *spec)
{
	return num_format_table[6][spec->mod](buf, fmt, spec);
}
static const char *fmtnop(char *buf, const char *fmt, struct format_spec *spec)
{
	return num_format_table[7][spec->mod](buf, fmt, spec);
}
/*
 * other conversion - uninmplemented
 */
static const char *unimpl(char *buf, const char *fmt, struct format_spec *spec)
{
	size_t fmt_len = fmt - spec->fmt_start;
	printf("unimplemented format \"%.*s\": '%c'\n",
	       (int)fmt_len, spec->fmt_start, *(fmt-1));
	return fmtnop(buf, fmt, spec);
}

/*
 * ERRNO
 */
static const char *f_serr(char *buf, const char *fmt, struct format_spec *spec)
{
	size_t err_str_len = 0;
	size_t sav = likely(spec->len < spec->maxlen) ? spec->maxlen - spec->len : 0;
#if defined STRERROR_R_CHAR_P || defined HAVE_MTSAFE_STRERROR || !defined HAVE_STRERROR_R
# ifdef STRERROR_R_CHAR_P
	/*
	 * the f***ing GNU-Version of strerror_r wich returns
	 * a char * to the buffer....
	 * This sucks especially in conjunction with strnlen,
	 * wich needs a #define __GNU_SOURCE, but conflicts
	 * with this...
	 */
	const char *s = strerror_r(errno, buf, sav);
# else
	/*
	 * Ol Solaris seems to have a static msgtable, so
	 * strerror is threadsafe and we don't have a
	 * _r version
	 */
	const char *s = strerror(errno);
# endif
	if(s)
		err_str_len = strnlen(s, sav);
	else {
		s = "Unknown system error";
		err_str_len = strlen(s) < sav ?
		              strlen(s) : sav;
	}

	if(s != buf)
		my_memcpy(buf, s, err_str_len);
#else
	if(!strerror_r(errno, buf, sav))
		err_str_len += strnlen(buf, sav);
	else
	{
		char *bs = buf;
		if(EINVAL == errno) {
			strlitcpy(bs, "Unknown errno value!");
			err_str_len = str_size("Unknown errno value!");
		} else if(ERANGE == errno) {
			strlitcpy(bs, "errno msg to long for buffer!");
			err_str_len = str_size("errno msg to long for buffer!");
		} else {
			strlitcpy(bs, "failure while retrieving errno msg!");
			err_str_len = str_size("failure while retrieving errno msg!");
		}
	}
#endif
	buf += err_str_len;
	spec->len += err_str_len;
	return end_format(buf, fmt, spec);
}

/*
 * STRING
 */
static const char *f_s(char *buf, const char *fmt, struct format_spec *spec)
{
	const char *s;
	char *t;
	size_t len = 0, diff;
	size_t maxlen = likely(spec->len < spec->maxlen) ? spec->maxlen - spec->len : 0;

	s = va_arg(spec->ap, const char *);

	if(unlikely('S' == *(fmt-1)))
		spec->mod = MOD_LONG;

	if(unlikely(spec->precision))
		maxlen = maxlen < spec->precision ? maxlen : spec->precision;

#ifdef HAVE_WCHAR_H
	if(likely(MOD_LONG != spec->mod) || !s)
	{
#else
	if(unlikely(MOD_LONG == spec->mod))
		s = "%l???s";
#endif
	if(unlikely(!s))
		s = "<null>";

	if(unlikely(spec->u.flags.space))
	{
		size_t r_len = strnlen(s, spec->precision);
		size_t d_len = spec->precision - r_len;
		if(d_len)
		{
			if(!spec->u.flags.left) {
				memset(buf, ' ', d_len < maxlen ? d_len : maxlen);
				maxlen -= d_len;
				my_memcpy(buf + d_len, s, r_len < maxlen ? r_len : maxlen);
			} else {
				memset(buf + d_len, ' ', r_len + d_len < maxlen ? d_len : maxlen - r_len);
				my_memcpy(buf, s, r_len < maxlen ? r_len : maxlen);
			}
		} else
			my_memcpy(buf, s, r_len < maxlen ? r_len : maxlen);
		buf += spec->precision;
		len += spec->precision;
	}
	else
	{
		t = strnpcpy(buf, s, maxlen);
		diff = t - buf;
		len += diff;
		if(diff >= maxlen)
		{ /* not complete? */
			if(spec->precision) {
				if(diff < spec->precision)
					len += spec->precision - diff;
			} else
				len += strlen(s + diff);
		}
		buf += len;
	}
#ifdef HAVE_WCHAR_H
	}
	else
	{
		mbstate_t ps;
		const wchar_t *ws = (const wchar_t *)s;

		memset(&ps, 0, sizeof(ps));
		for(; len < (maxlen - MB_CUR_MAX) && *ws != L'\0'; ws++) {
			size_t ret_val = wcrtomb(buf, *ws, &ps);
			if(ret_val != (size_t)-1) {
				len += ret_val;
				buf += ret_val;
			}
		}
		if(unlikely(*ws != L'\0' && len >= (maxlen - MB_CUR_MAX)))
		{
			size_t h_len = 0;
			char tbuf[MB_CUR_MAX];
			for(; *ws != L'\0'; ws++) {
				size_t ret_val = wcrtomb(tbuf, *ws, &ps);
				if((size_t)-1 != ret_val)
						h_len += ret_val;
			}
			len += h_len;
			buf += h_len;
		}
	}
#endif

	spec->len += len;
	return end_format(buf, fmt, spec);
}

/*
 * CHAR
 */
static const char *f_c(char *buf, const char *fmt, struct format_spec *spec)
{
	size_t sav = likely(spec->len < spec->maxlen) ? spec->maxlen - spec->len : 0;
	size_t len;

	if(unlikely('C' == *(fmt-1)))
		spec->mod = MOD_LONG;

	if(likely(MOD_LONG != spec->mod))
	{
		int x = va_arg(spec->ap, int);
		if(sav)
			*buf = (char)x;
		buf++;
		len = 1;
	}
	else
	{
#if !defined(HAVE_WCHAR_H)
// TODO: popping an int may be wrong
		int x GCC_ATTRIB_UNUSED = va_arg(spec->ap, int);
		if(sav)
			*buf = '?';
		buf++;
		len = 1;
#else
# if WINT_MAX < INT_MAX
#  define VARG_WINT_TYPE int
# else
#  define VARG_WINT_TYPE wint_t
# endif
		mbstate_t ps;
		char tbuf[MB_CUR_MAX];
		wint_t wc = va_arg(spec->ap, VARG_WINT_TYPE);
		size_t ret_val;

		memset(&ps, 0, sizeof(ps));
		ret_val = wcrtomb(tbuf, wc, &ps);
		if((size_t)-1 != ret_val) {
			my_memcpy(buf, tbuf, sav > ret_val ? ret_val : sav);
			len = ret_val;
		} else
			len = 0;
		buf += len;
#endif
	}

	spec->len += len;
	return end_format(buf, fmt, spec);
}

/*
 * pointer helper
 */
static noinline const char *f_p_guid(char *buf, const char *fmt, struct format_spec *spec)
{
	uint64_t d4;
	char *wptr;
	unsigned char *g = spec->t_store.v;
	size_t sav = likely(spec->len < spec->maxlen) ? spec->maxlen - spec->len : 0;
	size_t len = 8+4+4+4+12+4;
	uint32_t d1;
	unsigned i;
	uint16_t d2, d3;

	if(spec->u.flags.alternate)
		len += 2;
	if(len > sav) {
		buf += len;
		goto OUT_MORE;
	}

	/*
	 * guids come from windows systems, their format
	 * has a plattform endian touch (first 3 fields),
	 * but that plattform is "always" intel...
	 */
// TODO: GUIDs and enianess?
	d1 = get_unaligned_le32(g); g += 4;
	d2 = get_unaligned_le16(g); g += 2;
	d3 = get_unaligned_le16(g); g += 2;
	d4 = get_unaligned_be64(g);

	if(spec->u.flags.alternate)
		*buf++ = '{';

	wptr = buf = buf + 8+4+4+4+12+4;
	for(i = 0; i < 12; i++, d4 >>= 4)
		*--wptr = HEXUC_STRING[d4 & 0x0F];
	*--wptr = '-';
	for(i = 0; i < 4; i++, d4 >>= 4)
		*--wptr = HEXUC_STRING[d4 & 0x0F];
	*--wptr = '-';
	for(i = 0; i < 4; i++, d3 >>= 4)
		*--wptr = HEXUC_STRING[d3 & 0x0F];
	*--wptr = '-';
	for(i = 0; i < 4; i++, d2 >>= 4)
		*--wptr = HEXUC_STRING[d2 & 0x0F];
	*--wptr = '-';
	for(i = 0; i < 8; i++, d1 >>= 4)
		*--wptr = HEXUC_STRING[d1 & 0x0F];

	if(spec->u.flags.alternate)
		*buf++ = '}';
#if 0
	for(i = 0; i < 16; i++) {
		*buf++ = HEXUC_STRING[(g[i] >> 4) & 0x0F];
		*buf++ = HEXUC_STRING[ g[i]       & 0x0F];
		if(spec->u.flags.alternate)
		*buf++ = ':';
	}
	buf--;
#endif

OUT_MORE:
	spec->len += len;
	return end_format(buf, fmt, spec);
}

static noinline const char *f_p_ip(char *buf, const char *fmt, struct format_spec *spec)
{
	union combo_addr *addr = spec->t_store.v;
	char *ret_val;
	size_t sav = likely(spec->len < spec->maxlen) ? spec->maxlen - spec->len : 0;
	size_t len = 0;

	if(!addr) {
		mempcpy(buf, "<null>", str_size("<null>") < sav ? str_size("<null>") : sav);
		buf += str_size("<null>");
		len  = str_size("<null>");
		goto OUT_MORE;
	}

	if(spec->u.flags.alternate && AF_INET6 == addr->s.fam && len++ <= sav)
		*buf++ = '[';

	ret_val = combo_addr_print_c(addr, buf, sav);
	if(!ret_val) {
		if(sav <= INET6_ADDRSTRLEN)
			len += INET6_ADDRSTRLEN;
		else
			ret_val = buf;
	}
	len += ret_val - buf;
	buf = ret_val;
	if(spec->u.flags.alternate)
	{
		if(AF_INET6 == addr->s.fam && len++ <= sav)
			*buf++ = ']';
		if(len++ <= sav)
			*buf++ = ':';
		ret_val = put_dec_trunc(spec->conv_buf, ntohs(combo_addr_port(addr)));
		ret_val = strncpyrev(buf, ret_val - 1, spec->conv_buf, len < sav ? sav - len : 0);
		len += ret_val - buf;
		buf = ret_val;
	}

OUT_MORE:
	spec->len += len;
	return end_format(buf, fmt, spec);
}

static noinline const char *f_p_p(char *buf, const char *fmt, struct format_spec *spec)
{
	void *ptr = spec->t_store.v;
	size_t sav = likely(spec->len < spec->maxlen) ? spec->maxlen - spec->len : 0;
	size_t len;

	if((sizeof(void *) * 2) + 2 > sav)
		len = (sizeof(void *) * 2) + 2;
	else
		len = ptoa(buf, ptr) - buf;
	buf += len;
	spec->len += len;
	return end_format(buf, fmt, spec);
}

/*
 * POINTER
 */
static const char *f_p(char *buf, const char *fmt, struct format_spec *spec)
{
	void *ptr = va_arg(spec->ap, void *);
	char c;

	for(c = *fmt++; '#' == c || 'I' == c || 'G' == c; c = *fmt++)
	{
		if('#' == c)
			spec->u.flags.alternate = true;
		if('I' == c)
			spec->u.flags.ip = true;
		if('G' == c)
			spec->u.flags.guid = true;
	}
	fmt--;

	spec->t_store.v = ptr;
	if(spec->u.flags.guid)
		return f_p_guid(buf, fmt, spec);
	else if(spec->u.flags.ip)
		return f_p_ip(buf, fmt, spec);
	else
		return f_p_p(buf, fmt, spec);
}

/************************************************************************
 *
 * Other format stuff
 *
 ************************************************************************/

/*
 * width
 */
static const char *widths(char *buf, const char *fmt, struct format_spec *spec)
{
	int w = va_arg(spec->ap, int);
	if(w < 0) {
		spec->u.flags.left = true;
		w = -w;
	}
	spec->width = (unsigned)w;
	return format_dispatch(buf, fmt, spec);
}
static const char *widthn(char *buf, const char *fmt, struct format_spec *spec)
{
	unsigned w = 0;
	char c;
	for(c = *(fmt-1); c && isdigit((int)c); c = *fmt++) {
		w *= 10;
		w += c - '0';
	}
	fmt--;
	spec->width = w;
	return format_dispatch(buf, fmt, spec);
}

/*
 * precision
 */
static const char *prec_p(char *buf, const char *fmt, struct format_spec *spec)
{
	unsigned p = 0;
	char c = *fmt++;
	if('*' == c) {
		int t = va_arg(spec->ap, int);
		if(t > 0)
			p = (unsigned)t;
	} else {
		for(; c && isdigit((int)c); c = *fmt++) {
			p *= 10;
			p += c - '0';
		}
		fmt--;
	}
	spec->precision = p;
	return format_dispatch(buf, fmt, spec);
}

/*
 * flags
 */
static const char *flag_h(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->u.flags.alternate = true;
	return format_dispatch(buf, fmt, spec);
}
static const char *flag_0(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->u.flags.zero = true;
	return format_dispatch(buf, fmt, spec);
}
static const char *flag_m(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->u.flags.left = true;
	return format_dispatch(buf, fmt, spec);
}
static const char *flag_s(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->u.flags.space = true;
	return format_dispatch(buf, fmt, spec);
}
static const char *flag_p(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->u.flags.sign = true;
	return format_dispatch(buf, fmt, spec);
}
static const char *flag_I(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->u.flags.ip = true;
	return format_dispatch(buf, fmt, spec);
}
static const char *flag_t(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->u.flags.th_group = true;
	return format_dispatch(buf, fmt, spec);
}

/*
 * lenght mod
 */
static const char *lmod_L(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->mod = MOD_LONGDOUBLE;
	return format_dispatch(buf, fmt, spec);
}
static const char *lmod_H(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->mod = MOD_DECIMAL32;
	return format_dispatch(buf, fmt, spec);
}
static const char *lmod_D(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->mod = MOD_DECIMAL64 != spec->mod ? MOD_DECIMAL64 : MOD_DECIMAL128;
	return format_dispatch(buf, fmt, spec);
}
static const char *lmod_q(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->mod = MOD_QUAD;
	return format_dispatch(buf, fmt, spec);
}
static const char *lmod_j(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->mod = MOD_INTMAX_T;
	return format_dispatch(buf, fmt, spec);
}
static const char *lmod_z(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->mod = MOD_SIZE_T;
	return format_dispatch(buf, fmt, spec);
}
static const char *lmod_t(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->mod = MOD_PTRDIFF_T;
	return format_dispatch(buf, fmt, spec);
}
static const char *lmod_h(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->mod = MOD_SHORT != spec->mod ? MOD_SHORT : MOD_CHAR;
	return format_dispatch(buf, fmt, spec);
}
static const char *lmod_l(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->mod = MOD_LONG != spec->mod ? MOD_LONG : MOD_LONGLONG;
	return format_dispatch(buf, fmt, spec);
}

/*
 * specials
 */
static const char *lit_p(char *buf, const char *fmt, struct format_spec *spec)
{
	size_t sav = likely(spec->len < spec->maxlen) ? spec->maxlen - spec->len : 0;
	if(likely(sav))
		*buf++ = '%';
	spec->len++;
	return end_format(buf, fmt, spec);
}

static const char *p_len(char *buf, const char *fmt, struct format_spec *spec)
{
	void *n = va_arg(spec->ap, void *);
	switch(spec->mod)
	{
	case MOD_LONG:       *(long *)     n =      (long)spec->len; break;
	case MOD_QUAD:
	case MOD_LONGLONG:   *(long long *)n = (long long)spec->len; break;
	case MOD_INTMAX_T:   *(intmax_t *) n =  (intmax_t)spec->len; break;
	case MOD_SIZE_T:     *(size_t *)   n =    (size_t)spec->len; break;
	case MOD_SHORT:      *(short *)    n =     (short)spec->len; break;
	case MOD_CHAR:       *(char *)     n =      (char)spec->len; break;
	case MOD_NONE:
	case MOD_LONGDOUBLE:
	case MOD_PTRDIFF_T:
	case MOD_DECIMAL32:
	case MOD_DECIMAL64:
	case MOD_DECIMAL128:
	case MOD_MAX_NUM:    *(int  *)     n =       (int)spec->len; break;
	}
	return end_format(buf, fmt, spec);
}

/*
 * Jump table
 *
 * If we would categorize the format characters with a switch statement,
 * the compiler would propably generate a jump-table. Let's make this
 * explicitly.
 * This way we can tranfer control between the subfuncs. (decode-as-we-go)
 * And GCC, with -foptimize-sibling-calls, generates nice tail calls.
 */
static const fmt_func format_table[256] =
{
	/*           00,     01,     02,     03,     04,     05,     06,     07,     08,     09,     0A,     0B,     0C,     0D,     0E,     0F, */
	/* 00 */ fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*          NUL,    SOH,    STX,    ETX,    EOT,    ENQ,    ACK,    BEL,     BS,     HT,     LF,     VT,     FF,     CR,     SO,     SI, */
	/* 10 */ fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*          DLE,    DC1,    DC2,    DC3,    DC4,    NAK,    SYN,    ETB,    CAN,     EM,    SUB,    ESC,     FS,     GS,     RS,     US, */
	/* 20 */ flag_s, fmtnop, fmtnop, flag_h, unimpl,  lit_p, fmtnop, flag_t, fmtnop, fmtnop, widths, flag_p, fmtnop, flag_m, prec_p, fmtnop,
	/*        SPACE,      !,      ",      #,      $,      %,      &,      ',      (,      ),      *,      +,      ,,      -,      .,      /, */
	/* 30 */ flag_0, widthn, widthn, widthn, widthn, widthn, widthn, widthn, widthn, widthn, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*            0,      1,      2,      3,      4,      5,      6,      7,      8,      9,      :,      ;,      <,      =,      >,      ?, */
	/* 40 */ fmtnop,  f_fpx, fmtnop,    f_c, lmod_D,   f_fp,   f_fp,   f_fp, lmod_H, flag_I, fmtnop, fmtnop, lmod_L, fmtnop, fmtnop, fmtnop,
	/*            @,      A,      B,      C,      D,      E,      F,      G,      H,      I,      J,      K,      L,      M,      N,      O, */
	/* 50 */ fmtnop, fmtnop, fmtnop,    f_s, fmtnop, fmtnop, fmtnop, fmtnop,    f_X, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*            P,      Q,      R,      S,      T,      U,      V,      W,      X,      Y,      Z,      [,      \,      ],      ^,      _, */
	/* 60 */ fmtnop,  f_fpx, fmtnop,    f_c,    f_i,   f_fp,   f_fp,   f_fp, lmod_h,    f_i, lmod_j, fmtnop, lmod_l, f_serr,  p_len,    f_o,
	/*            `,      a,      b,      c,      d,      e,      f,      g,      h,      i,      j,      k,      l,      m,      n,      o, */
	/* 70 */    f_p, lmod_q, fmtnop,    f_s, lmod_t,    f_u, fmtnop, fmtnop,    f_x, fmtnop, lmod_z, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*            p,      q,      r,      s,      t,      u,      v,      w,      x,      y,      z,      {,      |,      },      ~,    DEL, */
	/* 80 */ fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*             ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       , */
	/* 90 */ fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*             ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       , */
	/* A0 */ fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*             ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       , */
	/* B0 */ fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*             ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       , */
	/* C0 */ fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*             ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       , */
	/* D0 */ fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*             ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       , */
	/* E0 */ fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*             ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       , */
	/* F0 */ fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*             ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       ,       , */
};

/*
 * Entry funcs
 */
int my_vsnprintf(char *buf, size_t maxlen, const char *fmt, va_list ap)
{
	struct format_spec cur_fmt;
	size_t diff;
	const char *m;

	va_copy(cur_fmt.ap, ap);
	cur_fmt.len = 0;
	cur_fmt.maxlen = maxlen;
	do
	{
		m = strchrnul(fmt, '%');
		diff = m - fmt;
		if(likely(diff)) {
			if(likely(cur_fmt.len + diff < cur_fmt.maxlen))
				buf = my_mempcpy(buf, fmt, diff);
			else
				buf = my_mempcpy(buf, fmt, cur_fmt.len >= cur_fmt.maxlen ? 0 : cur_fmt.maxlen - cur_fmt.len);
		}
		cur_fmt.len += diff;
		fmt = m;

		if('%' == *fmt)
		{
			cur_fmt.fmt_start = fmt;
			cur_fmt.precision = 0;
			cur_fmt.width = 0;
			cur_fmt.u.xyz = 0;
			cur_fmt.mod = MOD_NONE;
			fmt = format_dispatch(buf, fmt+1, &cur_fmt);
			buf = cur_fmt.wptr;
		} else
			break;
	} while(1);
	va_end(cur_fmt.ap);

	if(likely(cur_fmt.len < maxlen))
		*buf = '\0';

	return (int)cur_fmt.len;
}

int my_snprintf(char *buf, size_t maxlen, const char *fmt, ...)
{
	va_list args;
	int ret_val;

	va_start(args, fmt);
	ret_val = my_vsnprintf(buf, maxlen, fmt, args);
	va_end(args);
	return ret_val;
}

static char const rcsid_vsnpf[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
