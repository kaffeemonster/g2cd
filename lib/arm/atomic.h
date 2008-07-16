/*
 * atomic.h
 * atomic primitves for arm
 *
 * Thanks Linux Kernel
 * 
 * Copyright (c) 2008 Jan Seiffert
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
 * If it crashes, disable I_LIKE_ASM
 *
 */

#ifndef LIB_IMPL_ATOMIC_H
# define LIB_IMPL_ATOMIC_H

# define atomic_read(x)	((x)->d)
# define atomic_pread(x)	((x)->d)
# define atomic_sread(x)	((x)->next)

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
	void *ret, dummy;

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
			  /* %1 */ "=&r" (cmp)
			  /* %2 */ "=m" (atomic_read(ptr))
			: /* %3 */ "Ir" (oval),
			  /* %4 */ "Q" (atomic_read(ptr)),
			  /* %5 */ "r" (nval)
			: "cc");
	} while(cmp);
	return prev;
}

static always_inline void *atomic_cmppx(volatile void *nval, volatile void *oval, atomicptr_t *ptr)
{
	void *prev, cmp;

	do
	{
		__asm__ __volatile__(
			"ldrex	%0, %4\n\t"
			"mov	%1, #0\n\t"
			"teq	%0, %3\n\t"
			"strexeq	%1, %5, %4\n\t"
			: /* %0 */ "=&r" (prev),
			/* gcc < 3 needs this, "+m" will not work reliable */
			  /* %1 */ "=&r" (cmp)
			  /* %2 */ "=m" (atomic_pread(ptr))
			: /* %3 */ "r" (oval),
			  /* %4 */ "Q" (atomic_pread(ptr)),
			  /* %5 */ "r" (nval)
			: "cc");
	} while(cmp);
	return prev;
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
		: /* %3 */ "Q" (atomic_read(ptr))
		  /* %4 */ "Ir" (i)
		: "cc");
	return ret;
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
		: /* %3 */ "Q" (atomic_read(ptr))
		  /* %4 */ "Ir" (i));
	return ret;
}

# define atomic_inc(x)	((void)atomic_add_return(1, x))
# define atomic_dec(x)	((void)atomic_sub_return(1, x))
# define atomic_dec_test(x) (atommic_sub_return(1, (x)) == 0)

#endif /* LIB_IMPL_ATOMIC_H */
