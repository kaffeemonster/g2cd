/*
 * atomic.h
 * atomic primitves for mips
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
 * I don't have crosstools for mips nor do i understand
 * their assembler in full (register on their website to
 * download their f**king sheets???), but hey:
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
#  define MEM_ORDER	"\tsync\n"
# else
// TODO: Maybe a nop needed here to fill branch slot
/*
 * MIPS and its instruction timing...
 * after a branch or call (branch with link) the next
 * instruction after the branch still gets executed,
 * /before/ the branch, because it is already in the
 * cpu pipeline.
 * The MIPS assambler can automatically fill these
 * slots with nops itself. But you can also
 * fill these slots yourself...
 * Automagic with unclear manual override: needs
 * manual inspection what gets generated (binary).
 */
#  define MEM_ORDER
# endif

static always_inline void *atomic_px_32(void *val, atomicptr_t *ptr)
{
	void *ret_val, *dummy;

	__asm__ __volatile__(
		".set\tmips3\n"
		"1:\n\t"
		"ll\t%0, %3\n"
		".set\tmips0\n\t"
		"move\t%2, %z4\n"
		".set\tmips3\n\t"
		"sc\t%2, %1\n\t"
		"beqz\t%2, 1b\n"
		MEM_ORDER
		".set\tmips0\n"
		: /* %0 */ "=&r" (ret_val),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr)),
		  /* %2 */ "=&r" (dummy)
		: /* %3 */ "m" (atomic_pread(ptr)),
		  /* %4 */ "Jr" (val));
	return ret_val;
}

#if __mips == 64
static always_inline void *atomic_px_64(void *val, atomicptr_t *ptr)
{
	void *ret_val, *dummy;

	__asm__ __volatile__(
		".set\tmips3\n"
		"1:\n\t"
		"lld\t%0, %3\n"
		"move\t%2, %z4\n"
		"scd\t%2, %1\n\t"
		"beqz\t%2, 1b\n"
		MEM_ORDER
		".set\tmips0\n"
		: /* %0 */ "=&r" (ret_val),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr)),
		  /* %2 */ "=&r" (dummy)
		: /* %3 */ "m" (atomic_pread(ptr)),
		  /* %4 */ "Jr" (val));
	return ret_val;
}
#endif

extern void *_illigal_ptr_size(volatile void *,atomicptr_t *);
static always_inline void *atomic_px(void *val, atomicptr_t *ptr)
{
	switch(sizeof(val))
	{
	case 4:
		return atomic_px_32(val, ptr);
#if __mips == 64
	case 8:
		return atomic_px_64(val, ptr);
#endif
	}
	return _illigal_ptr_size(val, ptr);
}

static always_inline int atomic_x_32(int val, atomic_t *ptr)
{
	int ret_val, dummy;
	__asm__ __volatile__(
		".set\tmips3\n"
		"1:\n\t"
		"ll\t%0, %3\n"
		".set\tmips0\n\t"
		"move\t%2, %z4\n"
		".set\tmips3\n\t"
		"sc\t%2, %1\n\t"
		"beqz\t%2, 1b\n"
		MEM_ORDER
		".set\tmips0\n"
		: /* %0 */ "=&r" (ret_val),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr)),
		  /* %2 */ "=&r" (dummy)
		: /* %3 */ "m" (atomic_read(ptr)),
		  /* %4 */ "Jr" (val));
	return ret_val;
}

#if __mips == 64
static always_inline int atomic_x_64(int val, atomic_t *ptr)
{
	int ret_val, dummy;
	__asm__ __volatile__(
		".set\tmips3\n"
		"1:\n\t"
		"lld\t%0, %3\n"
		"move\t%2, %z4\n"
		"scd\t%2, %1\n\t"
		"beqz\t%2, 1b\n"
		MEM_ORDER
		".set\tmips0\n"
		: /* %0 */ "=&r" (ret_val),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr)),
		  /* %2 */ "=&r" (dummy)
		: /* %2 */ "m" (atomic_read(ptr)),
		  /* %3 */ "Jr" (val));
	return ret_val;
}
#endif

extern int _illigal_int_size(int, atomic_t *);
static always_inline int atomic_x(int val, atomic_t *ptr)
{
	switch(sizeof(val))
	{
	case 4:
		return atomic_x_32(val, ptr);
#if __mips == 64
	case 8:
		return atomic_x_64(val, ptr);
#endif
	}
	return _illigal_int_size(val, ptr);
}

static always_inline int atomic_cmpx_32(int nval, int oval, atomic_t *ptr)
{
	int prev;

	__asm__ __volatile__(
		".set\tpush\n"
		".set\tnoat\n"
		".set\tmips3\n"
		"1:\n\t"
		"ll\t%0, %3\n\t"
		"bne\t%0, %z2, 2f\n"
		".set\tmips0\n\t"
		"move\t$1, %z4\n"
		".set\tmips3\n\t"
		"sc\t$1, %1\n\t"
		"beqz\t$1, 1b\n"
		MEM_ORDER
		"2:\n"
		".set\tpop\n"
		: /* %0 */ "=&r" (prev),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "Jr" (oval),
		  /* %3 */ "m" (atomic_read(ptr)),
		  /* %4 */ "Jr" (nval));
	return prev;
}

#if __mips == 64
static always_inline int atomic_cmpx_64(int nval, int oval, atomic_t *ptr)
{
	int prev;

	__asm__ __volatile__(
		".set\tpush\n"
		".set\tnoat\n"
		".set\tmips3\n"
		"1:\n\t"
		"lld\t%0, %3\n\t"
		"bne\t%0, %z2, 2f\n\t"
		"move\t$1, %z4\n\t"
		"scd\t$1, %1\n\t"
		"beqz\t$1, 1b\n"
		MEM_ORDER
		"2:\n"
		".set\tpop\n"
		: /* %0 */  "=&r" (prev),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "Jr" (oval),
		  /* %3 */ "m" (atomic_read(ptr)),
		  /* %4 */ "Jr" (nval));
	return prev;
}
#endif

