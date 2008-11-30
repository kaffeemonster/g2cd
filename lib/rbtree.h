/*
 * rbtree.h
 * red black tree template code
 *
 * Copyright (c) 2006,2008 Jan Seiffert
 *
 * thanks go out to:
 * http://eternallyconfuzzled.com
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
 * $Id:$
 */

#ifndef LIB_RBTREE_H
# define LIB_RBTREE_H
/*
 * Introduction/Usage
 *
 * What is this?
 * This is a framework to generate code, handling a red black tree
 * (Template like, this time, unfortunatly, in C)
 *
 *
 * UAAARGH, this macro mumbo-jumbo is ... disgusting!?
 * Jep! I also hate it, but is the only way to get both:
 *  - minimum for you/me to write -> minimum errors
 *  - nearly maximum performance by avoidance of callbacks/func-pointers
 *    etc. Those who want a clean version, can rewrite it (exercise left
 *    for the reader)...
 * In C++ this could be "cleanly" handled with template code, if you
 * think template code looks clean @)
 *
 *
 * Ok, i will survive it, bring it on!
 * When?
 * Everytime you think it would be handy to stuff your data in a tree,
 * more precisely a red black tree, for fast search/insert/remove.
 * ( O(log n), when tree is balanced, and rbtrees rebalance "themselve" ;)
 *
 *
 * What?
 * Everything?! As long as you are able to work with C (dynamic allocs,
 * structs, pointer, returnvalues, conventions etc.).
 * Maybe look at the "How"
 * Note: You can not intermix the datatypes you put in a tree, except
 * you handle it in your struct/functions.
 *
 *
 * How?
 * ___ Include ___ :
 * Include this header in your .c file. There you will build the code.
 * If you include this header into another header (because you need
 * the types), please be carefull, not to "build" code (read further on).
 *
 * ___ Attach ___ :
 * Stuff a "struct rbnode" in your data structure or generate a new one
 * consisting of a "struct rbnode" and your data. Ex:
 *
 * struct something {
 * 	int a, b;
 * 	struct rbnode rb;
 * };
 *
 * ____ Name ___:
 * Now you need a name. Any name is fine, but it:
 *  - must be conforming to C
 *  - should be meaningfull to you(r) (datatype you want to manage)
 *
 * For the example above, lets take "something" as name
 *
 * (this name is exactly !NOT! meaningfull,
 * but you will see, how this works out).
 *
 * ___ Helper functions ___ :
 * Now you need three helper functions. These functions are needed 
 * by the generated code, to handle the data in the tree in a meaningfull
 * way in resp. to the tree.
 * The function names are build from a constant part and the name you
 * choose. Best shown with an example:
 *
 * int  rbnode_something_eq(struct something *a, struct something *b);
 * int  rbnode_something_lt(struct something *a, struct something *b);
 * void rbnode_something_cp(struct something *dest, struct something *src);
 *
 * These are also the prototypes of these three (example) functions. As
 * you can see, all function names start with "rbnode_" (they work on
 * rbnodes, and this is the "Namespace", not to colide with/pollute other),
 * then comes the name choosen by you, and then a suffix to denote
 * their duty.
 *
 * You may now read on at "___ Declare ___" to see, how things work together.
 * To write these three functions read on here.
 *
 * Details on these functions:
 * _eq stands for "equal". It shall test a and b for equallity. How this
 * has to happen, depends on your data.
 * It returns true (or !/not/<> 0) if, and only if they are equal,
 * false otherwise (0). Ex:
 *
 * int rbnode_something_eq(struct something *a, struct something *b)
 * {
 * 	return a->a == b->a && a->b == b->b;
 * }
 *
 * _lt stands for "lesser then". It shall test a and b if a is smaller than b.
 * (Note: it should not test for <= !!)
 * It returns 1 iff a is smaller than b, 0 otherwise.
 * (!!! AND ONLY ONE OR ZERO !!! NOT TWO, NOT THREE NOR MINUS ONE. The
 * return value is used as an index in an array, if you return something
 * else then 1 or 0, you will fuck yourself gently with a chainsaw...). Ex:
 *
 * int rbnode_something_lt(struct something *a, struct something *b)
 * {
 * 	return a->a < b->a || (a->a == b->a && a->b < b->b);
 * }
 *
 *	NOTE: Those comparison functions shall be "stable", esp.
 *	the *_lt. While you can make funky things whith these
 *	exported functs (for ex. match an IP against a IP-range/netmask),
 *	be sure they match this criteria.
 * It is the same as with other similar interfaces (ex. qsort(3)). If you
 * alter fields within your data while comparing (you are free to do so, its
 * your data ;), you should rethink if your comparison is still stable.
 * If you alter the/a comparison field from the "outside", your doomed,
 * your tree is not sorted anymore.
 *
 * _cp stands for copy. It shall copy the content from src to dest, !!EXCLUDING!!
 * the "struct rbnode". Ex:
 *
 * void rbnode_something_cp(struct something *dest, struct something *src)
 * {
 * 	dest->a = src->a;
 * 	dest->b = src->b;
 *	}
 *
 * The helper functions should be declared "inline", maybe also "static"
 * if you don't need external linkage.
 *
 * ___ Declare ___:
 * Now we actully "build" the code. Ex Put in your .c:
 * RBTREE_MAKE_FUNCS(something, struct something, rb)
 * which comes from the define:
 * RBTREE_MAKE_FUNCS(name, datatype, datatype member)
 *
 * Here comes your choosen name, it will be put in the function names
 * (thats why they should be meaningfull and conforming to C).
 * The datatype you whant to handle/stuff in the tree (the functions will
 * get "strongly" typed for it, your helper will recieve this type).
 * The name of the "struct rbnode" member within this datatype. (so rbnode
 * doesn't have to be the first within a stuct or some other (fragile)
 * magic, it only has to be named ;)
 *
 * __Use__ :
 * Now you have four additional functions to insert/remove/search within a tree.
 *
 * static int               rbtree_something_insert(struct rbtree *, struct something *);
 * static struct something *rbtree_something_remove(struct rbtree *, struct something *);
 * static struct something *rbtree_somtheing_search(struct rbtree *, struct something *);
 * static struct something *rbtree_something_lowest(struct rbtree *);
 *
 * All four functions take a rbtree as first argument, it is the tree you
 * want to work on. The second argument denotes the data you want to work with.
 * _insert returns true if data was insertet, otherwise false.
 * _remove returns the entry removed (so it could be freed by you), or NULL.
 * _search returns a pointer on the node which matched the search, or NULL.
 * _lowest returns a pointer to the lowest, rightmost node, or NULL.
 *
 * If you want external linkage, you have to introduce proxy funcs,
 * which will propably provide a better name for you.
 *
 * Internaly they use your three helper function to maintain the tree,
 * and find something when you search. So maybe now would be the right
 * moment to go back to "___ Helper functions ___" and really write
 * these helper functions, or it wont work...
 *
 * NOTE: if you alter the fields used within your comparison (_lt, _eq)
 * in the tree from the outside, you are bound for trouble, since the
 * tree is not sorted anymore...
 *
 * NOTE: if you use different rbtree_{a,b,c} with different datatypes
 * on the same tree, you will bust it. Say hello to SIGSEGV. If you want
 * to stuff different datatypes in the same tree, you will have to
 * explicitly handle it yourself and create a kind of container, and
 * think about how to give them a proper sorting...
 *
 * NOTE: Locking (concurrent access) has to be handeld by the user.
 * _insert & _remove alter (write) the tree. Since both functions also
 * rebalance the tree, maybe more than one element gets written.
 * (Elaborate loocking with atomics and so on is not implemented do
 * to this fact, and since the tree-state has to be seen as inconsistent
 * till these operations finish)
 * _search only reads the tree (as long as your helper don't do funky things)
 *
 * NOTE: The functions are written to work NOT recursivly, to avoid
 * a stackoverflow even on large trees (and maybe a performance benefit).
 * This makes them maybe a bit harder to read (at least if you think of
 * recursiv tree handling as "the natural way", even with tree-rotations),
 * and maybe a little bit bigger. But since this is mostly "ready for use"
 * code, you shouldn't mind.
 *
 * ___ Example ___ :
 * Ok, first we need a tree. Ex:
 *
 * struct rbtree value_tree;
 *
 * now we need some data to work with. Ex:
 *
 * struct something *x = malloc(sizeof(*x));
 * if(!x)
 * 	die("No mem");
 * x->a = 1;
 * x->b = 2;
 *
 * now we can insert it. Ex:
 *
 * int ret = rbtree_something_insert(&value_tree, x);
 * if(!ret)
 * 	x was not insertet, handle it (and free x ;)
 *
 * we can now search. Ex:
 *
 * struct something y, *z;
 * y.a = 1;
 * y.b = 2;
 * z = rbtree_something_search(&value_tree, &y);
 *
 * or remove it. Ex:
 *
 * z = rbtree_something_remove(&value_tree, &y);
 * // y is not valid afterwards anymore
 * if(z)
 * 	free(z);
 *
 *
 */

