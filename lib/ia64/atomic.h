/*
 * atomic.h
 * atomic primitves for ia64
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

# define atomic_read(x)	((x)->d)
# define atomic_set(x, y)	(((x)->d) = (y))
# define atomic_pread(x)	((x)->d)
# define atomic_pset(x, y)	((x)->d = (y))
# define atomic_sread(x)	((x)->next)
# define atomic_sset(x, y)	((x)->next = (y))

# ifdef __INTEL_COMPILER
#  define atomic_px_32(val, ptr) _InterlockedExchange(&atomic_pread(ptr), val)
#  define atomic_px_64(val, ptr) _InterlockedExchange64(&atomic_pread(ptr), val)
# else
static always_inline void *atomic_px_32(void *val, atomicptr_t *ptr)
{
	void *ret;
	__asm__ __volatile__(
		"xchg4		%0=[%2], %3"
		: /* %0 */ "=r" (ret),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "r" (&atomic_pread(ptr)),
		  /* %3 */ "r" (val));
	return ret;
}

static always_inline void *atomic_px_64(void *val, atomicptr_t *ptr)
{
	void *ret;
	__asm__ __volatile__(
		"xchg8		%0=[%2], %3"
		: /* %0 */ "=r" (ret),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "r" (&atomic_pread(ptr)),
		  /* %3 */ "r" (val));
	return ret;
}
# endif /* __INTEL_COMPILER */

extern void *_illigal_ptr_size(void *,atomicptr_t *);
static always_inline void *atomic_px(void *val, atomicptr_t *ptr)
{
	switch(sizeof(val))
	{
	case 4:
		return atomic_px_32(val, ptr);
	case 8:
		return atomic_px_64(val, ptr);
	}
	return _illigal_ptr_size(val, ptr);
}

# ifdef __INTEL_COMPILER
#  define atomic_x_32(val, ptr) _InterlockedExchange(&atomic_read(ptr), val)
#  define atomic_x_64(val, ptr) _InterlockedExchange64(&atomic_read(ptr), val)
# else
static always_inline int atomic_x_32(int val, atomic_t *ptr)
{
	int ret;
	__asm__ __volatile__(
		"xchg4		%0=[%2], %3"
		: /* %0 */ "=r" (ret),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */  "=m" (atomic_read(ptr))
		: /* %2 */ "r" (&atomic_read(ptr)),
		  /* %3 */ "r" (val));
	return ret;
}

static always_inline int atomic_x_64(int val, atomic_t *ptr)
{
	int ret;
	__asm__ __volatile__(
		"xchg8		%0=[%2], %3"
		: /* %0 */ "=r" (ret),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "r" (&atomic_read(ptr)),
		  /* %3 */ "r" (val));
	return ret;
}
# endif /* __INTEL_COMPILER */

extern int _illigal_int_size(int, atomic_t *);
static always_inline int atomic_x(int val, atomic_t *ptr)
{
	switch(sizeof(val))
	{
	case 4:
		return atomic_x_32(val, ptr);
	case 8:
		return atomic_x_64(val, ptr);
	}
	return _illigal_int_size(val, ptr);
}

/*
 * For the moment, we just build "acquiring" cmpxchg,
 * because we need them for atomic ops, not syncronisation primitives
 *
 */

# ifdef __INTEL_COMPILER
#  define atomic_cmpx_32(nval, oval, ptr) _InterlockedCompareExchange_acq(&atomic_read(ptr), nval, oval)
#  define atomic_cmpx_64(nval, oval, ptr) _InterlockedCompareExchange64_acq(&atomic_read(ptr), nval, oval)
# else
static always_inline int atomic_cmpx_32(int nval, int oval, atomic_t *ptr)
{
	int res;
	__asm__ __volatile__(
		/*
		 * we could let gcc load the application register, but we must avoid
		 * that the mov and the cmpxchg end up in the same instruction group
		 */
		"mov		ar.ccv=%4;;\n"
		"cmpxchg4.acq	%0=[%2], %3, ar.ccv"
		: /* %0 */ "=r" (res),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "r" (&atomic_read(ptr)),
		  /* %3 */ "r" (nval),
		  /* %4 */ "r" (oval)
		: "ar.ccv" /* M-Unit AR32 */ );
	return res;
}

static always_inline int atomic_cmpx_64(int nval, int oval, atomic_t *ptr)
{
	int res;
	__asm__ __volatile__(
		/*
		 * we could let gcc load the application register, but we must avoid
		 * that the mov and the cmpxchg end up in the same instruction group
		 */
		"mov		ar.ccv=%4;;\n"
		"cmpxchg8.acq	%0=[%2], %3, ar.ccv"
		: /* %0 */ "=r" (res),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "r" (&atomic_read(ptr)),
		  /* %3 */ "r" (nval),
		  /* %4 */ "r" (oval)
		: "ar.ccv" /* M-Unit AR32 */ );
	return res;
}
# endif /* __INTEL_COMPILER */

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

