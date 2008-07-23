/*
 * timeout.h
 * header-file for timeout.c
 *
 * Copyright (c) 2004, Jan Seiffert
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
 * $Id: timeout.h,v 1.7 2005/11/05 09:57:43 redbully Exp redbully $
 */

#ifndef _TIMEOUT_H
#define _TIMEOUT_H

#include <sys/time.h>
#include "lib/list.h"

struct timeout
{
	struct list_head l;
	struct timespec t;
};

#ifndef _TIMEOUT_C
#define _TOUT_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#else
#define _TOUT_EXTRN(x) x GCC_ATTR_VIS("hidden")
#endif

_TOUT_EXTRN(bool timeout_add(struct timeout *, unsigned int));
_TOUT_EXTRN(bool timeout_cancel(struct timeout *));
_TOUT_EXTRN(void timeout_timer_task_abort(void));
_TOUT_EXTRN(void *timeout_timer_task(void *));

#endif //_TIMEOUT_H
//EOF
