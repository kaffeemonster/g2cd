/*
 * builtin_defaults.h
 * header-file with the defaults for most server-settings, they
 * apply if no override of some kind is given
 *
 * Copyright (c) 2004, Jan Seiffert
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
 * $Id: builtin_defaults.h,v 1.8 2005/11/05 10:49:46 redbully Exp redbully $
 */

#ifndef _BUILTIN_DEFAULTS_H
#define _BUILTIN_DEFAULTS_H

#define DEFAULT_LOGLEVEL_START		LOGF_NOTICE
#define DEFAULT_LOGLEVEL		LOGF_DEVEL
#define DEFAULT_LOG_ADD_TIME		false
#define DEFAULT_LOG_TIME_FORMAT	"%b %d %H:%M:%S "

#define DEFAULT_PORT		5000
#define DEFAULT_ADDR		INADDR_ANY

#define DEFAULT_BE_DAEMON		false
#define DEFAULT_USER		"nobody"

#define DEFAULT_FILE_GUID		"guid.txt"
#define DEFAULT_FILE_PROFILE	"profile.xml"

#define DEFAULT_CON_MAX		300
#define DEFAULT_PCK_LEN_MAX		0x020000
#define DEFAULT_SEND_PROFILE		true

#define DEFAULT_SERVER_UPEER		false
#define DEFAULT_ENC_IN		ENC_DEFLATE
#define DEFAULT_ENC_OUT		ENC_NONE

// real constants, should not be run-time configurable
#define NORM_BUFF_CAPACITY 4096

#endif // _BUILTIN_DEFAULTS_H
