/*
 * log_facility.h
 * header-file for log_facility.c, the logging logic/magic
 *
 * Copyright (c) 2004-2010 Jan Seiffert
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
 * $Id: log_facility.h,v 1.7 2005/11/05 10:57:45 redbully Exp redbully $
 */

#ifndef LIB_LOG_FACILITY_H
# define LIB_LOG_FACILITY_H

# define str_size(x)	(sizeof(x) - 1)

# ifdef DEBUG_DEVEL
#  define logg_devel(x) logg_more(LOGF_DEVEL, __FILE__, __func__, __LINE__, 0, x)
#  define logg_develd(x,...) logg_more(LOGF_DEVEL, __FILE__, __func__, __LINE__, 0, x, __VA_ARGS__)
# else
#  define logg_devel(x) do { } while(0)
#  define logg_develd(x, ...) do { } while(0)
# endif

# ifdef DEBUG_DEVEL_OLD
#  define logg_devel_old(x) logg_more(LOGF_DEVEL_OLD, __FILE__, __func__, __LINE__, 0, x)
#  define logg_develd_old(x,...) logg_more(LOGF_DEVEL_OLD, __FILE__, __func__, __LINE__, 0, x, __VA_ARGS__)
# else
#  define logg_devel_old(x) do { } while(0)
#  define logg_develd_old(x, ...) do { } while(0)
# endif

# define logg_pos(x, y) logg_more(x, __FILE__, __func__, __LINE__, 0, y)
# define logg_posd(x, y, ...) logg_more(x, __FILE__, __func__, __LINE__, 0, y, __VA_ARGS__)

# define logg_errno(x, y) logg_more(x, __FILE__, __func__, __LINE__, 1, y)
# define logg_errnod(x, y, ...) logg_more(x, __FILE__, __func__, __LINE__, 1, y, __VA_ARGS__)

# define die(x)	\
	do { logg_more(LOGF_ERR, __FILE__, __func__, __LINE__, 0, x); exit(EXIT_FAILURE); } while(0)
# define diedie(x)	\
	do { logg_more(LOGF_ERR, __FILE__, __func__, __LINE__, 1, x); exit(EXIT_FAILURE); } while(0)

enum loglevel
{
	LOGF_SILENT=-1,/* turn off logging                   */
	LOGF_EMERG,    /* system is unusable                 */
	LOGF_ALERT,    /* action must be taken immediately   */
	LOGF_CRIT,     /* critical conditions                */
	LOGF_ERR,      /* error conditions                   */
	LOGF_WARN,     /* warning conditions                 */
	LOGF_NOTICE,   /* normal, but significant, condition */
	LOGF_INFO,     /* informational message              */
	LOGF_DEBUG,    /* debug-level                        */
	LOGF_DEVEL,    /* dump everything a dev wants to see */
	LOGF_DEVEL_OLD /* dump everything a dev once wanted  */
};

# include "../G2MainServer.h"
# include "other.h"

# ifndef _LOG_FACILITY_C
#  define _LOGF_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define _LOGF_EXTRN(x) x GCC_ATTR_VIS("hidden")
# endif /* _LOG_FACILITY_C */

_LOGF_EXTRN(int logg_ent(const enum loglevel level, const char *fmt, ...)) GCC_ATTR_PRINTF(2,3);
_LOGF_EXTRN(int logg_more_ent(const enum loglevel, const char *, const char *, const unsigned int, int, const char *, ...)) GCC_ATTR_PRINTF(6,7);

# ifdef __GNUC__
#  define logg(l,f, ...) \
	((l <= get_act_loglevel()) ? logg_ent(l, f, ## __VA_ARGS__) : (void)0)
#  define logg_more(le, fi, fu, li, e, fmt, ...) \
	((unlikely(le <= get_act_loglevel())) ? logg_more_ent(le, fi, fu, li, e, fmt, ## __VA_ARGS__) : (void)0)
# else
#  define logg	logg_ent
#  define logg_more	logg_more_ent
# endif

#include <stdarg.h>
#include <time.h>

_LOGF_EXTRN(int my_snprintf(char *buf, size_t maxlen, const char *fmt, ...)) GCC_ATTR_PRINTF(3,4);
_LOGF_EXTRN(int my_vsnprintf(char *buf, size_t maxlen, const char *fmt, va_list ap));
_LOGF_EXTRN(size_t print_ts(char *buf, size_t n, const time_t *t));
_LOGF_EXTRN(size_t print_lts(char *buf, size_t n, const time_t *t));
_LOGF_EXTRN(int read_ts(const char *buf, time_t *t));

#endif /* LIB_LOG_FACILITY_H */
