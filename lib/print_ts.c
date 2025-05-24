/*
 * print_ts.c
 * print a timestamp in fixed format
 *
 * Copyright (c) 2010-2022 Jan Seiffert
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
 * $Id: $
 *
 * math calc_dfields is derived from freebsd libc && glibc
 */

/*
 * strftime + gmtime_r and friends show up in the profile
 * place 4 in libc, and that with only one callsite on G2
 * handshaking.
 * They are as expensive as either malloc or free, and that
 * with our special packet allocator down...
 * Autschn, that sucks!
 *
 * We don't need all that fancy strftime stuff. Esp. we only
 * want to print UTC, and the precision isn't that important
 * (not building NTP here, no time conversion needed, we have
 * a timestamp which is near enough UTC).
 *
 * So cut the crap and get to the core part creating a
 * textual date and time.
 */

#include "other.h"
#include <time.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include "itoa.h"
#include "swab.h"

/*
 * print_ts: print a timestamp to a buffer in the format
 *           "%Y-%m-%dT%H:%MZ"
 *
 * buf: the target buffer
 * n: the target buffer size
 * t: the timestamp
 *
 * return value: the number of chars printed without trailing 0
 *               or 0
 *
 * WARNING: does not obey n ATM
 */
size_t print_ts(char *buf, size_t n, const time_t *t) GCC_ATTR_VIS("hidden");
size_t print_ts_rev(char *buf, size_t n, const time_t *t) GCC_ATTR_VIS("hidden");

/*
 * print_lts: print a timestamp to a buffer in the format
 *            "%Y-%m-%dT%H:%M:%SZ"
 *
 * buf: the target buffer
 * n: the target buffer size
 * t: the timestamp
 *
 * return value: the number of chars printed without trailing 0
 *               or 0
 *
 * WARNING: does not obey n ATM
 */
size_t print_lts(char *buf, size_t n, const time_t *t) GCC_ATTR_VIS("hidden");

/*
 * read_ts: reads a timestamp from a buffer in the format
 *          "%Y-%m-%dT%H:%M"
 *
 * buf: the source buffer
 * t: the target timestamp
 *
 * return value: 1 conversion succesfull, 0 error
 */
int read_ts(const char *buf, time_t *t) GCC_ATTR_VIS("hidden");

#define SECONDS_PER_MIN  (60)
#define SECONDS_PER_HOUR (SECONDS_PER_MIN * 60)
#define SECONDS_PER_DAY  (SECONDS_PER_HOUR * 24)

static const unsigned short days_in_year_till_month[13] =
{
	/* Normal year  */
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
};

static always_inline bool is_leap_year(unsigned long year)
{
	if(unlikely(year % 4 == 0))
	{
		if(unlikely(year % 100 == 0))
		{
			if(unlikely(year % 400 == 0))
				return true;
			return false;
		}
		return true;
	}
	return false;
}

static always_inline int mod_div(int a, int b)
{
	return a / b - (a % b < 0);
}

static always_inline int leaps_thru_end_of(int year)
{
	return mod_div(year, 4) - mod_div(year, 100) + mod_div(year, 400);
}

struct dfields
{
	unsigned long year;
	unsigned month, day, hour, minute, seconds;
};

static noinline bool calc_dfields(struct dfields *f, const time_t *t)
{
	/* 2 billion days is enough for 5 Mio years (and even on 16 bit we get 89) */
	int tdays, year;
	unsigned rem, utdays, i;

	/*
	 * We are dealing with timestamps.
	 * Of computer systems, running nodes, on the
	 * internet.
	 * Anybody claming he is somewhere before
	 * 1970 ...
	 *
	 * But beware, 2038 we have to revaluate the
	 * situation how the unix time stamp melt down
	 * is generally going to be fixed and whats the
	 * official status of TS > 2 billion/< 0.
	 *
	 * And this needs an general overhaul if years
	 * turn 5 digits
	 */
	if(unlikely(*t < 0 || (unsigned long)*t / SECONDS_PER_DAY > INT_MAX))
		return false;

	tdays      = (unsigned long)*t / SECONDS_PER_DAY;
	rem        = (unsigned long)*t % SECONDS_PER_DAY;
	f->hour    = rem / SECONDS_PER_HOUR;
	rem        = rem % SECONDS_PER_HOUR;
	f->minute  = rem / SECONDS_PER_MIN;
	f->seconds = rem % SECONDS_PER_MIN;
	year       = 1970; /* the POSIX epoch started 1970 */

	/* just fall in, save one check at the start
	 * worst thing is one unnecessary round of calcs which come out to 0
	 * if you pass in a 1970 timestamp, in all other cases you have to
	 * do the calcs anyway, but then you have done one additional round
	 * through the conditionals
	 */
	do
	{
		/* Guess a corrected year, assuming 365 days per year.  */
		int year_guessed = year + tdays / 365 - (tdays % 365 < 0);
		/* Adjust days and year to match the guessed year.  */
		tdays -= ((year_guessed - year) * 365 +
		         leaps_thru_end_of(year_guessed - 1) - leaps_thru_end_of(year - 1));
		year = year_guessed;
	} while(tdays < 0 || tdays >= (is_leap_year(year) ? 366 : 365));
	f->year = year;

	utdays = tdays; /* days is unsigned after the above loop */
	f->day  = 1;
	/* fix up leap years */
	if(is_leap_year(f->year) && utdays > 58) {
		if(utdays == 59)
			f->day = 2;
		utdays--;
	}

	for(i = (utdays / 31) + 1; i > 0 && utdays < days_in_year_till_month[i]; i--)
		/* do nothing */;
	utdays  -= days_in_year_till_month[i];
	f->month = i + 1;
	f->day  += utdays;

	return true;
}

