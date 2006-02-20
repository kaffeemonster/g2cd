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
static union lists
{
	atomicst_t head;
	char _padding[64]; /* get padding between the atomic vars for ppc et al. */
} hzp_threads, hzp_freelist;
static atomic_t nr_free;

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
 * If thread wants to play with us, he needs a little storage
 * where we can put a list within.
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
	atomic_push(&hzp_threads.head, &new_hzp->lst);

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
 * When a chunk shared memory under hzp controll should be freed
 * (meaning you acquired a _seperate_ writer lock and installed
 * some kind of substitute: new mem or NULL, whatever the program
 * flow can accept), it can not be freed immediately, because some
 * thread maybe still work with it (thats why we track references).
 * 
 * So we put it in a "to Free" list, and when all references are
 * gone, we can finaly free it, with the supplied callback (the
 * freemethod).
 *
 * This has to be maintained by a cyclic collector (and has to be
 * written, ATM here is a nice mem leak ;)
 *
 * Nice sideffect: can be used as poormans gc. Drop in what you want,
 * someone else will actually free it, so the calling thread can save
 * the free method latency.
 * 
 * params: item - memory to manage the free list
 *         data - the mem to free (and the callback understands)
 *         free_func - the free callback, either libc free or 
 *                     your own allocator for this object.
 */
inline void hzp_deferfree(struct hzp_free *item, void *data, void (*free_func)(void *))
{
	item->data = data;
	item->free_func = free_func;
	atomic_push(&hzp_freelist.head, &item->st);
	atomic_inc(&nr_free);
	logg_develd("would free: %p\n", data);
}

/*
 * hzp_scan helper
 */
struct hzp_fs
{
	struct hzp_fs *next;
	void *ptr;
};

static inline void hzp_fs_push(struct hzp_fs *head, struct hzp_fs *new)
{
	new->next = head->next;
	head->next = new;
}

static inline bool hzp_fs_contains(struct hzp_fs *head, const void *data)
{
	while(head)
	{
		if(head->ptr == data)
			return true;
		head = head->next;
	}
	return false;
}

/*
 * hzp_scan - scan the freelist
 * 
 *
 */
inline int hzp_scan(int threshold)
{
	atomicst_t thead = ATOMIC_INIT(NULL), mhead = ATOMIC_INIT(NULL);
	atomicst_t *whead = NULL;
	struct hzp_fs *uhead = NULL;
	int freed = 0;

	if(threshold > atomic_read(&nr_free))
		return 0;

	/* get all to free entrys */
	atomic_pxs(&mhead.next, &hzp_freelist.head);
	atomic_set(&nr_free, 0);

	if(!atomic_sread(&mhead))
		return 0;

	/* gather list of used mem */
	atomic_sset(&thead, atomic_sread(&hzp_threads.head));
	whead = &thead;
	while(atomic_sread(whead))
	{
		int i;
		struct hzp *entry = container_of(deatomic(atomic_sread(whead)), struct hzp, lst);
		if(entry->flags.used)
		{
			for(i = 0; i < HZP_MAX; i++)
			{
				void *hptr = deatomic(entry->ptr[i]);
				if(hptr)
				{
					struct hzp_fs *tmp = alloca(sizeof(*tmp));
					tmp->ptr = hptr;
					hzp_fs_push(uhead, tmp);
				}
			}
			/* shut up, we want to travers the list... */
			whead = deatomic(atomic_sread(whead));
		}
		else
		{
			logg_develd("unused hzp entry: %p\t%p\t%p, trying to free....\n",
				(void *)whead, deatomic(atomic_sread(whead)), (void *)entry);
			free(atomic_pop(whead));
		}
	}

	/* check gathered list against our to-free list */
	while((whead = atomic_pop(&mhead)))
	{
		struct hzp_free *mentry = container_of(whead, struct hzp_free, st);
		if(hzp_fs_contains(uhead, mentry->data))
		{
			atomic_push(&hzp_freelist.head, whead);
			atomic_inc(&nr_free);
		}
		else
		{
			void (*tmp_free_func)(void *) = mentry->free_func;
			void *tmp_data = mentry->data;
			logg_develd("freeing %p\t%p\n", (void *)mentry, tmp_data);
			tmp_free_func(tmp_data);
			/* attention! *mentry is now freed and invalid */
			freed++;
		}
	}
	return freed;
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
	{
		struct hzp *tmp = (struct hzp *) dt_hzp;
		tmp->flags.used = false;
	}
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id: $";
