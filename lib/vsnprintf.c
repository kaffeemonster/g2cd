/*
 * vsnprintf.c
 * {v}snprintf with extensions
 *
 * Copyright (c) 2008 Jan Seiffert
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
 * - Does not return the correct needed length when truncating
 * - Can not print floating point
 * - the maxlen is not always obeyed...
 * - octal is also not implemented
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
	MOD_PTRDIFF_T
} GCC_ATTRIB_PACKED;

struct format_spec
{
	const char *fmt_start;
	va_list *ap;
	const char *fmt;
	char *buf;
	size_t len;
	size_t maxlen;
	unsigned precision;
	unsigned width;
	enum length_mod mod;
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
		} flags;
		int xyz;
	} u;
};

/*
 * Conversions
 */

#define A_POP(type, prfx) \
		do { \
			d.prfx = va_arg(*fmt->ap, type); \
		} while(0)


static size_t f_i(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	union all_types d;
	size_t len = 0;
	char *ret_val = fmt->buf;

#define A_PRINT(type, prfx) \
		do { \
			d.prfx = va_arg(*fmt->ap, type); \
			if(unlikely(fmt->u.flags.sign) && d.prfx >= 0) { \
				if(len++ < fmt->maxlen) \
					*fmt->buf = '+'; \
				fmt->buf++; \
			} \
			if(fmt->u.flags.zero) \
				ret_val = prfx##toa_0fix(fmt->buf, d.prfx, fmt->width); \
			else if(fmt->u.flags.space) \
				ret_val = prfx##toa_sfix(fmt->buf, d.prfx, fmt->width); \
			else { \
				ret_val = prfx##ntoa(fmt->buf, fmt->maxlen, d.prfx); \
				ret_val = ret_val ? ret_val : fmt->buf; \
			} \
		} while(0)

	switch(fmt->mod)
	{
	case MOD_NONE:
		A_PRINT(int, i);
		break;
	case MOD_LONG:
		A_PRINT(long, l);
		break;
	case MOD_QUAD:
	case MOD_LONGLONG:
		A_PRINT(long long, ll);
		break;
	case MOD_CHAR:
		A_PRINT(int, c);
		break;
	case MOD_SHORT:
		A_PRINT(int, s);
		break;
	case MOD_SIZE_T:
		A_PRINT(ssize_t, z);
		break;
	case MOD_INTMAX_T:
		A_POP(intmax_t, j);
		break;
	case MOD_PTRDIFF_T:
		A_POP(ptrdiff_t, t);
		break;
	case MOD_LONGDOUBLE:
// TODO: think about this
		A_POP(long long, ll);
		break;
	}

#undef A_PRINT

	len += ret_val - fmt->buf;
	return len;
}

static size_t f_u(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	union all_types d;
	size_t len = 0;
	char *ret_val = fmt->buf;

#define A_PRINT(type, prfx) \
		do { \
			d.prfx = va_arg(*fmt->ap, type); \
			if(unlikely(fmt->u.flags.sign)) { \
				if(len++ < fmt->maxlen) \
					*fmt->buf = '+'; \
				fmt->buf++; \
			} \
			if(fmt->u.flags.zero) \
				ret_val = prfx##toa_0fix(fmt->buf, d.prfx, fmt->width); \
			else if(fmt->u.flags.space) \
				ret_val = prfx##toa_sfix(fmt->buf, d.prfx, fmt->width); \
			else { \
				ret_val = prfx##ntoa(fmt->buf, fmt->maxlen, d.prfx); \
				ret_val = ret_val ? ret_val : fmt->buf; \
			} \
		} while(0)

	switch(fmt->mod)
	{
	case MOD_NONE:
		A_PRINT(unsigned, u);
		break;
	case MOD_LONG:
		A_PRINT(unsigned long, ul);
		break;
	case MOD_QUAD:
	case MOD_LONGLONG:
		A_PRINT(unsigned long long, ull);
		break;
	case MOD_CHAR:
		A_PRINT(unsigned, uc);
		break;
	case MOD_SHORT:
		A_PRINT(unsigned, us);
		break;
	case MOD_SIZE_T:
		A_PRINT(size_t, uz);
		break;
	case MOD_INTMAX_T:
		A_POP(uintmax_t, uj);
		break;
	case MOD_PTRDIFF_T:
		A_POP(ptrdiff_t, t);
		break;
	case MOD_LONGDOUBLE:
// TODO: think about this
		A_POP(unsigned long long, ull);
		break;
	}

#undef A_PRINT

	len += ret_val - fmt->buf;
	return len;
}

