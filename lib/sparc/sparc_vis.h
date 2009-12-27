/*
 * sparc_vis.h
 * sparc vis instructions
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

#ifndef SPARC_VIS_H
# define SPARC_VIS_H

# define mkstr2(s) #s
# define mkstr(s) mkstr2(s)
# define SOVV (sizeof(unsigned long long))

/* vec_lvl */
static inline void *alignaddr(const void *ptr, int off)
{
	void *t;
	asm volatile ("alignaddr	%1, %2, %0" : "=r" (t) : "r" (ptr), "r" (off));
	return t;
}

/* vec_lvr */
static inline void *alignaddrl(const void *ptr, int off)
{
	void *t;
	asm volatile ("alignaddrl	%1, %2, %0" : "=r" (t) : "r" (ptr), "r" (off));
	return t;
}

#ifndef HAVE_VIS2
# define EDGE_FUNC(x) \
static GCC_ATTR_CONST inline unsigned edge##x(const void *p1, const void *p2) { \
	unsigned t; \
	asm ("edge"mkstr(x)"	%1, %2, %0" : "=r" (t) : "r" (p1), "r" (p2)); \
	return t; }
#else
# define EDGE_FUNC(x) \
static GCC_ATTR_CONST inline unsigned edge##x(const void *p1, const void *p2) { \
	unsigned t; \
	asm ("edge"mkstr(x)"n	%1, %2, %0" : "=r" (t) : "r" (p1), "r" (p2)); \
	return t; }

static inline void write_bmask1(unsigned a)
{
	asm volatile ("bmask	%0, %%g0, %%g0" : : "r" (a));
}

static inline void write_bmask2(unsigned a, unsigned b)
{
	asm volatile ("bmask	%0, %1, %%g0" : : "r" (a), "r" (b));
}

static inline unsigned long long bshuffle(unsigned long long a, unsigned long long b)
{
	unsigned long long ret;
	asm volatile ("bshuffle	%1, %2, %0" : "=e" (ret) : "e" (a), "e" (b));
	return ret;
}
#endif

EDGE_FUNC(8)
EDGE_FUNC(8l)
EDGE_FUNC(16)
EDGE_FUNC(16l)
EDGE_FUNC(32)
EDGE_FUNC(32l)

static inline unsigned read_gsr_32(void)
{
	unsigned t;
	asm volatile ("rd %%gsr, %0" : "=r" (t));
	return t;
}

static inline void write_gsr_32(unsigned t)
{
	asm volatile ("wr %%g0, %0, %%gsr" : : "rH" (t));
}
#define read_gsr() read_gsr_32()
#define write_gsr(x) write_gsr_32(x)

static inline unsigned long long read_gsr_64(void)
{
	unsigned long long t;
	asm volatile ("rd %%gsr, %0" : "=r" (t));
	return t;
}

static inline void write_gsr_64(unsigned long long t)
{
	asm volatile ("wr %%g0, %0, %%gsr" : : "rH" (t));
}

static inline unsigned long long faligndata(unsigned long long a, unsigned long long b)
{
	unsigned long long ret;
	asm volatile ("faligndata	%1, %2, %0" : "=e" (ret) : "e" (a), "e" (b));
	return ret;
}

static GCC_ATTR_CONST inline unsigned long long fzero(void)
{
	unsigned long long t;
	asm ("fzero %0" : "=e" (t));
	return t;
}

static GCC_ATTR_CONST inline unsigned long long fone(void)
{
	unsigned long long t;
	asm ("fone %0" : "=e" (t));
	return t;
}

static GCC_ATTR_CONST inline unsigned long long fpmerge(unsigned a, unsigned b)
{
	unsigned long long t;
	asm ("fpmerge	%1, %2, %0" : "=e" (t) : "e" (a), "e" (b));
	return t;
}

static inline unsigned fpack16(unsigned long long a)
{
	unsigned t;
	asm volatile ("fpack16	%1, %0" : "=e" (t) : "e" (a));
	return t;
}

static inline unsigned fpackfix(unsigned long long a)
{
	unsigned t;
	asm volatile ("fpackfix	%1, %0" : "=e" (t) : "e" (a));
	return t;
}

static inline unsigned long long fpack32(unsigned long long a, unsigned long long b)
{
	unsigned long long t;
	asm volatile ("fpack32	%1, %2, %0" : "=e" (t) : "e" (a), "e" (b));
	return t;
}

static GCC_ATTR_CONST inline unsigned long long fexpand(unsigned a)
{
	unsigned long long t;
	asm ("fexpand	%1, %0" : "=e" (t) : "e" (a));
	return t;
}

