/*
 * atomic.h
 * atomic primitves for riscv
 *
 * Copyright (c) 2006-2026 Jan Seiffert
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
 *
 */

#ifdef __riscv_atomic
/* zalrsc && zaamo */
# ifndef LIB_IMPL_ATOMIC_H
#  define LIB_IMPL_ATOMIC_H
#  define atomic_read(x)	((x)->d)
#  define atomic_set(x, y) (((x)->d) = (y))
#  define atomic_pread(x)	((x)->d)
#  define atomic_pset(x, y) ((x)->d = (y))
#  define atomic_sread(x)	((x)->next)
#  define atomic_sset(x, y) ((x)->next = (y))

#  define mb()  asm volatile("fence rw,rw" ::: "memory")
#  define rmb() asm volatile("fence r,rw" ::: "memory")
#  define wmb() asm volatile("fence rw,w" ::: "memory")
#  define read_barrier_depends() do { } while (0)

/*
 * Make sure to avoid asm operant size suffixes, this is
 * used in 32Bit & 64Bit
 * Fill in with these 
 *
 * %I should give operandsize (see ppc/atomic.h rant),
 * %S in upper case ...
 */
/* and it does not work.... */
// #  if defined(__GNUC__) && !defined(__clang__)
// #   define ISIZE "%I1"
// #   define PSIZE "%I1"
// #  else
/* using a .w/.d for int/ptr needs to be right */
#   define ISIZE "w"
#   if __riscv_xlen == 32
#    define PSIZE "w"
#   elif __riscv_xlen == 64
#    define PSIZE "d"
#   else
#    error "go home, your RISCV is drunk (funny XLEN)"
#   endif
// #  endif


static always_inline void *atomic_px(void *val, atomicptr_t *a)
{
	void *old_val;
	__asm__ __volatile__(
		"amoswap." PSIZE ".aqrl %0, %z3, %2"
		: /* %0 */ "=r" (old_val),
		  /* %1 */ "=m" (atomic_pread(a))
		: /* %2 */ "A" (atomic_pread(a)),
		  /* %3 */ "rJ" (val),
		  /* %4 */ "m" (atomic_pread(a))
	);
	return old_val;
}

static always_inline int atomic_x(int val, atomic_t *a)
{
	int old_val;
	__asm__ __volatile__(
		"amoswap." ISIZE ".aqrl %0, %z3, %2"
		: /* %0 */ "=r" (old_val),
		  /* %1 */ "=m" (atomic_read(a))
		: /* %2 */ "A" (atomic_read(a)),
		  /* %3 */ "rJ" (val),
		  /* %4 */ "m" (atomic_read(a))
	);
	return old_val;
}

#  ifdef __riscv_zacas
static always_inline void *atomic_cmppx(void *nval, void *oval, atomicptr_t *a)
{
	void *prev;
	__asm__ __volatile__(
		"amocas." PSIZE ".aqrl %0, %z3, %2"
		: /* %0 */ "=r" (prev),
		  /* %1 */ "=m" (atomic_pread(a))
		: /* %2 */ "A" (atomic_pread(a)),
		  /* %3 */ "rJ" (nval),
		  /* %4 */ "0" (oval),
		  /* %5 */ "m" (atomic_pread(a))
	);
	return prev;
}
#  else
static always_inline void *atomic_cmppx(void *nval, void *oval, atomicptr_t *a)
{
	void *prev;
	int success;

	__asm__ __volatile__(
		"j 1f\n"
		"2:\n\t"
		".option push\n\t" \
		".option arch, +zihintpause\n\t" \
		"pause\n\t" \
		".option pop\n\t"
		// "mv	%1, %0\n"
		"1:\n\t"
		"lr." PSIZE ".aq	%0, %4\n\t"
		"bne		%0, %1, 3f\n\t"
		"sc." PSIZE ".rl	%2, %z5, %4\n\t"
		"bnez		%2, 2b\n"
		"3:\n\t"
		: /* %0 */ "=r" (prev),
		  /* %1 */ "=r" (oval),
		  /* %2 */ "=r" (success),
		  /* %3 */ "=m" (atomic_pread(a))
		: /* %4 */ "A" (atomic_pread(a)),
		  /* %5 */ "rJ" (nval),
		  /* %6 */ "1" (oval),
		  /* %7 */ "m"(atomic_pread(a))
	);
	return prev;
}
#  endif

