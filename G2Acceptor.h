/*
 * G2Acceptor.h
 * header-file for G2Acceptor.c
 *
 * Copyright (c) 2004, Jan Seiffert
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
 * $Id: G2Acceptor.h,v 1.5 2004/12/18 18:06:13 redbully Exp redbully $
 */

#ifndef _G2ACCEPTOR_H
#define _G2ACCEPTOR_H

#define BACKLOG 12

#ifndef _G2ACCEPTOR_C
#define _G2ACC_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#else
#define _G2ACC_EXTRN(x) x GCC_ATTR_VIS("hidden")
#endif /* _G2ACCEPTOR_C */

_G2ACC_EXTRN(void *G2Accept(void *));

#endif //_G2ACCEPTOR_H
//EOF