# include "other.h"

enum rbdirection
{
	RB_LEFT = 0,
	RB_RIGHT = 1
};

enum rbcolor
{
	RB_BLACK,
	RB_RED
} GCC_ATTRIB_PACKED;

struct rbnode
{
	struct rbnode *child[2];
	enum rbcolor color;
};

struct rbtree
{
	struct rbnode *root;
};

# ifdef DEBUG_RBTREE_DOT
void print_tree_dot(struct rbnode *node);
struct rbtree mtree;
# else
#  define print_tree_dot(x) do { } while(0)
# endif

static inline struct rbnode *rbnode_rsingle(struct rbnode *root, int dir)
{
	struct rbnode *save = root->child[!dir];

//	printf("rotating %s\n", dir == RB_RIGHT ? "right" : "left");

	root->child[!dir] = save->child[dir];
	save->child[dir] = root;

	root->color = RB_RED;
	save->color = RB_BLACK;

	return save;
}

static inline struct rbnode *rbnode_rdouble (struct rbnode *root, int dir)
{
	root->child[!dir] = rbnode_rsingle(root->child[!dir], !dir);
	print_tree_dot(mtree.root);
	return rbnode_rsingle(root, dir);
}


static inline void rbnode_init(struct rbnode *node)
{
	node->child[RB_LEFT] = node->child[RB_RIGHT] = NULL;
	node->color = RB_RED;
}

