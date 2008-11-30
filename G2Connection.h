/*
 * G2Connection.h
 * home of g2_connection_t and header-file for G2Connection.c
 *
 * Copyright (c) 2004-2008 Jan Seiffert
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
 * $Id: G2Connection.h,v 1.11 2005/07/29 09:12:00 redbully Exp redbully $
 */

#ifndef _G2CONNECTION_H
# define _G2CONNECTION_H

/* Includes if included */
# include <stdbool.h>
# include <time.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <zlib.h>

/* Own */
# include "lib/other.h"

enum g2_connection_states
{
	UNCONNECTED,
	HAS_CONNECT_STRING,
	SEARCH_CAPS,
	CHECK_ACCEPT,
	CHECK_ADDR,
	CHECK_ENC_OUT,
	CHECK_UAGENT,
	CHECK_UPEER,
	BUILD_ANSWER,
	HEADER_2,
	ANSWER_200,
	SEARCH_CAPS_2,
	CHECK_CONTENT,
	CHECK_ENC_IN,
	FINISH_CONNECTION,
	G2CONNECTED
} GCC_ATTR_PACKED;

enum g2_connection_encodings
{
	ENC_NONE,
	ENC_DEFLATE,
	ENC_LZO,
} GCC_ATTR_PACKED;

# include "G2Packet.h"
# include "G2QHT.h"
# include "version.h"
# include "lib/sec_buffer.h"
# include "lib/combo_addr.h"
# include "lib/log_facility.h"
# include "lib/hzp.h"
# include "lib/list.h"

typedef struct g2_connection
{
	struct hzp_free  hzp;
	/* System Com-Things */
	union combo_addr remote_host;
	uint32_t         poll_interrests;
	int              com_socket;

	/* Internal States */
	union combo_addr sent_addr;
	double           time_diff;
	struct
	{
		time_t        PI;
		time_t        LNI;
		time_t        KHL;
		time_t        QHT;
		time_t        UPROC;
	} send_stamps;
	/* flags */
	struct
	{
		bool          dismissed;
		bool          accept_ok;
		bool          accept_g2;
		bool          content_ok;
		bool          content_g2;
		bool          addr_ok;
		bool          enc_in_ok;
		bool          enc_out_ok;
		bool          uagent_ok;
		bool          upeer;
		bool          upeer_ok;
		bool          upeer_needed;
		bool          upeer_needed_ok;
		bool          second_header;
		bool          has_written;
		bool          firewalled;
		bool          query_key_cache;
		bool          qht_reset_send;
		bool          t1;
	} flags;
	/* more state */
	enum g2_connection_states     connect_state;
	enum g2_connection_encodings  encoding_in;
	enum g2_connection_encodings  encoding_out;

	/* Buffer */
	char             uagent[40+1];
	char             vendor_code[4+1];
	uint8_t          guid[16];
	z_stream         z_decoder;
	z_stream         z_encoder;
/* ----- Everthing above this gets simply wiped ------ */
	struct norm_buff *recv;
	struct norm_buff *send;
	struct norm_buff *recv_u;
	struct norm_buff *send_u;
	struct qhtable   *qht;
	struct qhtable   *sent_qht;
// TODO: WTF these arrays where supossed to? zlib buffer?
	char             tmp1[32000], tmp2[32000];
	
	/* Packets */
	g2_packet_t      *build_packet;
	struct list_head packets_to_send;
} g2_connection_t;

struct g2_con_info
{
	size_t limit;
	size_t capacity;
	g2_connection_t *data[DYN_ARRAY_LEN];
};

typedef struct
{
	bool         (*action) (g2_connection_t *, size_t);
	const size_t length;
	const char   *txt;
} action_string;

# define MAX_HEADER_LENGTH        (NORM_BUFF_CAPACITY/2)
# define KNOWN_HEADER_FIELDS_SUM  19

/* Stringconstants */
/* var */
# define GNUTELLA_CONNECT_STRING "GNUTELLA CONNECT/0.6"
# define GNUTELLA_STRING         "GNUTELLA/0.6"
# define ACCEPT_G2               "application/x-gnutella2"
//#define OUR_UA	-> from version.h
# define G2_TRUE                 "True"
# define G2_FALSE                "False"
# define STATUS_200              "200 OK"
# define STATUS_400              "400 Bad Request"
# define STATUS_500              "500 Internal Error"
# define STATUS_501              "501 Not Implemented"

/* encoding */
# define ENC_NONE_S        "none"
# define ENC_DEFLATE_S     "deflate"
# define ENC_LZO_S         "lzo"

/* headerfields */
# define ACCEPT_KEY        "Accept"
# define UAGENT_KEY        "User-Agent"
# define ACCEPT_ENC_KEY    "Accept-Encoding"
# define REMOTE_ADR_KEY    "Remote-IP"
# define LISTEN_ADR_KEY    "Listen-IP"
# define CONTENT_KEY       "Content-Type"
# define CONTENT_ENC_KEY   "Content-Encoding"
# define UPEER_KEY         "X-Ultrapeer"
# define HUB_KEY           "X-Hub"
# define UPEER_NEEDED_KEY  "X-Ultrapeer-Needed"
# define HUB_NEEDED_KEY    "X-Hub-Needed"
# define X_TRY_UPEER_KEY   "X-Try-Ultrapeers"
# define X_TRY_HUB_KEY     "X-Try-Hubs"
# define X_REQUERIES_KEY   "X-Requeries"
# define GGEP_KEY          "GGEP"
# define PONG_C_KEY        "Pong-Caching"
# define QUERY_ROUTE_KEY   "X-Query-Routing"
# define UPTIME_KEY        "Uptime"
# define VEND_MSG_KEY      "Vendor-Message"

# ifndef _G2CONNECTION_C
#  define _G2CON_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#  define _G2CON_EXTRNVAR(x) extern x
# else
#  define _G2CON_EXTRN(x) inline x GCC_ATTR_VIS("hidden")
#  define _G2CON_EXTRNVAR(x)
# endif /* _G2CONNECTION_C */

_G2CON_EXTRNVAR(const action_string *KNOWN_ENCODINGS[];)
_G2CON_EXTRNVAR(const action_string *KNOWN_HEADER_FIELDS[KNOWN_HEADER_FIELDS_SUM];)

# define g2_con_clear(x) _g2_con_clear((x), 0);
# define g2_con_get_free() _g2_con_get_free(__FILE__, __func__, __LINE__)
# define g2_con_ret_free(x) _g2_con_ret_free((x), __FILE__, __func__, __LINE__)
_G2CON_EXTRN(g2_connection_t *g2_con_alloc(size_t));
_G2CON_EXTRN(void _g2_con_clear(g2_connection_t *, int) GCC_ATTR_FASTCALL);
_G2CON_EXTRN(void g2_con_free(g2_connection_t *) GCC_ATTR_FASTCALL);
_G2CON_EXTRN(g2_connection_t *_g2_con_get_free(const char *, const char *, const unsigned int));
_G2CON_EXTRN(void _g2_con_ret_free(g2_connection_t *, const char *, const char *, const unsigned int));
# ifdef HELGRIND_ME
_G2CON_EXTRN(void g2_con_helgrind_transfer(g2_connection_t *) GCC_ATTR_FASTCALL);
# else
#  define g2_con_helgrind_transfer(x)
# endif

#endif /* _G2CONNECTION_H */
/* EOF */
