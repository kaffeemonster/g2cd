/*
 * log_facility.c
 * logging logic/magic/functions
 *
 * Copyright (c) 2004-2010 Jan Seiffert
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
 * $Id: log_facility.c,v 1.7 2005/11/05 11:02:32 redbully Exp redbully $
 */

#include "other.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include "my_pthread.h"
#ifdef HAVE_SYSLOG_H
# include <syslog.h>
#endif
#include <unistd.h>
/* other */
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#endif
/* Own includes */
#include "../G2MainServer.h"
#include "log_facility.h"
#include "sec_buffer.h"
#include "itoa.h"
#include "my_bitopsm.h"

#define LOG_TIME_MAXLEN 128
#define logg_int_pos(x, y) \
	do_logging_int(x, "%s:%s()@%u: " y, __FILE__, __func__, __LINE__)

static inline int do_vlogging(const enum loglevel, const char *, va_list);
static inline int do_logging(const enum loglevel, const char *, size_t);
static inline int do_logging_int(const enum loglevel, const char *, ...);

/*
 * Work with a TLS-buffer to preprint the msg.
 * we could try to pass around the fmt and args (it worked this way till Dez 2006),
 * but there are two probs:
 * 1) classic syslog only is avail as elipses-func, vsyslog is a BSD-extension.
 *    So while you maybe find a C99(-like) system (with v[s|f|sn]print), if theres
 *    a vsyslog? (OK, glibc has it _all_ but still...)
 * 2) We want to manipulate the fmt, and for this we need memory anyway, so no more
 *    mumbo-jumbo any more, and no alloca (fmt's on the stack *brrr*, and BSD with
 *    its small default thread stacks...)
 * so we print it our self, and finaly pass down the finished string/buffer
 */
#ifndef HAVE___THREAD
static pthread_key_t key2logg;
static int logg_tls_ready;
/* protos */
	/* you better not kill this prot, our it won't work ;) */
static void logg_init(void) GCC_ATTR_CONSTRUCT;
static void logg_deinit(void) GCC_ATTR_DESTRUCT;

static void logg_init(void)
{
	/* set a default log level till config comes up */
	server.settings.logging.act_loglevel = LOGF_ERR;

	if(pthread_key_create(&key2logg, free))
		diedie("couln't create TLS key for logging");

	/*
	 * that constructors are called in random order is OK, as long as they
	 * are not called parralel...
	 */
	logg_tls_ready = true;
}

static void logg_deinit(void)
{
	struct big_buff *tmp_buf;

	if(unlikely(!logg_tls_ready))
		return;

	if((tmp_buf = pthread_getspecific(key2logg))) {
		free(tmp_buf);
		pthread_setspecific(key2logg, NULL);
	}
	pthread_key_delete(key2logg);
	logg_tls_ready = false;
}
#else
static __thread struct big_buff *local_logg_buffer;
#endif

static struct big_buff *logg_get_buf(void)
{
	struct big_buff *ret_buf;
#ifndef HAVE___THREAD
	/* for the rare case we are called from another constructor and are not ready */
	if(likely(logg_tls_ready)) {
		ret_buf = pthread_getspecific(key2logg);
		if(likely(ret_buf))
			return ret_buf;
	}
#else
	ret_buf = local_logg_buffer;
	if(likely(ret_buf))
		return ret_buf;
#endif

	ret_buf = malloc(sizeof(*ret_buf) + (NORM_BUFF_CAPACITY / 4));

#ifndef HAVE___THREAD
	/* Gnarf, when we we are called from another constuctor, log_level is still 0 */
	/* !!! but we won't get here, loglevel is now tested at the call site... !!! */
	if(!logg_tls_ready) {
		/*
		 * force compiler to recheck the bool, it optimizes the access
		 * away (check above plus only change in "not called" function)
		 * valgrind sees everything...
		 */
		barrier();
		server.settings.logging.act_loglevel = LOGF_ERR;
	}
#endif