static inline int IS_RED(struct rbnode *node)
{
	return node != NULL && node->color == RB_RED;
}

#define RBTREE_MAKE_FUNCS(fname, type, member) \
static inline int rbtree_##fname##_insert(struct rbtree *tree, type *data) \
{ \
	int ret_val = 0; \
	if(!tree) \
		return 0; \
	if(!data) \
		return 0; \
	rbnode_init(&data->member); \
	if(!tree->root) \
	{ /* empty tree */ \
		tree->root = &data->member; \
		ret_val = 1; \
	} \
	else \
	{ \
		struct rbnode head = {{NULL, NULL}, RB_BLACK}; /* fake tree root */ \
		struct rbnode *g, *t;     /* Grandparent & parent */ \
		struct rbnode *p, *q;     /* Iterator & parent */ \
		type *d;	/* grip to data */ \
		int dir = RB_LEFT, last = RB_LEFT; \
		t = &head; /* set up helpers */ \
		g = p = NULL; \
		q = t->child[RB_RIGHT] = tree->root; \
		while(1) \
		{ /* Search down the tree */ \
			if(!q) { /* Insert new node at the bottom */ \
				p->child[dir] = q = &data->member; \
				ret_val = 1; \
			} else if(IS_RED(q->child[RB_LEFT]) && \
			        IS_RED(q->child[RB_RIGHT])) \
			{ /* Color flip */ \
				q->color = RB_RED; \
				q->child[RB_LEFT]->color = RB_BLACK; \
				q->child[RB_RIGHT]->color = RB_BLACK; \
				print_tree_dot(mtree.root);\
			} \
			if(IS_RED(q) && IS_RED(p)) \
			{ /* Fix red violation */ \
				int dir2 = t->child[RB_RIGHT] == g; \
				if(q == p->child[last]) { \
					t->child[dir2] = rbnode_rsingle(g, !last); \
					print_tree_dot(mtree.root); \
				} else { \
				   t->child[dir2] = rbnode_rdouble(g, !last); \
				} \
			} \
			d = container_of(q, type, member); \
			if(rbnode_##fname##_eq(d, data)) /* stop if found */ \
				break; \
			last = dir; \
			dir = rbnode_##fname##_lt(d, data); \
			if(g != NULL) /* Update helpers */ \
				t = g; \
			g = p, p = q; \
			q = q->child[dir]; \
		} \
		tree->root = head.child[RB_RIGHT]; 	/* Update root */ \
	} \
	tree->root->color = RB_BLACK; /* Make root black */ \
	return ret_val; \
} \
static inline type *rbtree_##fname##_remove(struct rbtree *tree, type *data) \
{ \
	type *ret_val = NULL; \
	if(!tree) \
		return NULL; \
	if(!data) \
		return NULL; \
	if(tree->root) \
	{ \
		struct rbnode head = {{NULL, NULL}, RB_BLACK}; /* fake tree root */ \
		struct rbnode *q, *p, *g; /* Helpers */ \
		struct rbnode *f = NULL;  /* Found item */ \
		type *d; \
		int dir = RB_RIGHT; \
		q = &head; /* Set up helpers */ \
		g = p = NULL; \
		q->child[RB_RIGHT] = tree->root; \
		while(q->child[dir]) /* Search and push a red down */ \
		{ \
			int last = dir; \
			g = p, p = q; /* Update helpers */ \
			q = q->child[dir]; \
			d = container_of(q, type, member); \
			dir = rbnode_##fname##_lt(d, data); \
			if(rbnode_##fname##_eq(d, data)) /* Save found node */ \
				f = q; \
			if(!IS_RED(q) && !IS_RED(q->child[dir])) /* Push the red node down */ \
			{ \
				if(IS_RED(q->child[!dir])) \
					p = p->child[last] = rbnode_rsingle( q, dir); \
				else if(!IS_RED(q->child[!dir])) \
				{ \
					struct rbnode *s = p->child[!last]; \
					if(s) \
					{ \
						if(!IS_RED(s->child[!last]) && \
						   !IS_RED(s->child[last])) { \
							p->color = RB_BLACK; /* Color flip */ \
							s->color = RB_RED; \
							q->color = RB_RED; \
						} \
						else \
						{ \
							int dir2 = g->child[RB_RIGHT] == p; \
							if(IS_RED(s->child[last])) \
								g->child[dir2] = rbnode_rdouble(p, last); \
							else if(IS_RED(s->child[!last])) \
								g->child[dir2] = rbnode_rsingle(p, last); \
							q->color = g->child[dir2]->color = RB_RED; /* Ensure correct coloring */ \
							g->child[dir2]->child[RB_LEFT]->color = RB_BLACK; \
							g->child[dir2]->child[RB_RIGHT]->color = RB_BLACK; \
						} \
					} \
				} \
			} \
		} \
		if(f) /* Replace and remove if found */ \
		{ \
			type *d2 = container_of(f, type, member); \
			d = container_of(q, type, member); \
			rbnode_##fname##_cp(d2, d); \
			p->child[p->child[RB_RIGHT] == q] = q->child[q->child[RB_LEFT] == NULL]; \
			ret_val = container_of(q, type, member); \
		} \
		tree->root = head.child[RB_RIGHT]; /* Update root and make it black */ \
		if(tree->root) \
			tree->root->color = RB_BLACK; \
	} \
	return ret_val; \
} \
static inline type *rbtree_##fname##_search(struct rbtree *tree, type *data) \
{ \
	struct rbnode *t; \
	type *ret_val = NULL; \
	if(!tree) \
		return NULL; \
	if(!data) \
		return NULL; \
	if(!tree->root) \
		return NULL; \
	t = tree->root; \
	do \
	{ \
		type *d = container_of(t, type, member); \
		if(rbnode_##fname##_eq(d, data)) \
			ret_val = d; \
		else \
			t = t->child[rbnode_##fname##_lt(d, data)]; \
	} while(t && !ret_val); \
	return ret_val; \
} \
static inline type *rbtree_##fname##_lowest(struct rbtree *tree) \
{ \
	struct rbnode *q; \
	if(!tree) \
		return NULL; \
	if(!tree->root) \
		return NULL; \
	q = tree->root; \
	while(q->child[RB_RIGHT]) \
		q = q->child[RB_RIGHT]; \
	return container_of(q, type, member); \
}


#endif /* LIB_RBTREE_H */
