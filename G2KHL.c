/*
 * G2KHL.c
 * known hublist foo
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* System */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#endif
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#ifdef HAVE_DB
# define DB_DBM_HSEARCH 1
# include <db.h>
#else
# include <ndbm.h>
#endif
#include <netdb.h>
/* Own */
#define _G2KHL_C
#include "lib/other.h"
#include "G2KHL.h"
#include "G2MainServer.h"
#include "lib/log_facility.h"
#include "lib/combo_addr.h"
#include "lib/hlist.h"
#include "lib/rbtree.h"
#include "lib/my_bitops.h"

#define KHL_CACHE_SHIFT 8
#define KHL_CACHE_SIZE (1 << KHL_CACHE_SHIFT)
#define KHL_CACHE_HTSIZE (KHL_CACHE_SIZE/4)
#define KHL_CACHE_HTMASK (KHL_CACHE_HTSIZE-1)
#define KHL_CACHE_FILL 8

#define KHL_DUMP_IDENT "G2CDKHLDUMP\n"
#define KHL_DUMP_ENDIAN_MARKER 0x12345678
#define KHL_DUMP_VERSION 0

#define KHL_MNMT_STATI \
	ENUM_CMD( BOOT    ), \
	ENUM_CMD( FILL    ), \
	ENUM_CMD( GWC_RES ), \
	ENUM_CMD( GWC_CON ), \
	ENUM_CMD( GWC_REQ ), \
	ENUM_CMD( GWC_REC ), \
	ENUM_CMD( IDLE    ), \
	ENUM_CMD( MAXIMUM ) /* count */

#define ENUM_CMD(x) KHL_##x
enum khl_mnmt_states
{
	KHL_MNMT_STATI
};
#undef ENUM_CMD

#define GWC_RES_STATI \
	ENUM_CMD( HTTP            ), \
	ENUM_CMD( HTTP_11         ), \
	ENUM_CMD( HTTP_SKIP_WHITE ), \
	ENUM_CMD( HTTP_RESULT     ), \
	ENUM_CMD( CRLFCRLF        ), \
	ENUM_CMD( FIND_LINE       )

#define ENUM_CMD(x) GWC_RES_##x
enum gwc_res_states
{
	GWC_RES_STATI
};
#undef ENUM_CMD

struct gwc
{
	unsigned int q_count;
	unsigned int m_count;
	time_t seen_last;
	time_t access_last;
	unsigned int access_period;
	const char *url;
};

struct khl_cache_entry
{
	struct rbnode rb; /* keep first */
	bool used; /* fill possible hole */
	bool cluster;
	struct hlist_node node;
	struct khl_entry e;
};

/* Vars */
static DBM *gwc_db;
static struct {
	struct gwc data;
	struct addrinfo *addrinfo;
	struct addrinfo *next_addrinfo;
	struct norm_buff *buff;
	char *request_string;
	unsigned port;
	int socket;
	enum gwc_res_states state;
} act_gwc;
static struct {
	pthread_mutex_t lock;
	int num;
	uint32_t ht_seed;
	struct khl_cache_entry *next_free;
	struct khl_cache_entry *last_search;
	struct rbtree tree;
	struct hlist_head ht[KHL_CACHE_HTSIZE];
	struct khl_cache_entry entrys[KHL_CACHE_SIZE];
} cache;
#define ENUM_CMD(x) str_it(KHL_##x)
static const char khl_mnmt_states_names[][12] =
{
	KHL_MNMT_STATI
};
#undef ENUM_CMD

#ifdef DEBUG_DEVEL_OLD
# define ENUM_CMD(x) str_it(GWC_RES_##x)
static const char *gwc_res_http_names[] =
{
	GWC_RES_STATI
};
# undef ENUM_CMD
#endif


/*
 * Functs
 */