	if(!ret_buf) {
		/* we cannot logg with our self */
		logg_int_pos(LOGF_WARN, "no logg buffer\n");
		return NULL;
	}
	ret_buf->capacity = NORM_BUFF_CAPACITY / 4;
	buffer_clear(*ret_buf);

#ifndef HAVE___THREAD
	if(logg_tls_ready)
		pthread_setspecific(key2logg, ret_buf);
#else
	local_logg_buffer = ret_buf;
#endif

	return ret_buf;
}

static void logg_ret_buf(struct big_buff *ret_buf)
{
	buffer_clear(*ret_buf);
#ifndef HAVE___THREAD
	/* for the rare case a constructor screams */
	if(logg_tls_ready)
	{
		if(pthread_setspecific(key2logg, ret_buf)) {
			logg_int_pos(LOGF_EMERG, "adding logg buff back failed\n");
			free(ret_buf);
			exit(EXIT_FAILURE);
		}
	}
	else
		free(ret_buf);
#else
	local_logg_buffer = ret_buf;
#endif
}

static inline int do_vlogging(const enum loglevel level, const char *fmt, va_list args)
{
	FILE *log_where = level >= LOGF_NOTICE ? stdout : stderr;

#ifdef HAVE_SYSLOG_H
// TODO: properly distribute logmsg to all logging services.
#endif
	return vfprintf(log_where, fmt, args);
}

static inline int do_logging(const enum loglevel level, const char *string, size_t len)
{
	int log_where = level >= LOGF_NOTICE ? STDOUT_FILENO : STDERR_FILENO;

#ifdef HAVE_SYSLOG_H
// TODO: properly distribute logmsg to all logging services.
#endif
	return write(log_where, string, len);
}

static inline int do_logging_int(const enum loglevel level, const char *fmt, ...)
{
	va_list args;
	int ret_val;

	va_start(args, fmt);
	ret_val = do_vlogging(level, fmt, args);
	va_end(args);
	return ret_val;
}

static noinline size_t add_dyn_time_to_buffer(char buffer[LOG_TIME_MAXLEN], size_t max_len)
{
	struct tm time_now;
	time_t time_now_val = time(NULL);

	buffer[0] = '\0';
	if(!localtime_r(&time_now_val, &time_now)) {
		strlitcpy(buffer, "<Error breaking down time> ");
		return str_size("<Error breaking down time> ");
	}

	if(likely(strftime(buffer, max_len, server.settings.logging.time_date_format, &time_now)))
	{
		size_t len = strnlen(buffer, max_len);
		if(likely(len < max_len)) {
			buffer[len] = ' ';
			len++;
		}
		return len;
	}

	/*
	 * now we have a problem, there could be an error or there
	 * where just no characters to write acording to format,
	 * try to guess...
	 */
	/* Not a NUL byte at beginning? then it must be an error */
	if('\0' != buffer[0]) {
		strlitcpy(buffer, "<Error stringifying date> ");
		return str_size("<Error stringifying date> ");
	}

	return 0;
}

static noinline size_t add_fix_time_to_buffer(char buffer[LOG_TIME_MAXLEN], size_t max_len)
{
	time_t time_now_val = time(NULL);
	size_t len;

	len = print_lts(buffer, max_len, &time_now_val);
	if(likely(len < max_len)) {
		buffer[len] = ' ';
		len++;
	}
	return len;
}

static inline size_t add_time_to_buffer(char buffer[LOG_TIME_MAXLEN], size_t max_len)
{
// TODO: maybe use local_time_now?
	/* problem is if timeout thread has died... */
	if(likely(!server.settings.logging.time_date_format))
		return add_fix_time_to_buffer(buffer, max_len);
	else
		return add_dyn_time_to_buffer(buffer, max_len);
}

/*
 *
 */
