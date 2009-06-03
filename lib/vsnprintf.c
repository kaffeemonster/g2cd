/*
 * vsnprintf.c
 * {v}snprintf with extensions
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
 * $Id:$
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef WANT_WCHAR
# include <wchar.h>
#endif
#include <ctype.h>
#include <errno.h>
#include "other.h"
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
 * normaly a libc goes the way of stdio-boxing.
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
 * - Can not print floating point
 * - the maxlen is not always obeyed...
 * - Very fancy modifier/conversion options are ?broken?
 * - And surely not std. conform
 */

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
		} flags;
		int xyz;
	} u;
	enum length_mod mod;
	char conv_buf[(type_log10_aprox(intmax_t) * 2) + 4];
};

typedef const char *(*fmt_func)(char *buf, const char *fmt, struct format_spec *);

#define HEXUC_STRING "0123456789ABCDEFGHIJKL"
#define HEXLC_STRING "0123456789abcdefghijkl"

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
#define MAKE_SFUNC(prfx, type) \
static const char *v##prfx##toa(char *buf, const char *fmt, struct format_spec *spec) \
{ \
	type n = va_arg(spec->ap, type); \
	char *wptr; \
	wptr = spec->conv_buf; \
	do { *wptr++ = (n % 10) + '0'; n /= 10; } while(n); \
	if(n < 0) \
		*wptr++ = '-'; \
	else if(spec->u.flags.sign && !spec->u.flags.zero) \
		*wptr++ = '+'; \
	else if(spec->u.flags.space) \
		*wptr++ = ' '; \
	spec->wptr = wptr; \
	return decimal_finish(buf, fmt, spec); \
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
#define MAKE_UFUNC(prfx, type) \
static const char *v##prfx##toa(char *buf, const char *fmt, struct format_spec *spec) \
{ \
	type n = va_arg(spec->ap, type); \
	char *wptr; \
	wptr = spec->conv_buf; \
	do { *wptr++ = (n % 10) + '0'; n /= 10; } while(n); \
	spec->wptr = wptr; \
	return decimal_finish(buf, fmt, spec); \
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

static noinline const char *hex_finish(char *buf, const char *fmt, struct format_spec *spec)
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

#define MAKE_XFUNC(prfx, type) \
static const char *v##prfx##toxa(char *buf, const char *fmt, struct format_spec *spec) \
{ \
	type n = va_arg(spec->ap, type); \
	const char *hexchar = 'x' == *(fmt-1) ? HEXLC_STRING : HEXUC_STRING; \
	char *wptr; \
	wptr = spec->conv_buf; \
	do { *wptr++ = hexchar[n % 16]; n /= 16; } while(n); \
	spec->wptr = wptr; \
	return hex_finish(buf, fmt, spec); \
} \

MAKE_XFUNC(  u, unsigned)
MAKE_XFUNC( ul, unsigned long)
MAKE_XFUNC(ull, unsigned long long)
MAKE_XFUNC( uj, uintmax_t)
MAKE_XFUNC( uz, size_t)
MAKE_XFUNC( ut, ptrdiff_t)

/*
 * OCTAL
 */
#define MAKE_OFUNC(prfx, type) \
static const char *v##prfx##tooa(char *buf, const char *fmt, struct format_spec *spec) \
{ \
	type n = va_arg(spec->ap, type); \
	char *wptr; \
	wptr = spec->conv_buf; \
	do { *wptr++ = (n % 8) + '0'; n /= 8; } while(n); \
	if(spec->u.flags.alternate && '0' != *(wptr-1)) \
		*wptr++ = '0'; \
	spec->wptr = wptr; \
	return decimal_finish(buf, fmt, spec); \
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
	memcpy(buf, spec->fmt_start, spec->len + fmt_len < spec->maxlen ? fmt_len : spec->maxlen - spec->len);
	spec->len += fmt_len;
	return end_format(buf+ fmt_len, fmt, spec);
}

#define MAKE_NFUNC(prfx, type) \
static const char *v##prfx##toa(char *buf, const char *fmt, struct format_spec *spec) \
{ \
	type n GCC_ATTRIB_UNUSED = va_arg(spec->ap, type); \
	return nop_finish(buf, fmt, spec); \
} \

MAKE_NFUNC(  n, unsigned)
MAKE_NFUNC( nl, unsigned long)
MAKE_NFUNC(nll, unsigned long long)
MAKE_NFUNC( nj, uintmax_t)
MAKE_NFUNC( nz, size_t)
MAKE_NFUNC( nt, ptrdiff_t)

/*
 * FLOTING POINT - UNIMPLEMETNED
 * not over unimp, to pop the right arg size
 */
static noinline const char *fp_finish(char *buf, const char *fmt, struct format_spec *spec)
{
	size_t fmt_len = fmt - spec->fmt_start;
	printf("[FLOAT] unimplemented format \"%.*s\": '%c'\n",
	       (int)fmt_len, spec->fmt_start, *(fmt-1));
	return nop_finish(buf, fmt, spec);
}

static const char *vdtoa(char *buf, const char *fmt, struct format_spec *spec)
{
	double GCC_ATTRIB_UNUSED n = va_arg(spec->ap, double);
	return fp_finish(buf, fmt, spec);
}
static const char *vldtoa(char *buf, const char *fmt, struct format_spec *spec)
{
	long double GCC_ATTRIB_UNUSED n = va_arg(spec->ap, long double);
	return fp_finish(buf, fmt, spec);
}

static const char *vdtoxa(char *buf, const char *fmt, struct format_spec *spec)
{
	double GCC_ATTRIB_UNUSED n = va_arg(spec->ap, double);
	return fp_finish(buf, fmt, spec);
}
static const char *vldtoxa(char *buf, const char *fmt, struct format_spec *spec)
{
	long double GCC_ATTRIB_UNUSED n = va_arg(spec->ap, long double);
	return fp_finish(buf, fmt, spec);
}

/*
 * decider...
 */
/*
 * printing all this over one large array of function
 * pointers would be cool...
 */
static const fmt_func num_format_table[][MOD_MAX_NUM] =
{	/*                                                     l
	 *                                                      o
	 *                                             l         n                                    p
	 *                                              o         g                  i                 t
	 *                                               n                            n                 r
	 *                                                g         d                  t        s        d
	 *                                                           o                  m        i        i
	 *               n       c       s        l         l         u         q        a        z        f
	 *                o       h       o        o         o         b         u        x        e        f
	 *                 n       a       r        n         n         l         a        _        _        _
	 *                  e       r       t        g         g         e         d        t        t        t
	 *     HEXL */ { vutoxa, vutoxa, vutoxa, vultoxa, vulltoxa, vulltoxa, vulltoxa, vujtoxa, vuztoxa, vuttoxa}, /* 0 */
	/*     HEXU */ { vutoxa, vutoxa, vutoxa, vultoxa, vulltoxa, vulltoxa, vulltoxa, vujtoxa, vuztoxa, vuttoxa}, /* 1 */
	/* UNSIGNED */ {  vutoa,  vutoa,  vutoa,  vultoa,  vulltoa,  vulltoa,  vulltoa,  vujtoa,  vuztoa,  vuttoa}, /* 2 */
	/*   SIGNED */ {  vitoa,  vitoa,  vitoa,   vltoa,   vlltoa,   vlltoa,   vlltoa,   vjtoa,   vztoa,   vttoa}, /* 3 */
	/*    OCTAL */ { vutooa, vutooa, vutooa, vultooa, vulltooa, vulltooa, vulltooa, vujtooa, vuztooa, vuttooa}, /* 4 */
	/*       FP */ {  vdtoa,  vdtoa,  vdtoa,   vdtoa,    vdtoa,   vldtoa,    vdtoa,   vdtoa,   vdtoa,   vdtoa}, /* 5 */
	/*    FPHEX */ { vdtoxa, vdtoxa, vdtoxa,  vdtoxa,   vdtoxa,  vldtoxa,   vdtoxa,  vdtoxa,  vdtoxa,  vdtoxa}, /* 6 */
	/*      NOP */ {  vntoa,  vntoa,  vntoa,  vnltoa,  vnlltoa,  vnlltoa,  vnlltoa,  vnjtoa,  vnztoa,  vnttoa}  /* 7 */
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
#if defined HAVE_GNU_STRERROR_R || defined HAVE_MTSAFE_STRERROR
# ifdef HAVE_GNU_STRERROR_R
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
		memcpy(buf, s, err_str_len);
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
	size_t maxlen = spec->maxlen - spec->len;

	s = va_arg(spec->ap, const char *);

	if(unlikely('S' == *(fmt-1)))
		spec->mod = MOD_LONG;

	if(unlikely(spec->precision))
		maxlen = maxlen < spec->precision ? maxlen : spec->precision;

#ifdef WANT_WCHAR
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
				memcpy(buf + d_len, s, r_len < maxlen ? r_len : maxlen);
			} else {
				memset(buf + d_len, ' ', r_len + d_len < maxlen ? d_len : maxlen - r_len);
				memcpy(buf, s, r_len < maxlen ? r_len : maxlen);
			}
		} else
			memcpy(buf, s, r_len < maxlen ? r_len : maxlen);
		buf += spec->precision;
		len += spec->precision;
	}
	else
	{
		t = strnpcpy(buf, s, maxlen);
		diff = t - buf;
		len += diff;
		if(*t)
		{ /* not complete? */
			if(spec->precision) {
				if(diff < spec->precision)
					len += spec->precision - diff;
			} else
				len += strlen(s + diff);
		}
		buf += len;
	}
