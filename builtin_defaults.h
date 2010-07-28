/*
 * builtin_defaults.h
 * header-file with the defaults for most server-settings, they
 * apply if no override of some kind is given
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
 * $Id: builtin_defaults.h,v 1.8 2005/11/05 10:49:46 redbully Exp redbully $
 */

#ifndef _BUILTIN_DEFAULTS_H
#define _BUILTIN_DEFAULTS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define DEFAULT_LOGLEVEL_START	LOGF_NOTICE
#define DEFAULT_LOGLEVEL	LOGF_DEVEL
#define DEFAULT_LOG_ADD_TIME	false
#define DEFAULT_LOG_TIME_FORMAT	NULL

#ifdef WE_ARE_MAKING_A_RELEASE
# define DEFAULT_PORT		6346
# define DEFAULT_DATA_ROOT_DIR	CONFIGURED_CACHE_DIR
# define DEFAULT_CONFIG_FILE	CONFIGURED_CONF_DIR/g2cd.conf
#else
# define DEFAULT_PORT		5000
# define DEFAULT_DATA_ROOT_DIR	./
# define DEFAULT_CONFIG_FILE	g2cd.conf
#endif
#define DEFAULT_ADDR6	in6addr_any
#define DEFAULT_ADDR		INADDR_ANY

#define DEFAULT_BE_DAEMON	false
#define DEFAULT_DBUS	false

#define DEFAULT_USE_IPV4	true
#define DEFAULT_USE_IPV6	true

#define DEFAULT_USER		"nobody"
#define DEFAULT_NICE_ADJUST	3
#define DEFAULT_NUM_THREADS	0 /* autodetect */
#define DEFAULT_ENTROPY_SOURCE	"/dev/urandom"
#define DEFAULT_NICK_NAME	NULL
#define DEFAULT_GWC_BOOT	"http://cache.trillinux.org/g2/bazooka.php"
#define DEFAULT_GWC_DB		"gwc_cache"
#define DEFAULT_KHL_DUMP	"khl.dump"
#define DEFAULT_GUID_DUMP	"guid.dump"
#define DEFAULT_QHT_COMPRESSION COMP_DEFLATE
#define DEFAULT_QHT_COMPRESS_INTERNAL true
#define DEFAULT_QHT_MAX_PROMILLE 300

#define DEFAULT_NICK_SEND_CLIENTS false
#define DEFAULT_NICK_SEND_GPS true
#define DEFAULT_PROFILE_LAT	"43.119234"
#define DEFAULT_PROFILE_LONG	"6.359982"
#define DEFAULT_PROFILE_COUNTRY	"Somewhere"
#define DEFAULT_PROFILE_CITY	"Somecity"
#define DEFAULT_PROFILE_STATE	"Somestate"
#define DEFAULT_PROFILE_GENDER	"Male"

#define DEFAULT_CON_MAX		150
#define DEFAULT_HUB_MAX		4
#define DEFAULT_PCK_LEN_MAX	0x020000
#define DEFAULT_SEND_PROFILE	true

#define DEFAULT_SERVER_UPEER	true
#define DEFAULT_ENC_IN		ENC_NONE
#define DEFAULT_ENC_OUT		ENC_NONE
#define DEFAULT_HUB_ENC_IN	ENC_DEFLATE
#define DEFAULT_HUB_ENC_OUT	ENC_DEFLATE

/* real constants, should not be run-time configurable */
#define NORM_BUFF_CAPACITY	(4096 - (3 * sizeof(size_t)))
#define NORM_HZP_THRESHOLD	5

#endif /* _BUILTIN_DEFAULTS_H */
