/*
 * recv_buff.c
 * recvieve buffer allocator
 *
 * Copyright (c) 2007-2010 Jan Seiffert
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
 * $Id: $
 */

#include <stdlib.h>
#include "my_pthread.h"

#include "log_facility.h"
#include "recv_buff.h"
#include "other.h"


struct local_buffer_org
{
	int pos;
	struct norm_buff *buffs[3 * EVENT_SPACE];
};

/* Protos */
	/* You better not kill this proto, or it wount work ;) */
static inline struct norm_buff *recv_buff_alloc_system(void);
static inline void recv_buff_free_system(struct norm_buff *);
#ifndef VALGRIND_ME_RECV
static void recv_buff_init(void) GCC_ATTR_CONSTRUCT;
static void recv_buff_deinit(void) GCC_ATTR_DESTRUCT;
static void recv_buff_free_lorg(void *);

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

	if(pthread_key_create(&key2lrecv, recv_buff_free_lorg))
		diedie("couldn't create TLS key for recv_buff\n");

	for(i = 0; i < anum(free_buffs); i++)
	{
		struct norm_buff *tmp = recv_buff_alloc_system();
		if(tmp)
		{
			if((tmp = atomic_pxa(tmp, &free_buffs[i]))) {
				logg_pos(LOGF_CRIT, "another thread working while init???");
				recv_buff_free_system(tmp);
			}
		}
		else
		{
			logg_errno(LOGF_CRIT, "free_buffs memory");
			if(FB_TRESHOLD < i)
				break;

			for(; i > 0; --i) {
				tmp = NULL;
				recv_buff_free_system(atomic_pxa(tmp, &free_buffs[i]));
			}
			exit(EXIT_FAILURE);
		}
	}
}

static void recv_buff_deinit(void)
{
	size_t i = 0;

	pthread_key_delete(key2lrecv);

	do
	{
		struct norm_buff *ret = NULL;
		recv_buff_free_system(atomic_pxa(ret, &free_buffs[i]));
	} while(++i < anum(free_buffs));
}

static void recv_buff_free_lorg(void *vorg)
{
	struct local_buffer_org *org;
	int i;

	if(!vorg)
		return;
#ifndef DEBUG_DEVEL_OLD
	org = vorg;
	for(i = org->pos; i < (int)anum(org->buffs);) {
		struct norm_buff *tmp = org->buffs[i];
		org->buffs[i++] = NULL;
		recv_buff_free_system(tmp);
	}
	free(org);
#endif
}

static noinline struct local_buffer_org *recv_buff_init_lorg(void)
{
	struct local_buffer_org *org = calloc(1, sizeof(*org));
	int i;

	if(!org)
		return NULL;
	
	org->pos = anum(org->buffs);
	if(unlikely(pthread_setspecific(key2lrecv, org))) {
		logg_errno(LOGF_CRIT, "local recv_buff key not initialized?");
		free(org);
		return NULL;
	}

	for(i = org->pos; i > (int)((anum(org->buffs) / 3) + 2);)
	{
		org->buffs[--i] = recv_buff_alloc_system();
		if(!org->buffs[i]) {
			i++;
			break;
		}
	}
	org->pos = i;
	return org;
}
#endif

void recv_buff_local_refill(void)
{
#ifndef VALGRIND_ME_RECV
	struct local_buffer_org *org = pthread_getspecific(key2lrecv);
	int i;

	if(!org) {
		org = recv_buff_init_lorg();
		if(!org)
			return;
	}

	i = org->pos;
	if(unlikely(i > (int)((anum(org->buffs) / 3) + 2)))
	{
		for(; i > (int)((anum(org->buffs) / 3) + 2);)
		{
			org->buffs[--i] = recv_buff_alloc();
			if(!org->buffs[i]) {
				i++;
				break;
			}
		}
	}
	org->pos = i;
#endif
}

void recv_buff_local_free(void)
{
#ifndef VALGRIND_ME_RECV
	struct local_buffer_org *org = pthread_getspecific(key2lrecv);
	unsigned i;

	if(!org)
		return;

	for(i = org->pos; i < anum(org->buffs);) {
		struct norm_buff *tmp = org->buffs[i];
		org->buffs[i++] = NULL;
		recv_buff_free_system(tmp);
	}
#endif
}

/*
 * recv_buff_local_get - get a thread local recv_buff
 *
 */
struct norm_buff *recv_buff_local_get(void)
{
	struct norm_buff *ret_val;
#ifndef VALGRIND_ME_RECV
	struct local_buffer_org *org = pthread_getspecific(key2lrecv);

	if(!org) {
		org = recv_buff_init_lorg();
		if(!org)
			return recv_buff_alloc();
	}

	if(likely(org->pos < (int)anum(org->buffs)))
	{
		ret_val = org->buffs[org->pos];
		org->buffs[org->pos++] = NULL;
		logg_develd_old("allocating recv_buff local pos: %d\n", org->pos);
		ret_val->capacity = sizeof(ret_val->data);
		buffer_clear(*ret_val);
	}
	else
#endif
		ret_val = recv_buff_alloc();

	return ret_val;
}

/*
 * recv_buff_local_ret - return a thread local recv_buff
 */
void recv_buff_local_ret(struct norm_buff *buf)
{
#ifndef VALGRIND_ME_RECV
	struct local_buffer_org *org;

	if(!buf)
		return;

	org = pthread_getspecific(key2lrecv);
	if(!org)
	{
		org = recv_buff_init_lorg();
		if(!org) {
			recv_buff_free(buf);
			return;
		}
	}

	if(likely(org->pos > 0)) {
		org->buffs[--org->pos] = buf;
		logg_develd_old("freeing recv_buff local pos: %d\n", org->pos);
	}
	else
#endif
		recv_buff_free(buf);
}


static inline struct norm_buff *recv_buff_alloc_system(void)
{
	struct norm_buff *ret = malloc(sizeof(*ret));
	logg_devel_old("allocating recv_buffer from sys\n");
	if(ret) {
		ret->capacity = sizeof(ret->data);
		buffer_clear(*ret);
	}
	return ret;
}

noinline struct norm_buff *recv_buff_alloc(void)
{
#ifndef VALGRIND_ME_RECV
	int failcount = 0;
	struct norm_buff *ret_val = NULL;

	do
	{
		size_t i = 0;
		do
		{
			if(atomic_pread(&free_buffs[i]))
			{
				if((ret_val = atomic_pxa(ret_val, &free_buffs[i]))) {
					ret_val->capacity = sizeof(ret_val->data);
					buffer_clear(*ret_val);
					return ret_val;
				}
			}
		} while(++i < anum(free_buffs));
	} while(++failcount < 1);
#endif

	return recv_buff_alloc_system();
}

static inline void recv_buff_free_system(struct norm_buff *tf)
{
	logg_devel_old("freeing recv_buff to sys\n");
	free(tf);
}

void GCC_ATTR_FASTCALL recv_buff_free(struct norm_buff *ret)
{
#ifndef VALGRIND_ME_RECV
	int failcount = 0;

	if(!ret)
		return;

	buffer_clear(*ret);
	do
	{
		size_t i = 0;
		do
		{
			if(!atomic_pread(&free_buffs[i])) {
				if(!(ret = atomic_pxa(ret, &free_buffs[i])))
					return;
			}
		} while(++i < anum(free_buffs));
	} while(++failcount < 1);
#endif

	recv_buff_free_system(ret);
}

/*@unused@*/
static char const rcsid_rcb[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
