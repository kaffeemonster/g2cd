/*
 * x86.h
 * some x86 defines
 *
 * Copyright (c) 2006,2007 Jan Seiffert
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

#ifndef _X86_H
# define _X86_H

# ifdef __SSE__
#  define SSE_PREFETCH(x) "prefetchnta	" #x "\n\t"
#  define SSE_FENCE	"sfence\n"
#  ifdef __SSE2__
#   define SSE_MOVE(x, y) "movdqa	" #x ", " #y "\n\t"
#   define SSE_STORE(x, y) "movntdq	" #x ", " #y "\n\t"
#   define SSE_AND(x, y) "pand	" #x ", " #y "\n\t"
#   define SSE_XOR(x, y) "pxor	" #x ", " #y "\n\t"
#  else
#   define SSE_MOVE(x, y) "movaps	" #x ", " #y "\n\t"
#   define SSE_STORE(x, y) "movntps	" #x ", " #y "\n\t"
#   define SSE_AND(x, y) "andps	" #x ", " #y "\n\t"
#   define SSE_XOR(x, y) "xorps	" #x ", " #y "\n\t"
#  endif
# endif
# ifdef __MMX__
#  ifdef __SSE__
#	 define MMX_PREFETCH(x) "prefetchnta	" #x "\n\t"
#   define MMX_STORE(x, y) "movntq	" #x ", " #y "\n\t"
#   define MMX_FENCE	"sfence\n"
#  else
#   define MMX_PREFETCH(x)
#   define MMX_STORE(x, y) "movq	" #x ", " #y "\n\t"
#   define MMX_FENCE
#  endif
# endif
#endif /* _X86_H */