bool g2_khl_init(void)
{
	FILE *khl_dump;
	uint32_t *signature;
	const char *data_root_dir;
	const char *gwc_cache_fname;
	char *name, *buff;
	size_t name_len = 0;
	datum key, value;

	/* open the gwc cache db */
	if(server.settings.data_root_dir)
		data_root_dir = server.settings.data_root_dir;
	else
		data_root_dir = "./";
	if(server.settings.khl.gwc_cache_fname)
		gwc_cache_fname = server.settings.khl.gwc_cache_fname;
	else
		gwc_cache_fname = "gwc_cache";

	name_len += strlen(data_root_dir);
	name_len += strlen(gwc_cache_fname);

	name = alloca(name_len + 2);

	buff = strpcpy(name, data_root_dir);
	*buff++ = '/';
	strpcpy(buff, gwc_cache_fname);

	gwc_db = dbm_open(name, O_RDWR|O_CREAT, 00666);
	if(!gwc_db)
	{
		logg_errno(LOGF_NOTICE, "opening GWC cache");
		gwc_db = dbm_open(name, O_RDWR|O_CREAT|O_TRUNC, 00666);
		if(!gwc_db) {
			logg_errno(LOGF_ERR, "second try opening GWC cache");
			return false;
		}
	}

	/* no boot url?? K, continue, don't crash */
	if(!server.settings.khl.gwc_boot_url)
		goto init_next;

	/* make shure the boot url is in the cache */
	key.dptr  = (void*)(intptr_t)server.settings.khl.gwc_boot_url;
	key.dsize = strlen(key.dptr) + 1;

	value = dbm_fetch(gwc_db, key);
	if(!value.dptr)
	{
		struct gwc boot;
		/* boot url not in cache */
		logg_devel("putting GWC boot url in db\n");
		boot.q_count = 0;
		boot.m_count = 1;
		boot.seen_last = 0;
		boot.access_last = 0;
		boot.access_period = 60 * 60;
		boot.url = NULL;
		value.dptr  = (char *) &boot;
		value.dsize = sizeof(boot);
		if(dbm_store(gwc_db, key, value, DBM_REPLACE))
			logg_errno(LOGF_WARN, "not able to put boot GWC into cache");
	}

init_next:
	if(pthread_mutex_init(&cache.lock, NULL)) {
		logg_errno(LOGF_ERR, "initialising KHL cache lock");
		return false;
	}
	cache.ht_seed = (uint32_t) time(NULL);
	srand(cache.ht_seed);
	/* try to read a khl dump */
	if(!server.settings.khl.dump_fname)
		return true;

	name_len  = strlen(data_root_dir);
	name_len += strlen(server.settings.khl.dump_fname);
	name = alloca(name_len + 2);

	buff = strpcpy(name, data_root_dir);
	*buff++ = '/';
	strcpy(buff, server.settings.khl.dump_fname);

	khl_dump = fopen(name, "rb");
	if(!khl_dump) {
		logg_errno(LOGF_INFO, "couldn't open khl dump");
		return true;
	}

	buff = alloca(500);
	if(str_size(KHL_DUMP_IDENT) != fread(buff, 1, str_size(KHL_DUMP_IDENT), khl_dump)) {
		logg_posd(LOGF_INFO, "couldn't read ident from khl dump \"%s\"\n", name);
		goto out;
	}

	if(strncmp(buff, KHL_DUMP_IDENT, str_size(KHL_DUMP_IDENT))) {
		logg_posd(LOGF_INFO, "\"%s\" has wrong khl dump ident\n", name);
		goto out;
	}

	signature = (uint32_t *) buff;
	if(3 != fread(signature, sizeof(uint32_t), 3, khl_dump)) {
		logg_posd(LOGF_INFO, "couldn't read signature from khl dump \"%s\"\n", name);
		goto out;
	}

	if(signature[0] != KHL_DUMP_ENDIAN_MARKER) {
		logg_posd(LOGF_WARN, "khl dump \"%s\" has wrong %s, don't copy dumps around!\n", name, "endianess");
		goto out;
	}
	if(signature[1] != sizeof(struct khl_entry)) {
		logg_posd(LOGF_WARN, "khl dump \"%s\" has wrong %s, don't copy dumps around!\n", name, "datasize");
		goto out;
	}
	if(signature[2] != KHL_DUMP_VERSION) {
		logg_posd(LOGF_DEBUG, "khl dump \"%s\" has wrong %s, don't copy dumps around!\n", name, "version");
		goto out;
	}

	/* we don't care if we read 0 or 1 or KHL_CACHE_SIZE elements */
	do
	{
		struct khl_entry *e = (void *) buff;
		name_len = fread(e, sizeof(*e), 1, khl_dump);
		if(name_len)
			g2_khl_add(&e->na, e->when, false);
	} while(name_len);

out:
	fclose(khl_dump);
	return true;
}

static void gwc_writeback(struct gwc *gwc)
{
	datum tkey, tvalue;
	char *url;

	if(!gwc->url)
		return;

	url = (char *)(intptr_t)gwc->url;
	tkey.dptr    = url;
	tkey.dsize   = strlen(tkey.dptr) + 1;

	gwc->url = NULL;
	tvalue.dptr  = (char *) gwc;
	tvalue.dsize = sizeof(*gwc);
	if(dbm_store(gwc_db, tkey, tvalue, DBM_REPLACE))
		logg_errno(LOGF_WARN, "not able to save act. GWC into db");
	free(url);
}

static void gwc_clean(void)
{
	gwc_writeback(&act_gwc.data);

	if(act_gwc.addrinfo)
		freeaddrinfo(act_gwc.addrinfo);
	act_gwc.addrinfo = NULL;
	act_gwc.next_addrinfo = NULL;

	if(-1 != act_gwc.socket)
		close(act_gwc.socket);
	act_gwc.socket = -1;

	free(act_gwc.request_string);
	act_gwc.request_string = NULL;
	free(act_gwc.buff);
	act_gwc.buff = NULL;

	act_gwc.port = 80; /* general http */
	act_gwc.state = GWC_RES_HTTP;
}

