/*
 * mem_spn_ff.c
 * count 0xff span length, x86 implementation
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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
 * Intel has a good feeling how to create incomplete and
 * unsymetric instruction sets, so sometimes there is a
 * last "missing link".
 * Hammering on multiple integers was to be implemented with
 * MMX, but SSE finaly brought usefull comparison, but not
 * for 128 Bit SSE datatypes, only the old 64 Bit MMX...
 * SSE2 finaly brought fun for 128Bit.
 *
 * I would have a pure MMX version, but it is slower than the
 * generic version (58000ms vs. 54000ms), MMX was a really
 * braindead idea without a MMX->CPU-Reg move instruction
 * to do things not possible with MMX.
 */
/*
 * Sometimes you need a new perspective, like the altivec
 * way of handling things.
 * Lower address bits? Totaly overestimated.
 *
 * We don't precheck for alignment, 16 or 8 is very unlikely
 * instead we "align hard", do one load "under the address",
 * mask the excess info out and afterwards we are fine to go.
 *
 * Even this beeing a mem* function, the len can be seen as a
 * "hint". We can overread and underread, but should cut the
 * result (and not pass a page boundery, but we cannot because
 * we are aligned).
 */
/*
 * Normaly we could generate this with the gcc vector
 * __builtins, but since gcc whines when the -march does
 * not support the operation and we want to always provide
 * them and switch between them...
 */


#include "x86_features.h"

#define SOV8	8
#define SOV8M1	(SOV8-1)
#define SOV16	16
#define SOV16M1	(SOV16-1)

static size_t mem_spn_ff_x86(const void *s, size_t len)
{
	const unsigned char *p = s;

// TODO: do the align dance?
	if(len / SOST)
	{
		size_t t, u;
		asm (
			"repe scas (%0), %3\n\t"
			"je	1f\n\t"
			"sub	%4, %0\n\t"
			"lea	%c4(%2, %1, %c4), %2\n"
			"1:"
			: /* %0 */ "=D" (p),
			  /* %1 */ "=c" (t),
			  /* %2 */ "=r" (len),
			  /* %3 */ "=a" (u)
			: /* %4 */ "i" (SOST),
			  "0" (p),
			  "1" (len / SOST),
			  "2" (len % SOST),
			  "3" (~(size_t)0),
			  "m" (*(const size_t *)p)
		);
	}
	if(SOST != SO32)
	{
		if(len >= SO32 && 0xFFFFFFFFu == *(const uint32_t *)p) {
			p   += SO32;
			len -= SO32;
		}
	}
	if(len >= sizeof(uint16_t) && 0xFFFFu == *(const uint16_t *)p) {
		p   += sizeof(uint16_t);
		len -= sizeof(uint16_t);
	}
	if(len && 0xff == *p) {
		p++;
		len--;
	}

	return (size_t)(p - (const unsigned char *)s);
}

static __init_cdata const struct test_cpu_feature t_feat[] =
{
	{.func = (void (*)(void))mem_spn_ff_x86,     .features = {}, .flags = CFF_DEFAULT},
};

static size_t mem_spn_ff_runtime_sw(const void *s, size_t len);
/*
 * Func ptr
 */
static size_t (*mem_spn_ff_ptr)(const void *s, size_t len) = mem_spn_ff_runtime_sw;

/*
 * constructor
 */
static GCC_ATTR_CONSTRUCT __init void mem_spn_ff_select(void)
{
	mem_spn_ff_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer fails
 */
static __init size_t mem_spn_ff_runtime_sw(const void *s, size_t len)
{
	mem_spn_ff_select();
	return mem_spn_ff_ptr(s, len);
}

size_t mem_spn_ff(const void *s, size_t len)
{
	return mem_spn_ff_ptr(s, len);
}

static char const rcsid_msfx[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
