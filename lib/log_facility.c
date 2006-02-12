/*
 * log_facility.c
 * logging logic/magic/functions
 *
 * Copyright (c) 2004,2005,2006 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2
 * of the License, or any later version.
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
 * $Id: log_facility.c,v 1.7 2005/11/05 11:02:32 redbully Exp redbully $
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <alloca.h>
#include <syslog.h>
// other
#include "../other.h"
// Own includes
#include "../G2MainServer.h"
#include "log_facility.h"

#define LOG_TIME_MAXLEN 128

static inline int do_logging(const enum loglevel level, const char *fmt, va_list args)
{
	FILE *log_where = level >= LOG_NOTICE ? stdout : stderr;

// TODO: properly distribute logmsg to all logging services.
	return vfprintf(log_where, fmt, args);
}

static inline size_t add_time_to_buffer(char buffer[LOG_TIME_MAXLEN])
{
	struct tm time_now;
	time_t time_now_val = time(NULL);
	buffer[0] = '\0';

	if(!localtime_r(&time_now_val, &time_now))
	{
		strcpy(buffer, "<Error breaking down time> ");
		return str_size("<Error breaking down time> ");
	}

	if(strftime(buffer, LOG_TIME_MAXLEN, server.settings.logging.time_date_format ? server.settings.logging.time_date_format : "%b %d %H:%M:%S ", &time_now))
		return strnlen(buffer, LOG_TIME_MAXLEN);

	/* 
	 * now we have a problem, there could be an error or there
	 * where just no characters to write acording to format,
	 * try to guess...
	 */
	/* Not a NUL byte at beginning? then it must be an error */
	if('\0' != buffer[0])
	{
		buffer[0] = '\0';
		strcpy(buffer, "<Error stringifying date> ");
		return str_size("<Error stringifying date> ");
	}

	return 0;
}

inline int logg(const enum loglevel level, const char *fmt, ...)
{
	va_list args;
	int ret_val;

	if(level > server.settings.logging.act_loglevel)
		return 0;

	va_start(args, fmt);
	if(server.settings.logging.add_date_time)
	{
		char *new_fmt;
		
		new_fmt = alloca(strlen(fmt) + LOG_TIME_MAXLEN + 1);
		strcpy(new_fmt + add_time_to_buffer(new_fmt), fmt);
		
		ret_val = do_logging(level, new_fmt, args);
	}
	else
		ret_val = do_logging(level, fmt, args);
	va_end(args);

	return ret_val;
}

#define INT_PR_LENGTH 24
#define STRERROR_R_SIZE 512
inline int logg_more(
		const enum loglevel level,
		const char *file,
		const char *func,
		const unsigned int line,
		int log_errno,
		const char *fmt,
		...
	)
{
	va_list args;
	char *new_fmt = NULL, *w_ptr = NULL;
	size_t file_len, func_len, fmt_len, tmp_len, total_len;
	int ret_val, old_errno;
	/* make a local copy to prevent races */
	bool add_date_time = server.settings.logging.add_date_time;

	if(level > server.settings.logging.act_loglevel)
		return 0;

	old_errno  = errno;

	total_len  = file_len  = strlen(file);
	total_len += func_len  = strlen(func);
	total_len += fmt_len   = strlen(fmt);
	total_len += log_errno ? str_size(": ") + STRERROR_R_SIZE + 1 : 0;
	total_len += add_date_time ? LOG_TIME_MAXLEN : 0;
	/* other nity peaces inserted */
	total_len += str_size(", ") + str_size("(), ") + str_size(": ") + 1;

	new_fmt    = alloca(total_len);
	w_ptr      = new_fmt;
	
	if(add_date_time)
		w_ptr  += add_time_to_buffer(w_ptr);

	w_ptr      = strcpy(w_ptr, file) + file_len;
	*w_ptr++   = ',';
	*w_ptr++   = ' ';
	w_ptr      = strcpy(w_ptr, func) + func_len;
	*w_ptr++   = '('; 
	*w_ptr++   = ')';
	*w_ptr++   = ',';
	*w_ptr++   = ' ';
	if((INT_PR_LENGTH + 1) > (unsigned)(tmp_len = snprintf(w_ptr, INT_PR_LENGTH + 1, "%u", line)))
		w_ptr  += tmp_len;
	else
		w_ptr  += INT_PR_LENGTH;
	*w_ptr++   = ':';
	*w_ptr++   = ' ';
	w_ptr      = strcpy(w_ptr, fmt) + fmt_len;

	if(log_errno)
	{
		*w_ptr++ = ':'; *w_ptr++ = ' ';
		{
#if defined HAVE_GNU_STRERROR_R || defined HAVE_MTSAFE_STRERROR
# ifdef HAVE_GNU_STRERROR_R
			/*
			 * the f***ing GNU-Version of strerror_r wich returns
			 * a char * to the buffer....
			 * This sucks especially in conjunction with strnlen,
			 * wich needs a #define __GNU_SOURCE, but conflicts
			 * with this...
			 */
			const char *s = strerror_r(old_errno, w_ptr, STRERROR_R_SIZE);
# else
			/* 
			 * Ol Solaris seems to have a static msgtable, so
			 * strerror is threadsafe and we don't have a
			 * _r version
			 */
			const char *s = strerror(old_errno);
# endif
			if(!s)
				s = "Unknown system error";
			if(s != w_ptr)
				strncpy(w_ptr, s, STRERROR_R_SIZE);
			w_ptr += strnlen(w_ptr, STRERROR_R_SIZE);
#else
			if(!strerror_r(old_errno, w_ptr, STRERROR_R_SIZE))
				w_ptr += strnlen(w_ptr, STRERROR_R_SIZE);
			else
			{
				if(EINVAL == errno)
					w_ptr = strcpy(w_ptr, "Unknown errno value!") + str_size("Unknown errno value!");
				else if(ERANGE == errno)
					w_ptr = strcpy(w_ptr, "errno msg to long for buffer!") + str_size("errno msg to long for buffer!"); 
				else
					w_ptr = strcpy(w_ptr, "failure while retrieving errno msg!") + str_size("failure while retrieving errno msg!");
			}
#endif
		}
		
		*w_ptr++ = '\n';
		*w_ptr++ = '\0';
	}

	va_start(args, fmt);
	ret_val = do_logging(level, new_fmt, args);
	va_end(args);
	return ret_val;
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id: log_facility.c,v 1.7 2005/11/05 11:02:32 redbully Exp redbully $";
//EOF