#ifdef WANT_WCHAR
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
#ifndef WANT_WCHAR
// TODO: popping an int may be wrong
		int x GCC_ATTRIB_UNUSED = va_arg(spec->ap, int);
		if(sav)
			*buf = '?';
		buf++;
		len = 1;
#else
		mbstate_t ps;
		char tbuf[MB_CUR_MAX];
		wint_t wc = va_arg(spec->ap, wint_t);
		size_t ret_val;

		memset(&ps, 0, sizeof(ps));
		ret_val = wcrtomb(tbuf, wc, &ps);
		if((size_t)-1 != ret_val) {
			memcpy(buf, tbuf, sav > ret_val ? ret_val : sav);
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
 * POINTER
 */
static const char *f_p(char *buf, const char *fmt, struct format_spec *spec)
{
	void *ptr = va_arg(spec->ap, void *);
	char *ret_val;
	size_t sav = likely(spec->len < spec->maxlen) ? spec->maxlen - spec->len : 0;
	size_t len = 0;
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

	if(spec->u.flags.guid)
	{
		len = 32;
		if(spec->u.flags.alternate)
			len += 15;
		if(len <= sav)
		{
			unsigned i;
			unsigned char *g = ptr;

			for(i = 0; i < 16; i++) {
				*buf++ = HEXUC_STRING[(g[i] >> 4) & 0x0F];
				*buf++ = HEXUC_STRING[ g[i]       & 0x0F];
				if(spec->u.flags.alternate)
					*buf++ = ':';
			}
			if(spec->u.flags.alternate)
				buf--;
		}
		else
			buf += len;
	}
	else if(spec->u.flags.ip)
	{
		union combo_addr *addr = ptr;
		if(!addr) {
			mempcpy(buf, "<null>", str_size("<null>") < sav ? str_size("<null>") : sav);
			buf += str_size("<null>");
			len  = str_size("<null>");
			goto OUT_MORE;
		}

		if(spec->u.flags.alternate && AF_INET6 == addr->s_fam && len++ <= sav)
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
			if(AF_INET6 == addr->s_fam && len++ <= sav)
				*buf++ = ']';
			if(len++ <= sav)
				*buf++ = ':';
			ret_val = usntoa(buf, sav - len, ntohs(combo_addr_port(addr)));
			ret_val = ret_val ? ret_val : buf;
			len += ret_val - buf;
			buf = ret_val;
		}
	}
	else
	{
		if((sizeof(void *) * 2) + 2 > sav)
			len = (sizeof(void *) * 2) + 2;
		else
			len = ptoa(buf, ptr) - buf;
		buf += len;
	}

OUT_MORE:
	spec->len += len;
	return end_format(buf, fmt, spec);
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
	for(c = *(fmt-1); c && isdigit(c); c = *fmt++) {
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
		for(; c && isdigit(c); c = *fmt++) {
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

/*
 * lenght mod
 */
static const char *lmod_L(char *buf, const char *fmt, struct format_spec *spec)
{
	spec->mod = MOD_LONGDOUBLE;
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
	int *n = va_arg(spec->ap, int *);
	*n = (int) spec->len;
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
	/* 20 */ flag_s, fmtnop, fmtnop, flag_h, unimpl,  lit_p, fmtnop, fmtnop, fmtnop, fmtnop, widths, flag_p, fmtnop, flag_m, prec_p, fmtnop,
	/*        SPACE,      !,      ",      #,      $,      %,      &,      ',      (,      ),      *,      +,      ,,      -,      .,      /, */
	/* 30 */ flag_0, widthn, widthn, widthn, widthn, widthn, widthn, widthn, widthn, widthn, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*            0,      1,      2,      3,      4,      5,      6,      7,      8,      9,      :,      ;,      <,      =,      >,      ?, */
	/* 40 */ fmtnop,  f_fpx, fmtnop,    f_c, fmtnop,   f_fp,   f_fp,   f_fp, fmtnop, flag_I, fmtnop, fmtnop, lmod_L, fmtnop, fmtnop, fmtnop,
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
		if(likely(diff))
			buf = mempcpy(buf, fmt, cur_fmt.len + diff < cur_fmt.maxlen ? diff : cur_fmt.maxlen - cur_fmt.len);
		cur_fmt.len += diff;
		fmt = m;

		if('%' == *fmt)
		{
			cur_fmt.fmt_start = fmt;
			cur_fmt.precision = 0;
			cur_fmt.width = 0;
			cur_fmt.mod = MOD_NONE;
			cur_fmt.u.xyz = 0;
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
