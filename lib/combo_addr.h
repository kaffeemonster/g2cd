/*
 * combo_addr.h
 *
 * combined IPv4 & IPv6 address
 *
 * Copyright (c) 2008 Jan Seiffert
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

#ifndef COMBO_ADDR_H
# define COMBO_ADDR_H
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include "../other.h"

/*
 * Comboaddress to represent IPv4 & IPv6
 */
union combo_addr
{
	/* dirty deads done dirt cheap,
	 * sa_fam should alias with the first member
	 * off all struct: *_family
	 */
	sa_family_t         s_fam;
	struct sockaddr     sa;
	struct sockaddr_in  in;
	struct sockaddr_in6 in6;
};

static inline const char *combo_addr_print(const union combo_addr *src, char *dst, socklen_t cnt)
{
	return inet_ntop(src->s_fam,
		AF_INET == src->s_fam ? (const void *)&src->in.sin_addr : (const void *)&src->in6.sin6_addr,
		dst, cnt);
}

static inline int combo_addr_read(const char *src, union combo_addr *dst)
{
	int ret_val;

	ret_val = inet_pton(AF_INET, src, &dst->in.sin_addr);
	if(0 < ret_val)
		dst->s_fam = AF_INET;
	else if(0 == ret_val)
	{
		ret_val = inet_pton(AF_INET6, src, &dst->in6.sin6_addr);
		if(0 < ret_val)
			dst->s_fam = AF_INET6;
	}
	return ret_val;
}

static inline in_port_t combo_addr_port(const union combo_addr *addr)
{
	return AF_INET == addr->s_fam ? addr->in.sin_port : addr->in6.sin6_port;
}

#endif /* COMBO_ADDR_H */