static void gwc_kick(struct gwc *gwc)
{
	char *url;
	datum tkey;

	/* kick entry, except if boot gwc */
	if(strcmp(gwc->url, server.settings.khl.gwc_boot_url))
		return;

	url = (void *)(intptr_t)gwc->url;
	/* make shure gwc will not be saved */
	gwc->url = NULL;
	/* kick from db */
	tkey.dptr  = url;
	tkey.dsize = strlen(url) + 1;
	/* what to do on error? */
	dbm_delete(gwc_db, tkey);
	free(url);
}

static bool gwc_switch(void)
{
	int retry_count = 0;
	unsigned cycling;
	char *url;
	datum key, value;

retry:
	retry_count++;
	cycling = (unsigned)((((unsigned long long)rand())*8)/((unsigned long long)RAND_MAX));
	do
	{
		key = dbm_nextkey(gwc_db);
		if(!key.dptr)
			key = dbm_firstkey(gwc_db);
		if(!key.dptr) {
			/* uhm, no key? this should not happen */
			return false;
		}
	} while(cycling--);

	value = dbm_fetch(gwc_db, key);
	if(!value.dptr) { /* huh? key but no data? */
		if(5 < retry_count)
			return false;
		goto retry;
	}
	if(sizeof(struct gwc) != value.dsize)
	{
		/* what to do on error? */
		if(dbm_delete(gwc_db, key))
			logg_errno(LOGF_WARN, "not able to remove broken GWC from db");
		if(5 < retry_count)
			return false;
		goto retry;
	}

	gwc_clean();

	url = malloc(key.dsize);
	if(!url)
		return false;
	memcpy(&act_gwc.data, value.dptr, sizeof(act_gwc.data));
	memcpy(url, key.dptr, key.dsize);
	act_gwc.data.url = url;

	return true;
}

static bool gwc_resolv(void)
{
	char *url, *node, *service, *path, *wptr;
	struct addrinfo *res_res;
	int ret_val;

	url = malloc(strlen(act_gwc.data.url) + 1);
	if(!url)
		return false;
	service = strcpy(url, act_gwc.data.url);

	/* find protocol seperator */
	wptr = strstr(service, "://");
	if(!wptr) {
		logg(LOGF_INFO, "GWC url \"%s\" is malformed. Trying to drop...\n", url);
		goto resolv_fail;
	}
	/* skipp "://" */
	*wptr++ = '\0'; /* terminate */
	wptr++; wptr++;
	node = wptr;
	/* find next slash */
	wptr = strchr(wptr, '/');
	if(wptr)
		*wptr++ = '\0'; /* terminate */
	path = wptr;
	/* someone added a username? */
	wptr = strchr(node, '@');
	if(wptr)
		node = wptr + 1; /* skip */
	/* another port? */
	wptr = strchr(node, ':');
	if(wptr)
	{
		unsigned port;
		*wptr++ = '\0'; /* terminate */
		if(1 == sscanf(wptr, "%u", &port)) {
			if(port <= 0xFFFF)
				act_gwc.port = port;
		}
		/* failure? ignore, try 80 */
	}

	/* resolve */
	ret_val = getaddrinfo(node, service, NULL, &res_res);
	/* transient errors are bad, but not "kill that entry" bad */
	if(ret_val) {
		if(EAI_AGAIN  == ret_val || EAI_MEMORY == ret_val)
			ret_val = true;
		else
			goto resolv_fail;
	}
	act_gwc.next_addrinfo = act_gwc.addrinfo = res_res;

#define HTTP_REQ_START \
	"GET /"
#define HTTP_REQ_MID \
	"?get=1&hostfile=1&net=gnutella2&client=" OWN_VENDOR_CODE "&version=" OUR_VERSION "&ping=1 HTTP/1.1\r\n" \
	"Connection: close\r\n" \
	"User-Agent: " OUR_UA "\r\n" \
	"Cache-control: no-cache\r\n" \
	"Accept: text/plain\r\n" \
	"Host: "
#define HTTP_REQ_END \
	"\r\n" \
	"\r\n"
//		"Accept-Encoding: identity\r\n"
	/* because we have already extracted the foo, build request string */
	act_gwc.request_string =
		malloc(str_size(HTTP_REQ_START) + str_size(HTTP_REQ_MID) +
		       str_size(HTTP_REQ_END) + 2 * strlen(node) + strlen(path) + 1);
	if(act_gwc.request_string)
	{
		char *cp_ret;
		cp_ret = strplitcpy(act_gwc.request_string, HTTP_REQ_START);
		cp_ret = strpcpy(cp_ret, path);
		cp_ret = strplitcpy(cp_ret, HTTP_REQ_MID);
		cp_ret = strpcpy(cp_ret, node);
		cp_ret = strplitcpy(cp_ret, HTTP_REQ_END);
		*cp_ret = '\0';
	} else
		ret_val = true; /* malloc fail qualifies as transient... */
#undef HTTP_REQ_START
#undef HTTP_REQ_MID
#undef HHTP_REQ_END

	free(url);
	return !ret_val;
resolv_fail:
	/* resolving failed? */
	free(url);
	gwc_kick(&act_gwc.data);
	return false;
}

