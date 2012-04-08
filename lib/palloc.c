/*
 * palloc.c
 * Allocator for the pad
 *
 * Copyright (c) 2012 Jan Seiffert
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
 * $Id:$
 */

/*
 * Shamelessly plugged from the Paper:
 * "Efficient Implementation of the First-Fit Strategy for Dynamic
 * Storage Allocation" by R.P. Brent from the Australian National
 * University (c) 1989.
 *
 * Addapted from the given Pascal code by me.
 */

/*
 * This allocator mainly serves zlib, but also some other small
 * allocs.
 *
 * !! THIS heap is NOT locked !!
 * Why: it should be thread local.
 *
 * History:
 * normaly zlib inflate makes two allocs, one seems to be the
 * internal state (+9k on 32bit *cough*) and the other is the
 * window, norm. 32k for 15 bit.
 *
 * So this was a very stupid alloactor to serve these two allocs.
 * But, not to be too dependent on these internals (future zlib
 * versions) it was at least so intelligent it may also handle 3
 * allocs, and frees ... maybe.
 *
 * And since a deflater is very memory hungry, one time 6k,
 * three (!!!) times 32k * 2 and one time 16k * 4, total over
 * 260k, with a little more space in the heap we also can handle
 * a deflater.
 *
 * Since there already was an simple allocator, other small
 * uses could also be found.
 *
 * But there was a problem: there was a bug in the free block
 * merging. And fixing it was non-tivial.
 *
 * So this is a replacement by a full allocator.
 * Any simple allocator could be used for this.
 */

#define PALLOC_C
#ifdef HAVE_CONFIG_H
# include "../config.h"
#endif
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include "palloc.h"

typedef uint32_t pa_uaddress;

/* return d.s + index of segment containing address p */
static pa_exseg blseg(struct d_heap *d, pa_address p)
{
	return d->s + (p / d->wordsperseg);
}

/*
 * Doubles number of segments actually in use
 * assuming current number is s < PA_SMAX / 2
 */
static void bldouble(struct d_heap *d)
{
	pa_seg i, k;

	for(i = d->s; i < 2 * d->s; i++)
		d->pa[i] = PA_W;
	for(k = d->s; k != 0; k /= 2)
	{
		for(i = 0; i < k; i++) {
			d->st[2 * k + i] = d->st[k + i];
			d->st[3 * k + i] = 0;
		}
	}
	d->st[1] = d->st[2];
	d->s = 2 * d->s;
}

/*
 * Does housekeeping necessary if a block with
 * control word v[p] will be allocated or merged
 * with a block on it's left in a different segment
 */
static void blfix1(struct d_heap *d, pa_address p)
{
	pa_address sister, mx, pj;
	int pn;
	pa_exseg j;

	j = blseg(d, p);
	if(d->v[p] <= 0 || d->st[j] <= d->v[p])
	{
		pj = d->pa[j - d->s]; /* index of first block in segment */
		pn = (j + 1 - d->s) * d->wordsperseg; /* start of next segment */
		if((pa_uaddress)pn > PA_W)
			pn = PA_W; /* handle last segment */
		if(pj < pn)
		{
			mx = 1;
			do {
				mx = mx < d->v[pj] ? d->v[pj] : mx;
				pj = pj + abs(d->v[pj]);
			} while(pj < pn);
		}
		else
			mx = 0; /* no block starting in this segment */
		for(d->st[0] = 0; d->st[j] > mx; j /= 2) {
			d->st[j] = mx;
			sister = j & 1 ? d->st[j - 1] : d->st[j + 1];
			mx = mx < sister ? sister : mx;
		}
	}
}

/*
 * Does housekeeping necessary after a block with
 * control word v[p] is freed or merged with a
 * block on its right or created by splitting
 * (with p on the right of the split)
 */
static void blfix2(struct d_heap *d, pa_address p)
{
	pa_address vp;
	pa_exseg j;

	for(j = blseg(d, p); j >= 2 * d->s; j = blseg(d, p))
		bldouble(d);
	if(d->pa[j - d->s] > p)
		d->pa[j - d->s] = p;
	vp = d->v[p];
	for(d->st[0] = vp; d->st[j] < vp; j /= 2)
		d->st[j] = vp; /* go up branch of segment tree */
}

/*
 * Returns predecessor of block p (always exists
 * because of dummy first block). p and blpred
 * are indexes of control words
 */
