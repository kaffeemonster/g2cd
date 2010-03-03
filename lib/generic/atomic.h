/*
 * atomic.h
 * atomic primitves generic implementation
 *
 * Thanks Linux Kernel
 *
 * Copyright (c) 2006-2010 Jan Seiffert
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
 *
 */

#ifndef LIB_IMPL_ATOMIC_H
# define LIB_IMPL_ATOMIC_H

/*
 * this _should_ be save on every arch, as long as the compiler
 * respects volatile (and the arch is capable of loading from
 * memory...)
 * No, its not, weak memory order...
 */
# ifndef atomic_pread
#  define atomic_pread(x)	((x)->d)
# endif
# ifndef atomic_pset
#  define atomic_pset(x, y)	(((x)->d) = (y))
# endif
# ifndef atomic_read
#  define atomic_read(x)	((x)->d)
# endif
# ifndef atomic_set
#  define atomic_set(x, y)	(((x)->d) = (y))
# endif
# ifndef atomic_sread
#  define atomic_sread(x)	((x)->next)
# endif
# ifndef atomic_sset
#  define atomic_sset(x, y) ((x)->next = (y))
# endif

# ifndef atomic_inc
#  define atomic_inc(x)	gen_atomic_inc((x))
# endif
# ifndef atomic_inc_return
#  define atomic_inc_return(x)	gen_atomic_inc_return((x))
# endif
# ifndef atomic_dec
#  define atomic_dec(x)	gen_atomic_dec((x))
# endif
# ifndef atomic_dec_test
#  define atomic_dec_test(x) gen_atomic_dec_test((x))
# endif
# ifndef atomic_x
#  define atomic_x(x, y)	gen_atomic_x((x), (y))
# endif
# ifndef atomic_px
#  define atomic_px(x, y)	gen_atomic_px((x), (y))
# endif
# ifndef atomic_cmpx
#  define atomic_cmpx(x, y, z)	gen_atomic_cmpx((x), (y), (z))
# endif
# ifndef atomic_cmppx
#  define atomic_cmppx(x, y, z)	gen_atomic_cmppx((x), (y), (z))
# endif

/* if your arch is weakly ordered and smp, this will blow up */
# ifndef mb
#  define mb()	mbarrier()
# endif
# ifndef rmb
#  define rmb()	mbarrier()
# endif
# ifndef wmb
#  define wmb()	mbarrier()
# endif
# ifndef read_barrier_depends
#  define read_barrier_depends()	do { } while(0)
# endif

extern void gen_atomic_inc(atomic_t *);
extern int  gen_atomic_inc_return(atomic_t *);
extern void gen_atomic_dec(atomic_t *);
extern int  gen_atomic_dec_test(atomic_t *);
extern int  gen_atomic_x(int, atomic_t *);
extern void *gen_atomic_px(void *, atomicptr_t *);
extern int gen_atomic_cmpx(int nval, int oval, atomic_t *);
extern void *gen_atomic_cmppx(void *nval, void *oval, atomicptr_t *);

#endif /* LIB_IMPL_ATOMIC_H */
