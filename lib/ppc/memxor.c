/*
 * memxor.c
 * xor two memory region efficient, ppc implementation
 *
 * Copyright (c) 2005-2008 Jan Seiffert
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
 */

#ifdef __ALTIVEC__
# include <altivec.h>
# define SOVUC (sizeof(vector unsigned char))
# define ARCH_NAME_SUFFIX _altivec
# define ALIGNMENT_WANTED SOVUC
#else
# define ARCH_NAME_SUFFIX _ppc
# define ALIGNMENT_WANTED SOST
#endif

/*
 * blaerch, what have they done to my 68k...
 * Oh, and a WARNING: totaly untested, i don't own the hardware.
 */
/*
 * Newsflash!
 * PPC seems to be able to cope with unaligned access.
 * I always thought it would bomb out like an m68k, but no.
 * As always with ppc, they like to complicate the matter:
 * According to latest PowerPC ISA little endian (WTF? XBox-360?)
 * servergrade CPUs have "poor performance" in such a case, while
 * otherwise performance should be "good".
 *
 * I simply assume that larger access is always better then byte
 * access, basically the x86 trick.
 */
void *memxor(void *dst, const void *src, size_t len)
{
	char *dst_char = dst;
	const char *src_char = src;

	if(!dst || !src)
		return dst;

	/* we will kick this mem, fetch it... */
	prefetch(src_char);
	prefetch(src_char + 32);
	prefetch(src_char + 64);
	prefetch(src_char + 96);
	prefetchw(dst_char);
	prefetchw(dst_char + 32);
	prefetchw(dst_char + 64);
	prefetchw(dst_char + 96);

	if(SYSTEM_MIN_BYTES_WORK > len)
		goto handle_remaining;
	else
	{
		/* align dst, we will see what src looks like afterwards */
		size_t i = ALIGN_DIFF(dst_char, ALIGNMENT_WANTED);
#ifdef __ALTIVEC__
		i += SOVUC; /* make sure src is at least one vector in the memblock */
#endif
		len -= i;
		for(; i/SO32; i -= SO32, dst_char += SO32, src_char += SO32)
			*((uint32_t *)dst_char) ^= *((const uint32_t *)src_char);
		for(; i; i--)
			*dst_char++ ^= *src_char++;
		i = (((intptr_t)dst_char) & ((ALIGNMENT_WANTED * 2) - 1)) ^
		    (((intptr_t)src_char) & ((ALIGNMENT_WANTED * 2) - 1));
		/*
		 * We simply align the write side, and "don't care" for
		 * the read side.
		 * Either its also aligned by accident, or working
		 * unaligned dwordwise is still faster than bytewise.
		 */
		if(unlikely(i &  1))
			goto no_alignment_wanted;
		if(unlikely(i &  2))
			goto no_alignment_wanted;
// TODO: Does ppc64 handle unaligned 64Bit reads? ppc does not. Could hurt really bad (soft emu)
		if(unlikely(i &  4))
			goto alignment_size_t;
		if(i &  8)
			goto alignment_8;
		/* fall throuh */
		goto alignment_16;
		/* make label used */
		goto handle_remaining;
	}

	/* fall throuh if alignment fails */
	goto no_alignment_possible;
/* Special implementations below here */

	/*
	 * xor it with a hopefully bigger and
	 * maschine-native datatype
	 */
#ifdef __ALTIVEC__
	/*
	 * Oh man...
	 * You can tell me what you want, but for me this looks like:
	 *
	 * Steve:    "Hey guys, Intel came up with MMX!"
	 * <others>:  zzzzZZZzzzz...
	 * Steve:    "HEY!"
	 * IBM:      "Huh? MMX? What? Ahhh, Intel. Yeah, look, its from
	 *            Intel, we know them, pigs can fly..."
	 * Steve:    "Yeah, but we need to do something! Leading multimedia
	 *            market position yadda yadda yadda"
	 * Motorola: "Hmmm, Oh, K"
	 *            *search*
	 *           "Maybe ..."
	 *            *scratch head*
	 *           "There must be an old DSP blueprint from us down
	 *            here in the bottom drawer ..."
	 *            *dust* *cough*
	 *           "Ah, there you go ... we could simply piggy pack
	 *            this if we connect pin ..."
	 * Steve:    "Make it so!"
	 *
	 * Altivec can pump xx GB of data through its pipelines when
	 * simple instruction are computet (like this), but the FSB
	 * gets filled VERY early (G5 2 GHz ~> 1.2 GB), even the L1
	 * cache can not keep up with the Altivec engine ( WTF? ;( ).
	 *
	 * In essence a Citroen 2CV with a Ferrari engine...
	 *
	 * If Apple, IBM an Freescale would had *really* worked on
	 * it to solve this - killer...
	 * And i wished Intel would have welded a real DSP onto their x86,
	 * but since they don't seem to grasp the concept of a DSPs and
	 * always build FPUs on steroids...
	 *
	 * So since this is totaly IO bound, let the compiler do the
	 * dirty work, esp. prefetch, unroll.
	 * WAIT, NO, don't trust the compiler to much.
	 * Don't forget to switch on the compiler flags:
	 *  -Ox {-mcpu=bla|-maltivec} -fprefetch
	 */
alignment_16:
	if(len / SOVUC)
	{
		register vector unsigned char *dst_vec = (vector unsigned char *) dst_char;
		register vector const unsigned char *src_vec = (vector const unsigned char *) src_char;
		register vector unsigned char v[8];
		size_t small_len = len / SOVUC;
		register size_t smaller_len = small_len / 4;
		small_len %= 4;

		for(; smaller_len--; dst_vec += 4, src_vec += 4)
		{
			v[0] = vec_ld(0 * SOVUC, src_vec);
			v[4] = vec_ld(0 * SOVUC, dst_vec);
			v[1] = vec_ld(1 * SOVUC, src_vec);
			v[5] = vec_ld(1 * SOVUC, dst_vec);
			v[2] = vec_ld(2 * SOVUC, src_vec);
			v[6] = vec_ld(2 * SOVUC, dst_vec);
			v[3] = vec_ld(3 * SOVUC, src_vec);
			v[7] = vec_ld(3 * SOVUC, dst_vec);
			v[0] = vec_xor(v[0], v[4]);
			v[1] = vec_xor(v[1], v[5]);
			v[2] = vec_xor(v[2], v[6]);
			v[3] = vec_xor(v[3], v[7]);
			vec_st(v[0], 0 * SOVUC, dst_vec);
			vec_st(v[1], 1 * SOVUC, dst_vec);
			vec_st(v[2], 2 * SOVUC, dst_vec);
			vec_st(v[3], 3 * SOVUC, dst_vec);
		}
		for(; small_len--; dst_vec++, src_vec++)
			*dst_vec = vec_xor(*dst_vec, *src_vec);

		len %= SOVUC;
		dst_char  = (char *) dst_vec;
		src_char  = (const char *) src_vec;
		goto handle_remaining;
	}
alignment_8:
alignment_size_t:
no_alignment_wanted:
	/*
	 * Altivec can also handle unaligned data, but more the DSP-way
	 * of doing things: Simply do not connect some address lines
	 * (Lower address bits? Totally over estimed!) and shuffle the
	 * stuff around with funky instruction in lots of registers.
	 * Problem with this: We better stay in the passed mem region...
	 * And to make it efficient you better unrool your loop quite
	 * agressively.
	 */
	if(len/(4*SOVUC))
	{
		/* dst is aligned */
		register vector unsigned char *dst_vec = (vector unsigned char *) dst_char;
		/* only src sucks */
		register vector const unsigned char *src_vec;
		vector unsigned char v[9];          /* 0-8 */
		vector unsigned char vd[8];         /* 9-16 */
		vector unsigned char fix_alignment; /* 17 */
		size_t small_len = (len / SOVUC) - 1; /* make shure not to overread */
		register size_t smaller_len = small_len / 8;
		small_len %= 8;

		fix_alignment = vec_lvsl(0, (const volatile unsigned char *)src_char);
		src_vec = (vector const unsigned char *) src_char;
		v[8] = vec_ld(0, src_vec);
		while(smaller_len--)
		{
			/* load src */
			v[0] = v[8];
			v[1] = vec_ld(1 * SOVUC, src_vec); /* load always rounds address down */
			v[2] = vec_ld(2 * SOVUC, src_vec);
			v[3] = vec_ld(3 * SOVUC, src_vec);
			v[4] = vec_ld(4 * SOVUC, src_vec);
			v[5] = vec_ld(5 * SOVUC, src_vec);
			v[6] = vec_ld(6 * SOVUC, src_vec);
			v[7] = vec_ld(7 * SOVUC, src_vec);
			v[8] = vec_ld(8 * SOVUC, src_vec);
			src_vec += 8;

			/* load dest */
			vd[0] = vec_ld(0 * SOVUC, dst_vec);
			vd[1] = vec_ld(1 * SOVUC, dst_vec);
			vd[2] = vec_ld(2 * SOVUC, dst_vec);
			vd[3] = vec_ld(3 * SOVUC, dst_vec);
			vd[4] = vec_ld(4 * SOVUC, dst_vec);
			vd[5] = vec_ld(5 * SOVUC, dst_vec);
			vd[6] = vec_ld(6 * SOVUC, dst_vec);
			vd[7] = vec_ld(7 * SOVUC, dst_vec);

			/* permutate src for alignment */
			v[0] = vec_perm(v[0], v[1], fix_alignment);
			v[1] = vec_perm(v[1], v[2], fix_alignment);
			v[2] = vec_perm(v[2], v[3], fix_alignment);
			v[3] = vec_perm(v[3], v[4], fix_alignment);
			v[4] = vec_perm(v[4], v[5], fix_alignment);
			v[5] = vec_perm(v[5], v[6], fix_alignment);
			v[6] = vec_perm(v[6], v[7], fix_alignment);
			v[7] = vec_perm(v[7], v[8], fix_alignment);

			/* xor it */
			vd[0] = vec_xor(vd[0], v[0]);
			vd[1] = vec_xor(vd[1], v[1]);
			vd[2] = vec_xor(vd[2], v[2]);
			vd[3] = vec_xor(vd[3], v[3]);
			vd[4] = vec_xor(vd[4], v[4]);
			vd[5] = vec_xor(vd[5], v[5]);
			vd[6] = vec_xor(vd[6], v[6]);
			vd[7] = vec_xor(vd[7], v[7]);

			/* store it */
			vec_st(vd[0], 0 * SOVUC, dst_vec);
			vec_st(vd[1], 1 * SOVUC, dst_vec);
			vec_st(vd[2], 2 * SOVUC, dst_vec);
			vec_st(vd[3], 3 * SOVUC, dst_vec);
			vec_st(vd[4], 4 * SOVUC, dst_vec);
			vec_st(vd[5], 5 * SOVUC, dst_vec);
			vec_st(vd[6], 6 * SOVUC, dst_vec);
			vec_st(vd[7], 7 * SOVUC, dst_vec);
			dst_vec += 8;
		}
		for(; small_len--; dst_vec++, src_vec++)
		{
			v[0] = v[8];
			v[8] = vec_ld(1 * SOVUC, src_vec);
			v[0] = vec_perm(v[0], v[8], fix_alignment);
			*dst_vec = vec_xor(*dst_vec, v[0]);
		}

// TODO: Hmmm, how many bytes are left???
		len %= SOVUC;
		dst_char  = (char *) dst_vec;
		src_char  = (const char *) src_vec;
		goto handle_remaining;
	}
#else
alignment_16:
alignment_8:
alignment_size_t:
no_alignment_wanted:
#endif
/* All archs fallback-code
 * (hmmm, runs 3 times faster on Sparc)
 */
#ifdef __powerpc64__
	if(len/SOST)
	{
		register size_t *dst_sizet = (size_t *)dst_char;
		register const size_t *src_sizet = (const size_t *)src_char;
		register size_t small_len = len / SOST;
		len %= SOST;

		while(small_len--)
			*dst_sizet++ ^= *src_sizet++;

		dst_char = (char *) dst_sizet;
		src_char = (const char *) src_sizet;
		goto handle_remaining;
	}
#endif

handle_remaining:
	if(len/4)
	{
		register uint32_t *dst_u32 = (uint32_t *)dst_char;
		register const uint32_t *src_u32 = (const uint32_t *)src_char;
		register size_t small_len = len / 4;
		len %= 4;

		while(small_len--)
			*dst_u32++ ^= *src_u32++;

		dst_char = (char *) dst_u32;
		src_char = (const char *) src_u32;
	}

no_alignment_possible:
	/* xor whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ ^= *src_char++;

	return dst;
}

static char const rcsid_mx[] GCC_ATTR_USED_VAR = "$Id:$";
