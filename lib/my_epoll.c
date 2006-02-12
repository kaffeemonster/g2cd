/*
 * my_epoll.c
 * wrapper to emulate the epoll-interface with varius
 * other interfaces
 *
 * Copyright (c) 2004, 2005 Jan Seiffert
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
 * $Id: my_epoll.c,v 1.14 2005/11/05 10:38:33 redbully Exp redbully $
 */

#define _MY_EPOLL_C
#include "my_epoll.h"

#ifdef NEED_EPOLL_COMPAT
/* System includes */
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <pthread.h>
# include <errno.h>
# include <sys/types.h>
/* Own Includes */
# include "../other.h"
# include "log_facility.h"

# ifdef HAVE_POLL
/* Ok, lets emulate epoll with classical poll */
#  include "my_epoll_poll.c"
# elif defined HAVE_KEPOLL
/* Maybe we find a Userland not capable of EPoll (old libc),
 * but with a Kernel 2.6 underneath. So call the Kernel
 * manually (Ouch, I know, bad thing...) */
#  include "my_epoll_kepoll.c"
# elif defined HAVE_DEVPOLL
/* I don't like Solaris, but i like extra Performance */
#  include "my_epoll_devpoll.c"
# elif defined HAVE_KQUEUE
/* OK, lets use the BSD-way of doing things */
// TODO: write this stuff?
#  include "my_epoll_kqueue.c"
# endif

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id: my_epoll.c,v 1.14 2005/11/05 10:38:33 redbully Exp redbully $";
#endif /* _NEED_EPOLL_COMPAT */