#if 0
static char *pr_nop(char *buf)
{
	return buf;
}
#endif

static size_t f_X(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	union all_types d;
	size_t len = 0;
	char *ret_val = fmt->buf;

	if(unlikely(fmt->u.flags.alternate))
	{
		if(len++ < fmt->maxlen)
			*fmt->buf = '0';
		fmt->buf++;
		if(len++ < fmt->maxlen)
			*fmt->buf = 'x';
		fmt->buf++;
	}

#if 0
	/* printing all this over one large array of function pointers
	 * would be cool, but who pops the arg in the right size?
	 * sh*t-f****ng var args...
	 */
	char *(*funcs)()[MOD_MAXIMUM][4] =
	{
		{   utoXa,   utoXa_0fix,   utoXa_sfix, pr_nop }, /*        none */
		{  uctoXa,  uctoXa_0fix,  uctoXa_sfix, pr_nop }, /*        char */
		{  ustoXa,  ustoXa_0fix,  ustoXa_sfix, pr_nop }, /*       short */
		{  ultoXa,  ultoXa_0fix,  ultoXa_sfix, pr_nop }, /*        long */
		{ ulltoXa, ulltoXa_0fix, ulltoXa_sfix, pr_nop }, /*   long long */
		{  pr_nop,       pr_nop,       pr_nop, pr_nop }, /* long double */
		{ ulltoXa, ulltoXa_0fix, ulltoXa_sfix, pr_nop }, /*        quad */
		{  pr_nop,       pr_nop,       pr_nop, pr_nop }, /*    intmax_t */
		{  uztoXa,  uztoXa_0fix,  uztoXa_sfix, pr_nop }, /*      size_t */
		{  pr_nop,       pr_nop,       pr_nop, pr_nop }, /*   ptrdiff_t */
	}
#endif

#define A_PRINT(type, prfx) \
		do { \
			d.prfx = va_arg(*fmt->ap, type); \
			if(fmt->u.flags.zero) \
				ret_val = prfx##toXa_0fix(fmt->buf, d.prfx, fmt->width); \
			else if(fmt->u.flags.space) \
				ret_val = prfx##toXa_sfix(fmt->buf, d.prfx, fmt->width); \
			else { \
				ret_val = prfx##ntoXa(fmt->buf, fmt->maxlen, d.prfx); \
				ret_val = ret_val ? ret_val : fmt->buf; \
			} \
		} while(0)

	switch(fmt->mod)
	{
	case MOD_NONE:
		A_PRINT(unsigned, u);
		break;
	case MOD_LONG:
		A_PRINT(unsigned long, ul);
		break;
	case MOD_QUAD:
	case MOD_LONGLONG:
		A_PRINT(unsigned long long, ull);
		break;
	case MOD_CHAR:
		A_PRINT(unsigned, uc);
		break;
	case MOD_SHORT:
		A_PRINT(unsigned, us);
		break;
	case MOD_SIZE_T:
		A_PRINT(size_t, uz);
		break;
	case MOD_INTMAX_T:
		A_POP(uintmax_t, uj);
		break;
	case MOD_PTRDIFF_T:
		A_POP(ptrdiff_t, t);
		break;
	case MOD_LONGDOUBLE:
// TODO: think about this
		A_POP(unsigned long long, ull);
		break;
	}

#undef A_PRINT

	len += ret_val - fmt->buf;
	return len;
}