# ifdef __INTEL_COMPILER
#  define atomic_cmppx_32(nval, oval, ptr) _InterlockedCompareExchange_acq(&atomic_pread(ptr), nval, oval)
#  define atomic_cmppx_64(nval, oval, ptr) _InterlockedCompareExchange64_acq(&atomic_pread(ptr), nval, oval)
# else
static always_inline void *atomic_cmppx_32(void *nval, void *oval, atomicptr_t *ptr)
{
	void *res;
	__asm__ __volatile__(
		/*
		 * we could let gcc load the application register, but we must avoid
		 * that the mov and the cmpxchg end up in the same instruction group
		 */
		"mov		ar.ccv=%4;;\n"
		"cmpxchg4.acq	%0=[%2], %3, ar.ccv"
		: /* %0 */ "=r" (res),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "r" (&atomic_pread(ptr)),
		  /* %3 */ "r" (nval),
		  /* %4 */ "r" (oval)
		: "ar.ccv" /* M-Unit AR32 */ );
	return res;
}

static always_inline void *atomic_cmppx_64(void *nval, void *oval, atomicptr_t *ptr)
{
	void *res;
	__asm__ __volatile__(
		/*
		 * we could let gcc load the application register, but we must avoid
		 * that the mov and the cmpxchg end up in the same instruction group
		 */
		"mov		ar.ccv=%4;;\n"
		"cmpxchg8.acq	%0=[%2], %3, ar.ccv"
		: /* %0 */ "=r" (res),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "r" (&atomic_pread(ptr)),
		  /* %3 */ "r" (nval),
		  /* %4 */ "r" (oval)
		: "ar.ccv" /* M-Unit AR32 */ );
	return res;
}
# endif /* __INTEL_COMPILER */

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

static always_inline int _atomic_add_return(int val, atomic_t *ptr)
{
	int res;
	do
		res = atomic_read(ptr);
	while(atomic_cmpx(res + val, res, ptr) != res);
	return res;
}

static always_inline int _atomic_sub_return(int val, atomic_t *ptr)
{
	int res;
	do
		res = atomic_read(ptr);
	while(atomic_cmpx(res - val, res, ptr) != res);
	return res;
}

# ifdef __INTEL_COMPILER
#  define atomic_fetch_and_add_32(val, ptr) __fetchadd4_acq(&atomic_read(ptr), val)
#  define atomic_fetch_and_add_64(val, ptr) __fetchadd8_acq((&atomic_read(ptr), val)
# else
static always_inline int atomic_fetch_and_add_32(int val, atomic_t *ptr)
{
	int res;
	__asm__ __volatile__(
		"fetchadd4.acq	%0=[%2], %3"
		: /* %0 */ "=r" (res),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "r" (&atomic_pread(ptr)),
		  /* %3 */ "i" (val));
	return res;
}

static always_inline int atomic_fetch_and_add_64(int val, atomic_t *ptr)
{
	int res;
	__asm__ __volatile__(
		"fetchadd8.acq	%0=[%2], %3"
		: /* %0 */ "=r" (res),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "r" (&atomic_pread(ptr)),
		  /* %3 */ "i" (val));
	return res;
}
# endif /* __INTEL_COMPILER */

//TODO: I think this must be all substituted by macros...
static always_inline int atomic_fetch_and_add(int val, atomic_t *ptr)
{
	switch(sizeof(val))
	{
	case 4:
		return atomic_fetch_and_add_32(val, ptr);
	case 8:
		return atomic_fetch_and_add_64(val, ptr);
	}
	return _illigal_int_size(val, ptr);
}

# define atomic_add_return(val, ptr) \
(__builtin_constant_p(val) \
&&	(	((val) ==  1) || ((val) ==   4) \
	||	((val) ==  8) || ((val) ==  16) \
	|| ((val) == -1) || ((val) ==  -4) \
	|| ((val) == -8) || ((val) == -16))) \
? atomic_fetch_and_add((val), (ptr)) \
: _atomic_add_return((val), ptr)
	
# define atomic_sub_return(val, ptr) \
(__builtin_constant_p(val) \
&&	(	((val) ==  1) || ((val) ==   4) \
	||	((val) ==  8) || ((val) ==  16) \
	|| ((val) == -1) || ((val) ==  -4) \
	|| ((val) == -8) || ((val) == -16))) \
? atomic_fetch_and_add(-(val), (ptr)) \
: _atomic_sub_return((val), ptr)


# define atomic_inc(x) (atomic_add_return(1, (x)))
# define atomic_inc_return(x) (atomic_add_return(1, (x)))
# define atomic_dec(x) (atomic_sub_return(1, (x)))
# define atomic_dec_test(x) (atomic_sub_return(1, (x)) == 0)

#endif /* LIB_IMPL_ATOMIC_H */
