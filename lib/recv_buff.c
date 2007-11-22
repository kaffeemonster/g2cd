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

/* Protos */
	/* You better not kill this proto, or it wount work ;) */
static void recv_buff_init(void) GCC_ATTR_CONSTRUCT;
static void recv_buff_deinit(void) GCC_ATTR_DESTRUCT;
static inline struct norm_buff *recv_buff_alloc_system(void);
static inline void recv_buff_free_system(struct norm_buff *);

/* Vars */
	/* buffer buffer */
static atomicptra_t free_buffs[FB_CAP_START];
static pthread_key_t key2lrecv;
/*
 * recv_buff_init - init the recv_buff allocator
 * Get TLS key
 * fill buffer buffer
 *
 * exit() on failure
 */
static void recv_buff_init(void)
{
	size_t i;

	if(pthread_key_create(&key2lrecv, NULL))
		diedie("couldn't create TLS key for recv_buff\n");

	for(i = 0; i < anum(free_buffs); i++)
	{
		struct norm_buff *tmp = recv_buff_alloc_system();
		if(tmp)
		{
			if((tmp = atomic_pxa(tmp, &free_buffs[i])))
			{
				logg_pos(LOGF_CRIT, "another thread working while init???");
				recv_buff_free_system(tmp);
			}
		}
		else
		{
			logg_errno(LOGF_CRIT, "free_buffs memory");
			if(FB_TRESHOLD < i)
				break;

			for(; i > 0; --i)
			{
				tmp = NULL;
				recv_buff_free_system(atomic_pxa(tmp, &free_buffs[i]));
			}
			exit(EXIT_FAILURE);
		}
	}
}

static void recv_buff_deinit(void)
{
	pthread_key_delete(key2lrecv);
// TODO: free buffer up again, remainder, want to see them in valgrind
}

void recv_buff_local_refill(void)
{
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


static inline struct norm_buff *recv_buff_alloc_system(void)
{
	struct norm_buff *ret = malloc(sizeof(*ret));
	logg_devel("allocating recv_buffer from sys\n");
	if(ret)
	{
		ret->capacity = sizeof(ret->data);
		buffer_clear(*ret);
	}
	return ret;
}

struct norm_buff *recv_buff_alloc(void)
{
	return recv_buff_alloc_system();
}

static inline void recv_buff_free_system(struct norm_buff *tf)
{
	free(tf);
}

void recv_buff_free(struct norm_buff *ret)
{
	if(!ret)
		return;
	recv_buff_free_system(ret);
}