static pa_address blpred(struct d_heap *d, pa_address p)
{
	pa_address q, qn;
	pa_exseg j;

	j = blseg(d, p);
	if(d->pa[j - d->s] == p)
	{ /* find rightmost nonempty segment to left */
		while(d->st[j - 1] == 0)
			j /= 2;
		j = j - 1;
		while(j < d->s)
			j = d->st[2 * j + 1] > 0 ? 2 * j + 1 : 2 * j;
	}
	qn = d->pa[j - d->s];
	do { /* follow chain until reach p */
		q = qn;
		qn += abs(d->v[qn]);
	} while(qn != p);
	return q;
}


/*
 * Returns index of a block of at least size words,
 * or 0 if no such block exists.
 * Note that size excludes control word for block and
 * may be zero. blinit must be called to perform initialization
 * before calling blnew.
 */
static pa_address blnew (struct d_heap *d, size_t size)
{
	pa_address n, p, vp;
	pa_exseg j;
	bool fix1;

	if((pa_uaddress)d->st[1] <= size)
		return 0; /* size too large */
	else
	{ /* find first segment containing large enough free block */
		n = size + 1; /* n is length including control word */
		j = 1;
		while(j < d->s)
			j = d->st[2 * j] >= n ? 2 * j : 2 * j + 1;
		p = d->pa[j - d->s]; /* index of control word of first block in segment j */
		while(d->v[p] < n)
			p = p + abs(d->v[p]);
		/* Now p is index of control word of required block */
		vp = d->v[p];
		d->v[p] = -n; /* flag block as allocated */
		fix1 = (vp == d->st[j]); /* blfix2 may change st[j] */
		if(vp > n)
		{ /* split block */
			d->v[p + n] = vp - n;
			if(blseg(d, p + n) > j)
				blfix2(d, p + n); /* necessary as p + n in another segment */
		}
		if(fix1)
			blfix1(d, p); /* necessary to update st here */
		return p + 1; /* return index of first word after control word */
	}
}

/*
 * Disposes of a block obtained using blnew. va is the index that was
 * returned by blnew.
 */
static void bldisp (struct d_heap *d, pa_address va)
{
	pa_address p, pr;
	pa_exseg j, jn;
	ssize_t pn, vp;

	p = va - 1; /* index of control word */
	vp = -d->v[p]; /* block already free if d.v[p] > 0 */
	if(vp > 0)
	{
		d->v[p] = vp; /* flag as free */
		j = blseg (d, p);
		pn = p + vp;
		if((pa_uaddress)pn < PA_W && d->v[pn] >= 0)
		{ /* next block free, so merge */
			d->v[p] = vp + d->v[pn];
			jn = blseg(d, pn);
			if(jn > j) {
				d->pa[jn - d->s] = p + d->v[p];
				blfix1(d, pn);
			}
		}
		pr = blpred (d, p); /* preceding block */
		if(d->v[pr] >= 0)
		{ /* preceding block free, so merge */
			d->v[pr] = d->v[pr] + d->v[p];
			if(d->pa[j - d->s] == p) { /* adjust pointer to first block in segment j */
				d->pa[j - d->s] = pr + d->v[pr];
				blfix1 (d, p);
			}
			blfix2(d, pr);
		}
		else if(d->v[p] > d->st[j])
			blfix2(d, p);
	}
}

/*
 * return size of a block allocated by call to blnew
 */
/*
static int blsize(struct d_heap *d, pa_address va)
{
	if(d->v[va - 1] <= 0)
		die("");
	return -(d->v[va - 1] + 1);
}
*/

void *pa_alloc(void *opaque, unsigned int num, unsigned int size)
{
	struct d_heap *d;
	size_t bsize;
	pa_address resa;

	if(!opaque || size > UINT_MAX / num)
		return NULL;
	d = opaque;
	bsize = num * size;
	bsize = (bsize + sizeof(ssize_t) - 1) / sizeof(ssize_t);

	/* start lock */
	resa = blnew(d, bsize);
	/* end lock */
	return &d->v[resa];
}

void pa_free(void *opaque, void *addr)
{
	struct d_heap *d;
	ssize_t *x;
	pa_address y;

	if(!addr || !opaque)
		return;
	d = opaque;
	x = addr;
	if(x < d->v || x > &d->v[PA_W])
		return;
	y = x - d->v;

	/* start lock */
	bldisp(d, y);
	/* end lock */
}

/*
 * initialize segment tree, etc.
 * Must be called before calling any other bl func
 */
void pa_init(struct d_heap *d)
{
	d->s = 1; /* may be increased by bldouble */
	d->st[1] = PA_W;
	d->pa[0] = 0;
	d->v[0] = PA_W; /* one large free block v [1 ... PA_W - 1] */
	d->wordsperseg = (PA_W / PA_SMAX) + 1;
	blnew(d, 0); /* create dummy block as sentinel */
}

