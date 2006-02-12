/*
 * timeout.c
 * thread to handle the varios server timeouts needed
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
 * $Id: timeout.c,v 1.14 2005/11/05 09:57:22 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
// System Includes
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
// Own Includes
#include "other.h"
#define _TIMEOUT_C
#include "timeout.h"

inline bool add_timeout(GCC_ATTR_UNUSED_PARAM(to_id *, new_timeout))
{
	return true;
}

inline bool cancel_timeout(GCC_ATTR_UNUSED_PARAM(const to_id, who_to_cancel))
{
	return true;
}

void *timeout_timer_task(GCC_ATTR_UNUSED_PARAM(void *, param))
{
	pthread_exit(NULL);
	return(NULL);
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id: timeout.c,v 1.14 2005/11/05 09:57:22 redbully Exp redbully $";
// EOF
