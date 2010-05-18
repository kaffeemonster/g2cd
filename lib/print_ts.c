/*
 * print_ts.c
 * print a timestamp in fixed format
 *
 * Copyright (c) 2010 Jan Seiffert
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
 * $Id: $
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
 * (no time conversion needed, we have a timestamp which is
 * near UTC).
 *
 * So cut the crap and get to the core part creating a
 * textual date and time.
 * This is blatantly ripped out of glibc which is:

   Copyright (C) 1991, 1993, 1997 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
 */

#include <time.h>
#include <limits.h>
#include <stdbool.h>

#include "itoa.h"

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
size_t print_ts(char *buf, size_t n, const time_t *t);

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
size_t print_lts(char *buf, size_t n, const time_t *t);

#define SECONDS_PER_MIN  (60)
#define SECONDS_PER_HOUR (SECONDS_PER_MIN * 60)
#define SECONDS_PER_DAY  (SECONDS_PER_HOUR * 24)

static const unsigned short days_in_year_till_month[2][13] =
{
	/* Normal years.  */
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
	/* Leap years.  */
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

static bool is_leap_year(unsigned long year)
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

static long div(long a, long b)
{
	return a / b - (a % b < 0);
}

static long leaps_thru_end_of(long year)
{
	return div(year, 4) - div(year, 100) + div(year, 400);
}

struct dfields
{
	unsigned long year;
	unsigned short month, day, hour, minute, seconds;
};

static noinline int calc_dfields(struct dfields *f, const time_t *t)
{
	long tdays, year;
	unsigned rem;
	const unsigned short *s, *e;

	/*
	 * We are dealing with timestamps.
	 * Of computer systems.
	 * Anybody claming he has something older than
	 * 1970...
	 *
	 * But beware, 2031 we have to revaluate the
	 * situation.
	 * And needs an overhaul if years turn 5 digits
	 */
	if(unlikely(*t < 0 || *t / SECONDS_PER_DAY > LONG_MAX))
		return 0;

	tdays      =  *t / SECONDS_PER_DAY;
	rem        =  *t % SECONDS_PER_DAY;
	f->hour    = rem / SECONDS_PER_HOUR;
	rem        = rem % SECONDS_PER_HOUR;
	f->minute  = rem / SECONDS_PER_MIN;
	f->seconds = rem % SECONDS_PER_MIN;
	year       = 1970; /* the POSIX epoch started 1970 */

	while(tdays < 0 || tdays >= (is_leap_year(year) ? 366 : 365))
	{
		/* Guess a corrected year, assuming 365 days per year.  */
		long year_guessed = year + tdays / 365 - (tdays % 365 < 0);
		/* Adjust days and year to match the guessed year.  */
		tdays -= ((year_guessed - year) * 365 +
		         leaps_thru_end_of(year_guessed - 1) - leaps_thru_end_of(year - 1));
		year = year_guessed;
	}
	f->year = year;

	s = days_in_year_till_month[is_leap_year(f->year)];
	e = s + (tdays / 31) + 1;
	while(e >= s)
		if(tdays < *e)
			e--;
		else
			break;
	tdays    -= *e;
	f->month  = (e - s) + 1;
	f->day    = tdays + 1;

	return 1;
}

size_t print_ts(char *buf, size_t n, const time_t *t)
{
	struct dfields f;
	char *wptr;

	if(unlikely(!buf || n < 18 || !calc_dfields(&f, t)))
		return 0;

	wptr = ultoa(buf, f.year);
	*wptr++ = '-';
	wptr = ustoa_0fix(wptr, f.month, 2);
	*wptr++ = '-';
	wptr = ustoa_0fix(wptr, f.day, 2);
	*wptr++ = 'T';
	wptr = ustoa_0fix(wptr, f.hour, 2);
	*wptr++ = ':';
	wptr = ustoa_0fix(wptr, f.minute, 2);
	*wptr++ = 'Z';
	*wptr = '\0';
	return wptr - buf;
}

size_t print_lts(char *buf, size_t n, const time_t *t)
{
	struct dfields f;
	char *wptr;

	if(unlikely(!buf || n < 21 || !calc_dfields(&f, t)))
		return 0;

	wptr = ultoa(buf, f.year);
	*wptr++ = '-';
	wptr = ustoa_0fix(wptr, f.month, 2);
	*wptr++ = '-';
	wptr = ustoa_0fix(wptr, f.day, 2);
	*wptr++ = 'T';
	wptr = ustoa_0fix(wptr, f.hour, 2);
	*wptr++ = ':';
	wptr = ustoa_0fix(wptr, f.minute, 2);
	*wptr++ = ':';
	wptr = ustoa_0fix(wptr, f.seconds, 2);
	*wptr++ = 'Z';
	*wptr = '\0';
	return wptr - buf;
}

/*@unused@*/
static char const rcsid_prts[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
