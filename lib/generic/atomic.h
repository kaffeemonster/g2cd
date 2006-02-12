/*
 * atomic.h
 * atomic primitves generic implementation
 *
 * Thanks Linux Kernel
 * 
 * Copyright (c) 2006, Jan Seiffert
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
 * $Id:$
 * 
 */

#ifndef LIB_IMPL_ATOMIC_H
#define LIB_IMPL_ATOMIC_H

/*
 * this _should_ be save on every arch, as long as the compiler
 * respects volatile (and the arch is capable of loading from
 * memory...)
 * No, its not, weak memory order...
 */
# define atomic_pread(x)	((x)->d)
# define atomic_pset(x, y)	(((x)->d) = (y))
# define atomic_read(x)	((x)->d)
# define atomic_set(x, y)	(((x)->d) = (y))
# define atomic_sread(x)	((x)->next)
# define atomic_sset(x, y) ((x)->next = (y))

# define atomic_inc(x)	gen_atomic_inc((x))
# define atomic_dec(x)	gen_atomic_dec((x))
# define atomic_x(x, y)	gen_atomic_x((x), (y))
# define atomic_px(x, y)	gen_atomic_py((x), (y))
# define atomic_cmppx(x, y, z)	gen_atomic_cmppx((x), (y), (z))

extern void gen_atomic_inc(atomic_t *);
extern void gen_atomic_dec(atomic_t *);
extern int  gen_atomic_x(int, atomic_t *);
extern void *gen_atomic_px(void *, atomicptr_t *);
extern void *gen_atomic_cmppx(void *nval, void *oval, atomicptr_t *);

#endif /* LIB_IMPL_ATOMIC_H */