#define STRERROR_R_SIZE 512
static int logg_internal(const enum loglevel level,
                         const char *file,
                         const char *func,
                         const unsigned int line,
                         int log_errno,
                         const char *fmt,
                         va_list args
                        )
{
	struct big_buff *logg_buff;
	int ret_val, old_errno, retry_cnt;

	old_errno  = errno;

	/* get our tls-print buf */
	if(!(logg_buff = logg_get_buf()))
	{
		/*
		 * hmmm, something went wrong, lets see, if we can get the message to
		 * the user by other means
		 */
		return do_vlogging(level, fmt, args);
	}

	/* add time, if wanted, to buffer */
	if(server.settings.logging.add_date_time)
		logg_buff->pos += add_time_to_buffer(buffer_start(*logg_buff), buffer_remaining(*logg_buff));

	/* put out the "extra" stuff, if there */
	if(file)
	{
		retry_cnt = 0;
		prefetch(strnpcpy);
		prefetch(file);
		prefetch(func);
		/*
		 * calling snprintf for 2 * strcpy + an itoa is "nice"
		 * but goes round the full stdio bloat:
		 * snprintf->vsnprintf->vfprintf-> myriads of funcs to print
		 */
		do
		{
			char *sptr, *stmp, *rstart;
			size_t remaining;

			stmp = sptr = buffer_start(*logg_buff);
			remaining = buffer_remaining(*logg_buff);
			rstart = strnpcpy(sptr, file, remaining);
			remaining -= rstart - sptr;
			if(unlikely(remaining < 7))
				goto realloc;
			sptr = rstart;

			*sptr++ = ':';
			remaining--;
			rstart = strnpcpy(sptr, func, remaining);
			remaining -= rstart - sptr;
			if(unlikely(remaining < 7))
				goto realloc;
			sptr = rstart;

			*sptr++ = '(';
			*sptr++ = ')';
			*sptr++ = '@';
			remaining -= 3;
			rstart = untoa(sptr, remaining, line);
			if(unlikely(!rstart))
				goto realloc;
			remaining -= rstart - sptr;
			if(unlikely(remaining < 2))
				goto realloc;
			sptr = rstart;
			*sptr++ = ':';
			*sptr++ = ' ';
			logg_buff->pos += sptr - stmp;
			break;
realloc:
			/* now we are in a slow path, no need to hurry */
			{
				struct big_buff *tmp_buff;
				size_t len = strlen(file) + strlen(func) + 6 + 30 + strlen(fmt) * 3;
				len = ROUND_ALIGN(len * 2, 2048) + logg_buff->capacity;
				tmp_buff = realloc(logg_buff, sizeof(*logg_buff) + len);
				if(tmp_buff) {
					logg_buff = tmp_buff;
					logg_buff->limit = logg_buff->capacity = len;
				} else
					break;
				retry_cnt++;
			}
		} while(retry_cnt < 4);
	}

	ret_val = 0; retry_cnt = 0;
	/* format the message in tls */
	do
	{
		size_t len = logg_buff->capacity;
		va_list tmp_valist;
		if(ret_val < 0)
		{
			len *= 2;
			if(++retry_cnt > 4) {
				ret_val = 0;
				break;
			}
		}
		else if((size_t)ret_val > buffer_remaining(*logg_buff))
			len = ROUND_ALIGN((size_t)ret_val * 2, 1024); /* align to a k */
		if(unlikely(len != logg_buff->capacity))
		{
			struct big_buff *tmp_buf = realloc(logg_buff, sizeof(*logg_buff) + len);
			if(tmp_buf) {
				logg_buff = tmp_buf;
				logg_buff->limit = logg_buff->capacity = len;
			} else {
				ret_val = buffer_remaining(*logg_buff);
				break;
			}
		}
		/* put msg printed out in buffer */
		va_copy(tmp_valist, args);
		ret_val = my_vsnprintf(buffer_start(*logg_buff), buffer_remaining(*logg_buff), fmt, tmp_valist);
		va_end(tmp_valist);
		/* error? repeat */
	} while(unlikely(ret_val < 0 || (size_t)ret_val > buffer_remaining(*logg_buff)));
	logg_buff->pos += (size_t)ret_val;

	/* add errno string if wanted */
	if(log_errno)
	{
		if(buffer_remaining(*logg_buff) < STRERROR_R_SIZE + 4)
		{
			size_t len = logg_buff->capacity * 2;
			struct big_buff *tmp_buff = realloc(logg_buff, sizeof(*logg_buff) + len);
			if(!tmp_buff)
				goto no_errno;
			logg_buff = tmp_buff;
			logg_buff->limit = logg_buff->capacity += len;
		}
		*buffer_start(*logg_buff) = ':'; logg_buff->pos++;
		*buffer_start(*logg_buff) = ' '; logg_buff->pos++;
		{
#if defined STRERROR_R_CHAR_P || defined HAVE_MTSAFE_STRERROR || !defined HAVE_STRERROR_R
			size_t err_str_len;
# ifdef STRERROR_R_CHAR_P
			/*
			 * the f***ing GNU-Version of strerror_r wich returns
			 * a char * to the buffer....
			 * This sucks especially in conjunction with strnlen,
			 * wich needs a #define __GNU_SOURCE, but conflicts
			 * with this...
			 */
			const char *s = strerror_r(old_errno, buffer_start(*logg_buff), buffer_remaining(*logg_buff)-2);
# else
			/*
			 * Ol Solaris seems to have a static msgtable, so
			 * strerror is threadsafe and we don't have a
			 * _r version
			 */
			const char *s = strerror(old_errno);
# endif
			if(s)
				err_str_len = strnlen(s, buffer_remaining(*logg_buff)-2);
			else {
				s = "Unknown system error";
				err_str_len = strlen(s) < (buffer_remaining(*logg_buff)-2) ?
				              strlen(s) : buffer_remaining(*logg_buff)-2;
			}

			if(s != buffer_start(*logg_buff))
				my_memcpy(buffer_start(*logg_buff), s, err_str_len);
			logg_buff->pos += err_str_len;
#else
			if(!strerror_r(old_errno, buffer_start(*logg_buff), buffer_remaining(*logg_buff)))
				logg_buff->pos += strnlen(buffer_start(*logg_buff), buffer_remaining(*logg_buff));
			else
			{
				char *bs = buffer_start(*logg_buff);
				if(EINVAL == errno) {
					strlitcpy(bs, "Unknown errno value!");
					logg_buff->pos += str_size("Unknown errno value!");
				} else if(ERANGE == errno) {
					strlitcpy(bs, "errno msg to long for buffer!");
					logg_buff->pos += str_size("errno msg to long for buffer!");
				} else {
					strlitcpy(bs, "failure while retrieving errno msg!");
					logg_buff->pos += str_size("failure while retrieving errno msg!");
				}
			}
#endif
		}
		*buffer_start(*logg_buff) = '\n'; logg_buff->pos++;
		*buffer_start(*logg_buff) = '\0';
	}
no_errno:

	/* output that stuff */
	buffer_flip(*logg_buff);
	ret_val = do_logging(level, buffer_start(*logg_buff), buffer_remaining(*logg_buff));
	logg_ret_buf(logg_buff);
	return ret_val;
}

int logg_ent(const enum loglevel level, const char *fmt, ...)
{
	va_list args;
	int ret_val;

	/* we already checked at the call site */
	if(unlikely(level > get_act_loglevel()))
		return 0;

	va_start(args, fmt);
	ret_val = logg_internal(level, NULL, NULL, 0, false, fmt, args);
	va_end(args);
	return ret_val;
}

int logg_more_ent(const enum loglevel level,
                  const char *file,
                  const char *func,
                  const unsigned int line,
                  int log_errno,
                  const char *fmt,
                  ...
                 )
{
	va_list args;
	int ret_val;

	if(unlikely(level > get_act_loglevel()))
		return 0;

	va_start(args, fmt);
	ret_val = logg_internal(level, file, func, line, log_errno, fmt, args);
	va_end(args);
	return ret_val;
}

static char const rcsid_lf[] GCC_ATTR_USED_VAR = "$Id: log_facility.c,v 1.7 2005/11/05 11:02:32 redbully Exp redbully $";
/* EOF */
