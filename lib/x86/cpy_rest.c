/*
 * cpy_rest.c
 * copy a byte trailer, x86 impl
 *
 * Copyright (c) 2008-2011 Jan Seiffert
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
 * $Id: $
 */

/*
 * I confess, its slower than the generic version,
 * but a computet goto looks so nice in kcachegrind
 * (the sizzle sells the steak), esp on x86 which
 * makes this hard with it's variable length
 * instructions. Most compiler will NEVER
 * generate this.
 */
/*
 * Making this an extern function makes the stack handling ugly
 * (compiler generates a frame to save edi/esi), but saves lots
 * of space.
 */
noinline GCC_ATTR_FASTCALL char *cpy_rest(char *dst, const char *src, unsigned i)
{
	size_t t;

// TODO: Optimize for size (more jumps) or less (fixed) jumps
	asm(
#ifdef __i386__
		"xchg	%0, %%edi\n\t"
		"xchg	%1, %%esi\n\t"
# define MOVSQ	"movsl\n\tmovsl\n\t"
		"neg	%4\n\t"
# ifndef __PIC__
#  define CLOB
#  define JUMP_POS (i-15)
		"lea	1f(,%4,8), %2\n\t"
# else
#  define CLOB
#  define JUMP_POS (i-15)
	/*
	 * PIC must be the acronym for Pain In the Chest ;)
	 * Hint: a compiler jump table does not look better,
	 * target addresses also have to be loaded somewhere
	 * from PIC-storage. This code is not meant to be
	 * put into a .so, but to give no grief for those
	 * compiling their executables with -fPIC let them
	 * pay their bill...
	 */
		"lea	1f-3f(,%4,8), %2\n\t"
		"call	i686_add_pc\n"
		"3:\n\t"
		".subsection 2\n"
		"i686_add_pc:\n\t"
		"addl (%%esp), %2\n\t"
		"ret\n\t"
		".previous\n\t"
# endif
#else
# define CLOB "&"
# define MOVSQ	"movsq\n\t"
# define JUMP_POS (15-i)
	/*
	 * Legacy galore...
	 * Since this is not a compiler generated addressing
	 * we better make sure it "works". Using the
	 * addressing sheme from above gives us a working
	 * Symbol + where to go absolut address, cool.
	 * But on amd64 it seems you can only encode a 32 Bit
	 * displacement, so this symbol-is-displacement-
	 * absolut-from-zero-trick means you programm better
	 * runs in the lower 4GB mem. We can not guarantee this
	 * since OSs are normaly free to put you where they
	 * want, for what ever profit. The toolchain for
	 * this configuration will generate code which works.
	 *
	 * So this has to be solved with position independent
	 * code (PIC).
	 *
	 * PIC is on x86 normaly a PITA. Accessing your position
	 * is heavy. Good for your code...
	 * So AMD, not taking as many drugs as Intel, warped
	 * x86 to the 21st century and implemented instruction
	 * pointer relative addressing. Yeah! OK, 64 bit
	 * constants everywhere are a bit heavy...
	 * Unfortunatly, either trapped in the x86 legacy
	 * structure, or also not really sober, you can not
	 * use all x86 addressing modes with ipra, only
	 * displacement...
	 * So do it in two steps.
	 */
		"lea	1f(%%rip), %2\n\t"
		"lea	(%2,%q4,8), %2\n\t"
#endif
		"jmp	*%2\n\t" /* computet goto FTW */
		".p2align 4,,7\n\t"
		".p2align 3\n"
		"1:\n\t"
	/*  0 */
		MOVSQ		/* 2 */
		"movsl\n\t"	/* 1 */
		"movsw\n\t"	/* 2 */
		"movsb\n\t"	/* 1 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/*  8 */
		MOVSQ		/* 2 */
		"movsl\n\t"	/* 1 */
		"movsw\n\t"	/* 2 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 16 */
		MOVSQ		/* 2 */
		"movsl\n\t"	/* 1 */
		"movsb\n\t"	/* 1 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 24 */
		MOVSQ		/* 2 */
		"movsl\n\t"	/* 1 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 32 */
		MOVSQ		/* 2 */
		"movsw\n\t"	/* 2 */
		"movsb\n\t"	/* 1 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 40 */
		MOVSQ		/* 2 */
		"movsw\n\t"	/* 2 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 48 */
		MOVSQ		/* 2 */
		"movsb\n\t"	/* 1 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 56 */
		MOVSQ		/* 2 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 64 */
		"movsl\n\t"	/* 1 */
		"movsw\n\t"	/* 2 */
		"movsb\n\t"	/* 1 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 72 */
		"movsl\n\t"	/* 1 */
		"movsw\n\t"	/* 2 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 80 */
		"movsl\n\t"	/* 1 */
		"movsb\n\t"	/* 1 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 88 */
		"movsl\n\t"	/* 1 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 96 */
		"movsw\n\t"	/* 2 */
		"movsb\n\t"	/* 1 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 104 */
		"movsw\n\t"	/* 2 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 112 */
		"movsb\n\t"	/* 1 */
		"jmp	2f\n\t"	/* 2 */
		".p2align 3, 0x90\n\t"
	/* 120 */
		"2:\n\t"
#ifdef __i386__
		"xchg	%0, %%edi\n\t"
		"mov	%1, %%esi\n\t"
#endif
	:
#ifdef __i386__
	  /* %0 */ "=a" (dst),
	  /* %1 */ "=d" (src),
#else
	  /* %0 */ "=D" (dst),
	  /* %1 */ "=S" (src),
#endif
	  /* %2 */ "="CLOB"r" (t),
	  /* %3 */ "=m" (*dst)
	: /* %4 */ "r" (JUMP_POS),
	  /* %5 */ "0" (dst),
	  /* %6 */ "1" (src)
	);
	return dst;
#undef CLOB
#undef MOVSQ
}

char GCC_ATTR_FASTCALL *cpy_rest_o(char *dst, const char *src, unsigned i)
{
	cpy_rest(dst, src, i);
	return dst;
}

char GCC_ATTR_FASTCALL *cpy_rest0(char *dst, const char *src, unsigned i)
{
	dst[i] = '\0';
	return cpy_rest(dst, src, i);
}

static char const rcsid_cprx[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
