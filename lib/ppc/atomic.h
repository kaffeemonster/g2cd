/*
 * atomic.h
 * atomic primitves for ppc
 *
 * Thanks Linux Kernel
 * 
 * Copyright (c) 2006, 2008 Jan Seiffert
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
 * as a simple userspace app we will never meet write through MEMIO or
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
 * Yipiee! PPC is so cooool and fucking complex for no apparent reason.
 * Come on, they must have smoked something, uhhh, gime my 68k back ;(
 * This is the story of the dreaded X-only adressing mode...
 */

/* Read further down...
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
 * And again read further down...
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

/*
 * Salvation at last.
 * Why does one first has to read the gcc-ppc-port-source? Hmmm...
 *
 * GCC knows operant print modifiers, but this is AFAIK nowhere CLEARLY
 * documented (they are incidentally mentioned for 32 bit x86 to get a
 * grip on 64bit operants, but thats it).
 *
 * Now you may ask, why bother? What to do with them?
 * Let me explain:
 * First met them on the AVR port (8-Bit uController), to get to the low
 * and highbyte of asm-operants and other stuff you need due to HW constrains.
 * Example: (add to a 16-bit pointer and load from it) (right to left syntax)
 * 	asm ( "add %A0, %A2"
 * 	      "adc %B0, %B2"
 * 	      "ld  %1, %a0"
 * 	    : "=e" (pointer), "=r" (byte)
 * 	    : "r" (displacement) );
 * which may generate:
 * 	add r30, r24
 * 	adc r31, r25
 * 	ld  r22, X
 * (Note that this inline asm is pointless (most of the time...), GCC can
 * generate this code from the C source fine (most of the time...))
 *
 * Aha, look how r30&r31 get transformed to the X pointer reg (which share
 * the same location). OK, a little more grip into gcc internal foobar.
 *
 * Unfortunately i still not found a clear official listing of these modifiers.
 * For AVR i always go to a wiki-page (!!!) from some enthusiast users. (And
 * after some time you memorized the common ones, but still, when you need a
 * very special foo, back to the page, look it up)
 *
 * Why bother you with this on the PPC atomic ops?
 *
 * BECAUSE PPC ALSO HAS MORE THEN ENOUGH OF THESE MODIFIERS!!!
 *
 * Undocumented, AFAIK.
 * Wait, i forgot: "Read the Source, Luke"...
 * (rs6000.c:print_operant())
 *
 * With this you can write elaborate ppc inline asm like this:
 *
 * 	asm ("stw%U0%X0 %1, %0" : "=m" (*addr) : "r" (val));
 *
 * Huh? WTF? Exactly, now you may get:
 * 	stw   9,7
 * or
 * 	stwx  9,6,7
 * or
 * 	stwu  9,16(7)
 * or
 * 	stwux 9,6,7
 * %X0 injects an "x" for you when gcc generates an indexed memoperand.
 * %U0 injects an "u" for you when gcc thinks it needs the calked addr later.
 *
 * "And how does this solve our Problem?" you may ask.
 * lwarx/stwcx don't have a non-X-form. Emiting an x or not does not help us.
 * Wait and see...
 *
 * %y emits 0,r or ra,rb
 *
 * nice for X-form addresses (they put it under "Altivec or SPE", so
 * it may generate other stuff too, but only if the input operant matches ;).
 *
 * Unfortunately this does not solve it completely, we still need the right
 * constrain to be compatible with %y, which is "Z" (NOTE: it looks so
 * from the gcc-source, they use it themselves), other constrains allow addr-
 * modes which are not handled in %y and may generate an compiler error.
 * But this constrain exists only since >=gcc-4.0.
 * This can be rectified by the "V" constrain, not offsetable/no displacement
 * (%y is a printmodifier, it can not allocate a register for the
 * displacement), because the X-Form does not take an immediate displacement.
 * Then GCC should calc the address "by hand" and it then can be printed by
 * %y again (no worse than old status quo)
 *
 * So #define the constrain based on compiler version and always use %y.
 *
 * TODO: thorough test required
 * my crosscompiler cowardly refuses to generate indexed loads,
 * optimises them to pointer arith. to save loop counter
 */

# if _GNUC_PREREQ(4, 1)
#  define PPC_MEM_CONSTRAIN	"Z"
# else
#  define PPC_MEM_CONSTRAIN	"V"
# endif


