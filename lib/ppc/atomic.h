/*
 * atomic.h
 * atomic primitves for ppc
 *
 * Thanks Linux Kernel
 * 
 * Copyright (c) 2006, Jan Seiffert
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
#  define EIEIO	"eieio\n"
/*
 * as a simple userspace app we will never meet write through mem or
 * alike, until someone directly maps networkhw buffers to userspace...
 * so a simple lightwight sync should suffice
 */
#  define SYNC	"\n\tlwsync"
# else
#  define EIEIO
#  define SYNC
# endif

# ifdef __PPC405__
#  define PPC405_ERR77(op)	"dcbt\t"str_it(op)"\n\t"
# else
#  define  PPC405_ERR77(op)
# endif

/* 
 * Read further down...
 *
 * my gcc 3.4.5 ppc-crosscompiler generates a nice RT,RA,RB sequence in
 * the inline asm for the memref constraints (mostly %2) on the
 * lwarx/stwcx.
 * This creates nice array offsets for access when inlined.
 * Say r10 contains array base-pointer and r11 contains index, now we get:
 * 	...
 * 	slwi	9,11,2	# index * 4
 * #APP
 * 1:
 * 	lwarx	0,9,10	# linked load (10 + 9) to 0
 * 	...
 *
 * Problem is, does this always work "The Right Way"[TM]?
 */

/*
 * Ex-TODO: check that gcc always generates correct 2nd/3rd lwarx/stwcx operand
 *
 * OK, forget about it, gcc-4.1.1 generates nice _wrong_ sequence.
 * Seems not to work always.
 * Very nice is the fact, that the GCC manual misses some existing register
 * contraints for Power/ppc/rs6000, like a,Z,Y etc.
 * And the best fact: gcc has no constraint for what I need: a memop which is
 * allways indexed (even if the index is 0) (something like Z)
 * It gets even better:  rs6000.h even mentiones the addressing mode
 * (sum of two regs), but, no constrain...
 *
 * Back to the traditional way, always pass 0 as offset, let gcc calc ptr
 * (with maybe added offset) explicitly
 */

static always_inline void *atomic_px(void *val, atomicptr_t *ptr)
{
	void *tmp;

	__asm__ __volatile__(
		"1:\n\t"
		"lwarx\t%0,0,%2\n\t"
		PPC405_ERR77(%2)
		"stwcx.\t%3,0,%2\n\t"
		"bne-\t1b"
		SYNC
		: /* %0 */ "=&r" (tmp),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "b" (&atomic_pread(ptr)),
		  /* %3 */ "r" (val),
		  /* %4 */ "m" (atomic_pread(ptr))
		: "cc");

	return tmp;
}

static always_inline int atomic_x(int val, atomic_t *ptr)
{
	int tmp;

	__asm__ __volatile__(
		"1:\n\t"
		"lwarx\t%0,0,%2\n\t"
		PPC405_ERR77(%2)
		"stwcx.\t%3,0,%2\n\t"
		"bne-\t1b"
		SYNC
		: /* %0 */ "=&r" (tmp),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "b" (&atomic_read(ptr)),
		  /* %3 */ "r" (val),
		  /* %4 */ "m" (atomic_read(ptr))
		: "cc");

	return tmp;
}

static always_inline void *atomic_cmppx(volatile void *nval, volatile void *oval, atomicptr_t *ptr)
{
	void *prev;

	__asm__ __volatile__ (
		"1:\n\t"
		"lwarx\t%0,0,%2 \n\t"
		"cmpw\t0,%0,%3 \n\t"
		"bne\t2f\n\t"
		PPC405_ERR77(%2)
		"stwcx.\t%4,0,%2 \n\t"
		"bne-\t1b"
		SYNC
		"\n2:"
		: /* %0 */ "=&r" (prev),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "b" (&atomic_pread(ptr)),
		  /* %3 */ "r" (oval),
		  /* %4 */ "r" (nval),
		  /* %5 */ "m" (atomic_pread(ptr))
		: "cc");

	return prev;
}

static always_inline void atomic_inc(atomic_t *ptr)
{
	int tmp;
	__asm__ __volatile__(
		"1:\n\t"
		"lwarx\t%0,0,%2\n\t"
		"addic\t%0,%0,1\n\t"
		PPC405_ERR77(%2)
		"stwcx.\t%0,0,%2 \n\t"
		"bne-\t1b"
		: /* %0 */ "=&r" (tmp),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "b" (atomic_read(ptr)),
		  /* %3 */ "m" (atomic_read(ptr))
		: "cc");
}

static always_inline void atomic_dec(atomic_t *ptr)
{
	int tmp;
	__asm__ __volatile__(
		"1:\n\t"
		"lwarx\t%0,0,%2\n\t"
		"addic\t%0,%0,-1\n\t"
		PPC405_ERR77(%2)
		"stwcx.\t%0,0,%2\n\t"
		"bne-\t1b"
		: /* %0 */ "=&r" (tmp),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "b" (&atomic_read(ptr)),
		  /* %3 */ "m" (atomic_read(ptr))
		: "cc");
}

static always_inline int atomic_dec_return(atomic_t *ptr)
{
	int tmp;
	__asm__ __volatile__(
		"1:\n\t"
		"lwarx\t%0,0,%2\n\t"
		"addic\t%0,%0,-1\n\t"
		PPC405_ERR77(%2)
		"stwcx.\t%0,0,%2\n\t"
		"bne-\t1b"
		SYNC
		: /* %0 */ "=&r" (tmp),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "b" (&atomic_read(ptr)),
		  /* %3 */ "m" (atomic_read(ptr))
		: "cc");
	return tmp;
}

#define atomic_dec_test(x) (atomic_dec_return((x)) == 0)

#endif /* LIB_IMPL_ATOMIC_H */