static noinline int dfields_calc(time_t *t, const struct dfields *f)
{
	unsigned long tsecs;
	unsigned mon  = f->month; /* month is needed to be 0 to 11 */
	unsigned year = f->year;

	/* checked every minute value till 3237 against glibc strftime+gmtime_r */
	/*
	 * If we are pre March (Jan. & Feb.), no need to count
	 * in a leap. Timewarp back a year instead so the year
	 * test does not add a leap day.
	 * Or shift our zero point to March so Feb is at end of year.
	 */
	if(0 >= (int) (mon -= 2)) {
		mon  += 12;
		year -= 1;
	}
	tsecs  = year * 365; /* get all days for the years */
	tsecs -= 719499; /* subtract all days + leap days from 0 to 1970 */
	tsecs += year/4 - year/100 + year/400; /* add all leap days for this year */
	tsecs += 367*mon/12; /* multiply month by 30.58, the average day count... */
// TODO: check day for correct value, if someone throws in the 55th of whatever
	/* or let it simply wrap, to correct... */
	tsecs += f->day; /* add day of month */
	/* bring it to hours */
	tsecs *= 24;
	tsecs += f->hour;
	tsecs *= 60;
	tsecs += f->minute;
	/* somewhere here we are bound for overflow 32 bit signed in 2038 */
	tsecs *= 60;
	tsecs += f->seconds;

	if(sizeof(time_t) == 4)
		*t = (time_t)(tsecs & 0x7fffffff); /* time_t should be signed, don't wrap */
	else
		*t = (time_t)tsecs;
	return true;
}

static noinline GCC_ATTR_FASTCALL char *put_dec_8bit_2fix(char *buf, unsigned q)
{
	unsigned r;

	r      = (q * 0xcd) >> 11;
	*buf++ = (q - 10 * r) + '0';

	q      = (r * 0xd) >> 7;
	*buf++ = (r - 10 * q)  + '0';

	if(unlikely(q))
		*buf++ = q + '0';

	return buf;
}

size_t print_ts_rev(char *buf, size_t n, const time_t *t)
{
	struct dfields f;
	char *wptr;

	if(unlikely(!buf || n < 18 || !calc_dfields(&f, t)))
		return 0;

	wptr = buf;
	*wptr++ = 'Z';
	wptr = put_dec_8bit_2fix(wptr, f.minute);
	*wptr++ = ':';
	wptr = put_dec_8bit_2fix(wptr, f.hour);
	*wptr++ = 'T';
	wptr = put_dec_8bit_2fix(wptr, f.day);
	*wptr++ = '-';
	wptr = put_dec_8bit_2fix(wptr, f.month);
	*wptr++ = '-';
	wptr = put_dec_trunc(wptr, f.year); /* will keep us afloat till 99,999 */
	return wptr - buf;
}

size_t print_ts(char *buf, size_t n, const time_t *t)
{
	size_t res;

	res = print_ts_rev(buf + 1, n - 1, t);
	if(res) {
		buf[0] = '\0';
		strreverse_l(buf, buf + res);
	}
	return res;
}

size_t print_lts(char *buf, size_t n, const time_t *t)
{
	struct dfields f;
	char *wptr;

	if(unlikely(!buf || n < 21 || !calc_dfields(&f, t)))
		return 0;

	wptr = buf;
	*wptr++ = '\0';
	*wptr++ = 'Z';
	wptr = put_dec_8bit_2fix(wptr, f.seconds);
	*wptr++ = ':';
	wptr = put_dec_8bit_2fix(wptr, f.minute);
	*wptr++ = ':';
	wptr = put_dec_8bit_2fix(wptr, f.hour);
	*wptr++ = 'T';
	wptr = put_dec_8bit_2fix(wptr, f.day);
	*wptr++ = '-';
	wptr = put_dec_8bit_2fix(wptr, f.month);
	*wptr++ = '-';
	wptr = put_dec_trunc(wptr, f.year); /* will keep us afloat till 99,999 */
	strreverse_l(buf, wptr - 1);
	return (wptr - 1) - buf;
}

int read_ts(const char *buf, time_t *t)
{
	struct dfields f;
	char *e;
	unsigned long x;

	errno = 0;
	x = strtoul(buf, &e, 10);
	if(unlikely((ULONG_MAX == x && ERANGE == errno) || buf == e || x < 1970))
		return 0;
	f.year = x;
	if(unlikely('-' != *e))
		return 0;
	buf = e + 1;
	x = strtoul(buf, &e, 10);
	if(unlikely((ULONG_MAX == x && ERANGE == errno) || buf == e || (x < 1 || x > 12)))
		return 0;
	f.month = x;
	if(unlikely('-' != *e))
		return 0;
	buf = e + 1;
	x = strtoul(buf, &e, 10);
	if(unlikely((ULONG_MAX == x && ERANGE == errno) || buf == e || (x < 1 || x > 31)))
		return 0;
	f.day = x;
	if(unlikely('T' != *e))
		return 0;
	buf = e + 1;
	x = strtoul(buf, &e, 10);
	if(unlikely((ULONG_MAX == x && ERANGE == errno) || buf == e || x > 23))
		return 0;
	f.hour = x;
	if(unlikely(':' != *e))
		return 0;
	buf = e + 1;
	x = strtoul(buf, &e, 10);
	if(unlikely((ULONG_MAX == x && ERANGE == errno) || buf == e || x > 59))
		return 0;
	f.minute = x;
//	if(unlikely('Z' != *e && '\0' != *e))
//		return 0;
	f.seconds = 0;

	return dfields_calc(t, &f);
}

/*@unused@*/
static char const rcsid_prts[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
