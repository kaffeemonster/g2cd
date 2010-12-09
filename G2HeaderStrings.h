/*
 * G2HeaderStrings.h
 * definitions of the G2 header strings
 *
 * Copyright (c) 2004-2010 Jan Seiffert
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

#ifndef _G2HEADERSTRINGS_H
# define _G2HEADERSTRINGS_H

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
# define ENC_MAX_S           ENC_DEFLATE_S

# define LONGEST_HEADER_FIELD     25
/* headerfields */
# define ACCEPT_KEY        "Accept"
# define UAGENT_KEY        "User-Agent"
# define ACCEPT_ENC_KEY    "Accept-Encoding"
# define REMOTE_ADR_KEY    "Remote-IP"
# define LISTEN_ADR_KEY    "Listen-IP"
# define X_MY_ADDR_KEY     "X-My-Address"
# define X_NODE_KEY        "X-Node"
# define NODE_KEY          "Node"
# define CONTENT_KEY       "Content-Type"
# define CONTENT_ENC_KEY   "Content-Encoding"
# define UPEER_KEY         "X-Ultrapeer"
# define HUB_KEY           "X-Hub"
# define UPEER_NEEDED_KEY  "X-Ultrapeer-Needed"
# define HUB_NEEDED_KEY    "X-Hub-Needed"
# define X_TRY_UPEER_KEY   "X-Try-Ultrapeers"
# define X_TRY_HUB_KEY     "X-Try-Hubs"
# define X_LEAF_MAX_KEY    "X-Leaf-Max"
/* Leacher broken stuff */
# define X_AUTH_CH_KEY     "X-Auth-Challenge"
/* G1 Stuff we want to silently ignore */
# define X_VERSION_KEY     "X-Version"
# define X_MAX_TTL_KEY     "X-Max-TTL"
# define X_GUESS_KEY       "X-Guess"
# define X_REQUERIES_KEY   "X-Requeries"
# define X_LOC1_PREF_KEY   "X-Locale-Pref"
# define X_LOC2_PREF_KEY   "X-Local-Pref"
# define X_Q_ROUT_KEY      "X-Query-Routing"
# define X_UQ_ROUT_KEY     "X-Ultrapeer-Query-Routing"
# define X_DYN_Q_KEY       "X-Dynamic-Querying"
# define X_EXT_PROBES_KEY  "X-Ext-Probes"
# define X_DEGREE_KEY      "X-Degree"
# define X_PROBE_Q_KEY     "X-Probe-Queries"
# define BYE_PKT_KEY       "Bye-Packet"
# define GGEP_KEY          "GGEP"
# define PONG_C_KEY        "Pong-Caching"
# define UPTIME_KEY        "Uptime"
# define VEND_MSG_KEY      "Vendor-Message"
# define CRAWLER_KEY       "Crawler"
#endif
