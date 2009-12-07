/*
 * atomic.h
 * atomic primitves for arm
 *
 * Thanks Linux Kernel
 *
 * Copyright (c) 2008-2009 Jan Seiffert
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

/*
 * Uhhh, Ahhh, can't ... restrain ... myself
 * must .. optimize
 *
 * OK, here's the deal.
 * This is surely ___B R O K E N___
 * I don't have crosstools for arm nor do i understand
 * their assembler in full, but hey:
 * arch does linked load/store, like ppc, maybe someone
 * would find it usefull.
 *
 * I have the feeling this will never work...
 * i have a note here which says:
 * 1) do not use pc for load address
 * 2) do not use r14 for Rd1
 * 3) make Rd1 even
 * 4) use a register pair for Rd1 Rd2
 *
 * There is no constrain or what ever to get this.
 * (i read through the GCC source...)
 * Point nr 1. may be prevented by Q, because all
 * addressing to pc would be relative, so the compiler
 * should lower it.
 * Point 2 and 3 -> ooops.
 * Point 4 is only possible by a dirty trick, use
 * a double wide type and then use the H
 * print modifier to get the low/high part. But
 * using a double wide type has its downsides...
 *
 * If it crashes, disable I_LIKE_ASM
 *
 */

#ifndef LIB_IMPL_ATOMIC_H
# define LIB_IMPL_ATOMIC_H

# define atomic_read(x)	((x)->d)
# define atomic_pread(x)	((x)->d)
# define atomic_sread(x)	((x)->next)
//TODO: hmmm, we need some of those?
# ifndef smp_mb
#  define smp_mb() do { } while(0)
# endif

static always_inline void atomic_set(atomic_t *ptr, int val)
{
	long tmp;
	__asm__ __volatile__(
		"1:\n\t"
		"ldrex	%0, %2\n\t"
		"strex	%0, %3, %2\n\t"
		"teq	%0, #0\n\t"
		"bne	1b\n"
		: /* %0 */ "=&r" (tmp),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "Q" (atomic_read(ptr)),
		  /* %3 */ "r" (val)
		: "cc");
}

static always_inline void atomic_pset(atomicptr_t *ptr, void *val)
{
	void *tmp;
	__asm__ __volatile__(
		"1:\n\t"
		"ldrex	%0, %2\n\t"
		"strex	%0, %3, %2\n\t"
		"teq	%0, #0\n\t"
		"bne	1b\n"
		: /* %0 */ "=&r" (tmp),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "Q" (atomic_pread(ptr)),
		  /* %3 */ "r" (val)
		: "cc");
}

static always_inline void atomic_sset(atomicst_t *ptr, atomicst_t *val)
{
	void *tmp;
	__asm__ __volatile__(
		"1:\n\t"
		"ldrex	%0, %2\n\t"
		"strex	%0, %3, %2\n\t"
		"teq	%0, #0\n\t"
		"bne	1b\n"
		: /* %0 */ "=&r" (tmp),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_sread(ptr))
		: /* %2 */ "Q" (atomic_sread(ptr)),
		  /* %3 */ "r" (atomic_sread(val))
		: "cc");
}

static always_inline void *atomic_px(void *val, atomicptr_t *ptr)
{
	void *ret, *dummy;

	__asm__ __volatile__(
		"1:\n\t"
		"ldrex	%0, %3\n\t"
		"strex	%2, %4, %3\n\t"
		"teq	%2, #0\n\t"
		"bne	1b\n"
		: /* %0 */ "=&r" (ret),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr)),
		  /* %2 */ "=&r" (dummy)
		: /* %3 */ "Q" (atomic_pread(ptr)),
		  /* %4 */ "r" (val)
		: "cc");
	smp_mb();
	return ret;
}

static always_inline int atomic_x(int val, atomic_t *ptr)
{
	int ret, dummy;

	__asm__ __volatile__(
		"1:\n\t"
		"ldrex	%0, %3\n\t"
		"strex	%2, %4, %3\n\t"
		"teq	%2, #0\n\t"
		"bne	1b\n"
		: /* %0 */ "=&r" (ret),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr)),
		  /* %2 */ "=&r" (dummy)
		: /* %3 */ "Q" (atomic_read(ptr)),
		  /* %4 */ "r" (val)
		: "cc");
	smp_mb();
	return ret;
}

