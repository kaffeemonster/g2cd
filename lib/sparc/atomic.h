/*
 * atomic.h
 * atomic primitves for sparc/sparc64
 *
 * Copyright (c) 2006-2010 Jan Seiffert
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
 * Thanks go to the Linux Kernel for the ideas
 *
 * $Id:$
 */

#ifndef LIB_IMPL_ATOMIC_H
# define LIB_IMPL_ATOMIC_H

/* membars, anyone? */
# define atomic_read(x)	((x)->d)
# define atomic_set(x, y) (((x)->d) = (y))
# define atomic_pread(x)	((x)->d)
# define atomic_pset(x, y) ((x)->d = (y))
# define atomic_sread(x)	((x)->next)
# define atomic_sset(x, y) ((x)->next = (y))

/*
 * gcc sets __sparcv8 even if you say "gimme v9" to not confuse solaris
 * tools and signal "32 bit mode". So how to detect a real v9 to do
 * v9ish stuff, mister sun? Great tennis! This will Bomb on a real v8
 * (maybe not a v8+) ...
 */
/* according to a manual i have even v7 had swap */
# if defined(__sparcv7) || defined(__sparc_v7__) || defined(__sparcv8) || defined(__sparc_v8__) || defined(__sparcv9) || defined(__sparc_v9__)
/* could make problems on _real_ old Fujitsu build sparcs */
#  define atomic_px atomic_px
#  define atomic_x atomic_x
static always_inline void *atomic_px_32(void *val, atomicptr_t *ptr)
{
	__asm__ __volatile__(
		"swap	%2, %0"
		: /* %0 */ "=&r" (val),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "m" (atomic_pread(ptr)),
		  /* %3 */ "0" (val));
	return val;
}

#  if defined(__sparcv9) || defined(__sparc_v9__)
static always_inline void *atomic_cmppx_64(void *, void *, atomicptr_t *);
static always_inline void *atomic_px_64(void *val, atomicptr_t *ptr)
{
	void *res;
	do
		res = atomic_pread(ptr);
	while(atomic_cmppx_64(val, res, ptr) != res);
	return res;
}
#  endif

extern void *_illigal_ptr_size(void *,atomicptr_t *);
static always_inline void *atomic_px(void *val, atomicptr_t *ptr)
{
	switch(sizeof(val))
	{
	case 4:
		return atomic_px_32(val, ptr);
#  if defined(__sparcv9) || defined(__sparc_v9__)
	case 8:
		return atomic_px_64(val, ptr);
#  endif
	}
	return _illigal_ptr_size(val, ptr);
}

// TODO: hmmm, swap deprecatet on sparc v9?
static always_inline int atomic_x_32(int val, atomic_t *ptr)
{
	__asm__ __volatile__(
		"swap	%2, %0"
		: /* %0 */ "=&r" (val),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */  "=m" (atomic_read(ptr))
		: /* %2 */ "m" (atomic_read(ptr)),
		  /* %3 */ "0" (val));
	return val;
}

#  if defined(__sparcv9) || defined(__sparc_v9__)
static always_inline int atomic_cmpx_64(int, int, atomic_t *);
static always_inline int atomic_x_64(int val, atomic_t *ptr)
{
	int res;
	do
		res = atomic_read(ptr);
	while(atomic_cmpx_64(val, res, ptr) != res);
	return res;
}
#  endif

extern int _illigal_int_size(int, atomic_t *);
static always_inline int atomic_x(int val, atomic_t *ptr)
{
	switch(sizeof(val))
	{
	case 4:
		return atomic_x_32(val, ptr);
#  if defined(__sparcv9) || defined(__sparc_v9__)
	case 8:
		return atomic_x_64(val, ptr);
#  endif
	}
	return _illigal_int_size(val, ptr);
}

#  if !defined(__sparcv9) && !defined(__sparc_v9__) && !defined(HAVE_REAL_V9)
    /* stbar any one? Also avail on v8  */
#   define wmb()	asm volatile("stbar" ::: "memory")
#   undef LIB_IMPL_ATOMIC_H
#   include "../generic/atomic.h"
#  else
#   ifdef HAVE_SMP
#    define MEMBAR_1	"membar #StoreLoad | #LoadLoad\n\t"
#    define MEMBAR_2	"membar #StoreLoad | #StoreStore"
/*
 * work around spitfire erratum:
 * The chip locks up itself till the next trap (interrupt) when a membar
 * is executed shortly after a misprediced branch.
 * Prevent that by putting the membar into the delay slot of a branch always
 */