static int gwc_connect(void)
{
	union combo_addr *addr;
	struct addrinfo *tai;
	int fd, ret_val;

	tai = act_gwc.next_addrinfo;
	fd = socket(tai->ai_family, SOCK_STREAM, 0);
	/* we have a socket? */
	if(0 <= fd)
		act_gwc.next_addrinfo = act_gwc.next_addrinfo->ai_next; /* mark next to connect */
	else {
		/* we don't have a socket... */
		ret_val = -2; /* soft error */
		/* ... because the addr familiy is whatever? */
		if(!(tai->ai_family == AF_INET || tai->ai_family == AF_INET6))
			act_gwc.next_addrinfo = act_gwc.next_addrinfo->ai_next; /* mark next to connect */
		/* .. because the kernel want something? */
		else {
			/* not transient error? */
			if(!(ENOMEM  == errno || ENOBUFS == errno || EMFILE  == errno))
				act_gwc.next_addrinfo = act_gwc.next_addrinfo->ai_next; /* mark next to connect */
		}
		goto out_err;
	}

	addr = (union combo_addr *)tai->ai_addr;
	/* make sure address family is copied to the struct sockaddr field */
	addr->s_fam = tai->ai_family;
	combo_addr_set_port(addr, htons(act_gwc.port));

	logg(LOGF_INFO, "Connecting to GWC \"%s\" for Hubs\n", act_gwc.data.url);

	do {
		errno = 0;
		ret_val = connect(fd, &addr->sa, tai->ai_addrlen);
	} while(ret_val && EINTR == errno);
	time(&act_gwc.data.access_last);
	if(!ret_val)
		act_gwc.socket = fd;
	else
	{
		close(fd);
		ret_val = -2; /* soft error */
out_err:
		/* ok, do we have someone left to connect? */
		if(!act_gwc.next_addrinfo) {
			/* no? make it a permanent error */
			ret_val = -1;
			gwc_kick(&act_gwc.data);
		}
	}
	return ret_val;
}

static bool gwc_request(void)
{
	ssize_t ret_val;
	int len = strlen(act_gwc.request_string);

	do {
		errno = 0;
		ret_val = send(act_gwc.socket, act_gwc.request_string, len, 0);
	} while(-1 == ret_val && EINTR == errno);

// TODO: handle partial send

	if(len == ret_val) {
		shutdown(act_gwc.socket, SHUT_WR); /* we are finished */
	} else
		goto out_err;

	free(act_gwc.request_string);
	act_gwc.request_string = NULL;

	act_gwc.buff = malloc(sizeof(*act_gwc.buff));
	if(!act_gwc.buff)
		goto out_err;
	act_gwc.buff->capacity = sizeof(act_gwc.buff->data);
	buffer_clear(*act_gwc.buff);

	return true;
out_err:
	close(act_gwc.socket);
	act_gwc.socket = -1;
	return false;
}

