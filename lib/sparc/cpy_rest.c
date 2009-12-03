/*
 * cpy_rest.c
 * copy a byte trailer, sparc/sparc/64 impl.
 *
 * Copyright (c) 2009 Jan Seiffert
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
 * $Id: $
 */

#if defined(HAVE_REAL_V9) || defined(__sparcv9) || defined(__sparc_v9__)
# include "sparc_vis.h"
# define ALIGNMENT_WANTED SOVV

noinline GCC_ATTR_FASTCALL char *cpy_rest(char *dst, const char *src, unsigned i)
{
	const unsigned long long *src_p, *src_p_n;
	unsigned long long *dst_p;
	char *dst_c = dst, *dst_end, *dst_end2;
	const char *src_c = src, *src_end;
	unsigned long long s, s1, s0;
	unsigned emask, off;

	if(!i)
		return dst;

	dst_p = (void *)ALIGN_DOWN(dst_c, SOVV);
	off = SOVV - ALIGN_DOWN_DIFF(dst_c, SOVV);
	dst_end = (dst_c + i - 1);
	src_end = src_c + i;
	dst_end2 = dst_end - SOVV;

	/* gen edge mask */
	emask = edge8(dst_c, dst_end);
	/* prep source and set GSR */
	src_p = alignaddr(src_c, 0);
	/* load first 8 byte */
	s0 = *src_p++;
	/* already behind end? */
	if((const void *)src_p < (const void *)src_end)
		s1 = *src_p; /* cool, more bytes avail */
	else
		s1 = fzero(); /* no, fill with zeros */
	/* cut off garbage at start */
	s = faligndata(s0, s1);

	/* set GSR with offset */
	alignaddr(NULL, off);
	/* swizzle bytes in place */
	s = faligndata(s, s);
	/* store first 8 bytes according to mask */
	pst8_P(s, dst_p++, emask);

	/* everything written? */
	if((void *)dst_p > (void *)dst_end)
		goto out_nst;
	/* create next src */
	src_p_n = alignaddr(src_c, off);
	/* already behind end? */
	if((const void *)src_p >= (const void *)src_end)
		goto out_st; /* write last stuff from buffers */

	/* prep. next 8 byte */
	if(src_p_n != src_p)
		s1 = *src_p_n;
	src_p = src_p_n + 1;
	s0 = fpsrc1(s1);

	/* copy full 8 byte if we have them */
	if((void *)dst_p <= (void *)dst_end2)
	{
		s1 = *src_p++;
		s = faligndata(s0, s1);
		*dst_p++ = s;
		s0 = fpsrc1(s1);
	}

	/* read last bits, if avail. */
	if((const void *)src_p < (const void *)src_end)
		s1 = *src_p;
	else
		s1 = fzero();
	/* align last data */
	s = faligndata(s0, s1);
out_st:
	/* generate end mask */
	emask = edge8(dst_p, dst_end);
	/* write out tail */
	pst8_P(s, dst_p, emask);
out_nst:
	return dst_end + 1;
}

noinline GCC_ATTR_FASTCALL char *cpy_rest_o(char *dst, const char *src, unsigned i)
{
	const unsigned long long *src_p, *src_p_n;
	unsigned long long *dst_p;
	char *dst_c = dst, *dst_end, *dst_end2;
	const char *src_c = src, *src_end;
	unsigned long long s, s1, s0;
	unsigned emask, off;

	if(!i)
		return dst;

	dst_p = (void *)ALIGN_DOWN(dst_c, SOVV);
	off = SOVV - ALIGN_DOWN_DIFF(dst_c, SOVV);
	dst_end = (dst_c + i - 1);
	src_end = src_c + i;
	dst_end2 = dst_end - SOVV;

	/* gen edge mask */
	emask = edge8(dst_c, dst_end);
	/* prep source and set GSR */
	src_p = alignaddr(src_c, 0);
	/* load first 8 byte */
	s0 = *src_p++;
	/* already behind end? */
	if((const void *)src_p < (const void *)src_end)
		s1 = *src_p; /* cool, more bytes avail */
	else
		s1 = fzero(); /* no, fill with zeros */
	/* cut off garbage at start */
	s = faligndata(s0, s1);

	/* set GSR with offset */
	alignaddr(NULL, off);
	/* swizzle bytes in place */
	s = faligndata(s, s);
	/* store first 8 bytes according to mask */
	pst8_P(s, dst_p++, emask);

	/* everything written? */
	if((void *)dst_p > (void *)dst_end)
		goto out_nst;
	/* create next src */
	src_p_n = alignaddr(src_c, off);
	/* already behind end? */
	if((const void *)src_p >= (const void *)src_end)
		goto out_st; /* write last stuff from buffers */

	/* prep. next 8 byte */
	if(src_p_n != src_p)
		s1 = *src_p_n;
	src_p = src_p_n + 1;
	s0 = fpsrc1(s1);

	/* copy full 8 byte if we have them */
	if((void *)dst_p <= (void *)dst_end2)
	{
		s1 = *src_p++;
		s = faligndata(s0, s1);
		*dst_p++ = s;
		s0 = fpsrc1(s1);
	}

	/* read last bits, if avail. */
	if((const void *)src_p < (const void *)src_end)
		s1 = *src_p;
	else
		s1 = fzero();
	/* align last data */
	s = faligndata(s0, s1);
out_st:
	/* generate end mask */
	emask = edge8(dst_p, dst_end);
	/* write out tail */
	pst8_P(s, dst_p, emask);
out_nst:
	return dst;
}

GCC_ATTR_FASTCALL char *cpy_rest0(char *dst, const char *src, unsigned i)
{
	dst[i] = '\0';
	return cpy_rest(dst, src, i);
}

static char const rcsid_cprs[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/cpy_rest.c"
#endif
/* EOF */