static GCC_ATTR_CONST inline unsigned fhigh(unsigned long long a)
{
	unsigned t;
	asm ("fmovs	%H1, %0" : "=e" (t) : "e" (a));
	return t;
}

#define CMP_FUNC(x) \
static GCC_ATTR_CONST inline unsigned fcmp##x(unsigned long long a, unsigned long long b) { \
	unsigned t; \
	asm ("fcmp"mkstr(x)"	%1, %2, %0" : "=r" (t) : "e" (a), "e" (b)); \
	return t; }
#define CMPC_FUNC(x) \
static GCC_ATTR_CONST inline unsigned fcmp##x(unsigned long long a, unsigned long long b) { \
	unsigned t; \
	asm ("fcmp"mkstr(x)"	%1, %2, %0" : "=r" (t) : "%e" (a), "e" (b)); \
	return t; }

CMP_FUNC(gt16)
CMP_FUNC(gt32)
CMP_FUNC(le16)
CMP_FUNC(le32)
CMPC_FUNC(ne16)
CMPC_FUNC(ne32)
CMPC_FUNC(eq16)
CMPC_FUNC(eq32)

#define LOGC_FUNC3(x) \
static GCC_ATTR_CONST inline unsigned long long fp##x(unsigned long long a, unsigned long long b) { \
	unsigned long long t; \
	asm ("f"mkstr(x)"	%1, %2, %0" : "=e" (t) : "%e" (a), "e" (b)); \
	return t; }
#define LOG_FUNC3(x) \
static GCC_ATTR_CONST inline unsigned long long fp##x(unsigned long long a, unsigned long long b) { \
	unsigned long long t; \
	asm ("f"mkstr(x)"	%1, %2, %0" : "=e" (t) : "e" (a), "e" (b)); \
	return t; }
#define LOG_FUNC2(x) \
static GCC_ATTR_CONST inline unsigned long long fp##x(unsigned long long a) { \
	unsigned long long t; \
	asm ("f"mkstr(x)"	%1, %0" : "=e" (t) : "e" (a)); \
	return t; }

LOGC_FUNC3(or)
LOGC_FUNC3(nor)
LOGC_FUNC3(and)
LOGC_FUNC3(nand)
LOGC_FUNC3(xor)
LOGC_FUNC3(xnor)
LOG_FUNC3(ornot1)
LOG_FUNC3(ornot2)
LOG_FUNC3(andnot1)
LOG_FUNC3(andnot2)
LOG_FUNC2(src1)
LOG_FUNC2(src2)
LOG_FUNC2(not1)
LOG_FUNC2(not2)

#define ARITH_FUNC(x) \
static GCC_ATTR_CONST inline unsigned long long fp##x(unsigned long long a, unsigned long long b) { \
	unsigned long long t; \
	asm ("fp"mkstr(x)"	%1, %2, %0" : "=e" (t) : "e" (a), "e" (b)); \
	return t; }
#define ARITHC_FUNC(x) \
static GCC_ATTR_CONST inline unsigned long long fp##x(unsigned long long a, unsigned long long b) { \
	unsigned long long t; \
	asm ("fp"mkstr(x)"	%1, %2, %0" : "=e" (t) : "%e" (a), "e" (b)); \
	return t; }

ARITHC_FUNC(add16)
ARITHC_FUNC(add32)
ARITH_FUNC(sub16)
ARITH_FUNC(sub32)

static GCC_ATTR_CONST inline unsigned long long pdist(unsigned long long a, unsigned long long b, unsigned long long acc)
{
	asm ("pdist	%1, %2, %0" : "=e" (acc) : "e" (a), "e" (b), "0" (acc));
	return acc;
}

#define PST_FUNC(x) \
static inline void pst##x(unsigned long long a, void *ptr, unsigned mask) \
{ asm ("stda	%1, [%2 + %3] #ASI_PST"mkstr(x) : "=m" (*(unsigned long long *)ptr) : "e" (a), "r" (ptr), "r" (mask), "m" (*(unsigned long long *)ptr)); }

PST_FUNC(8_P)
PST_FUNC(8_PL)
PST_FUNC(16_P)
PST_FUNC(16_PL)
PST_FUNC(32_P)
PST_FUNC(32_PL)

#undef EDGE_FUNC
#undef CMP_FUNC
#undef LOGC_FUNC3
#undef LOG_FUNC3
#undef LOG_FUNC2
#undef ARITHC_FUNC
#undef ARITH_FUNC
#undef PST_FUNC
#undef mkstr2
#undef mkstr

#endif