static void gwc_handle_line(char *line, time_t lnow)
{
	char *wptr;
	char response;

	/* we need at least 2 chars */
	if(2 > strlen(line))
		return;

	logg_develd_old("gwc \"%s\" response: \"%s\"\n", act_gwc.data.url, line);
	/* skip whitespace */
	for(wptr = line; *wptr && isblank(*wptr); wptr++);

	response = *wptr++;
	/* make shure there is a field seperator */
	if('|' != *wptr++)
		return;
	/* what kind of response in this line? */
	switch(response)
	{
	case 'I':
	case 'i':
		{
			char *next;
			int period;

			next = strstr(wptr, "access|period|");
			if(!next)
				break;
			wptr = next + str_size("access|period|");
			/* skip whitespace at period start */
			for(; *wptr && isblank(*wptr); wptr++);
			if(*wptr && isdigit(*wptr))
				period = atoi(wptr);
			else
				break;

			act_gwc.data.access_period = period > 3 * 60 ? period : 60 * 60;
		}
		break;
	case 'H':
	case 'h':
		{
			union combo_addr a;
			int since = 0;
			char *next, *port, *ipsix_end;

			/* first try to find next seperator */
			next = strchr(wptr, '|');
			if(next) {
				*next++ = '\0';
				/* skip whitespace at timestamp start */
				for(; *next && isblank(*next); next++);
				if(*next && isdigit(*next))
					since = atoi(next);
			}

			/* skip whitespace at addr start */
			for(; *wptr && isblank(*wptr); wptr++);
			/*
			 * playing with fire here, a moron maybe forgot the [ ] needed
			 * around literal IPv6 addresses where a port may be specified
			 */
			if((ipsix_end = strrchr(wptr, ']')))
			{
				/* try to skip the [ ] */
				if('[' == *wptr)
					wptr++;
				*ipsix_end++ = '\0';
				port = strrchr(ipsix_end, ':');
			} else
				port = strrchr(wptr, ':');
			if(port)
				*port++ = '\0';
			/* skip whitespace at port start */
			for(; *port && isblank(*port); port++);
			if(!(*port && isdigit(*port)))
				port = NULL;

			memset(&a, 0, sizeof(a));
			if(0 >= combo_addr_read(wptr, &a))
				break; /* nothing to read */

			if(port && isdigit(*port))
				combo_addr_set_port(&a, htons(atoi(port)));
			else
				combo_addr_set_port(&a, htons(6551));

			/*
			 * this host is in the gwc cache since 'since' senconds.
			 * Since hosts are short lived, we turn the wheel back in time.
			 */
			g2_khl_add(&a, lnow - since, false);
		}
		break;
	case 'U':
	case 'u':
		{
			struct gwc new_gwc;
			datum key, value;
			int since = 0;
			char *next;

			next = strstr(wptr, "http://");
			if(!next)
				break;
			wptr = next;

			/* first try to find next seperator */
			next = strchr(wptr, '|');
			if(next) {
				*next++ = '\0';
				/* skip whitespace at timestamp start */
				for(; *next && isblank(*next); next++);
				if(*next && isdigit(*next))
					since = atoi(next);
			}
			/* trim trailing whitespace */
			for(next = wptr + strlen(wptr) - 1; next >= wptr && isblank(*next); next--)
				*next = '\0';

			key.dptr  = (void *)wptr;
			key.dsize = strlen(key.dptr) + 1;

			value = dbm_fetch(gwc_db, key);
			if(!value.dptr)
			{
				/* this url is not in cache */
				logg_posd(LOGF_DEBUG, "putting GWC \"%s\" in cache\n", wptr);
				new_gwc.q_count = 0;
				new_gwc.m_count = 1;
				new_gwc.seen_last = lnow;
				new_gwc.access_last = 0;
				new_gwc.access_period = 60 * 60;
				new_gwc.url = NULL;
				value.dptr  = (char *) &new_gwc;
				value.dsize = sizeof(new_gwc);
			}
			else
			{
				struct gwc *known_gwc = (void *) value.dptr;
				known_gwc->seen_last = lnow;
			}
			/*
			 * this gwc cache is in this gwc cache since 'since' senconds.
			 * Since gwcs are long lived, we do NOT turn the wheel back in time.
			 */
			if(dbm_store(gwc_db, key, value, DBM_REPLACE))
				logg_errno(LOGF_WARN, "error accessing gwc db");
		}
		break;
	default:
		/* unknown response, don't bother */
		break;
	}
}

static int gwc_handle_response(void)
{
	struct norm_buff *buff = act_gwc.buff;
	char *wptr;
	bool keep_going = true;

	logg_develd_old("state: %s\n", gwc_res_http_names[act_gwc.state]);
	do
	{
	switch(act_gwc.state)
	{
	case GWC_RES_HTTP:
		wptr = strstr(buffer_start(*buff), "HTTP/");
		if(!wptr) {
			buffer_skip(*buff);
			keep_going = false;
			break;
		}
		buff->pos += wptr + str_size("HTTP/") - buffer_start(*buff);
		act_gwc.state++;
	case GWC_RES_HTTP_11:
		{
			int tmp, a, b;
			if(4 > buffer_remaining(*buff)) {
				keep_going = false;
				break;
			}
			tmp = sscanf(buffer_start(*buff), "%d.%d ", &a, &b);
			if(2 != tmp) {
				return false;
			}
			if(a != 1 || b < 1) {
				return false;
			}
			buff->pos += 4;
			act_gwc.state++;
		}
	case GWC_RES_HTTP_SKIP_WHITE:
		{
			bool goto_to_next = false;
			while(buffer_remaining(*buff)) {
				char c = *buffer_start(*buff) & 0x7F;
				if(isblank(c))
					buff->pos++;
				else if(isdigit(c)) {
					act_gwc.state++;
					goto_to_next = true;
					break;
				}
				else
					return false;
			}
			if(unlikely(!goto_to_next)) {
				keep_going = false;
				break;
			}
		}
	case GWC_RES_HTTP_RESULT:
		if(4 > buffer_remaining(*buff)) {
			keep_going = false;
			break;
		}
		if('2' != *buffer_start(*buff)) {
			if('3' != *buffer_start(*buff))
				return false;
			buff->pos++;
			if('0' != *buffer_start(*buff))
				return false;
			buff->pos++;
			if('2' != *buffer_start(*buff))
				return false;
			buff->pos++;
			/* 302 Moved Temporaly */
// TODO: handle HTTP moved, reenable identity encoding
//			KHL_GWC_REQ
			return false;
		}
		buff->pos++;
		if('0' != *buffer_start(*buff))
			return false;
		buff->pos++;
		if('0' != *buffer_start(*buff))
			return false;
		buff->pos++;
		if(!isspace(*buffer_start(*buff) & 0x7F))
			return false;
		buff->pos++;
		act_gwc.state++;
	case GWC_RES_CRLFCRLF:
		/*
		 * if a server send \n\r or something like that,
		 * they deserve to be punished
		 */
		wptr = strstr(buffer_start(*buff), "\r\n\r\n");
		if(!wptr) {
			buffer_skip(*buff);
			keep_going = false;
			break;
		}
		buff->pos += wptr + str_size("\r\n\r\n") - buffer_start(*buff);
		act_gwc.state++;
	case GWC_RES_FIND_LINE:
		if(buffer_remaining(*buff))
		{
			time_t lnow = time(NULL);
			char *end = buffer_start(*buff) + buffer_remaining(*buff);
			for(wptr = buffer_start(*buff); wptr < end; wptr++) {
				if('\r' == *wptr)
					*wptr = '\n';
			}

			wptr = buffer_start(*buff);
			for(end = strchr(wptr, '\n'); end; wptr = end, end = strchr(end, '\n'))
			{
				*end++ = '\0';
				buff->pos += end - buffer_start(*buff);
				gwc_handle_line(wptr, lnow);
			}
		}
		keep_going = false;
		break;
	}
	}
	while(keep_going);

	return true;
}