static always_inline int atomic_cmpx(int nval, int oval, atomic_t *ptr)
{
	switch(sizeof(nval))
	{
	case 4:
		return atomic_cmpx_32(nval, oval, ptr);
#if __mips == 64
	case 8:
		return atomic_cmpx_64(nval, oval, ptr);
#endif
	}
	return _illigal_int_size(nval, ptr);
}

static always_inline void *atomic_cmppx_32(volatile void *nval, volatile void *oval, atomicptr_t *ptr)
{
	void *prev;
	__asm__ __volatile__(
		".set\tpush\n"
		".set\tnoat\n"
		".set\tmips3\n"
		"1:\n\t"
		"ll\t%0, %3\n\t"
		"bne\t%0, %z2, 2f\n"
		".set\tmips0\n\t"
		"move\t$1, %z4\n"
		".set\tmips3\n\t"
		"sc\t$1, %1\n\t"
		"beqz\t$1, 1b\n"
		MEM_ORDER
		"2:\n"
		".set\tpop\n"
		: /* %0 */ "=&r" (prev),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "Jr" (oval),
		  /* %3 */ "m" (atomic_pread(ptr)),
		  /* %4 */ "Jr" (nval));
	return prev;
}

#if __mips == 64
static always_inline void *atomic_cmppx_64(volatile void *nval, volatile void *oval, atomicptr_t *ptr)
{
	void *prev;
	__asm__ __volatile__(
		".set\tpush\n"
		".set\tnoat\n"
		".set\tmips3\n"
		"1:\n\t"
		"lld\t%0, %3\n\t"
		"bne\t%0, %z2, 2f\n\t"
		"move\t$1, %z4\n\t"
		"scd\t$1, %1\n\t"
		"beqz\t$1, 1b\n"
		MEM_ORDER
		"2:\n"
		".set\tpop\n"
		: /* %0 */ "=&r" (prev),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ "Jr" (oval),
		  /* %3 */ "m" (atomic_pread(ptr)),
		  /* %4 */ "Jr" (nval));
	return prev;
}
#endif

static always_inline void *atomic_cmppx(volatile void *nval, volatile void *oval, atomicptr_t *ptr)
{
	switch(sizeof(nval))
	{
	case 4:
		return atomic_cmppx_32(nval, oval, ptr);
#if __mips == 64
	case 8:
		return atomic_cmppx_64(nval, oval, ptr);
#endif
	}
	return _illigal_ptr_size(nval, ptr);
}

static always_inline void atomic_inc(atomic_t *ptr)
{
	int tmp;

	__asm__ __volatile__(
		".set\tmips3\n"
		"1:\n\t"
		"ll\t%0, %2\n\t"
		"addiu\t%0, 1\n\t"
		"sc\t%0, %1\n\t"
		"beqz\t%0, 1b\n"
		".set mips0\n"
		: /* %0 */ "=&r" (tmp),
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "m" (atomic_read(ptr)));
}

static always_inline void atomic_dec(atomic_t *ptr)
{
	int tmp;

	__asm__ __volatile__(
		".set\tmips3\n"
		"1:\n\t"
		"ll\t%0, %2\n\t"
		"addiu\t%0,-1\n\t"
		"sc\t%0, %1\n\t"
		"beqz\t%0, 1b\n"
		".set mips0\n"
		: /* %0 */ "=&r" (tmp),
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "m" (atomic_read(ptr)));
}

static always_inline void atomic_add(int i, atomic_t *ptr)
{
	int tmp;

	__asm__ __volatile__(
		".set\tmips3\n"
		"1:\n\t"
		"ll\t%0, %2\n\t"
		"addu\t%0, %z3\n\t"
		"sc\t%0, %1\n\t"
		"beqz\t%0, 1b\n"
		".set mips0\n"
		: /* %0 */ "=&r" (tmp),
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "m" (atomic_read(ptr))
		  /* %3 */ "IJr" (i));
}

static always_inline void atomic_sub(int i, atomic_t *ptr)
{
	int tmp;

	__asm__ __volatile__(
		".set\tmips3\n"
		"1:\n\t"
		"ll\t%0, %2\n\t"
		"subu\t%0, %z3\n\t"
		"sc\t%0, %1\n\t"
		"beqz\t%0, 1b\n"
		".set mips0\n"
		: /* %0 */ "=&r" (tmp),
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ "m" (atomic_read(ptr))
		  /* %3 */ "IJr" (i));
}

static always_inline int atomic_dec_return(atomic_t *ptr)
{
	int tmp, dummy;

	__asm__ __volatile__(
		".set\tmips3\n"
		"1:\n\t"
		"ll\t%0, %3\n\t"
		"addiu\t%0,-1\n\t"
		"move\t%2, %0\n\t"
		"sc\t%2, %1\n\t"
		"beqz\t%2, 1b\n"
		MEM_ORDER
		".set mips0\n"
		: /* %0 */ "=&r" (tmp),
		  /* %1 */ "=m" (atomic_read(ptr)),
		  /* %2 */ "=&r" (dummy)
		: /* %3 */ "m" (atomic_read(ptr)));	
	return tmp;
}

# define atomic_dec_test(x) (atommic_dec_return((x)) == 0)

#endif /* LIB_IMPL_ATOMIC_H */
