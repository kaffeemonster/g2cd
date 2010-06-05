/*
 * gup.h
 * grand unified poller, header file
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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

#ifndef GUP_H
# define GUP_H

/* Own */
# include "lib/other.h"

enum guppies
{
	GUP_G2CONNEC_HANDSHAKE,
	GUP_G2CONNEC,
	GUP_ACCEPT,
	GUP_UDP,
	GUP_ABORT,
};

struct simple_gup
{
	enum guppies gup;
	int fd;
};

union gup
{
	enum guppies gup;
	struct simple_gup s_gup;
/*	struct g2_connection g2_gup; */ /* circle dependency */
};

/* forward declare */
struct g2_packet;
struct g2_connection;

# ifndef GUP_C
#  define GUP_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define GUP_EXTRN(x) x GCC_ATTR_VIS("hidden")
# endif /* GUP_C */

GUP_EXTRN(void *gup(void *));
GUP_EXTRN(int accept_timeout(void *));
GUP_EXTRN(int handler_active_timeout(void *arg));
GUP_EXTRN(int handler_z_flush_timeout(void *arg));
GUP_EXTRN(void g2_handler_con_mark_write(struct g2_packet *, struct g2_connection *));
GUP_EXTRN(void gup_con_mark_write(struct g2_connection *));
#endif /* GUP_H */
/* EOF */