static int gwc_receive(void)
{
	struct norm_buff *buff = act_gwc.buff;
	ssize_t result;
	int ret_val = 1;

	do {
		errno = 0;
		result = recv(act_gwc.socket, buffer_start(*buff), buffer_remaining(*buff)-1, 0);
	} while(-1 == ret_val && EINTR == errno);

	switch(result)
	{
	default:
		buff->pos += result;
		break;
	case 0:
		if(buffer_remaining(*buff)-1)
		{
			if(EAGAIN != errno)
			{
				/* we have reached EOF */
				close(act_gwc.socket);
				act_gwc.socket = -1;
				ret_val = 0;
			}
			else
				logg_devel("nothing to read!\n");
		}
		break;
	case -1:
		if(EAGAIN != errno) {
			logg_errno(LOGF_DEBUG, "reading gwc");
			close(act_gwc.socket);
			act_gwc.socket = -1;
			ret_val = -1;
		}
	}
	/* we should have 1 char space left put a '\0' at the end */
	*buffer_start(*buff) = '\0';
	/* no buff->pos++, the zero is only for safety */
	buffer_flip(*buff);
	if(!gwc_handle_response()) {
			if(-1 != act_gwc.socket)
				close(act_gwc.socket);
			act_gwc.socket = -1;
			ret_val = -1;
	}
	buffer_compact(*buff);
	if(!buffer_remaining(*buff)) {
		/* We made no forward progress? Trxy to fix it... */
		buffer_flip(*buff);
		buffer_skip(*buff);
		buffer_compact(*buff);
	}
	return ret_val;
}

bool g2_khl_tick(int *fd)
{
	static enum khl_mnmt_states state = KHL_BOOT;
	bool long_poll = false;

	if(KHL_IDLE != state)
		logg_develd("state: %s\n", khl_mnmt_states_names[state]);

	switch(state)
	{
	case KHL_BOOT:
		state = KHL_CACHE_FILL > cache.num ? KHL_FILL : KHL_IDLE;
		break;
	case KHL_FILL:
		if(gwc_switch())
			state = KHL_GWC_RES;
		break;
	case KHL_GWC_RES:
		if(gwc_resolv())
			state = KHL_GWC_CON;
		else
			state = KHL_FILL;
		break;
	case KHL_GWC_CON:
		{
			int result = gwc_connect();
			if(!result)
				state = KHL_GWC_REQ;
			else {
				if(-1 == result)
					state = KHL_FILL;
				break;
			}
		}
	case KHL_GWC_REQ:
		if(gwc_request())
		{
			int fd_flags;
			/* get the fd-flags and add nonblocking  */
			if(-1 != (fd_flags = fcntl(act_gwc.socket, F_GETFL)))
				fcntl(act_gwc.socket, F_SETFL, fd_flags | O_NONBLOCK);
			state = KHL_GWC_REC;
		}
		else {
			state = KHL_FILL;
			break;
		}
	case KHL_GWC_REC:
		{
			int result = gwc_receive();
			if(0 > result)
				state = KHL_FILL;
			else if (0 < result)
				*fd = act_gwc.socket;
			else {
				act_gwc.data.q_count++;
				time(&act_gwc.data.seen_last);
				gwc_clean();
				state = KHL_IDLE;
			}
		}
		break;
	case KHL_IDLE:
		if(KHL_CACHE_FILL > cache.num)
			state = KHL_FILL;
		else
			state = KHL_IDLE;
		long_poll = true;
		break;
	case KHL_MAXIMUM:
		logg_devel("Ouch! khl state is MAXIMUM?? Should not happen! Fixing up...");
		state = KHL_IDLE;
	}

	return long_poll;
}

