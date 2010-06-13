/*
 * hlist.h
 * Header for a double link list with single pointer list head
 *
 * blatantly ripped out of linux/list.h
 * unfortunatly, it does not contain a copyright
 *
 * $Id: $
 */

#ifndef LIB_HLIST_H
# define LIB_HLIST_H

# include "other.h"
# define HLIST_POISON1	((void *)0xAAAAAAAA)
# define HLIST_POISON2	((void *)0x55555555)

/*
 * Double linked lists with a single pointer list head.
 * Mostly useful for hash tables where the two pointer list head is
 * too wasteful.
 * You lose the ability to access the tail in O(1).
 */

struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};

# define HLIST_HEAD_INIT { .first = NULL }
# define HLIST_HEAD(name) struct hlist_head name = { .first = NULL }
# define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
	h->next = NULL;
	h->pprev = NULL;
}

/* ----------------	ADD	---------------- */
static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node *first = h->first;
	n->next = first;
	if (first)
		first->pprev = &n->next;
	h->first = n;
	n->pprev = &h->first;
}

/* next must be != NULL */
static inline void hlist_add_before(struct hlist_node *n,
					struct hlist_node *next)
{
	n->pprev = next->pprev;
	n->next = next;
	next->pprev = &n->next;
	*(n->pprev) = n;
}

static inline void hlist_add_after(struct hlist_node *n,
					struct hlist_node *next)
{
	next->next = n->next;
	n->next = next;
	next->pprev = &n->next;

	if(next->next)
		next->next->pprev  = &next->next;
}

/* ----------------	REMOVE	---------------- */
static inline void __hlist_del(struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;
	*pprev = next;
	if (next)
		next->pprev = pprev;
}

static inline void hlist_del(struct hlist_node *n)
{
	__hlist_del(n);
	n->next = LIST_POISON1;
	n->pprev = LIST_POISON2;
}

/* ----------------	LOGIC	---------------- */
static inline int hlist_unhashed(const struct hlist_node *h)
{
	return !h->pprev;
}

static inline int hlist_empty(const struct hlist_head *h)
{
	return !h->first;
}

/* ----------------	HELPER	---------------- */
# define hlist_entry(ptr, type, member) container_of(ptr,type,member)

# define hlist_for_each(pos, head) \
	for(pos = (head)->first; pos && ({ prefetch(pos->next); 1; }); \
	    pos = pos->next)

# define hlist_for_each_safe(pos, n, head) \
	for(pos = (head)->first; pos && ({ n = pos->next; 1; }); \
	    pos = n)

/**
 * hlist_for_each_entry	- iterate over list of given type
 * @tpos:	the type * to use as a loop cursor.
 * @pos:	the &struct hlist_node to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the hlist_node within the struct.
 */
# define hlist_for_each_entry(tpos, pos, head, member) \
	for(pos = (head)->first; \
	    pos && ({ prefetch(pos->next); 1;}) && \
	    ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
	    pos = pos->next)

/**
 * hlist_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @tpos:	the type * to use as a loop cursor.
 * @pos:	the &struct hlist_node to use as a loop cursor.
 * @n:		another &struct hlist_node to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the hlist_node within the struct.
 */
# define hlist_for_each_entry_safe(tpos, pos, n, head, member) \
	for(pos = (head)->first; \
	    pos && ({ n = pos->next; 1; }) && \
	    ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
	    pos = n)

#endif
