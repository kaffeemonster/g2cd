/*
 * atomic.h
 * atomic primitves for alpha
 *
 * Thanks Linux Kernel
 * 
 * Copyright (c) 2007, Jan Seiffert
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
 * I don't have crosstools for alpha nor do i understand
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
# define atomic_set(x, y) (((x)->d) = (y))
# define atomic_pread(x)	((x)->d)
# define atomic_pset(x, y) ((x)->d = (y))
# define atomic_sread(x)	((x)->next)
# define atomic_sset(x, y) ((x)->next = (y))

# ifdef HAVE_SMP
#  define MEM_ORDER	"\tmb\n"
# else
#  define MEM_ORDER
# endif

/*
 * I like good docs:
 * The following sequence should not be used:
 * try_again:	LDQ_L    R1, x
 *           	<modify  R1>
 *           	STQ_C    R1, x
 *           	BEQ      R1, try_again
 * That sequence penalizes performance when the STQ_C succeeds, because
 * the sequence contains a backward branch, which is predicted to be
 * taken in the Alpha architecture. ... Instead, a forward branch should
 * be used to handle the failure case, as shown in Section 5.5.2.
 *
 * Hmmm, maybe the same with mips??
 *
 */

static inline void *atomic_px(void *val, atomicptr_t *ptr)
{
	unsigned long long dummy;

	__asm__ __volatile__(
		"1:\n\t"
		"ldq_l\t%0,%3\n"
		"bis\t$31,%4,%2\n"
		"stq_c\t%2,%1\n\t"
		"beq\t%2,2f\n"
		MEM_ORDER
		".subsection 2\n"
		"2:\n\t"
		"br\t1b\n"
		".previous\n"
		: /* %0 */ "=&r" (val),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr)),
		  /* %2 */ "=&r" (dummy)
		: /* %3 */ "m" (atomic_pread(ptr)),
		  /* %4 */ "rI" (val));
	return val;
}

static inline int atomic_x(int val, atomic_t *ptr)
{
	unsigned long long dummy;
	__asm__ __volatile__(
		"1:\n\t"
		"ldl_l\t%0,%3\n"
		"bis\t$31,%4,%2\n"
		"stl_c\t%2, %1\n\t"
		"beq\t%2,2f\n"
		MEM_ORDER
		".subsection 2\n"
		"2:\n\t"
		"br\t1b\n"
		".previous\n"
		: /* %0 */ "=&r" (val),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr)),
		  /* %2 */ "=&r" (dummy)
		: /* %3 */ "m" (atomic_read(ptr)),
		  /* %4 */ "rI" (val));
	return val;
}

static inline int atomic_cmpx(int nval, int oval, atomic_t *ptr)
{
	unsigned long long prev, cmp;

	__asm__ __volatile__(
		"1:\n\t"
		"ldl_l\t%0,%4\n\t"
		"cmpeq %0,%3,%1\n\t"
		"beq\t%1,2f\n\t"
		"mov\t%5,%1\n\t"
		"stl_c\t%1,%2\n\t"
		"beq\t%1,3f\n"
		MEM_ORDER
		"2:\n"
		".subsection 2\n"
		"3:\n\t"
		"br\t1b\n"
		".previous\n"
		: /* %0 */ "=&r" (prev),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=&r" (cmp)
		  /* %2 */ "=m" (atomic_read(ptr))
		: /* %3 */ "r" (oval),
		  /* %4 */ "m" (atomic_read(ptr)),
		  /* %5 */ "r" (nval));
	return prev;
}

static inline void *atomic_cmppx(volatile void *nval, volatile void *oval, atomicptr_t *ptr)
{
	void *prev;
	unsigned long long cmp;
	__asm__ __volatile__(
		"1:\n\t"
		"ldq_l\t%0,%4\n\t"
		"cmpeq\t%0,%3,%1\n\t"
		"bne\t%1,2f\n"
		"mov\t%5,%1\n\t"
		"stq_c\t%1,%2\n\t"
		"beq\t%1,3f\n"
		MEM_ORDER
		"2:\n"
		".subsection 2\n"
		"3:\n\t"
		"br\t1b\n"
		".previous\n"
		: /* %0 */ "=&r" (prev),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=&r" (cmp)
		  /* %2 */ "=m" (atomic_pread(ptr))
		: /* %3 */ "r" (oval),
		  /* %4 */ "m" (atomic_pread(ptr)),
		  /* %5 */ "r" (nval));
	return prev;
}

static inline void atomic_inc(atomic_t *ptr)
{
	unsigned long long tmp;

	__asm__ __volatile__(
		"1:\n\t"
		"ldl_l\t%0,%2\n\t"
		"addl\t%0,1,%0\n\t"
		"stl_c\t%0,%1\n\t"
		"beq\t%0,2f\n"
		".subsection 2\n"
		"2:\n\t"
		"br\t1b\n"
		".previous\n"
		: /* %0 */ "=&r" (tmp),
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "m" (atomic_read(ptr)));
}

static inline void atomic_dec(atomic_t *ptr)
{
	unsigned long long tmp;

	__asm__ __volatile__(
		"1:\n\t"
		"ldl_l\t%0,%2\n\t"
		"subl\t%0,1,%0\n\t"
		"stl_c\t%0,%1\n\t"
		"beq\t%0,2f\n"
		".subsection 2\n"
		"2:\n\t"
		"br\t1b\n"
		".previous\n"
		: /* %0 */ "=&r" (tmp),
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "m" (atomic_read(ptr)));
}

static inline void atomic_add(int i, atomic_t *ptr)
{
	unsigned long long tmp;

	__asm__ __volatile__(
		"1:\n\t"
		"ldl_l\t%0,%2\n\t"
		"addl\t%0,%3,%0\n\t"
		"stl_c\t%0,%1\n\t"
		"beq\t%0,2f\n"
		".subsection 2\n"
		"2:\n\t"
		"br\t1b\n"
		".previous"
		: /* %0 */ "=&r" (tmp),
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "m" (atomic_read(ptr))
		  /* %3 */ "Ir" (i));
}

static inline void atomic_sub(int i, atomic_t *ptr)
{
	unsigned long long tmp;

	__asm__ __volatile__(
		"1:\n\t"
		"ldl_l\t%0,%2\n\t"
		"subl\t%0,%3,%0\n\t"
		"stl_c\t%0,%1\n\t"
		"beq\t%0,2f\n"
		".subsection 2\n"
		"2:\n\t"
		"br\t1b\n"
		".previous\n"
		: /* %0 */ "=&r" (tmp),
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "m" (atomic_read(ptr))
		  /* %3 */ "Ir" (i));
}

static inline int atomic_dec_return(atomic_t *ptr)
{
	long long tmp, ret_val;

	__asm__ __volatile__(
		"1:\n\t"
		"ldl_l\t%0,%3\n\t"
		"subl\t%0,1,%2\n\t"
		"subl\t%0,1,%0\n\t"
		"stl_c\t%0,%1\n\t"
		"beq\t%0,2f\n"
		MEM_ORDER
		".subsection 2\n"
		"2:\n\t"
		"br\t1b\n"
		".previous\n"
		: /* %0 */ "=&r" (tmp),
		  /* %1 */ "=m" (atomic_read(ptr)),
		  /* %2 */ "=&r" (ret_val)
		: /* %3 */ "m" (atomic_read(ptr)));	
	return (int) ret_val;
}

# define atomic_dec_test(x) (atommic_dec_return((x)) == 0)

#endif /* LIB_IMPL_ATOMIC_H */
