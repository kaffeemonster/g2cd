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
	unsigned precision;
	unsigned width;
	enum length_mod mod;
	struct
	{
		bool sign:1;
		bool zero:1;
		bool space:1;
		bool left:1;
		bool alternate:1;
		bool ip:1;
	} flags;
};

#define A_POP(type, prfx) \
		do { \
			d.prfx = va_arg(*ap, type); \
		} while(0)


static noinline size_t format_i(struct format_spec *fmt, char *buf, size_t maxlen, va_list *ap)
{
	union all_types d;
	size_t len = 0;
	char *ret_val = buf;

#define A_PRINT(type, prfx) \
		do { \
			d.prfx = va_arg(*ap, type); \
			if(unlikely(fmt->flags.sign) && d.prfx >= 0) { \
				if(len <= maxlen) \
					*buf++ = '+'; \
				len++; \
			} \
			if(fmt->flags.zero) \
				ret_val = prfx##toa_0fix(buf, d.prfx, fmt->width); \
			else if(fmt->flags.space) \
				ret_val = prfx##toa_sfix(buf, d.prfx, fmt->width); \
			else { \
				ret_val = prfx##ntoa(buf, maxlen, d.prfx); \
				ret_val = ret_val ? ret_val : buf; \
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

	len += ret_val - buf;
	buf  = ret_val;

	return len;
}

static noinline size_t format_u(struct format_spec *fmt, char *buf, size_t maxlen, va_list *ap)
{
	union all_types d;
	size_t len = 0;
	char *ret_val = buf;

#define A_PRINT(type, prfx) \
		do { \
			d.prfx = va_arg(*ap, type); \
			if(unlikely(fmt->flags.sign)) { \
				if(len <= maxlen) \
					*buf++ = '+'; \
				len++; \
			} \
			if(fmt->flags.zero) \
				ret_val = prfx##toa_0fix(buf, d.prfx, fmt->width); \
			else if(fmt->flags.space) \
				ret_val = prfx##toa_sfix(buf, d.prfx, fmt->width); \
			else { \
				ret_val = prfx##ntoa(buf, maxlen, d.prfx); \
				ret_val = ret_val ? ret_val : buf; \
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

	len += ret_val - buf;
	buf  = ret_val;

	return len;
}

#if 0
static char *pr_nop(char *buf)
{
	return buf;
}
#endif

static noinline size_t format_X(struct format_spec *fmt, char *buf, size_t maxlen, va_list *ap)
{
	union all_types d;
	size_t len = 0;
	char *ret_val = buf;

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
			d.prfx = va_arg(*ap, type); \
			if(fmt->flags.zero) \
				ret_val = prfx##toXa_0fix(buf, d.prfx, fmt->width); \
			else if(fmt->flags.space) \
				ret_val = prfx##toXa_sfix(buf, d.prfx, fmt->width); \
			else { \
				ret_val = prfx##ntoXa(buf, maxlen, d.prfx); \
				ret_val = ret_val ? ret_val : buf; \
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

	len += ret_val - buf;
	buf  = ret_val;

	return len;
}

static noinline size_t format_x(struct format_spec *fmt, char *buf, size_t maxlen, va_list *ap)
{
	union all_types d;
	size_t len = 0;
	char *ret_val = buf;

#define A_PRINT(type, prfx) \
		do { \
			d.prfx = va_arg(*ap, type); \
			if(fmt->flags.zero) \
				ret_val = prfx##toxa_0fix(buf, d.prfx, fmt->width); \
			else if(fmt->flags.space) \
				ret_val = prfx##toxa_sfix(buf, d.prfx, fmt->width); \
			else { \
				ret_val = prfx##ntoxa(buf, maxlen, d.prfx); \
				ret_val = ret_val ? ret_val : buf; \
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

	len += ret_val - buf;
	buf  = ret_val;

	return len;
}

static noinline size_t format_o(struct format_spec *fmt, char *buf, size_t maxlen GCC_ATTRIB_UNUSED, va_list *ap)
{
	union all_types d;
	size_t len = 0;
	char *ret_val = buf;

// TODO: octal print anyone?

	switch(fmt->mod)
	{
	case MOD_NONE:
		A_POP(unsigned, u);
		break;
	case MOD_LONG:
		A_POP(unsigned long, ul);
		break;
	case MOD_QUAD:
	case MOD_LONGLONG:
		A_POP(unsigned long long, ull);
		break;
	case MOD_CHAR:
		A_POP(unsigned, uc);
		break;
	case MOD_SHORT:
		A_POP(unsigned, us);
		break;
	case MOD_SIZE_T:
		A_POP(size_t, uz);
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

	len += ret_val - buf;
	buf  = ret_val;

	return len;
}

static noinline size_t format_fp(struct format_spec *fmt, char *buf, size_t maxlen, va_list *ap, const char *fmt_str)
{
	union all_types d;
	size_t fmt_len = fmt_str - fmt->fmt_start;

	printf("unimplemented format \"%.*s\": '%c'\n",
	       (int)fmt_len, fmt->fmt_start, *(fmt_str-1));
	/* whatever the user wants, don't know, print literatly */
	mempcpy(buf, fmt->fmt_start, fmt_len < maxlen ? fmt_len : maxlen);

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

static noinline size_t format_strerror(struct format_spec *fmt GCC_ATTRIB_UNUSED, char *buf, size_t maxlen, va_list *ap GCC_ATTRIB_UNUSED)
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
	const char *s = strerror_r(errno, buf, maxlen);
# else
	/*
	 * Ol Solaris seems to have a static msgtable, so
	 * strerror is threadsafe and we don't have a
	 * _r version
	 */
	const char *s = strerror(errno);
# endif
	if(s)
		err_str_len = strnlen(s, maxlen);
	else {
		s = "Unknown system error";
		err_str_len = strlen(s) < maxlen ?
		              strlen(s) : maxlen;
	}

	if(s != buf)
		memcpy(buf, s, err_str_len);
#else
	if(!strerror_r(errno, buf, maxlen))
		err_str_len += strnlen(buf, maxlen);
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
	return err_str_len;
}


static noinline  size_t format_s(struct format_spec *fmt, char *buf, size_t maxlen, va_list *ap)
{
	const char *s;
	char *t;
	size_t len = 0, diff;

	s = va_arg(*ap, const char *);

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

	if(unlikely(fmt->flags.space))
	{
		size_t r_len = strnlen(s, fmt->precision);
		size_t d_len = fmt->precision - r_len;
		if(d_len) {
// TODO: field left justified
			memset(buf, ' ', d_len < maxlen ? d_len : maxlen);
			maxlen -= d_len;
		}
		memcpy(buf + d_len, s, r_len < maxlen ? r_len : maxlen);
		len += fmt->precision;
	}
	else
	{
		t = strnpcpy(buf, s, maxlen);
		diff = t - buf;
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
			size_t ret_val = wcrtomb(buf, *ws, &ps);
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


static noinline size_t format_c(struct format_spec *fmt, char *buf, size_t maxlen, va_list *ap)
{
	size_t len = 0;

	if(likely(MOD_LONG != fmt->mod))
	{
		int c = va_arg(*ap, int);
		if(maxlen)
			*buf = (char) c;
		len++;
	}
	else
	{
#ifndef WANT_WCHAR
// TODO: popping an int may be wrong
		int x GCC_ATTRIB_UNUSED = va_arg(*ap, int);
		if(maxlen)
			*buf = '?';
		len++;
#else
		mbstate_t ps;
		char tbuf[MB_CUR_MAX];
		wint_t wc = va_arg(*ap, wint_t);
		size_t ret_val;

		memset(&ps, 0, sizeof(ps));
		ret_val = wcrtomb(tbuf, wc, &ps);
		if((size_t)-1 != ret_val) {
			memcpy(buf, tbuf, ret_val <= maxlen ? ret_val : maxlen - len);
			len += ret_val;
		}
#endif
	}

	return len;
}

static noinline size_t format_p(struct format_spec *fmt, char *buf, size_t maxlen, va_list *ap)
{
	void *ptr = va_arg(*ap, void *);
	char *ret_val;
	size_t len = 0;

	if(!fmt->flags.ip)
	{
		if((sizeof(void *) * 2) + 2 > maxlen)
			len = (sizeof(void *) * 2) + 2;
		else
			len = ptoa(buf, ptr) - buf;
	}
	else
	{
		union combo_addr *addr = ptr;
		if(!addr) {
			strlitcpy(buf, "<null>");
			return str_size("<null>");
		}

		if(fmt->flags.alternate && AF_INET6 == addr->s_fam && len++ <= maxlen)
			*buf++ = '[';

		ret_val = combo_addr_print_c(addr, buf, maxlen);
		if(!ret_val) {
			if(maxlen <= INET6_ADDRSTRLEN)
				len += INET6_ADDRSTRLEN;
			else
				ret_val = buf;
		}
		len += ret_val - buf;
		if(fmt->flags.alternate)
		{
			buf = ret_val;
			if(AF_INET6 == addr->s_fam && len++ <= maxlen)
				*buf++ = ']';
			if(len++ <= maxlen)
				*buf++ = ':';
			ret_val = usntoa(buf, maxlen - len, ntohs(combo_addr_port(addr)));
			ret_val = ret_val ? ret_val : buf;
			len += ret_val - buf;
		}
	}

	return len;
}

int my_vsnprintf(char *buf, size_t maxlen, const char *fmt, va_list ap)
{
	struct format_spec cur_fmt;
	size_t len = 0, ret_len;
	bool in_fmt = false;
	char c;
	cur_fmt.fmt_start = fmt;

	for(c = *fmt++; c; c = *fmt++)
	{
		if(likely(!in_fmt))
		{
			if(likely('%' != c)) {
				if(likely(len++ <= maxlen))
					*buf++ = c;
			} else {
				in_fmt = true;
				memset(&cur_fmt, 0, sizeof(cur_fmt));
				cur_fmt.fmt_start = fmt - 1;
			}
			continue;
		}
		switch(c)
		{
		/* conversion */
		case 'd':
		case 'i':
			ret_len = format_i(&cur_fmt, buf, maxlen - len, &ap);
			buf += ret_len;
			len += ret_len;
			in_fmt = false;
			break;
		case 'u':
			ret_len = format_u(&cur_fmt, buf, maxlen - len, &ap);
			buf += ret_len;
			len += ret_len;
			in_fmt = false;
			break;
		case 'x':
			ret_len = format_x(&cur_fmt, buf, maxlen - len, &ap);
			buf += ret_len;
			len += ret_len;
			in_fmt = false;
			break;
		case 'X':
			ret_len = format_X(&cur_fmt, buf, maxlen - len, &ap);
			buf += ret_len;
			len += ret_len;
			in_fmt = false;
			break;
		case 'o':
			ret_len = format_o(&cur_fmt, buf, maxlen - len, &ap);
			buf += ret_len;
			len += ret_len;
			in_fmt = false;
			break;
		case 'S':
			cur_fmt.mod = MOD_LONG;
		case 's':
			ret_len = format_s(&cur_fmt, buf, maxlen - len, &ap);
			buf += ret_len;
			len += ret_len;
			in_fmt = false;
			break;
		case 'C':
			cur_fmt.mod = MOD_LONG;
		case 'c':
			ret_len = format_c(&cur_fmt, buf, maxlen - len, &ap);
			buf += ret_len;
			len += ret_len;
			in_fmt = false;
			break;
		case 'p':
p_fmt_retry:
			c = *fmt;
			if('#' == c || 'I' == c)
			{
				fmt++;
				if('#' == c)
					cur_fmt.flags.alternate = true;
				if('I' == c)
					cur_fmt.flags.ip = true;
				goto p_fmt_retry;
			}
			ret_len = format_p(&cur_fmt, buf, maxlen - len, &ap);
			buf += ret_len;
			len += ret_len;
			in_fmt = false;
			break;
		case 'm':
			ret_len = format_strerror(&cur_fmt, buf, maxlen - len, &ap);
			buf += ret_len;
			len += ret_len;
			in_fmt = false;
			break;
		/* floating point conversion - UNIMPLEMETNED */
		case 'e':
		case 'E':
		case 'f':
		case 'F':
		case 'g':
		case 'G':
		case 'a':
		case 'A':
			ret_len = format_fp(&cur_fmt, buf, maxlen - len, &ap, fmt);
			buf += ret_len;
			len += ret_len;
			in_fmt = false;
			break;
		/* specials */
		case '%':
			if(likely(len++ < maxlen))
				*buf++ = '%';
			in_fmt = false;
			break;
		case 'n':
			{
				int *n = va_arg(ap, int *);
				*n = (int) len;
				in_fmt = false;
			}
			break;
		/* precision */
		case '.':
			{
				unsigned p = 0;
				c = *fmt++;
				if('*' == c) {
					int t = va_arg(ap, int);
					if(t > 0)
						p = (unsigned)t;
				} else {
					for(; c && isdigit(c); c = *fmt++) {
						p *= 10;
						p += c - '0';
					}
					fmt--;
				}
				cur_fmt.precision = p;
			}
			break;
		/* width */
		case '*':
			{
				int w = va_arg(ap, int);
				if(w < 0) {
					cur_fmt.flags.left = true;
					w = -w;
				}
				cur_fmt.width = (unsigned)w;
			}
			break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			{
				unsigned w = 0;
				for(; c && isdigit(c); c = *fmt++) {
					w *= 10;
					w += c - '0';
				}
				fmt--;
				cur_fmt.width = w;
			}
			break;
		/* flags */
		case '#': cur_fmt.flags.alternate = true; break;
		case '0': cur_fmt.flags.zero  = true;     break;
		case '-': cur_fmt.flags.left  = true;     break;
		case ' ': cur_fmt.flags.space = true;     break;
		case '+': cur_fmt.flags.sign  = true;     break;
		case 'I': cur_fmt.flags.ip    = true;     break;
		/* length mod */
		case 'L': cur_fmt.mod = MOD_LONGDOUBLE;   break;
		case 'q': cur_fmt.mod = MOD_QUAD;         break;
		case 'j': cur_fmt.mod = MOD_INTMAX_T;     break;
		case 'z': cur_fmt.mod = MOD_SIZE_T;       break;
		case 't': cur_fmt.mod = MOD_PTRDIFF_T;    break;
		case 'h': cur_fmt.mod = MOD_SHORT != cur_fmt.mod ? MOD_SHORT : MOD_CHAR; break;
		case 'l': cur_fmt.mod = MOD_LONG != cur_fmt.mod ? MOD_LONG : MOD_LONGLONG; break;
		/* other conversion - uninmplemented */
		case '$':
			{
				size_t fmt_len = fmt - cur_fmt.fmt_start;
				printf("unimplemented format \"%.*s\": '%c'\n",
				       (int)fmt_len, cur_fmt.fmt_start, c);
			}
		default:
			{
				size_t fmt_len = fmt - cur_fmt.fmt_start;
				union all_types dummy GCC_ATTRIB_UNUSED;
				/*
				 * we don't understand the format, but at least we
				 * must remove it from the arglist, or we will crash
				 * if we search a pointer later on.
				 * Assume an int, because std-promotion makes an int
				 * out of everything, if its a (long) long, only
				 * the modifier can help or ups...
				 */
				switch(cur_fmt.mod)
				{
				case MOD_NONE:
				case MOD_CHAR:
				case MOD_SHORT:
					{ dummy.i = va_arg(ap, int); break; }
				case MOD_LONG:
					{ dummy.l = va_arg(ap, long); break; }
				case MOD_LONGLONG:
					{ dummy.ll = va_arg(ap, long long); break; }
				case MOD_LONGDOUBLE:
					{ dummy.L = va_arg(ap, long double); break; }
				case MOD_QUAD:
					{ dummy.q = va_arg(ap, int64_t); break; }
				case MOD_INTMAX_T:
					{ dummy.j = va_arg(ap, intmax_t); break; }
				case MOD_SIZE_T:
					{ dummy.z = va_arg(ap, size_t); break; }
				case MOD_PTRDIFF_T:
					{ dummy.t = va_arg(ap, ptrdiff_t); break;}
				}
				/* whatever the user wants, don't know, print literatly */
				if(likely(len + fmt_len <= maxlen))
					buf = mempcpy(buf, cur_fmt.fmt_start, fmt_len);
				else
					buf = mempcpy(buf, cur_fmt.fmt_start, maxlen - len);
				len += fmt_len;
				in_fmt = false;
			}
		}
	}

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