static always_inline int atomic_cmpx(int nval, int oval, atomic_t *ptr)
{
	int prev, cmp;

	do
	{
		__asm__ __volatile__(
			"ldrex	%0, %4\n\t"
			"mov	%1, #0\n\t"
			"teq	%0, %3\n\t"
			"strexeq	%1, %5, %4\n\t"
			: /* %0 */ "=&r" (prev),
			/* gcc < 3 needs this, "+m" will not work reliable */
			  /* %1 */ "=&r" (cmp),
			  /* %2 */ "=m" (atomic_read(ptr))
			: /* %3 */ "Ir" (oval),
			  /* %4 */ "Q" (atomic_read(ptr)),
			  /* %5 */ "r" (nval)
			: "cc");
	} while(cmp);
	smp_mb();
	return prev;
}

static always_inline void *atomic_cmppx(void *nval, void *oval, atomicptr_t *ptr)
{
	void *prev, *cmp;

	do
	{
		__asm__ __volatile__(
			"ldrex	%0, %4\n\t"
			"mov	%1, #0\n\t"
			"teq	%0, %3\n\t"
			"strexeq	%1, %5, %4\n\t"
			: /* %0 */ "=&r" (prev),
			/* gcc < 3 needs this, "+m" will not work reliable */
			  /* %1 */ "=&r" (cmp),
			  /* %2 */ "=m" (atomic_pread(ptr))
			: /* %3 */ "r" (oval),
			  /* %4 */ "Q" (atomic_pread(ptr)),
			  /* %5 */ "r" (nval)
			: "cc");
	} while(cmp);
	smp_mb();
	return prev;
}

static always_inline void atomic_add(int i, atomic_t *ptr)
{
	int ret, cmp;

	__asm__ __volatile__(
		"1:\n\t"
		"ldrex	%0, %3\n\t"
		"add	%0, %0, %4\n\t"
		"strex	%1, %0, %3\n\t"
		"teq	%1, #0\n\t"
		"bne	1b\n"
		: /* %0 */ "=&r" (ret),
		  /* %1 */ "=&r" (cmp),
		  /* %2 */ "=m" (atomic_read(ptr))
		: /* %3 */ "Q" (atomic_read(ptr)),
		  /* %4 */ "Ir" (i)
		: "cc");
}

static always_inline int atomic_add_return(int i, atomic_t *ptr)
{
	int ret, cmp;

	__asm__ __volatile__(
		"1:\n\t"
		"ldrex	%0, %3\n\t"
		"add	%0, %0, %4\n\t"
		"strex	%1, %0, %3\n\t"
		"teq	%1, #0\n\t"
		"bne	1b\n"
		: /* %0 */ "=&r" (ret),
		  /* %1 */ "=&r" (cmp),
		  /* %2 */ "=m" (atomic_read(ptr))
		: /* %3 */ "Q" (atomic_read(ptr)),
		  /* %4 */ "Ir" (i)
		: "cc");
	smp_mb();
	return ret;
}

static always_inline void atomic_sub(int i, atomic_t *ptr)
{
	int ret, cmp;

	__asm__ __volatile__(
		"1:\n\t"
		"ldrex	%0, %3\n\t"
		"sub	%0, %0, %4\n\t"
		"strex	%1, %0, %3\n\t"
		"teq	%1, #0\n\t"
		"bne	1b\n"
		: /* %0 */ "=&r" (ret),
		  /* %1 */ "=&r" (cmp),
		  /* %2 */ "=m" (atomic_read(ptr))
		: /* %3 */ "Q" (atomic_read(ptr)),
		  /* %4 */ "Ir" (i)
		: "cc");
}


static always_inline int atomic_sub_return(int i, atomic_t *ptr)
{
	int ret, cmp;

	__asm__ __volatile__(
		"1:\n\t"
		"ldrex	%0, %3\n\t"
		"sub	%0, %0, %4\n\t"
		"strex	%1, %0, %3\n\t"
		"teq	%1, #0\n\t"
		"bne	1b\n"
		: /* %0 */ "=&r" (ret),
		  /* %1 */ "=&r" (cmp),
		  /* %2 */ "=m" (atomic_read(ptr))
		: /* %3 */ "Q" (atomic_read(ptr)),
		  /* %4 */ "Ir" (i)
		: "cc");
	smp_mb();
	return ret;
}

# define atomic_inc(x)	atomic_add(1, x)
# define atomic_inc_return(x)	(atomic_add_return(1, x))
# define atomic_dec(x)	atomic_sub(1, x)
# define atomic_dec_test(x) (atomic_sub_return(1, (x)) == 0)

#endif /* LIB_IMPL_ATOMIC_H */
