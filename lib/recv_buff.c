/*
 * recv_buff.c
 * recvieve buffer allocator
 * 
 * Copyright (c) 2007 Jan Seiffert
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

#include <stdlib.h>
#include <pthread.h>

#include "log_facility.h"
#include "recv_buff.h"
#include "../other.h"

static pthread_key_t key2lrecv;

/* Protos */
	/* You better not kill this proto, or it wount work ;) */
static void recv_buff_init(void) GCC_ATTR_CONSTRUCT;
static void recv_buff_deinit(void) GCC_ATTR_DESTRUCT;

/*
 * recv_buff_init - init the recv_buff allocator
 * Get TLS key
 *
 * exit() on failure
 */
static void recv_buff_init(void)
{
	if(pthread_key_create(&key2lrecv, NULL))
		diedie("couldn't create TLS key for recv_buff");
}

static void recv_buff_deinit(void)
{
	pthread_key_delete(key2lrecv);
}

/*
 * recv_buff_local_get - get a thread local recv_buff
 *
 */
struct norm_buff *recv_buff_local_get(void)
{
	struct norm_buff *ret = pthread_getspecific(key2lrecv);

	if(ret)
		return ret;

	ret = recv_buff_alloc();
	if(!ret)
	{
		logg_errno(LOGF_DEVEL, "no local recv_buff");
		return NULL;
	}
	
	return ret;
}

/*
 * recv_buff_local_ret - return a thread local recv_buff
 */
void recv_buff_local_ret(struct norm_buff *buf)
{
	if(pthread_setspecific(key2lrecv, buf))
	{
		logg_errno(LOGF_CRIT, "local recv_buff key not initised?");
		recv_buff_free(buf);
	}
}



struct norm_buff *recv_buff_alloc(void)
{
	struct norm_buff *ret = malloc(sizeof(*ret));
	logg_devel("allocating recv_buffer");
	if(ret)
	{
		ret->capacity = sizeof(ret->data);
		buffer_clear(*ret);
	}
	return ret;
}

void recv_buff_free(struct norm_buff *ret)
{
	free(ret);
}
