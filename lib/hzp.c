/*
 * hzp.c
 * hzp internal voodoo
 *
 * Copyright (c) 2006 Jan Seiffert
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
 * $Id: $
 */

#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#include "log_facility.h"
#include "atomic.h"
#include "hzp.h"
#include "../other.h"

struct hzp
{
	volatile void *ptr[HZP_MAX];
	struct hzp_flags
	{
		bool used;
	} flags;
	atomicst_t lst;
};

static pthread_key_t key2hzp;
static atomicst_t hzp_head;

/* Protos */
	/* You better not kill this proto, our it wount work ;) */
static void hzp_init(void) GCC_ATTR_CONSTRUCT;
static void hzp_free(void *dt_hzp);
static inline struct hzp *hzp_alloc_intern(void);

/*
 * hzp_init - init hzp
 * Get a TSD key and register our free callback for it.
 *
 * returns : 0 succes, < 0 failure
 */
static void hzp_init(void) 
{
	if(pthread_key_create(&key2hzp, hzp_free))
	{
		logg_errno(LOGF_EMERG, "couldn't create TLS key for hzp");
		exit(EXIT_FAILURE);
	}
}

/*
 * hzp_alloc - allocate mem and lay it in the TSD
 * A thread wonts to play with us, so he needs a little storage
 * and put within list.
 *
 * returns: true - everythings fine
 *          false - ooops
 */
inline bool hzp_alloc(void)
{
	if(pthread_getspecific(key2hzp))
		return true;

	return hzp_alloc_intern() ? true : false;
}

static inline struct hzp *hzp_alloc_intern(void)
{
	struct hzp *new_hzp = calloc(1, sizeof(*new_hzp));
	if(!new_hzp)
		return NULL;

	new_hzp->flags.used = true;
	atomic_push(&hzp_head, &new_hzp->lst);

	if(pthread_setspecific(key2hzp, new_hzp))
	{
		new_hzp->flags.used = false;
		logg_errno(LOGF_CRIT, "hzp key not initilised?");
		return NULL;
	}

	return new_hzp;
}

/*
 * hzp_ref - register a reference under a key for a subsystem
 *
 * this is the point of it all, whenever a thread wants to
 * _*read*_ access to shared mem of a subsystem denoted by
 * the key, he has to register the mem. something in the
 * fashion of:
 *		char *local_data;
 *		do
 *			hzp_ref(HZP_subsys, (local_data = shared_data));
 *		while(local_data != shared_data);
 *
 *	shared_data must be volatile, or the compiler would
 *	optimize the loop away, but the loop is essential.
 *
 *	ATM each thread can only hold _one_ reference for
 *	_each_ subsystem. (after hzp_alloc)
 *
 *	params: key - the subsystem you want
 *	        new_ref - the mem you want to reference
 */
inline void hzp_ref(enum hzps key, void *new_ref)
{
	struct hzp *t_hzp;

	if(!(t_hzp = pthread_getspecific(key2hzp)))
	{
		if(!(t_hzp = hzp_alloc_intern()))
		{
			logg_errno(LOGF_EMERG, "thread with no hzp, couldn't get one, were doomed!!");
			return;
		}
	}

	if(HZP_MAX <= key)
	{
		logg_errno(LOGF_CRIT, "thread with wrong hzp key?!");
		return;
	}

	t_hzp->ptr[key] = new_ref;
}

/*
 * hzp_unref - the oposite of hzp_ref.
 * When a thread is finished with its read_acces, it has to
 * unregister the mem, _after_ the last access.
 * If you not do this, you have a problem:
 *  a !! memory leak !! valgrind can _not_ find, since it will be
 *  reachable but from our POV not freeable, there is a reference
 *  to it...
 *
 *  params: key - the subsystem you mean
 */
inline void hzp_unref(enum hzps key)
{
	hzp_ref(key, NULL);
}

/*
 * hzp_deferfree - called for hzp freeing
 * When a chunk shared memory under hzp should be freed
 * (meaning your accuierd a _seperate_ writer lock and installed
 * some kind of substitute: new mem or NULL, whatever the program
 * flow can accept), it can not be freed immidiatly, because some
 * thread maybe still works with it (thats why we track references).
 * 
 * So we put it in a "to Free" list, and when all references are
 * gone, we can finaly free it, with the supplied callback (the
 * freemethod).
 *
 * This has to be maintained by a cyclic collector (and has to be
 * written, ATM here is a nice mem leak ;)
 *
 * params: data - the mem to free (and the callback understands)
 *         free_func - the free callback, either libc free or 
 *                     your own allocator for this object.
 */
inline void hzp_deferfree(void *data, void (*free_func)(void *))
{
	logg_develd("would free: %p\twith: %p\n", data, free_func);
// TODO: nice memory leak...
}

/*
 * hzp_free - thread exit call back from pthread_key interface
 * 
 * When a thread exits, this func gets called with its data
 * at the TSD entry this was registered for.
 * There should be a struct hzp, mark it unused, so a collector
 * can pick it up.
 *
 * params : dt_hzp - the mem associated with the just exiting thread
 */
static void hzp_free(void *dt_hzp)
{
	if(dt_hzp)
		((struct hzp *) dt_hzp)->flags.used = false;
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id: $";