static size_t f_x(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	union all_types d;
	size_t len = 0;
	char *ret_val = fmt->buf;

	if(unlikely(fmt->u.flags.alternate))
	{
		if(len++ < fmt->maxlen)
			*fmt->buf = '0';
		fmt->buf++;
		if(len++ < fmt->maxlen)
			*fmt->buf = 'x';
		fmt->buf++;
	}

#define A_PRINT(type, prfx) \
		do { \
			d.prfx = va_arg(*fmt->ap, type); \
			if(fmt->u.flags.zero) \
				ret_val = prfx##toxa_0fix(fmt->buf, d.prfx, fmt->width); \
			else if(fmt->u.flags.space) \
				ret_val = prfx##toxa_sfix(fmt->buf, d.prfx, fmt->width); \
			else { \
				ret_val = prfx##ntoxa(fmt->buf, fmt->maxlen, d.prfx); \
				ret_val = ret_val ? ret_val : fmt->buf; \
			} \
		} while(0)

	switch(fmt->mod)
	{
	case MOD_NONE:
		A_PRINT(unsigned, u);
		break;
	case MOD_LONG:
		A_PRINT(unsigned long, ul);
		break;
	case MOD_QUAD:
	case MOD_LONGLONG:
		A_PRINT(unsigned long long, ull);
		break;
	case MOD_CHAR:
		A_PRINT(unsigned, uc);
		break;
	case MOD_SHORT:
		A_PRINT(unsigned, us);
		break;
	case MOD_SIZE_T:
		A_PRINT(size_t, uz);
		break;
	case MOD_INTMAX_T:
		A_POP(uintmax_t, uj);
		break;
	case MOD_PTRDIFF_T:
		A_POP(ptrdiff_t, t);
		break;
	case MOD_LONGDOUBLE:
// TODO: think about this
		A_POP(unsigned long long, ull);
		break;
	}

#undef A_PRINT

	len += ret_val - fmt->buf;
	return len;
}

/*
 * floating point conversion - UNIMPLEMETNED
 * not over unimp, to pop the right arg size
 */
static size_t f_fp(struct format_spec *fmt, unsigned char c)
{
	union all_types d;
	size_t fmt_len = fmt->fmt - fmt->fmt_start;

	printf("unimplemented format \"%.*s\": '%c'\n",
	       (int)fmt_len, fmt->fmt_start, c);
	/* whatever the user wants, don't know, print literatly */
	mempcpy(fmt->buf, fmt->fmt_start, fmt_len < fmt->maxlen ? fmt_len : fmt->maxlen);

	switch(fmt->mod)
	{
	case MOD_NONE:
	case MOD_LONG:
	case MOD_QUAD:
	case MOD_LONGLONG:
	case MOD_CHAR:
	case MOD_SHORT:
	case MOD_SIZE_T:
	case MOD_INTMAX_T:
	case MOD_PTRDIFF_T:
		A_POP(double, e);
		break;
	case MOD_LONGDOUBLE:
		A_POP(long double, L);
		break;
	}

	return fmt_len;
}

#undef A_POP