void g2_khl_end(void)
{
	FILE *khl_dump;
	uint32_t signature[3];
	const char *data_root_dir;
	char *name, *wptr;
	size_t name_len, i;

	if(gwc_db)
		dbm_close(gwc_db);

	/* try to write a khl dump */
	if(!server.settings.khl.dump_fname)
		return;

	if(server.settings.data_root_dir)
		data_root_dir = server.settings.data_root_dir;
	else
		data_root_dir = "./";

	name_len  = strlen(data_root_dir);
	name_len += strlen(server.settings.khl.dump_fname);
	name = alloca(name_len + 1);

	wptr = strpcpy(name, data_root_dir);
	strcpy(wptr, server.settings.khl.dump_fname);

	khl_dump = fopen(name, "wb");
	if(!khl_dump) {
		logg_errno(LOGF_INFO, "couldn't open khl dump");
		return;
	}

	fputs(KHL_DUMP_IDENT, khl_dump);

	signature[0] = KHL_DUMP_ENDIAN_MARKER;
	signature[1] = sizeof(struct khl_entry);
	signature[2] = KHL_DUMP_VERSION;

	fwrite(signature, sizeof(signature[0]), 3, khl_dump);

	for(i = 0; i < KHL_CACHE_SIZE; i++) {
		if(cache.entrys[i].used)
			fwrite(&cache.entrys[i].e, sizeof(cache.entrys[0].e), 1, khl_dump);
	}
	fclose(khl_dump);

	if(act_gwc.data.url)
		free((void *)(intptr_t)act_gwc.data.url);
}


static struct khl_cache_entry *khl_cache_entry_alloc(void)
{
	struct khl_cache_entry *e;
	unsigned i;

	if(KHL_CACHE_SIZE <= cache.num)
		return NULL;

	if(cache.next_free)
	{
		e = cache.next_free;
		cache.next_free = NULL;
		if(likely(!e->used))
			goto check_next_free;
	}

	for(i = 0, e = NULL; i < KHL_CACHE_SIZE; i++)
	{
		if(!cache.entrys[i].used) {
			e = &cache.entrys[i];
			break;
		}
	}

	if(e)
	{
check_next_free:
		e->used = true;
		cache.num++;
		if(e < &cache.entrys[KHL_CACHE_SIZE-1]) {
			if(!e[1].used)
				cache.next_free = &e[1];
		}
	}
	return e;
}

static void khl_cache_entry_free(struct khl_cache_entry *e)
{
	memset(e, 0, sizeof(*e));
	if(!cache.next_free)
		cache.next_free = e;
	cache.num--;
}

static struct khl_cache_entry *cache_ht_lookup(const union combo_addr *addr)
{
	uint32_t h = combo_addr_hash(addr, cache.ht_seed);
	struct hlist_node *n;
	struct khl_cache_entry *e;

// TODO: check for mapped ip addr?
// TODO: when IPv6 is common, change it
	if(likely(addr->s_fam == AF_INET))
	{
		hlist_for_each_entry(e, n, &cache.ht[h & KHL_CACHE_HTMASK], node)
		{
			if(e->e.na.s_fam != AF_INET)
				continue;
			if(e->e.na.in.sin_addr.s_addr == addr->in.sin_addr.s_addr &&
			   e->e.na.in.sin_port == addr->in.sin_port)
				return e;
		}
	}
	else
	{
		hlist_for_each_entry(e, n, &cache.ht[h & KHL_CACHE_HTMASK], node)
		{
			if(e->e.na.s_fam != AF_INET6)
				continue;
			if(IN6_ARE_ADDR_EQUAL(&e->e.na.in6.sin6_addr, &addr->in6.sin6_addr) &&
			   e->e.na.in.sin_port == addr->in.sin_port)
				return e;
		}
	}

	return NULL;
}

static void cache_ht_add(struct khl_cache_entry *e)
{
	uint32_t h = combo_addr_hash(&e->e.na, cache.ht_seed);
	hlist_add_head(&e->node, &cache.ht[h & KHL_CACHE_HTMASK]);
}

static void cache_ht_del(struct khl_cache_entry *e)
{
	hlist_del(&e->node);
}

static inline int rbnode_cache_eq(struct khl_cache_entry *a, struct khl_cache_entry *b)
{
	return a->e.when == b->e.when && combo_addr_eq(&a->e.na, &b->e.na);
}

