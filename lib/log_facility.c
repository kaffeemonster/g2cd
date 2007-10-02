/*
 * log_facility.c
 * logging logic/magic/functions
 *
 * Copyright (c) 2004,2005,2006,2007 Jan Seiffert
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
 * $Id: log_facility.c,v 1.7 2005/11/05 11:02:32 redbully Exp redbully $
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <alloca.h>
#include <syslog.h>
// other
#include "../other.h"
// Own includes
#include "../G2MainServer.h"
#include "log_facility.h"
#include "sec_buffer.h"

#define LOG_TIME_MAXLEN 128
#define logg_int_pos(x, y) \
	do_logging(x, "%s:%s()@%u: " y, __FILE__, __func__, __LINE__)

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
static pthread_key_t key2logg;
static int logg_tls_ready;
/* protos */
	/* you better not kill this prot, our it won't work ;) */
static void logg_init(void) GCC_ATTR_CONSTRUCT;
static void logg_deinit(void) GCC_ATTR_DESTRUCT;
static inline int do_vlogging(const enum loglevel, const char *, va_list);
static inline int do_logging(const enum loglevel, const char *, ...);

static void logg_init(void)
{
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

	if(!logg_tls_ready)
		return;

	if((tmp_buf = pthread_getspecific(key2logg)))
	{
		free(tmp_buf);
		pthread_setspecific(key2logg, NULL);
	}
	pthread_key_delete(key2logg);
	logg_tls_ready = false;
}

static struct big_buff *logg_get_buf(void)
{
	struct big_buff *ret_buf;
	
	/* for the rare case we are called from another constructor and are not ready */
	if(logg_tls_ready && (ret_buf = pthread_getspecific(key2logg)))
		return ret_buf;

	ret_buf = malloc(sizeof(*ret_buf) + (NORM_BUFF_CAPACITY / 4));

	if(!ret_buf)
	{
		/* we cannot logg with our self */
		logg_int_pos(LOGF_WARN, "no logg buffer\n");
		return NULL;
	}
	ret_buf->capacity = NORM_BUFF_CAPACITY / 4;
	buffer_clear(*ret_buf);

	if(logg_tls_ready)
		pthread_setspecific(key2logg, ret_buf);

	return ret_buf;
}

static void logg_ret_buf(struct big_buff *ret_buf)
{
	/* for the rare case a constructor screams */
	if(logg_tls_ready)
	{
		buffer_clear(*ret_buf);
		if(pthread_setspecific(key2logg, ret_buf))
		{
			logg_int_pos(LOGF_EMERG, "adding logg buff back failed\n");
			free(ret_buf);
			exit(EXIT_FAILURE);
		}
	}
	else
		free(ret_buf);
}

static inline int do_vlogging(const enum loglevel level, const char *fmt, va_list args)
{
	FILE *log_where = level >= LOG_NOTICE ? stdout : stderr;

// TODO: properly distribute logmsg to all logging services.
	return vfprintf(log_where, fmt, args);
}

static inline int do_logging(const enum loglevel level, const char *fmt, ...)
{
	va_list args;
	int ret_val;

	va_start(args, fmt);
	ret_val = do_vlogging(level, fmt, args);
	va_end(args);
	return ret_val;
}

