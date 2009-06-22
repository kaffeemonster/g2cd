/*
 * atomic.h
 * atomic primitves for x86
 *
 * Thanks Linux Kernel
 *
 * Copyright (c) 2006-2008 Jan Seiffert
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
# define atomic_set(x, y) (((x)->d) = (y))
# define atomic_pread(x)	((x)->d)
# define atomic_pset(x, y) ((x)->d = (y))
# define atomic_sread(x)	((x)->next)
# define atomic_sset(x, y) ((x)->next = (y))

# ifdef HAVE_SMP
#  define LOCK "lock\n"
# else
#  define LOCK
# endif

/*
 * Make sure to avoid asm operant size suffixes, this is
 * used in 32Bit & 64Bit
 *
 * %z should give operandsize (see ppc/atomic.h rant), don't
 * know since when...
 */

static always_inline void *atomic_px(void *val, atomicptr_t *ptr)
{
	__asm__ __volatile__(
		"xchg	%0, %2"
		: /* %0 */ "=r" (val),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "m"	(atomic_pread(ptr)),
		  /* %3 */ "0" (val));
	return val;
}

static always_inline int atomic_x(int val, atomic_t *ptr)
{
	__asm__ __volatile__(
		"xchg	%0, %2"
		: /* %0 */ "=r" (val),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "m"	(atomic_read(ptr)),
		  /* %3 */ "0" (val));
	return val;
}

# if defined(__GNUC__) && !defined(__clang__)
#  define ASIZE "%z0"
# else
/* using a incl/decl/whateverl for int may be wrong */
#  define ASIZE "l"
# endif

static always_inline void *atomic_cmppx(volatile void *nval, volatile void *oval, atomicptr_t *ptr)
{
	void *prev;
	__asm__ __volatile__(
		LOCK "cmpxchg %2,%3"
		: /* %0 */ "=a"(prev),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "r"(nval),
		  /* %3 */ "m"(atomic_pread(ptr)),
		  /* %4 */ "0"(oval)
		: "cc");
	return prev;
}

static always_inline void atomic_push(atomicst_t *head, atomicst_t *node)
{
	void *prev, *tmp;
	__asm__ __volatile__(
		"1:\n\t"
		"mov	%0, %3\n\t"
		"mov	%0, %2\n\t"
		LOCK "cmpxchg %4,%5\n\t"
		"cmp	%0, %2\n\t"
		"jne	1b\n"
		: /* %0 */ "=a"(prev),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_sread(head)),
		  /* %2 */ "=&r" (tmp),
		  /* %3 */ "=m" (atomic_sread(node))
		: /* %4 */ "r"(node),
		  /* %5 */ "m"(atomic_sread(head)),
		  /* %6 */ "0"(atomic_sread(head))
		: "cc");
}
# define ATOMIC_PUSH_ARCH

static always_inline void atomic_inc(atomic_t *ptr)
{
	__asm__ __volatile__(
		LOCK "inc" ASIZE " %0"
		/* gcc < 3 needs this, "+m" will not work reliable */
		: /* %0 */ "=m" (atomic_read(ptr))
		: /* %1 */ "m" (atomic_read(ptr))
		: "cc");
}

static always_inline void atomic_dec(atomic_t *ptr)
{
	__asm__ __volatile__(
		LOCK "dec" ASIZE " %0"
		/* gcc < 3 needs this, "+m" will not work reliable */
		: /* %0 */ "=m" (atomic_read(ptr))
		: /* %1 */ "m" (atomic_read(ptr))
		: "cc");
}

static always_inline int atomic_dec_test(atomic_t *ptr)
{
	unsigned char c;
	__asm__ __volatile__(
		LOCK "dec" ASIZE " %0\n\t"
		"setz	%1"
		/* gcc < 3 needs this, "+m" will not work reliable */
		: /* %0 */ "=m" (atomic_read(ptr)),
		  /* %1 */ "=qm" (c)
		: /* %2 */ "m" (atomic_read(ptr))
		: "cc");
	return c != 0;
}

static always_inline int atomic_inc_return(atomic_t *ptr)
{
	int c;
	__asm__ __volatile__(
		LOCK "xadd %1, %0\n\t"
		/* gcc < 3 needs this, "+m" will not work reliable */
		: /* %0 */ "=m" (atomic_read(ptr)),
		  /* %1 */ "=r" (c)
		: /* %2 */ "m" (atomic_read(ptr)),
		  /* %3 */ "1" (1)
		: "cc");
	return c;
}

# undef ASIZE
#endif /* LIB_IMPL_ATOMIC_H */