static size_t f_serr(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	size_t err_str_len = 0;
#if defined HAVE_GNU_STRERROR_R || defined HAVE_MTSAFE_STRERROR
# ifdef HAVE_GNU_STRERROR_R
	/*
	 * the f***ing GNU-Version of strerror_r wich returns
	 * a char * to the buffer....
	 * This sucks especially in conjunction with strnlen,
	 * wich needs a #define __GNU_SOURCE, but conflicts
	 * with this...
	 */
	const char *s = strerror_r(errno, fmt->buf, fmt->maxlen);
# else
	/*
	 * Ol Solaris seems to have a static msgtable, so
	 * strerror is threadsafe and we don't have a
	 * _r version
	 */
	const char *s = strerror(errno);
# endif
	if(s)
		err_str_len = strnlen(s, fmt->maxlen);
	else {
		s = "Unknown system error";
		err_str_len = strlen(s) < fmt->maxlen ?
		              strlen(s) : fmt->maxlen;
	}

	if(s != fmt->buf)
		memcpy(fmt->buf, s, err_str_len);
#else
	if(!strerror_r(errno, fmt->buf, fmt->maxlen))
		err_str_len += strnlen(fmt->buf, fmt->maxlen);
	else
	{
		char *bs = fmt->buf;
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
	return err_str_len;
}


static size_t f_s(struct format_spec *fmt, unsigned char c)
{
	const char *s;
	char *t;
	size_t len = 0, diff;
	size_t maxlen = fmt->maxlen;

	s = va_arg(*fmt->ap, const char *);

	if(unlikely('S' == c))
		fmt->mod = MOD_LONG;

	if(unlikely(fmt->precision))
		maxlen = maxlen < fmt->precision ? maxlen : fmt->precision;

#ifdef WANT_WCHAR
	if(likely(MOD_LONG != fmt->mod) || !s)
	{
#else
	if(unlikely(MOD_LONG == fmt->mod))
		s = "%l???s";
#endif
	if(unlikely(!s))
		s = "<null>";

	if(unlikely(fmt->u.flags.space))
	{
		size_t r_len = strnlen(s, fmt->precision);
		size_t d_len = fmt->precision - r_len;
		if(d_len) {
// TODO: field left justified
			memset(fmt->buf, ' ', d_len < maxlen ? d_len : maxlen);
			maxlen -= d_len;
		}
		memcpy(fmt->buf + d_len, s, r_len < maxlen ? r_len : maxlen);
		len += fmt->precision;
	}
	else
	{
		t = strnpcpy(fmt->buf, s, maxlen);
		diff = t - fmt->buf;
		len += diff;
		if(*t)
		{ /* not complete? */
			if(fmt->precision) {
				if(diff < fmt->precision)
					len += fmt->precision - diff;
			} else
				len += strlen(s + diff);
		}
	}
#ifdef WANT_WCHAR
	}
	else
	{
		mbstate_t ps;
		const wchar_t *ws = (const wchar_t *)s;

		memset(&ps, 0, sizeof(ps));
		for(; len <= (maxlen - MB_CUR_MAX) && *ws != L'\0'; ws++) {
			size_t ret_val = wcrtomb(fmt->buf, *ws, &ps);
			if(ret_val != (size_t)-1)
				len += ret_val;
		}
		if(unlikely(*ws != L'\0' && len >= (maxlen - MB_CUR_MAX)))
		{
			char tbuf[MB_CUR_MAX];
			for(; *ws != L'\0'; ws++) {
				size_t ret_val = wcrtomb(tbuf, *ws, &ps);
			if((size_t)-1 != ret_val)
					len += ret_val;
			}
		}
	}
#endif
	return len;
}


static size_t f_c(struct format_spec *fmt, unsigned char c)
{
	size_t len;

	if(unlikely('C' == c))
		fmt->mod = MOD_LONG;

	if(likely(MOD_LONG != fmt->mod))
	{
		int x = va_arg(*fmt->ap, int);
		if(fmt->maxlen)
			*fmt->buf = (char)x;
		len = 1;
	}
	else
	{
#ifndef WANT_WCHAR
// TODO: popping an int may be wrong
		int x GCC_ATTRIB_UNUSED = va_arg(*fmt->ap, int);
		if(fmt->maxlen)
			*fmt->buf = '?';
		len = 1;
#else
		mbstate_t ps;
		char tbuf[MB_CUR_MAX];
		wint_t wc = va_arg(*fmt->ap, wint_t);
		size_t ret_val;

		memset(&ps, 0, sizeof(ps));
		ret_val = wcrtomb(tbuf, wc, &ps);
		if((size_t)-1 != ret_val) {
			memcpy(fmt->buf, tbuf, ret_val <= fmt->maxlen ? ret_val : fmt->maxlen);
			len = ret_val;
		} else
			len = 0;
#endif
	}

	return len;
}

static size_t f_p(struct format_spec *fmt, unsigned char c)
{
	void *ptr = va_arg(*fmt->ap, void *);
	char *ret_val;
	size_t len = 0;

	for(c = *fmt->fmt++; '#' == c || 'I' == c; c = *fmt->fmt++)
	{
		if('#' == c)
			fmt->u.flags.alternate = true;
		if('I' == c)
			fmt->u.flags.ip = true;
	}
	fmt->fmt--;

	if(!fmt->u.flags.ip)
	{
		if((sizeof(void *) * 2) + 2 > fmt->maxlen)
			len = (sizeof(void *) * 2) + 2;
		else
			len = ptoa(fmt->buf, ptr) - fmt->buf;
	}
	else
	{
		union combo_addr *addr = ptr;
		if(!addr) {
			memcpy(fmt->buf, "<null>", str_size("<null>") < fmt->maxlen ? str_size("<null>") : fmt->maxlen);
			return str_size("<null>");
		}

		if(fmt->u.flags.alternate && AF_INET6 == addr->s_fam && len++ <= fmt->maxlen)
			*fmt->buf++ = '[';

		ret_val = combo_addr_print_c(addr, fmt->buf, fmt->maxlen);
		if(!ret_val) {
			if(fmt->maxlen <= INET6_ADDRSTRLEN)
				len += INET6_ADDRSTRLEN;
			else
				ret_val = fmt->buf;
		}
		len += ret_val - fmt->buf;
		if(fmt->u.flags.alternate)
		{
			fmt->buf = ret_val;
			if(AF_INET6 == addr->s_fam && len++ <= fmt->maxlen)
				*fmt->buf++ = ']';
			if(len++ <= fmt->maxlen)
				*fmt->buf++ = ':';
			ret_val = usntoa(fmt->buf, fmt->maxlen - len, ntohs(combo_addr_port(addr)));
			ret_val = ret_val ? ret_val : fmt->buf;
			len += ret_val - fmt->buf;
		}
	}

	return len;
}

#define format_dispatch(f) format_table[*((const unsigned char *)(f)->fmt)]((f), *((const unsigned char *)(f)->fmt++))
typedef size_t (*fmt_func)(struct format_spec *, unsigned char);
static const fmt_func format_table[256];

/*
 * width
 */
static size_t widths(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	int w = va_arg(*fmt->ap, int);
	if(w < 0) {
		fmt->u.flags.left = true;
		w = -w;
	}
	fmt->width = (unsigned)w;
	return format_dispatch(fmt);
}
static size_t widthn(struct format_spec *fmt, unsigned char c)
{
	unsigned w = 0;
	for(; c && isdigit(c); c = *fmt->fmt++) {
		w *= 10;
		w += c - '0';
	}
	fmt->fmt--;
	fmt->width = w;
	return format_dispatch(fmt);
}

/*
 * precision
 */
static size_t prec_p(struct format_spec *fmt, unsigned char c)
{
	unsigned p = 0;
	c = *fmt->fmt++;
	if('*' == c) {
		int t = va_arg(*fmt->ap, int);
		if(t > 0)
			p = (unsigned)t;
	} else {
		for(; c && isdigit(c); c = *fmt->fmt++) {
			p *= 10;
			p += c - '0';
		}
		fmt->fmt--;
	}
	fmt->precision = p;
	return format_dispatch(fmt);
}

/*
 * flags
 */
static size_t flag_h(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	fmt->u.flags.alternate = true;
	return format_dispatch(fmt);
}
static size_t flag_0(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	fmt->u.flags.zero = true;
	return format_dispatch(fmt);
}
static size_t flag_m(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	fmt->u.flags.left = true;
	return format_dispatch(fmt);
}
static size_t flag_s(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	fmt->u.flags.space = true;
	return format_dispatch(fmt);
}
static size_t flag_p(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	fmt->u.flags.sign = true;
	return format_dispatch(fmt);
}
static size_t flag_I(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	fmt->u.flags.ip = true;
	return format_dispatch(fmt);
}

/*
 * lenght mod
 */
static size_t lmod_L(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	fmt->mod = MOD_LONGDOUBLE;
	return format_dispatch(fmt);
}
static size_t lmod_q(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	fmt->mod = MOD_QUAD;
	return format_dispatch(fmt);
}
static size_t lmod_j(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	fmt->mod = MOD_INTMAX_T;
	return format_dispatch(fmt);
}
static size_t lmod_z(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	fmt->mod = MOD_SIZE_T;
	return format_dispatch(fmt);
}
static size_t lmod_t(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	fmt->mod = MOD_PTRDIFF_T;
	return format_dispatch(fmt);
}
static size_t lmod_h(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	fmt->mod = MOD_SHORT != fmt->mod ? MOD_SHORT : MOD_CHAR;
	return format_dispatch(fmt);
}
static size_t lmod_l(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	fmt->mod = MOD_LONG != fmt->mod ? MOD_LONG : MOD_LONGLONG;
	return format_dispatch(fmt);
}

/*
 * specials
 */
static size_t lit_p(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	if(likely(fmt->maxlen))
		*fmt->buf = '%';
	return 1;
}
static size_t p_len(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	int *n = va_arg(*fmt->ap, int *);
	*n = (int) fmt->len;
	return 0;
}

/*
 * other conversion - uninmplemented
 */
static size_t fmtnop(struct format_spec *fmt, unsigned char c GCC_ATTRIB_UNUSED)
{
	size_t fmt_len = fmt->fmt - fmt->fmt_start;
	union all_types dummy GCC_ATTRIB_UNUSED;
	/*
	 * we don't understand the format, but at least we
	 * must remove it from the arglist, or we will crash
	 * if we search a pointer later on.
	 * Assume an int, because std-promotion makes an int
	 * out of everything, if its a (long) long, only
	 * the modifier can help or ups...
	 */
	switch(fmt->mod)
	{
	case MOD_NONE:
	case MOD_CHAR:
	case MOD_SHORT:
		{ dummy.i = va_arg(*fmt->ap, int); break; }
	case MOD_LONG:
		{ dummy.l = va_arg(*fmt->ap, long); break; }
	case MOD_LONGLONG:
		{ dummy.ll = va_arg(*fmt->ap, long long); break; }
	case MOD_LONGDOUBLE:
		{ dummy.L = va_arg(*fmt->ap, long double); break; }
	case MOD_QUAD:
		{ dummy.q = va_arg(*fmt->ap, int64_t); break; }
	case MOD_INTMAX_T:
		{ dummy.j = va_arg(*fmt->ap, intmax_t); break; }
	case MOD_SIZE_T:
		{ dummy.z = va_arg(*fmt->ap, size_t); break; }
	case MOD_PTRDIFF_T:
		{ dummy.t = va_arg(*fmt->ap, ptrdiff_t); break;}
	}
	/* whatever the user wants, don't know, print literatly */
	memcpy(fmt->buf, fmt->fmt_start, fmt_len < fmt->maxlen ? fmt_len : fmt->maxlen);
	return fmt_len;
}
static size_t unimpl(struct format_spec *fmt, unsigned char c)
{
	size_t fmt_len = fmt->fmt - fmt->fmt_start;
	printf("unimplemented format \"%.*s\": '%c'\n",
	       (int)fmt_len, fmt->fmt_start, c);
	return fmtnop(fmt, c);
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
	/* 40 */ fmtnop,   f_fp, fmtnop,    f_c, fmtnop,   f_fp,   f_fp,   f_fp, fmtnop, flag_I, fmtnop, fmtnop, lmod_L, fmtnop, fmtnop, fmtnop,
	/*            @,      A,      B,      C,      D,      E,      F,      G,      H,      I,      J,      K,      L,      M,      N,      O, */
	/* 50 */ fmtnop, fmtnop, fmtnop,    f_s, fmtnop, fmtnop, fmtnop, fmtnop,    f_X, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop, fmtnop,
	/*            P,      Q,      R,      S,      T,      U,      V,      W,      X,      Y,      Z,      [,      \,      ],      ^,      _, */
	/* 60 */ fmtnop,   f_fp, fmtnop,    f_c,    f_i,   f_fp,   f_fp,   f_fp, lmod_h,    f_i, lmod_j, fmtnop, lmod_l, f_serr,  p_len, unimpl,
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

int my_vsnprintf(char *buf, size_t maxlen, const char *fmt, va_list ap)
{
	struct format_spec cur_fmt;
	char *m;
	size_t len = 0, diff;

	cur_fmt.ap = &ap;
	cur_fmt.fmt = fmt;
	do
	{
		m = strchrnul(cur_fmt.fmt, '%');
		diff = m - cur_fmt.fmt;
		if(likely(diff)) {
			mempcpy(buf, cur_fmt.fmt, len + diff < maxlen ? diff : maxlen - len);
			buf += diff;
			len += diff;
		}
		if('%' == *m)
		{
			size_t ret_len;

			cur_fmt.fmt = m + 1;
			cur_fmt.fmt_start = cur_fmt.fmt - 1;
			cur_fmt.buf = buf;
			cur_fmt.len = len;
			cur_fmt.maxlen = maxlen - len;
			cur_fmt.precision = 0;
			cur_fmt.width = 0;
			cur_fmt.mod = MOD_NONE;
			cur_fmt.u.xyz = 0;

			ret_len = format_dispatch(&cur_fmt);
			buf += ret_len;
			len += ret_len;
		}
		else
			break;
	} while(1);

	if(likely(len < maxlen))
		*buf = '\0';

	return (int) len;
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
