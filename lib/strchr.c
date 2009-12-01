/*
 * strchr.c
 * strchr, as a wrapper around strchrnul
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

/*
 * strchr - strchr
 * s: the string to search
 * c: the character to search
 *
 * return value: a pointer to the character c or NULL if not
 *               found
 *
 * Since we have a nice strchrnul, we simply provide this wrapper
 * to also have a nice strchr. This is as good as a native impl.
 * (both search for c or '\0', return NULL on '\0' is done in this
 * wrapper)
 * Since strchr is not a core function, this wrapper is OK.
 */

#define IN_STRWHATEVER
#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifndef STRCHRNUL_DEFINED
char *strchrnul(const char *s, int c);
# define STRCHRNUL_DEFINED
#endif
char *strchr(const char *s, int c);

/*
 * The truth is:
 * GCC fucks up the stack handling.
 * The ideal complete code for this function should look like
 *
 * call     strchrnul
 * movzbl  (%eax), %ecx
 * test     %ecx, %ecx
 * cmovz    %ecx, %eax
 * ret
 *
 * which is, minus the call and ret, the same code i would add
 * in a native impl. of strchr after copying the code of strchrnul.
 *
 * GCC creates this monster:
 * 00000000 <strchr>:
 *   0:   83 ec 0c                sub    $0xc,%esp
 *   3:   8b 4c 24 14             mov    0x14(%esp),%ecx
 *   7:   8b 44 24 10             mov    0x10(%esp),%eax
 *   b:   89 4c 24 04             mov    %ecx,0x4(%esp)
 *   f:   89 04 24                mov    %eax,(%esp)
 *  12:   e8 fc ff ff ff          call   13 <strchr+0x13>
 *  17:   89 c2                   mov    %eax,%edx
 *  19:   31 c0                   xor    %eax,%eax
 *  1b:   80 3a 00                cmpb   $0x0,(%edx)
 *  1e:   0f 45 c2                cmovne %edx,%eax
 *  21:   83 c4 0c                add    $0xc,%esp
 *  24:   c3                      ret
 *
 * Even if the core code looks different (maybe the compiler is
 * right, foo pipeline stalls, whatever), WTF is he mucking with
 * the stack and the args?
 */

char *strchr(const char *s, int c)
{
	char *ret_val;
	if(unlikely(!c))
		return (char *)(intptr_t)s + strlen(s);
	ret_val = strchrnul(s, c);
	return likely('\0' != *ret_val) ? ret_val : NULL;
}

/*@unused@*/
static char const rcsid_sc[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