#  ifdef __riscv_zacas
static always_inline int atomic_cmpx(int nval, int oval, atomic_t *a)
{
	int prev;
	__asm__ __volatile__(
		"amocas." ISIZE ".aqrl %0, %z3, %2"
		: /* %0 */ "=r" (prev),
		  /* %1 */ "=m" (atomic_read(a))
		: /* %2 */ "A" (atomic_read(a)),
		  /* %3 */ "rJ" (nval),
		  /* %4 */ "0" (oval),
		  /* %5 */ "m" (atomic_read(a))
	);
	return prev;
}
#  else
static always_inline int atomic_cmpx(int nval, int oval, atomic_t *a)
{
	int prev;
	int success;

	__asm__ __volatile__(
		"j 1f\n"
		"2:\n\t"
		".option push\n\t" \
		".option arch, +zihintpause\n\t" \
		"pause\n\t" \
		".option pop\n\t"
		// "mv	%1, %0\n"
		"1:\n\t"
		"lr." PSIZE ".aq	%0, %4\n\t"
		"bne		%0, %1, 3f\n\t"
		"sc." PSIZE ".rl	%2, %z5, %4\n\t"
		"bnez		%2, 2b\n"
		"3:\n\t"
		: /* %0 */ "=r" (prev),
		  /* %1 */ "=r" (oval),
		  /* %2 */ "=r" (success),
		  /* %3 */ "=m" (atomic_read(a))
		: /* %4 */ "A" (atomic_read(a)),
		  /* %5 */ "rJ" (nval),
		  /* %6 */ "1" (oval),
		  /* %7 */ "m"(atomic_read(a))
	);
	return prev;
}
#  endif

static always_inline void atomic_bit_set(atomic_t *a, int i)
{
	int mask = 1 << i;
	int old_val;
	__asm__ __volatile__(
		"amoor." ISIZE ".aqrl %0, %3, %2"
		: /* %0 */ "=r" (old_val),
		  /* %1 */ "=m" (atomic_read(a))
		: /* %2 */ "A" (atomic_read(a)),
		  /* %3 */ "r" (mask),
		  /* %4 */ "m" (atomic_read(a))
	);
}
#  define ATOMIC_BIT_SET_ARCH

static always_inline void atomic_inc(atomic_t *a)
{
	int old_val;
	__asm__ __volatile__(
		"amoadd." ISIZE ".aqrl %0, %3, %2"
		: /* %0 */ "=r" (old_val),
		  /* %1 */ "=m" (atomic_read(a))
		: /* %2 */ "A" (atomic_read(a)),
		  /* %3 */ "r" (1),
		  /* %4 */ "m" (atomic_read(a))
	);
}

static always_inline void atomic_dec(atomic_t *a)
{
	int old_val;
	__asm__ __volatile__(
		"amoadd." ISIZE ".aqrl %0, %3, %2"
		: /* %0 */ "=r" (old_val),
		  /* %1 */ "=m" (atomic_read(a))
		: /* %2 */ "A" (atomic_read(a)),
		  /* %3 */ "r" (-1),
		  /* %4 */ "m" (atomic_read(a))
	);
}

static always_inline int atomic_dec_test(atomic_t *a)
{
	int old_val;
	__asm__ __volatile__(
		"amoadd." ISIZE ".aqrl %0, %3, %2"
		: /* %0 */ "=r" (old_val),
		  /* %1 */ "=m" (atomic_read(a))
		: /* %2 */ "A" (atomic_read(a)),
		  /* %3 */ "r" (-1),
		  /* %4 */ "m" (atomic_read(a))
	);
	return 1 == old_val;
}

static always_inline int atomic_inc_return(atomic_t *a)
{
	int old_val;
	__asm__ __volatile__(
		"amoadd." ISIZE ".aqrl %0, %3, %2"
		: /* %0 */ "=r" (old_val),
		  /* %1 */ "=m" (atomic_read(a))
		: /* %2 */ "A" (atomic_read(a)),
		  /* %3 */ "r" (1),
		  /* %5 */ "m" (atomic_read(a))
	);
	return old_val;
}

#  undef ISIZE
#  undef PSIZE
# endif /* LIB_IMPL_ATOMIC_H */
#else
# include "../generic/atomic.h"
#endif