static always_inline void *atomic_px(void *val, atomicptr_t *ptr)
{
	void *tmp;

	__asm__ __volatile__(
		"1:\n\t"
		"lwarx\t%0,%y2\n\t"
		PPC405_ERR77(%y2)
		"stwcx.\t%3,%y2\n\t"
		"bne-\t1b"
		SYNC
		: /* %0 */ "=&r" (tmp),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ PPC_MEM_CONSTRAIN (atomic_pread(ptr)),
		  /* %3 */ "r" (val),
		  /* %4 */ "m" (atomic_pread(ptr)) /* dependency only, see above */
		: "cc");

	return tmp;
}

static always_inline int atomic_x(int val, atomic_t *ptr)
{
	int tmp;

	__asm__ __volatile__(
		"1:\n\t"
		"lwarx\t%0,%y2\n\t"
		PPC405_ERR77(%y2)
		"stwcx.\t%3,%y2\n\t"
		"bne-\t1b"
		SYNC
		: /* %0 */ "=&r" (tmp),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ PPC_MEM_CONSTRAIN (atomic_read(ptr)),
		  /* %3 */ "r" (val),
		  /* %4 */ "m" (atomic_read(ptr)) /* dependency only, see above */
		: "cc");

	return tmp;
}

static always_inline void *atomic_cmppx(volatile void *nval, volatile void *oval, atomicptr_t *ptr)
{
	void *prev;

	__asm__ __volatile__ (
		"1:\n\t"
		"lwarx\t%0,%y2 \n\t"
		"cmpw\t0,%0,%3 \n\t"
		"bne\t2f\n\t"
		PPC405_ERR77(%y2)
		"stwcx.\t%4,%y2 \n\t"
		"bne-\t1b"
		SYNC
		"\n2:"
		: /* %0 */ "=&r" (prev),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_pread(ptr))
		: /* %2 */ PPC_MEM_CONSTRAIN (atomic_pread(ptr)),
		  /* %3 */ "r" (oval),
		  /* %4 */ "r" (nval),
		  /* %5 */ "m" (atomic_pread(ptr)) /* dependency only, see above */
		: "cc");

	return prev;
}

static always_inline void atomic_inc(atomic_t *ptr)
{
	int tmp;
	__asm__ __volatile__(
		"1:\n\t"
		"lwarx\t%0,%y2\n\t"
		"addic\t%0,%0,1\n\t"
		PPC405_ERR77(%y2)
		"stwcx.\t%0,%y2 \n\t"
		"bne-\t1b"
		: /* %0 */ "=&r" (tmp),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ PPC_MEM_CONSTRAIN (atomic_read(ptr)),
		  /* %3 */ "m" (atomic_read(ptr)) /* dependency only, see above */
		: "cc");
}

static always_inline void atomic_dec(atomic_t *ptr)
{
	int tmp;
	__asm__ __volatile__(
		"1:\n\t"
		"lwarx\t%0,%y2\n\t"
		"addic\t%0,%0,-1\n\t"
		PPC405_ERR77(%y2)
		"stwcx.\t%0,%y2\n\t"
		"bne-\t1b"
		: /* %0 */ "=&r" (tmp),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ PPC_MEM_CONSTRAIN (atomic_read(ptr)),
		  /* %3 */ "m" (atomic_read(ptr)) /* dependency only, see above */
		: "cc");
}

static always_inline int atomic_dec_return(atomic_t *ptr)
{
	int tmp;
	__asm__ __volatile__(
		"1:\n\t"
		"lwarx\t%0,%y2\n\t"
		"addic\t%0,%0,-1\n\t"
		PPC405_ERR77(%y2)
		"stwcx.\t%0,%y2\n\t"
		"bne-\t1b"
		SYNC
		: /* %0 */ "=&r" (tmp),
		/* gcc < 3 needs this, "+m" will not work reliable */
		  /* %1 */ "=m" (atomic_read(ptr))
		: /* %2 */ PPC_MEM_CONSTRAIN (atomic_read(ptr)),
		  /* %3 */ "m" (atomic_read(ptr)) /* dependency only, see above */
		: "cc");
	return tmp;
}

#define atomic_dec_test(x) (atomic_dec_return((x)) == 0)

#endif /* LIB_IMPL_ATOMIC_H */