static inline size_t add_time_to_buffer(char buffer[LOG_TIME_MAXLEN], size_t max_len)
{
	struct tm time_now;
	time_t time_now_val = time(NULL);
	buffer[0] = '\0';

	if(!localtime_r(&time_now_val, &time_now))
	{
		strcpy(buffer, "<Error breaking down time> ");
		return str_size("<Error breaking down time> ");
	}

	if(strftime(buffer, max_len, server.settings.logging.time_date_format ? server.settings.logging.time_date_format : "%b %d %H:%M:%S ", &time_now))
		return strnlen(buffer, max_len);

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

#define STRERROR_R_SIZE 512
static int logg_internal(
		const enum loglevel level,
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

//	if(level > server.settings.logging.act_loglevel)
//		return 0;

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
		ret_val = 0; retry_cnt = 0;
		do
		{
			size_t len = logg_buff->capacity;
			if(ret_val < 0)
			{
				len *= 2;
				if(++retry_cnt > 4)
				{
					ret_val = 0;
					break;
				}
			}
			else if((size_t)ret_val > buffer_remaining(*logg_buff))
				len = (((size_t)ret_val * 2) + 1023) & ~((size_t) 1023);
			if(len != logg_buff->capacity)
			{
				struct big_buff *tmp_buf = realloc(logg_buff, sizeof(*logg_buff) + len);
				if(tmp_buf)
				{
					logg_buff = tmp_buf;
					logg_buff->limit = logg_buff->capacity = len;
				}
				else
				{
					ret_val = buffer_remaining(*logg_buff);
					break;
				}
			}
			ret_val = snprintf(buffer_start(*logg_buff), buffer_remaining(*logg_buff), "%s:%s()@%u: ", file, func, line);
		} while(ret_val < 0 || (size_t)ret_val > buffer_remaining(*logg_buff));
		logg_buff->pos += (size_t)ret_val;
	}

	ret_val = 0; retry_cnt = 0;
	do
	{
		size_t len = logg_buff->capacity;
		va_list tmp_valist;
		if(ret_val < 0)
		{
			len *= 2;
			if(++retry_cnt > 4)
			{
				ret_val = 0;
				break;
			}
		}
		else if((size_t)ret_val > buffer_remaining(*logg_buff))
			len = (((size_t)ret_val * 2) + 1023) & ~((size_t)1023); /* align to a k */
		if(len != logg_buff->capacity)
		{
			struct big_buff *tmp_buf = realloc(logg_buff, sizeof(*logg_buff) + len);
			if(tmp_buf)
			{
				logg_buff = tmp_buf;
				logg_buff->limit = logg_buff->capacity = len;
			}
			else
			{
				ret_val = buffer_remaining(*logg_buff);
				break;
			}
		}
		/* put msg printed out in buffer */
		va_copy(tmp_valist, args);
		ret_val = vsnprintf(buffer_start(*logg_buff), buffer_remaining(*logg_buff), fmt, tmp_valist);
		va_end(tmp_valist);
		/* error? repeat */
	} while(ret_val < 0 || (size_t)ret_val > buffer_remaining(*logg_buff));
	logg_buff->pos += (size_t)ret_val;
	
	
	if(log_errno)
	{
		if(buffer_remaining(*logg_buff) < STRERROR_R_SIZE + 4)
		{
			struct big_buff *tmp_buff = realloc(logg_buff, sizeof(*logg_buff) + logg_buff->capacity * 2);
			if(!tmp_buff)
				goto no_errno;
			logg_buff = tmp_buff;
			logg_buff->limit = logg_buff->capacity += 1024;
		}
		*buffer_start(*logg_buff) = ':'; logg_buff->pos++;
		*buffer_start(*logg_buff) = ' '; logg_buff->pos++;
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
			const char *s = strerror_r(old_errno, buffer_start(*logg_buff), buffer_remaining(*logg_buff));
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
			if(s != buffer_start(*logg_buff))
				strncpy(buffer_start(*logg_buff), s, buffer_remaining(*logg_buff) - 2);
			logg_buff->pos += strnlen(buffer_start(*logg_buff), buffer_remaining(*logg_buff));
#else
			if(!strerror_r(old_errno, buffer_start(*logg_buff), buffer_remaining(*logg_buff)))
				logg_buff->pos += strnlen(buffer_start(*logg_buff), buffer_remaining(*logg_buff));
			else
			{
				if(EINVAL == errno) {
					strcpy(buffer_start(*logg_buff), "Unknown errno value!"); logg_buff->pos += str_size("Unknown errno value!");
				} else if(ERANGE == errno) {
					strcpy(buffer_start(*logg_buff), "errno msg to long for buffer!"); logg_buff->pos += str_size("errno msg to long for buffer!"); 
				} else {
					strcpy(buffer_start(*logg_buff), "failure while retrieving errno msg!"); logg_buff->pos += str_size("failure while retrieving errno msg!");
				}
			}
#endif
		}
		*buffer_start(*logg_buff) = '\n'; logg_buff->pos++;
		*buffer_start(*logg_buff) = '\0'; logg_buff->pos++;
	}
no_errno:

	buffer_flip(*logg_buff);
	// Feed temp. in old func
	ret_val = do_logging(level, "%s", buffer_start(*logg_buff));
	logg_ret_buf(logg_buff);
	return ret_val;
}

int logg_ent(const enum loglevel level, const char *fmt, ...)
{
	va_list args;
	int ret_val;

	if(level > get_act_loglevel())
		return 0;

	va_start(args, fmt);
	ret_val = logg_internal(level, NULL, NULL, 0, false, fmt, args);
	va_end(args);
	return ret_val;
}

int logg_more_ent(
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
	int ret_val;

	if(level > get_act_loglevel())
		return 0;

	va_start(args, fmt);
	ret_val = logg_internal(level, file, func, line, log_errno, fmt, args);
	va_end(args);
	return ret_val;
}


static char const rcsid_lf[] GCC_ATTR_USED_VAR = "$Id: log_facility.c,v 1.7 2005/11/05 11:02:32 redbully Exp redbully $";
//EOF
