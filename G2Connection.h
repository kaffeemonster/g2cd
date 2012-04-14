/*
 * G2Connection.h
 * home of g2_connection_t and header-file for G2Connection.c
 *
 * Copyright (c) 2004-2012 Jan Seiffert
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
 * $Id: G2Connection.h,v 1.11 2005/07/29 09:12:00 redbully Exp redbully $
 */

#ifndef _G2CONNECTION_H
# define _G2CONNECTION_H

/* Includes if included */
# include <stdbool.h>
# include <time.h>
# include <sys/types.h>
# include <zlib.h>

/* Own */
# include "lib/other.h"
# include "lib/combo_addr.h"

enum g2_connection_states
{
	UNCONNECTED,
	HAS_CONNECT_STRING,
	ADVANCE_TIMEOUTS,
	CHECK_ACCEPT,
	CHECK_ADDR,
	CHECK_UPEER,
	CHECK_UAGENT,
	CHECK_ENC_OUT,
	BUILD_ANSWER,
	HEADER_2,
	ANSWER_200,
	SEARCH_CAPS_2,
	CANCEL_TIMEOUTS,
	CHECK_CONTENT,
	CHECK_ENC_IN,
	FINISH_CONNECTION,
	G2CONNECTED
} GCC_ATTR_PACKED;

#define G2_CONNECTION_ENCODINGS \
	ENUM_CMD(ENC_NONE   ), \
	ENUM_CMD(ENC_DEFLATE), \
	ENUM_CMD(ENC_LZO    )

#define ENUM_CMD(x) x
enum g2_connection_encodings
{
	G2_CONNECTION_ENCODINGS
} GCC_ATTR_PACKED;
#undef ENUM_CMD

# include "G2Packet.h"
# include "G2QHT.h"
# include "version.h"
# include "timeout.h"
# include "gup.h"
# include "lib/sec_buffer.h"
# include "lib/combo_addr.h"
# include "lib/log_facility.h"
# include "lib/hzp.h"
# include "lib/list.h"
# include "lib/hlist.h"

typedef struct g2_connection
{
	enum guppies     gup; /* keep this first */
	struct hzp_free  hzp;
	struct hlist_node registry;
	struct list_head hub_list;
	/* System Com-Things */
	union combo_addr remote_host;
	uint32_t         poll_interrests;
	some_fd          com_socket;

	/* Internal States */
	union combo_addr sent_addr;
	time_t           connect_time;
	time_t           last_send;
	time_t           last_active;
	long             time_diff;
	/* flags */
	struct
	{
		bool          dismissed;
		bool          upeer;
		bool          upeer_needed;
		bool          has_written;
		bool          firewalled;
		bool          query_key_cache;
		bool          router;
		bool          hub_able;
		bool          t1;
		bool          last_data_active;
	} flags;
	union
	{
		struct
		{
			struct
			{
				time_t     PI;
				time_t     LNI;
				time_t     KHL;
				time_t     QHT;
				time_t     UPROC;
			} send_stamps;
			struct
			{
				time_t     Q2;
			} recv_stamps;
			unsigned leaf_count;
			bool z_flush;
		} handler;
		struct
		{
			size_t         header_bytes_recv;
			struct
			{
				bool    accept_ok;
				bool    accept_g2;
				bool    content_ok;
				bool    content_g2;
				bool    addr_ok;
				bool    enc_in_ok;
				bool    enc_out_ok;
				bool    uagent_ok;
				bool    upeer_ok;
				bool    upeer_needed_ok;
				bool    second_header;
				bool    had_crawler;
			} flags;
		} accept;
	} u;
	/* more state */
	enum g2_connection_states     connect_state;
	enum g2_connection_encodings  encoding_in;
	enum g2_connection_encodings  encoding_out;

	/* Buffer */
	char             uagent[40+1];
	char             vendor_code[4+1];
	uint8_t          guid[16];
	z_stream         *z_decoder;
	z_stream         *z_encoder;
/* ----- Everthing above this gets simply wiped ------ */
	struct norm_buff *recv;
	struct norm_buff *send;
	struct norm_buff *recv_u;
	struct norm_buff *send_u;
	struct qhtable   *qht;
	struct qhtable   *sent_qht;
// TODO: WTF these arrays where supossed to? zlib buffer?
//	char             tmp1[32000], tmp2[32000];

	struct timeout   active_to;
	struct timeout   aux_to;
	/* Packets */
	g2_packet_t      *build_packet;
	shortlock_t      pts_lock;
	struct list_head packets_to_send;
	mutex_t  lock;
} g2_connection_t;

typedef struct
{
	bool       (*action) (g2_connection_t *, size_t);
	size_t       length;
	const char  *txt;
} action_string;

typedef struct
{
	unsigned char index;
	unsigned char num;
} action_index;

# define MAX_HEADER_LENGTH        (NORM_BUFF_CAPACITY/2)

# include "G2HeaderStrings.h"

# ifndef _G2CONNECTION_C
#  define _G2CON_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#  define _G2CON_EXTRNVAR(x) extern x
# else
#  define _G2CON_EXTRN(x) inline x GCC_ATTR_VIS("hidden")
#  define _G2CON_EXTRNVAR(x)
# endif /* _G2CONNECTION_C */

_G2CON_EXTRNVAR(const action_string * const KNOWN_ENCODINGS[];)
_G2CON_EXTRNVAR(const action_index  KNOWN_HEADER_FIELDS_INDEX[LONGEST_HEADER_FIELD];)
_G2CON_EXTRNVAR(const action_string KNOWN_HEADER_FIELDS[];)

# define g2_con_clear(x) _g2_con_clear((x), 0);
# ifdef DEBUG_CON_ALLOC
#  define g2_con_get_free() _g2_con_get_free(__FILE__, __func__, __LINE__)
#  define g2_con_ret_free(x) _g2_con_ret_free((x), __FILE__, __func__, __LINE__)
_G2CON_EXTRN(g2_connection_t *_g2_con_get_free(const char *, const char *, const unsigned int) GCC_ATTR_MALLOC);
_G2CON_EXTRN(void _g2_con_ret_free(g2_connection_t *, const char *, const char *, const unsigned int));
# else
_G2CON_EXTRN(g2_connection_t *g2_con_get_free(void) GCC_ATTR_MALLOC);
_G2CON_EXTRN(void g2_con_ret_free(g2_connection_t *));
# endif
_G2CON_EXTRN(g2_connection_t *g2_con_alloc(size_t) GCC_ATTR_MALLOC);
_G2CON_EXTRN(void _g2_con_clear(g2_connection_t *, int) GCC_ATTR_FASTCALL);
_G2CON_EXTRN(void g2_con_free(g2_connection_t *));
_G2CON_EXTRN(void g2_con_free_glob(g2_connection_t *));
# ifdef HELGRIND_ME
_G2CON_EXTRN(void g2_con_helgrind_transfer(g2_connection_t *) GCC_ATTR_FASTCALL);
# else
#  define g2_con_helgrind_transfer(x)
# endif
_G2CON_EXTRN(bool g2_packet_add_LNI(g2_connection_t *connec));

#endif /* _G2CONNECTION_H */
/* EOF */
