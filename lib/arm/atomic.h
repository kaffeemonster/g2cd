/*
 * atomic.h
 * atomic primitves for arm
 *
 * Copyright (c) 2008-2010 Jan Seiffert
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
 * Thanks got to the Linux Kernel for the ideas
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

# ifdef HAVE_SMP
#  if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || \
      defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__)
#   define dmb()	asm volatile("mcr p15, 0, %0, c7, c10, 5" :: "r" (0) : "memory")
#  elif defined(__ARM_ARCH_7A__)
#   define dmb()	asm volatile("dmb" ::: "memory")
#  else
#   define dmb()	mbarrier()
#  endif
#  define mb()	dmb()
#  define rmb()	dmb()
#  define wmb()	dmb()
#  define arm_mb_after() dmb()
# else
#  define mb()	mbarrier()
#  define rmb()	mbarrier()
#  define wmb()	mbarrier()
#  define arm_mb_after()	do { } while(0)
# endif
# define read_barrier_depends()	do { } while(0)

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
	arm_mb_after();
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
	arm_mb_after();
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
			"it	eq\n\t"
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
	arm_mb_after();
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
			"it	eq\n\t"
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
	arm_mb_after();
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
	arm_mb_after();
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
	arm_mb_after();
	return ret;
}

# define atomic_inc(x)	atomic_add(1, x)
# define atomic_inc_return(x)	(atomic_add_return(1, x))
# define atomic_dec(x)	atomic_sub(1, x)
# define atomic_dec_test(x) (atomic_sub_return(1, (x)) == 0)

#endif /* LIB_IMPL_ATOMIC_H */