static inline int rbnode_cache_lt(struct khl_cache_entry *a, struct khl_cache_entry *b)
{
	int ret = a->e.when < b->e.when;
	if(ret)
		return ret;
	ret = a->e.when > b->e.when;
	if(ret)
		return !ret;
	ret = a->e.na.s_fam < b->e.na.s_fam;
	if(ret)
		return ret;
	ret = a->e.na.s_fam > b->e.na.s_fam;
	if(ret)
		return !ret;

// TODO: when IPv6 is common, change it
	if(likely(AF_INET == a->e.na.s_fam))
	{
		ret = a->e.na.in.sin_addr.s_addr < b->e.na.in.sin_addr.s_addr;
		if(ret)
			return ret;
		ret = a->e.na.in.sin_addr.s_addr > b->e.na.in.sin_addr.s_addr;
		if(ret)
			return !ret;
		return a->e.na.in.sin_port < b->e.na.in.sin_port;
	}
	else
	{
		ret =  a->e.na.in6.sin6_addr.s6_addr32[0] < b->e.na.in6.sin6_addr.s6_addr32[0] ||
		       a->e.na.in6.sin6_addr.s6_addr32[1] < b->e.na.in6.sin6_addr.s6_addr32[1] ||
		       a->e.na.in6.sin6_addr.s6_addr32[2] < b->e.na.in6.sin6_addr.s6_addr32[2] ||
		       a->e.na.in6.sin6_addr.s6_addr32[3] < b->e.na.in6.sin6_addr.s6_addr32[3];
		if(ret)
			return ret;
		ret =  a->e.na.in6.sin6_addr.s6_addr32[0] > b->e.na.in6.sin6_addr.s6_addr32[0] ||
		       a->e.na.in6.sin6_addr.s6_addr32[1] > b->e.na.in6.sin6_addr.s6_addr32[1] ||
		       a->e.na.in6.sin6_addr.s6_addr32[2] > b->e.na.in6.sin6_addr.s6_addr32[2] ||
		       a->e.na.in6.sin6_addr.s6_addr32[3] > b->e.na.in6.sin6_addr.s6_addr32[3];
		if(ret)
			return !ret;
		return a->e.na.in6.sin6_port < b->e.na.in6.sin6_port;
	}
}

static inline void rbnode_cache_cp(struct khl_cache_entry *dest, struct khl_cache_entry *src)
{
	memcpy(&dest->used, &src->used,
	       sizeof(struct khl_cache_entry) - offsetof(struct khl_cache_entry, used));
}

RBTREE_MAKE_FUNCS(cache, struct khl_cache_entry, rb);

void g2_khl_add(const union combo_addr *addr, time_t when, bool cluster)
{
	struct khl_cache_entry *e, *t;

	/* check for bogus addresses */
	if(unlikely(!combo_addr_is_public(addr))) {
		logg_develd("addr %pI is privat, not added\n", addr);
		return;
	}
	logg_develd_old("adding: %p#I, %u\n", addr, (unsigned)when);


	if(unlikely(pthread_mutex_lock(&cache.lock)))
		return;

	/* already in the cache? */
	e = cache_ht_lookup(addr);
	if(e)
	{
		/* entry newer? */
		if(e->e.when >= when)
			goto out_unlock;
		if(!(e = rbtree_cache_remove(&cache.tree, e))) {
			logg_devel("remove failed\n");
			goto life_tree_error; /* something went wrong... */
		}
		e->e.when = when;
		if(!rbtree_cache_insert(&cache.tree, e)) {
			logg_devel("insert failed\n");
			goto life_tree_error;
		}
		goto out_unlock;
life_tree_error:
		cache_ht_del(e);
		khl_cache_entry_free(e);
		goto out_unlock;
	}

	t = rbtree_cache_lowest(&cache.tree);
	if(likely(t) && t->e.when < when && KHL_CACHE_SIZE <= cache.num)
	{
		logg_devel("found older entry\n");
		if(!(t = rbtree_cache_remove(&cache.tree, t)))
			goto out_unlock; /* the tree is amiss? */
		cache_ht_del(t);
		e = t;
	}
	else
		e = khl_cache_entry_alloc();
	if(!e)
		goto out_unlock;

	e->e.na = *addr;
	e->e.when = when;
	e->cluster = cluster;

	if(!rbtree_cache_insert(&cache.tree, e)) {
		khl_cache_entry_free(e);
		goto out_unlock;
	}
	cache_ht_add(e);

out_unlock:
	if(unlikely(pthread_mutex_unlock(&cache.lock)))
		diedie("gnarf, KHL cache lock stuck, bye!");
}

size_t g2_khl_fill_p(struct khl_entry p[], size_t len, int s_fam)
{
	struct khl_cache_entry *e;
	size_t res;

	if(unlikely(pthread_mutex_lock(&cache.lock)))
		return 0;

	e = cache.last_search;
	if(unlikely(!e))
		e = &cache.entrys[0];

	for(res = 0; res < len; res++)
	{
		struct khl_cache_entry *w = NULL;
		unsigned i = 0;

		for(; i < KHL_CACHE_SIZE; i++)
		{
			if(likely(e->used && e->e.na.s_fam == s_fam))
				w = e;
			if(++e >= &cache.entrys[KHL_CACHE_SIZE])
				e = &cache.entrys[0];
			if(w)
				break;
		}
		if(unlikely(!w))
			break;
		memcpy(&p[res], &w->e, sizeof(p[0]));
	}
	cache.last_search = e;

	if(unlikely(pthread_mutex_unlock(&cache.lock)))
		diedie("gnarf, KHL cache lock stuck, bye!");

	return res;
}

/*@unused@*/
static char const rcsid_khl[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