#   define sparc_membar_safe(type) \
do {	asm volatile("ba,pt	%%xcc, 1f\n\t" \
		     " membar	" type "\n" \
		     "1:\n" \
		     ::: "memory"); \
} while (0)
/*
 * Sparcs are mostly used in TSO, so they would not need membars.
 * But there are the evil twins RMO and PSO. If you do not have some
 * membars for breakfast they will kick you in the nuts.
 * At least some doc said somewhere that in TSO membars are nops...
 */
#    define mb()	membar_safe("#StoreLoad|#LoadStore|#StoreStore|#LoadLoad")
#    define rmb()	membar_safe("#StoreLoad|#LoadLoad")
#    define wmb()	membar_safe("#LoadStore|#StoreStore")
#   else
#    define MEMBAR_1
#    define MEMBAR_2
#    define mb()	mbarrier()
#    define rmb()	mbarrier()
#    define wmb()	mbarrier()
#   endif
#   define read_barrier_depends()	do { } while(0)

static always_inline int atomic_cmpx_32(int nval, int oval, atomic_t *ptr)
{
	__asm__ __volatile__(
		MEMBAR_1
		"cas	%3, %2, %0\n\t"
		MEMBAR_2
		: /* %0 */ "=&r" (nval),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "r" (oval),
		  /* %3 */ "m" (atomic_read(ptr)),
		  /* %4 */ "0" (nval));
	return nval;
}

static always_inline int atomic_cmpx_64(int nval, int oval, atomic_t *ptr)
{
	__asm__ __volatile__(
		MEMBAR_1
		"casx	%3, %2, %0\n\t"
		MEMBAR_2
		: /* %0 */  "=&r" (nval),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "r" (oval),
		  /* %3 */ "m" (atomic_read(ptr)),
		  /* %4 */ "0" (nval));
	return nval;
}

static always_inline int atomic_cmpx(int nval, int oval, atomic_t *ptr)
{
	switch(sizeof(nval))
	{
	case 4:
		return atomic_cmpx_32(nval, oval, ptr);
	case 8:
		return atomic_cmpx_64(nval, oval, ptr);
	}
	return _illigal_int_size(nval, ptr);
}

static always_inline void *atomic_cmppx_32(void *nval, void *oval, atomicptr_t *ptr)
{
	void *prev;
	__asm__ __volatile__(
		MEMBAR_1
		"cas	%3, %2, %0\n\t"
		MEMBAR_2
		: /* %0 */ "=&r" (prev),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "r" (oval),
		  /* %3 */ "m" (atomic_pread(ptr)),
		  /* %4 */ "0" (nval));
	return prev;
}

// TODO: gcc happiliy chooses the 64 bit variant, but issues ld not ldx loads, sizeof gone berserk? BUG?
static always_inline void *atomic_cmppx_64(void *nval, void *oval, atomicptr_t *ptr)
{
	void *prev;
	__asm__ __volatile__(
		MEMBAR_1
		"casx	%3, %2, %0\n\t"
		MEMBAR_2
		: /* %0 */ "=&r" (prev),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "r" (oval),
		  /* %3 */ "m" (atomic_pread(ptr)),
		  /* %4 */ "0" (nval));
	return prev;
}

static always_inline void *atomic_cmppx(void *nval, void *oval, atomicptr_t *ptr)
{
	switch(sizeof(nval))
	{
	case 4:
		return atomic_cmppx_32(nval, oval, ptr);
	case 8:
		return atomic_cmppx_64(nval, oval, ptr);
	}
	return _illigal_ptr_size(nval, ptr);
}

static always_inline int atomic_add_return(int val, atomic_t *ptr)
{
	int res;
	do
		res = atomic_read(ptr);
	while(atomic_cmpx(res + val, res, ptr) != res);
	return res;
}

static always_inline int atomic_sub_return(int val, atomic_t *ptr)
{
	int res;
	do
		res = atomic_read(ptr);
	while(atomic_cmpx(res - val, res, ptr) != res);
	return res;
}

#   define atomic_inc(x) ((void) atomic_add_return(1, (x)))
#   define atomic_inc_return(x) (atomic_add_return(1, (x)))
#   define atomic_dec(x) ((void) atomic_sub_return(1, (x)))
#   define atomic_dec_test(x) (atomic_sub_return(1, (x)) == 0)
#  endif /* sparc_v9 */
# endif /* spac_v7 || sparc_v8 || sparc_v9 */

#endif /* LIB_IMPL_ATOMIC_H */
