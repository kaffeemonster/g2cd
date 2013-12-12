/*
 * G2KHL.c
 * known hublist foo
 *
 * Copyright (c) 2008-2012 Jan Seiffert
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
 * $Id:$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* System */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#elif defined(HAVE_MALLOC_H)
# include <malloc.h>
#endif
#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_LIBCURL
# include <curl/curl.h>
#endif
#include "lib/combo_addr.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef DRD_ME
# include <valgrind/drd.h>
#endif
#define W32_NEED_DB
#include "lib/my_pthread.h"
#ifndef WIN32
# include <netdb.h>
# ifdef HAVE_DB
#  define DB_DBM_HSEARCH 1
#  define DBM_TYPE DBM *
#  include <db.h>
# elif defined(HAVE_NDBM_H)
#  include <ndbm.h>
#  define DBM_TYPE DBM *
# else
#  include <gdbm.h>
#  define DBM_TYPE  GDBM_FILE
static DBM_TYPE dbm_open(const char *f, int flags, int mode)
{
	int nflags, nmode;

	flags &= O_RDONLY | O_RDWR | O_CREAT | O_TRUNC;
	if(O_RDONLY == flags)
		nflags = GDBM_READER, nmode = 0;
	else if((O_RDWR|O_CREAT))
		nflags = GDBM_WRCREAT, mode = mode;
	else if(O_TRUNC == (flags & O_TRUNC))
		nflags = GDBM_NEWDB, mode = mode;
	else
		nflags = GDBM_WRITER, mode = 0;
	return gdbm_open((char *)(intptr_t)f, 0, nflags, nmode, NULL);
}
#  define dbm_close      gdbm_close
#  define dbm_fetch      gdbm_fetch
#  define dbm_store      gdbm_store
#  define dbm_delete     gdbm_delete
#  define dbm_firstkey   gdbm_firstkey
#  define dbm_nextkey(a) gdbm_nextkey(a, gdbm_firstkey(a))
#  define DBM_REPLACE    GDBM_REPLACE
# endif
#else
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0501
# endif
# include <ws2tcpip.h>
# define DBM_TYPE DBM *
#endif
/* Own */
#define _G2KHL_C
#include "lib/other.h"
#include "G2KHL.h"
#include "G2MainServer.h"
#include "lib/my_epoll.h"
#include "lib/log_facility.h"
#include "lib/hlist.h"
#include "lib/rbtree.h"
#include "lib/my_bitops.h"
#include "lib/ansi_prng.h"
#include "timeout.h"
#ifndef O_BINARY
# define O_BINARY 0
#endif

#define KHL_CACHE_SHIFT 11
#define KHL_CACHE_SIZE (1 << KHL_CACHE_SHIFT)
#define KHL_CACHE_HTSIZE (KHL_CACHE_SIZE/8)
#define KHL_CACHE_HTMASK (KHL_CACHE_HTSIZE-1)
#define KHL_CACHE_FILL 8

#define KHL_DUMP_IDENT "G2CDKHLDUMP\n"
#define KHL_DUMP_ENDIAN_MARKER 0x12345678
#define KHL_DUMP_VERSION 0

#ifdef HAVE_LIBCURL
#  define KHL_MNMT_STATI \
	ENUM_CMD( BOOT      ), \
	ENUM_CMD( FILL      ), \
	ENUM_CMD( GWC_RES   ), \
	ENUM_CMD( GWC_WAIT  ), \
	ENUM_CMD( GWC_KICK  ), \
	ENUM_CMD( GWC_CLEAN ), \
	ENUM_CMD( IDLE      ), \
	ENUM_CMD( MAXIMUM   ) /* count */
#else
# define KHL_MNMT_STATI \
	ENUM_CMD( BOOT      ), \
	ENUM_CMD( FILL      ), \
	ENUM_CMD( GWC_RES   ), \
	ENUM_CMD( GWC_CON   ), \
	ENUM_CMD( GWC_REQ   ), \
	ENUM_CMD( GWC_REC   ), \
	ENUM_CMD( IDLE      ), \
	ENUM_CMD( MAXIMUM   ) /* count */
#endif

#define ENUM_CMD(x) KHL_##x
enum khl_mnmt_states
{
	KHL_MNMT_STATI
};
#undef ENUM_CMD

#ifndef HAVE_LIBCURL
# define GWC_RES_STATI \
	ENUM_CMD( HTTP            ), \
	ENUM_CMD( HTTP_11         ), \
	ENUM_CMD( HTTP_SKIP_WHITE ), \
	ENUM_CMD( HTTP_RESULT     ), \
	ENUM_CMD( CRLFCRLF        ), \
	ENUM_CMD( FIND_LINE       )

# define ENUM_CMD(x) GWC_RES_##x
enum gwc_res_states
{
	GWC_RES_STATI
};
# undef ENUM_CMD
#endif

struct gwc
{
	const char *url;
	unsigned int q_count;
	unsigned int m_count;
	time_t seen_last;
	time_t access_last;
	unsigned int access_period;
};

struct khl_cache_entry
{
	struct rb_node rb;
	struct hlist_node node;
	struct khl_entry e;
	bool cluster;
};

/* Vars */
static DBM_TYPE gwc_db;
static struct {
	struct gwc data;
	struct norm_buff *buff;
#ifdef HAVE_LIBCURL
	CURL *chandle;
	CURLM *mhandle;
	struct curl_slist *header_stuff;
	char *request_url;
	time_t timeout_last_update;
	long timeout_ms;
	unsigned int curl_version_num;
	int curl_features;
#else
	struct addrinfo *addrinfo;
	struct addrinfo *next_addrinfo;
	char *request_string;
	unsigned port;
	int socket;
	enum gwc_res_states state;
#endif
} act_gwc;
static struct {
	mutex_t lock;
	int khl_dump;
	int num;
	enum khl_mnmt_states state;
	uint32_t ht_seed;
	struct hlist_head free_list;
	struct rb_root tree;
	struct hlist_head ht[KHL_CACHE_HTSIZE];
	struct khl_cache_entry entrys[KHL_CACHE_SIZE];
} cache;
#define ENUM_CMD(x) str_it(KHL_##x)
static const char khl_mnmt_states_names[][16] =
{
	KHL_MNMT_STATI
};
#undef ENUM_CMD

#if defined(DEBUG_DEVEL_OLD) && !defined(HAVE_LIBCURL)
# define ENUM_CMD(x) str_it(GWC_RES_##x)
static const char *gwc_res_http_names[] =
{
	GWC_RES_STATI
};
# undef ENUM_CMD
#endif


static void put_boot_gwc_in_cache(void)
{
	datum key, value;

	/* no boot url?? K, continue, don't crash */
	if(!server.settings.khl.gwc_boot_url)
		return;

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
}

/*
 * Functs
 */
#ifdef HAVE_LIBCURL
static void handle_watch(struct sock_com *sc, short events)
{
	int ev = 0;
	int rhandle = 0;
	CURLMcode curlm_res;

	ev |= events & POLLIN  ? CURL_CSELECT_IN  : 0;
	ev |= events & POLLOUT ? CURL_CSELECT_OUT : 0;
	ev |= events & POLLERR ? CURL_CSELECT_ERR : 0;

	curlm_res = curl_multi_socket_action(act_gwc.mhandle, sc->fd, ev, &rhandle);
	if(CURLM_OK != curlm_res) {
		logg_posd(LOGF_INFO, "error in multi_socket_action: %s\n", curl_multi_strerror(curlm_res));
		cache.state = KHL_GWC_KICK;
	} else {
		if(!rhandle)
			cache.state = KHL_GWC_CLEAN;
	}
}

static int add_watch(curl_socket_t s, int action)
{
	struct sock_com *sc;
	short events = 0;

	switch(action)
	{
	case CURL_POLL_NONE:
		break;
	case CURL_POLL_IN:
		events |= POLLIN;
		break;
	case CURL_POLL_OUT:
		events |= POLLOUT;
		break;
	case CURL_POLL_INOUT:
		events |= POLLIN | POLLOUT;
		break;
	case CURL_POLL_REMOVE:
		/* should not happen!! */
// TODO: do a remove
		return 0;
	}

	logg_develd_old("adding watch: %#hx %s\n", events, action != CURL_POLL_NONE ? "enabled" : "disabled");
	sc = sock_com_add_fd(handle_watch, NULL, s, events, action != CURL_POLL_NONE);
	if(sc) {
		CURLMcode curlm_res = curl_multi_assign(act_gwc.mhandle, s, sc);
		if(CURLM_OK != curlm_res) {
			logg_posd(LOGF_INFO, "couldn't assign sock_com to libcurl socket: %s\n", curl_multi_strerror(curlm_res));
			return -1;
		} else
			return 0;
	} else
		return -1;
}

static int toggle_watch(int action, struct sock_com *sc)
{
	short events = 0;

	switch(action)
	{
	case CURL_POLL_NONE:
		break;
	case CURL_POLL_IN:
		events |= POLLIN;
		break;
	case CURL_POLL_OUT:
		events |= POLLOUT;
		break;
	case CURL_POLL_INOUT:
		events |= POLLIN | POLLOUT;
		break;
	case CURL_POLL_REMOVE:
		/* should not happen!! */
// TODO: do a remove
		return 0;
	}
	logg_develd_old("toggling watch %p oev: %#hx nev: %#hx oe: %i ne: %i \n",
		sc, sc->events, events, sc->enabled, action != CURL_POLL_NONE);
	sc->enabled = action != CURL_POLL_NONE;
	sc->events = events;
	return 0;
}

static int remove_watch(struct sock_com *s)
{
	logg_develd_old("want to remove watch: %p\n", s);

	if(!s)
		return 0;

	logg_develd_old("removing watch: %p\n", s);
	sock_com_delete(s);
	return 0;
}

static int gwc_curl_socket_callback(CURL *easy /* easy handle */ GCC_ATTR_UNUSED_PARAM,
                                    curl_socket_t s /* socket */,
                                    int action /* see values below */,
                                    void *userp /* private callback pointer */ GCC_ATTR_UNUSED_PARAM,
                                    void *socketp)
{
	if(CURL_POLL_REMOVE == action)
		return remove_watch(socketp);

	if(!socketp)
		return add_watch(s, action);

	return toggle_watch(action, socketp);
}

static int gwc_curl_timeout_callback(CURLM *multi /* multi handle */ GCC_ATTR_UNUSED_PARAM,
                                     long timeout_ms /* the new timeout */,
                                     void *userp /* private callback pointer */ GCC_ATTR_UNUSED_PARAM)
{
	logg_develd_old("new timeout for %p: %ld\n", multi, timeout_ms);
	act_gwc.timeout_last_update = local_time_now;
	act_gwc.timeout_ms = timeout_ms;
	return 0;
}

#endif

bool __init g2_khl_init(void)
{
	struct khl_entry *e;
	uint32_t *signature;
	const char *data_root_dir;
	const char *gwc_cache_fname;
	char *name, *buff;
	size_t name_len = 0, i;
#ifdef HAVE_LIBCURL
	CURLcode curl_res;
	CURLMcode curlm_res;
	curl_version_info_data *cvi_data;
#endif

#ifdef DRD_ME
	DRD_IGNORE_VAR(cache.num);
#endif
	/* open the gwc cache db */
	if(server.settings.data_root_dir)
		data_root_dir = server.settings.data_root_dir;
	else {
		logg_devel("setting datarootdir hard\n");
		data_root_dir = ".";
	}
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

	gwc_db = dbm_open(name, O_RDWR|O_CREAT|O_BINARY, 00666);
	if(!gwc_db)
	{
		logg_errno(LOGF_NOTICE, "opening GWC cache");
		gwc_db = dbm_open(name, O_RDWR|O_CREAT|O_TRUNC|O_BINARY, 00666);
		if(!gwc_db) {
			logg_errno(LOGF_ERR, "second try opening GWC cache");
			return false;
		}
	}

	put_boot_gwc_in_cache();

	if((errno = mutex_init(&cache.lock))) {
		logg_errno(LOGF_ERR, "initialising KHL cache lock");
		return false;
	}
	/* shuffle all entrys in the free list */
	for(i = 0; i < KHL_CACHE_SIZE; i++)
		hlist_add_head(&cache.entrys[i].node, &cache.free_list);
	random_bytes_get(&cache.ht_seed, sizeof(cache.ht_seed));

	/*
	 * Librarys...
	 * My Berkley DB (the provider of my dbm) needs also some
	 * kind of random (a seed?).
	 * But since it does not know if the app using it already
	 * initialised the random number generator, it does so itself.
	 * Unfortunatly for us, we already did this, with some fine
	 * entropy from /dev/urandom and aes and whatnot.
	 * DB in contrast uses some inferior time() and uname stuff.
	 * And makes our init void.
	 * So reinit it...
	 */
	random_bytes_get(&name_len, sizeof(name_len));
	srand((unsigned int)name_len);

#ifdef HAVE_LIBCURL
	curl_res = curl_global_init(CURL_GLOBAL_ALL & (~CURL_GLOBAL_WIN32));
	if(curl_res) {
		logg_posd(LOGF_ERR, "Initialising libcurl failed with %i\n", curl_res);
		return false;
	}
	act_gwc.mhandle = curl_multi_init();
	if(!act_gwc.mhandle) {
		logg_pos(LOGF_ERR, "Can't get a libcurl multihandle\n");
		return false;
	}
	act_gwc.header_stuff = curl_slist_append(NULL, "Cache-control: no-cache");
	act_gwc.header_stuff = curl_slist_append(act_gwc.header_stuff, "Accept: text/plain");
	curlm_res = curl_multi_setopt(act_gwc.mhandle, CURLMOPT_SOCKETFUNCTION, gwc_curl_socket_callback);
	if(CURLM_OK != curlm_res) {
		logg_posd(LOGF_ERR, "Can't register socket callback with libcurl: %s\n", curl_multi_strerror(curlm_res));
		return false;
	}
	curlm_res = curl_multi_setopt(act_gwc.mhandle, CURLMOPT_TIMERFUNCTION, gwc_curl_timeout_callback);
	if(CURLM_OK != curlm_res) {
		logg_posd(LOGF_ERR, "Can't register timeout callback with libcurl: %s\n", curl_multi_strerror(curlm_res));
		return false;
	}
	cvi_data = curl_version_info(CURLVERSION_NOW);
	act_gwc.curl_version_num  = cvi_data->version_num;
	act_gwc.curl_features     = cvi_data->features;
#endif

	/* try to read a khl dump */
	if(!server.settings.khl.dump_fname)
		return true;

	name_len  = strlen(data_root_dir);
	name_len += strlen(server.settings.khl.dump_fname);
	name = alloca(name_len + 2);

	buff = strpcpy(name, data_root_dir);
	*buff++ = '/';
	strcpy(buff, server.settings.khl.dump_fname);

	cache.khl_dump = open(name, O_CREAT|O_RDWR|O_BINARY, 0664);
	if(-1 == cache.khl_dump) {
		logg_errnod(LOGF_INFO, "couldn't open khl dump \"%s\"", name);
		goto out;
	}

	buff = alloca(500);
	if(str_size(KHL_DUMP_IDENT) != read(cache.khl_dump, buff, str_size(KHL_DUMP_IDENT))) {
		logg_posd(LOGF_INFO, "couldn't read ident from khl dump \"%s\"\n", name);
		goto out;
	}

	if(strncmp(buff, KHL_DUMP_IDENT, str_size(KHL_DUMP_IDENT))) {
		logg_posd(LOGF_INFO, "\"%s\" has wrong khl dump ident\n", name);
		goto out;
	}

	signature = (uint32_t *)buff;
	if(sizeof(uint32_t) * 3 != read(cache.khl_dump, signature, sizeof(uint32_t) * 3)) {
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
		e = (void *) buff;
		name_len = read(cache.khl_dump, e, sizeof(*e));
		if(sizeof(*e) == name_len)
			g2_khl_add(&e->na, e->when, false);
	} while(sizeof(*e) == name_len);

out:
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

#ifdef HAVE_LIBCURL
	free(act_gwc.request_url);
	act_gwc.request_url = NULL;

	if(act_gwc.chandle) {
		curl_multi_remove_handle(act_gwc.mhandle, act_gwc.chandle);
		curl_easy_cleanup(act_gwc.chandle);
		act_gwc.chandle = NULL;
	}
#else
	if(act_gwc.addrinfo)
		freeaddrinfo(act_gwc.addrinfo);
	act_gwc.addrinfo = NULL;
	act_gwc.next_addrinfo = NULL;

	if(-1 != act_gwc.socket)
		closesocket(act_gwc.socket);
	act_gwc.socket = -1;

	free(act_gwc.request_string);
	act_gwc.request_string = NULL;

	act_gwc.port = 80; /* general http */
	act_gwc.state = GWC_RES_HTTP;
#endif
	free(act_gwc.buff);
	act_gwc.buff = NULL;
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

static bool gwc_get_rand(datum *key, datum *value)
{
	int retry_count = 0;
	unsigned cycling;

retry:
	retry_count++;
	cycling = (unsigned)((((unsigned long long)rand())*8)/((unsigned long long)RAND_MAX));
	do
	{
		*key = dbm_nextkey(gwc_db);
		if(!key->dptr)
			*key = dbm_firstkey(gwc_db);
		if(!key->dptr) {
			/* uhm, no key? this should not happen */
			return false;
		}
	} while(cycling--);

	*value = dbm_fetch(gwc_db, *key);
	if(!value->dptr) { /* huh? key but no data? */
		if(5 < retry_count)
			return false;
		goto retry;
	}
	if(sizeof(struct gwc) != value->dsize)
	{
		/* what to do on error? */
		if(dbm_delete(gwc_db, *key))
			logg_errno(LOGF_WARN, "not able to remove broken GWC from db");
		if(5 < retry_count)
			return false;
		goto retry;
	}

	return true;
}

static bool gwc_switch(void)
{
	char *url;
	datum key, value;

	if(!gwc_get_rand(&key, &value))
		return false;

	gwc_clean();

	url = malloc(key.dsize);
	if(!url)
		return false;
	memcpy(&act_gwc.data, value.dptr, sizeof(act_gwc.data));
	memcpy(url, key.dptr, key.dsize);
	act_gwc.data.url = url;

	return true;
}

const char *g2_khl_get_url(void)
{
	datum key, value;

	if(!gwc_get_rand(&key, &value))
		return NULL;

// TODO: is this save? or will the db remove the data? and when?
	return key.dptr;
}

static noinline void gwc_handle_line(char *line, time_t lnow)
{
	char *wptr;
	char response;

	/* we need at least 2 chars */
	if(2 > strlen(line))
		return;

	logg_develd_old("gwc \"%s\" response: \"%s\"\n", act_gwc.data.url, line);
	/* skip whitespace */
	wptr = str_skip_space(line);

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
			/* skip whitespace at period start */
			wptr = str_skip_space(next + str_size("access|period|"));
			if(*wptr && isdigit_a(*(unsigned char *)wptr))
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
			char *next;

			/* first try to find next seperator */
			next = strchr(wptr, '|');
			if(next) {
				*next++ = '\0';
				/* skip whitespace at timestamp start */
				next = str_skip_space(next);
				if(*next && isdigit_a(*(unsigned char *)next))
					since = atoi(next);
			}

			/* skip whitespace at addr start */
			wptr = str_skip_space(wptr);

			memset(&a, 0, sizeof(a));
			if(!combo_addr_read_wport(wptr, &a)) {
				logg_develd("failed to parse \"%s\"\n", wptr);
				break;
			}

			if(!combo_addr_port(&a))
				combo_addr_set_port(&a, htons(6346));

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
			size_t len;
//			int since = 0;
			char *next, *after_prot;

			next = strstr(wptr, "http://");
			if(!next)
			{
#ifdef HAVE_LIBCURL
				if(!(act_gwc.curl_features & CURL_VERSION_SSL))
					break;
				next = strstr(wptr, "https://");
				if(next)
					after_prot = next + str_size("https://");
				else
#endif
					break;
			} else {
				after_prot = next + str_size("http://");
			}
			wptr = next;

			/* first try to find next seperator */
			next = strchr(wptr, '|');
			if(next) {
				len = next - wptr;
				*next++ = '\0';
#if 0
				/* skip whitespace at timestamp start */
				next = str_skip_space(next);
				if(*next && isdigit_a(*(unsigned char *)next))
					since = atoi(next);
#endif
			}
			else
				len = strlen(wptr);
			/* trim trailing whitespace */
			for(next = wptr + len; next--, next >= wptr && isspace_a(*(unsigned char *)next);)
				/* nothing to do */;

			if(next < wptr)
				break;
			len = next + 1 - wptr;
			wptr[len] = '\0';

			if(NULL == strchr(after_prot, '/')) {
				wptr[len++] = '/';
				wptr[len] = '\0';
			}

			key.dptr  = (void *)wptr;
			key.dsize = len + 1;

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
			else {
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

#ifdef HAVE_LIBCURL
static size_t gwc_curl_readfunc(void *ptr, size_t size, size_t nmemb, void *userdata GCC_ATTR_UNUSED_PARAM)
{
	/* this function should not get called */
	memset(ptr, 0, size * nmemb);
	return size * nmemb;
}

static size_t gwc_curl_writefunc(char *ptr, size_t size, size_t nmemb, void *userdata GCC_ATTR_UNUSED_PARAM)
{
	size_t pos = 0;
	struct norm_buff *buff = act_gwc.buff;

	logg_develd_old("size: %zu\tnmemb: %zu\n", size, nmemb);
	/* this should not happen, since size should be 1 */
	if(unlikely(nmemb > ((~((size_t)0))/size)))
		return 0;
	size = size * nmemb;

	do {
		size_t rem = buffer_remaining(*buff);
		char *wptr;

		if(!rem)
		{
			/* buffer still full? force forward progress */
			buffer_flip(*buff);
			buffer_skip(*buff);
			buffer_compact(*buff);
			rem = buffer_remaining(*buff);
		}
		rem = rem < size - pos ? rem : size - pos;
		memcpy(buffer_start(*buff), ptr + pos, rem);
		pos += rem;
		buff->pos += rem;
		buffer_flip(*buff);

		/* turn \r -> \n */
		while((wptr = memchr(buffer_start(*buff), '\r', buffer_remaining(*buff)))) {
			*wptr++ = '\n';
			buff->pos = wptr - buff->data;
		}
		buff->pos = 0;

		while((wptr = memchr(buffer_start(*buff), '\n', buffer_remaining(*buff)))) {
			*wptr++ = '\0';
			gwc_handle_line(buffer_start(*buff), local_time_now);
			buff->pos = wptr - buff->data;
		}
		buffer_compact(*buff);
	} while(size - pos);

	return size;
}

/* the progressmeter callback */
static int gwc_curl_meterfunc(void *clientp GCC_ATTR_UNUSED_PARAM,
                              double dltotal GCC_ATTR_UNUSED_PARAM, double dlnow GCC_ATTR_UNUSED_PARAM,
                              double ultotal GCC_ATTR_UNUSED_PARAM, double ulnow GCC_ATTR_UNUSED_PARAM)
{
	/*
	 * should not be called, we do not want a progress
	 * meter, but to prevent prints, provide and set callback
	 */
	return 0;
}

#if 0
static int gwc_curl_debugfunc(CURL *handle, curl_infotype ci, char *data, size_t dlen, void *userp)
{

}
#endif

# define do_an_setopt(option, value) do { \
	if(CURLE_OK != (cres = curl_easy_setopt(act_gwc.chandle, option, value))) { \
		opt_name = str_it(option); \
		goto out_err_opt; \
	} } while(0)
# define do_an_setopt_missing_ok(option, value) do { \
	cres = curl_easy_setopt(act_gwc.chandle, option, value); \
	if(CURLE_OK != cres && CURLE_FAILED_INIT != cres) { \
		opt_name = str_it(option); \
		goto out_err_opt; \
	} } while(0)
# define HTTP_GET_PARAM "?get=1&hostfile=1&net=gnutella2&client=" OWN_VENDOR_CODE "&version=" OUR_VERSION "&ping=1"
static bool gwc_curl_prepare(void)
{
	const char *opt_name;
	size_t slen;
	CURLcode cres;
	CURLMcode cmres;

	logg_develd("asking gwc \%s\"\n", act_gwc.data.url);
	slen = strlen(act_gwc.data.url);
	act_gwc.request_url = malloc(slen + sizeof(HTTP_GET_PARAM));
	if(!act_gwc.request_url) {
		logg_errno(LOGF_INFO, "couldn't request mem for request_url");
		return false;
	}
	memcpy(act_gwc.request_url, act_gwc.data.url, slen);
	strplitcpy(act_gwc.request_url + slen, HTTP_GET_PARAM);

	act_gwc.buff = malloc(sizeof(*act_gwc.buff));
	if(!act_gwc.buff) {
		logg_errno(LOGF_INFO, "couldn't request mem for buffer");
		return false;
	}
	act_gwc.buff->capacity = sizeof(act_gwc.buff->data);
	buffer_clear(*act_gwc.buff);

	act_gwc.chandle = curl_easy_init();
	if(!act_gwc.chandle) {
		logg_pos(LOGF_INFO, "couldn't get libcurl easy handle\n");
		return false;
	}

# ifdef DEBUG_DEVEL_OLD
	do_an_setopt(CURLOPT_VERBOSE, 1l);
# endif
	do_an_setopt(CURLOPT_PROGRESSFUNCTION, gwc_curl_meterfunc);
	do_an_setopt(CURLOPT_WRITEFUNCTION, gwc_curl_writefunc);
	do_an_setopt(CURLOPT_READFUNCTION, gwc_curl_readfunc);
	do_an_setopt(CURLOPT_PROTOCOLS, CURLPROTO_HTTP|CURLPROTO_HTTPS);
	do_an_setopt(CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP|CURLPROTO_HTTPS);
	do_an_setopt(CURLOPT_AUTOREFERER, 1l);
	do_an_setopt(CURLOPT_FOLLOWLOCATION, 1l);
	do_an_setopt(CURLOPT_MAXREDIRS, 30l);
	do_an_setopt(CURLOPT_USERAGENT, OUR_UA);
	do_an_setopt(CURLOPT_HTTPHEADER, act_gwc.header_stuff);
# if LIBCURL_VERSION_NUM >= 0x071506
	if(act_gwc.curl_version_num >= 0x071506)
		do_an_setopt(CURLOPT_ACCEPT_ENCODING, "");
	else
# endif
		do_an_setopt(CURLOPT_ENCODING, "");
	do_an_setopt_missing_ok(CURLOPT_WILDCARDMATCH, 0l);
	do_an_setopt(CURLOPT_URL, act_gwc.request_url);

	cmres = curl_multi_add_handle(act_gwc.mhandle, act_gwc.chandle);
	if(CURLM_OK != cmres) {
		logg_posd(LOGF_INFO, "adding easy handle to multi handle: %s\n", curl_multi_strerror(cmres));
		goto out_err;
	}

	return true;
out_err_opt:
	logg_posd(LOGF_INFO, "libcurl setting \"%s\": %s\n", opt_name, curl_easy_strerror(cres));
out_err:
	curl_easy_cleanup(act_gwc.chandle);
	act_gwc.chandle = NULL;
	return false;
}

#else
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
			if(port <= 0xFFFF) {
				act_gwc.port = port;
				service = wptr;
			}
		}
		/* failure? ignore, try 80 */
	}

//TODO: is passing service usefull? Seems to make trouble when setup b0rken
	/* resolve */
//	logg_develd("node: \"%s\" service: \"%s\"\n", node, service);
	ret_val = getaddrinfo(node, service, NULL, &res_res);
	/* transient errors are bad, but not "kill that entry" bad */
	if(ret_val) {
		if(EAI_AGAIN  == ret_val || EAI_MEMORY == ret_val)
			ret_val = true;
		else {
			logg_posd(LOGF_NOTICE, "resolving failed: %s\n", gai_strerror(ret_val));
			goto resolv_fail;
		}
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
			if(!(ENOMEM  == s_errno || ENOBUFS == s_errno || EMFILE  == s_errno))
				act_gwc.next_addrinfo = act_gwc.next_addrinfo->ai_next; /* mark next to connect */
		}
		goto out_err;
	}

	addr = (union combo_addr *)tai->ai_addr;
	/* make sure address family is copied to the struct sockaddr field */
	addr->s.fam = tai->ai_family;
	casalen_ii(addr);
	combo_addr_set_port(addr, htons(act_gwc.port));

	logg(LOGF_INFO, "Connecting to GWC \"%s\" for Hubs\n", act_gwc.data.url);

	do {
		set_s_errno(0);
		ret_val = connect(fd, casa(addr), tai->ai_addrlen);
	} while(ret_val && EINTR == s_errno);
	time(&act_gwc.data.access_last);
	if(!ret_val)
		act_gwc.socket = fd;
	else
	{
		closesocket(fd);
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
		set_s_errno(0);
		ret_val = send(act_gwc.socket, act_gwc.request_string, len, 0);
	} while(-1 == ret_val && EINTR == s_errno);

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
	closesocket(act_gwc.socket);
	act_gwc.socket = -1;
	return false;
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
				unsigned char c = *buffer_start(*buff);
				if(isspace_a(c))
					buff->pos++;
				else if(isdigit_a(c)) {
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
		if(!isspace_a(*buffer_start(*buff) & 0x7F))
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
	struct sock_com *s;
	struct norm_buff *buff = act_gwc.buff;
	ssize_t result;
	int ret_val = 1;

	s = sock_com_fd_find(act_gwc.socket);

	do {
		set_s_errno(0);
		result = recv(act_gwc.socket, buffer_start(*buff), buffer_remaining(*buff)-1, 0);
	} while(-1 == result && EINTR == s_errno);

	switch(result)
	{
	default:
		buff->pos += result;
		break;
	case 0:
		if(buffer_remaining(*buff)-1)
		{
			if(EAGAIN != s_errno)
			{ /* we have reached EOF */
				sock_com_delete(s);
				closesocket(act_gwc.socket);
				act_gwc.socket = -1;
				ret_val = 0;
				free(s);
				s = NULL;
			}
			else
				logg_devel("nothing to read!\n");
		}
		break;
	case -1:
		if(!(EAGAIN == s_errno || EWOULDBLOCK == s_errno))
		{
			logg_serrno(LOGF_DEBUG, "reading gwc");
			sock_com_delete(s);
			closesocket(act_gwc.socket);
			act_gwc.socket = -1;
			ret_val = -1;
			free(s);
			s = NULL;
		}
	}
	/* we should have 1 char space left put a '\0' at the end */
	*buffer_start(*buff) = '\0';
	/* no buff->pos++, the zero is only for safety */
	buffer_flip(*buff);
	if(!gwc_handle_response()) {
		sock_com_delete(s);
		if(-1 != act_gwc.socket)
			closesocket(act_gwc.socket);
		act_gwc.socket = -1;
		ret_val = -1;
		free(s);
		s = NULL;
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
#endif

bool g2_khl_tick(void)
{
	bool long_poll = false;

	if(KHL_IDLE != cache.state)
		logg_develd("state: %s\n", khl_mnmt_states_names[cache.state]);

	switch(cache.state)
	{
	case KHL_BOOT:
		cache.state = KHL_CACHE_FILL > cache.num ? KHL_FILL : KHL_IDLE;
		break;
	case KHL_FILL:
		if(gwc_switch())
			cache.state = KHL_GWC_RES;
		else
			put_boot_gwc_in_cache();
		break;
	case KHL_GWC_RES:
#ifdef HAVE_LIBCURL
		if(gwc_curl_prepare())
			cache.state = KHL_GWC_WAIT;
		else
			cache.state = KHL_FILL;
		break;
	case KHL_GWC_WAIT:
		logg_develd_old("time: %ld %ld %ld\n", act_gwc.timeout_ms, act_gwc.timeout_last_update, local_time_now);
		if(act_gwc.timeout_ms >= 0 &&
		   (act_gwc.timeout_last_update + ((act_gwc.timeout_ms+999)/1000)) <= local_time_now)
		{
			int rhandles = 0;
			logg_devel_old("calling action\n");
			CURLMcode cmres = curl_multi_socket_action(act_gwc.mhandle, CURL_SOCKET_TIMEOUT, 0, &rhandles);
			if(CURLM_OK != cmres) {
				logg_posd(LOGF_INFO, "error calling multi_socket_action on timeout: %s\n", curl_multi_strerror(cmres));
				cache.state = KHL_GWC_KICK;
			} else {
				logg_develd_old("rhandles: %i\n", rhandles);
				if(!rhandles)
					cache.state = KHL_GWC_CLEAN;
			}
		}
		break;
	case KHL_GWC_KICK:
		gwc_kick(&act_gwc.data);
	case KHL_GWC_CLEAN:
		gwc_clean();
		cache.state = KHL_IDLE;
		break;
#else
		if(gwc_resolv())
			cache.state = KHL_GWC_CON;
		else
			cache.state = KHL_FILL;
		break;
	case KHL_GWC_CON:
		{
			int result = gwc_connect();
			if(!result)
				cache.state = KHL_GWC_REQ;
			else {
				if(-1 == result)
					cache.state = KHL_FILL;
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
			if(!sock_com_add_fd(G2KHL_SOCK_COM_HANDLER, NULL, act_gwc.socket,
			                    POLLIN|POLLERR|POLLHUP, true))
			{
				act_gwc.data.q_count++;
				time(&act_gwc.data.seen_last);
				gwc_clean();
				cache.state = KHL_FILL;
				break;
			}
			cache.state = KHL_GWC_REC;
		}
		else {
			cache.state = KHL_FILL;
			break;
		}
	case KHL_GWC_REC:
		{
			int result = gwc_receive();
			if(0 > result)
				cache.state = KHL_FILL; /* clean?? */
			else if (0 == result) {
				act_gwc.data.q_count++;
				time(&act_gwc.data.seen_last);
				gwc_clean();
				cache.state = KHL_IDLE;
			}
		}
		break;
#endif
	case KHL_IDLE:
		if(KHL_CACHE_FILL > cache.num)
			cache.state = KHL_FILL;
		else
			cache.state = KHL_IDLE;
// TODO: anounce ourself to gwc
		/*
		 * wget -v -S -U G2CD -O -
		 * "http://cache.trillinux.org/g2/bazooka.php?update=1&ip=217.87.64.8:6346&x_leaves=0&net=gnutella2&client=G2CD&version=0.0.0.10&ping=1"
		 */
		long_poll = true;
		break;
	case KHL_MAXIMUM:
		logg_devel("Ouch! khl state is MAXIMUM?? Should not happen! Fixing up...");
		cache.state = KHL_IDLE;
	}

	return long_poll;
}

void g2_khl_end(void)
{
	uint32_t signature[3];

	if(gwc_db)
		dbm_close(gwc_db);

	if(cache.khl_dump <= 0)
	{
		const char *data_root_dir;
		char *name, *wptr;
		size_t name_len;
		/* try to write a khl dump */
		if(!server.settings.khl.dump_fname)
			return;

		if(server.settings.data_root_dir)
			data_root_dir = server.settings.data_root_dir;
		else
			data_root_dir = ".";

		name_len  = strlen(data_root_dir);
		name_len += strlen(server.settings.khl.dump_fname);
		name = alloca(name_len + 2);

		wptr = strpcpy(name, data_root_dir);
		*wptr++ = '/';
		strcpy(wptr, server.settings.khl.dump_fname);

		cache.khl_dump = open(name, O_CREAT|O_RDWR|O_BINARY, 0664);
		if(-1 == cache.khl_dump) {
			logg_errnod(LOGF_INFO, "couldn't open khl dump \"%s\"", name);
			return;
		}
	}
	else
	{
		if((off_t)-1 == lseek(cache.khl_dump, 0, SEEK_SET)) {
			logg_errno(LOGF_INFO, "couldn't seek khl dump");
			goto out;
		}
		if(0 > ftruncate(cache.khl_dump, 0)) {
			logg_errno(LOGF_INFO, "couldn't truncate khl dump");
			goto out;
		}
	}

	if(str_size(KHL_DUMP_IDENT) != write(cache.khl_dump, KHL_DUMP_IDENT, str_size(KHL_DUMP_IDENT)))
		goto out;

	signature[0] = KHL_DUMP_ENDIAN_MARKER;
	signature[1] = sizeof(struct khl_entry);
	signature[2] = KHL_DUMP_VERSION;

	if(3 * sizeof(signature[0]) == write(cache.khl_dump, signature, sizeof(signature[0]) * 3))
	{
		struct rb_node *n;
		struct khl_cache_entry *k;

		for(n = rb_first(&cache.tree); n; n = rb_next(n)) {
			k = rb_entry(n, struct khl_cache_entry, rb);
			if(sizeof(k->e) != write(cache.khl_dump, &k->e, sizeof(k->e)))
				break;
		}
	}

out:
	close(cache.khl_dump);
	if(act_gwc.data.url)
		free((void *)(intptr_t)act_gwc.data.url);
#ifdef HAVE_LIBCURL
	if(act_gwc.chandle) {
		curl_multi_remove_handle(act_gwc.mhandle, act_gwc.chandle);
		curl_easy_cleanup(act_gwc.chandle);
		act_gwc.chandle = NULL;
	}
	if(act_gwc.mhandle) {
		curl_multi_cleanup(act_gwc.mhandle);
		act_gwc.chandle = NULL;
	}
	if(act_gwc.header_stuff) {
		curl_slist_free_all(act_gwc.header_stuff);
		act_gwc.header_stuff = NULL;
	}
	curl_global_cleanup();
#endif
}


static struct khl_cache_entry *khl_cache_entry_alloc(void)
{
	struct khl_cache_entry *e;
	struct hlist_node *n;

	if(hlist_empty(&cache.free_list))
		return NULL;

	n = cache.free_list.first;
	e = hlist_entry(n, struct khl_cache_entry, node);
	hlist_del(&e->node);
	INIT_HLIST_NODE(&e->node);
	RB_CLEAR_NODE(&e->rb);
	cache.num++;
	return e;
}

static void khl_cache_entry_free(struct khl_cache_entry *e)
{
	memset(e, 0, sizeof(*e));
	hlist_add_head(&e->node, &cache.free_list);
	cache.num--;
}

static uint32_t cache_ht_hash(const union combo_addr *addr)
{
	return combo_addr_hash(addr, cache.ht_seed);
}

static struct khl_cache_entry *cache_ht_lookup(const union combo_addr *addr, uint32_t h)
{
	struct hlist_node *n;
	struct khl_cache_entry *e;

// TODO: check for mapped ip addr?
// TODO: when IPv6 is common, change it
	if(likely(addr->s.fam == AF_INET))
	{
		hlist_for_each_entry(e, n, &cache.ht[h & KHL_CACHE_HTMASK], node)
		{
			if(e->e.na.s.fam != AF_INET)
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
			if(e->e.na.s.fam != AF_INET6)
				continue;
			if(IN6_ARE_ADDR_EQUAL(&e->e.na.in6.sin6_addr, &addr->in6.sin6_addr) &&
			   e->e.na.in.sin_port == addr->in.sin_port)
				return e;
		}
	}

	return NULL;
}

static void cache_ht_add(struct khl_cache_entry *e, uint32_t h)
{
	hlist_add_head(&e->node, &cache.ht[h & KHL_CACHE_HTMASK]);
}

static void cache_ht_del(struct khl_cache_entry *e)
{
	hlist_del(&e->node);
}

static int khl_entry_cmp(struct khl_cache_entry *a, struct khl_cache_entry *b)
{
	int ret = (long)a->e.when - (long)b->e.when;
	if(ret)
		return ret;
	return combo_addr_cmp(&a->e.na, &b->e.na);
}

static noinline bool khl_rb_cache_insert(struct khl_cache_entry *e)
{
	struct rb_node **p = &cache.tree.rb_node;
	struct rb_node *parent = NULL;

	while(*p)
	{
		struct khl_cache_entry *n = rb_entry(*p, struct khl_cache_entry, rb);
		int result = khl_entry_cmp(n, e);

		parent = *p;
		if(result < 0)
			p = &((*p)->rb_left);
		else if(result > 0)
			p = &((*p)->rb_right);
		else
			return false;
	}
	rb_link_node(&e->rb, parent, p);
	rb_insert_color(&e->rb, &cache.tree);

	return true;
}

static noinline bool khl_rb_cache_remove(struct khl_cache_entry *e)
{
	struct rb_node *n = &e->rb;

	if(RB_EMPTY_NODE(n))
		return false;

	rb_erase(n, &cache.tree);
	RB_CLEAR_NODE(n);
	cache_ht_del(e);

	return true;
}

static struct khl_cache_entry *khl_cache_last(void)
{
	struct rb_node *n = rb_last(&cache.tree);
	if(!n)
		return NULL;
	return rb_entry(n, struct khl_cache_entry, rb);
}

void g2_khl_add(const union combo_addr *addr, time_t when, bool cluster)
{
	struct khl_cache_entry *e;
	uint32_t h;

	/* check for bogus addresses */
	if(unlikely(!combo_addr_is_public(addr))) {
		logg_develd_old("addr %pI is private, not added\n", addr);
		return;
	}
	logg_develd_old("adding: %p#I, %u\n", addr, (unsigned)when);

	h = cache_ht_hash(addr);

	/*
	 * Prevent the compiler from moving the calcs into
	 * the critical section
	 */
	barrier();

	if(unlikely(mutex_lock(&cache.lock)))
		return;

	/* already in the cache? */
	e = cache_ht_lookup(addr, h);
	if(e)
	{
		/* entry newer? */
		if(e->e.when >= when)
			goto out_unlock;
		if(!khl_rb_cache_remove(e)) {
			logg_devel("remove failed\n");
			goto life_tree_error; /* something went wrong... */
		}
// TODO: cluster state changes??
		e->cluster = cluster;
	}
	else
	{
		e = khl_cache_last();
		if(likely(e) &&
		   e->e.when < when &&
		   hlist_empty(&cache.free_list))
		{
			logg_devel_old("found older entry\n");
			if(!khl_rb_cache_remove(e))
				goto out_unlock; /* the tree is amiss? */
		}
		else
			e = khl_cache_entry_alloc();
		if(!e)
			goto out_unlock;

		e->cluster = cluster;
		e->e.na = *addr;
	}
	e->e.when = when;

	if(!khl_rb_cache_insert(e)) {
		logg_devel("insert failed\n");
life_tree_error:
		khl_cache_entry_free(e);
	}
	else
		cache_ht_add(e, h);

out_unlock:
	if(unlikely(h = mutex_unlock(&cache.lock))) {
		errno = h;
		diedie("gnarf, KHL cache lock stuck, bye!");
	}
}

size_t g2_khl_fill_s(struct khl_entry p[], size_t len, int s_fam)
{
	struct khl_cache_entry *e;
	struct rb_node *n;
	size_t res, res_t, wrap_arounds;

	if(unlikely(mutex_lock(&cache.lock)))
		return 0;

	for(res = 0, n = rb_first(&cache.tree), wrap_arounds = 0;
	    res < len; n = rb_next(n))
	{
		if(!n) {
			if(unlikely(wrap_arounds++))
				break;
			n = rb_first(&cache.tree);
			if(unlikely(!n))
				break;
		}
		e = rb_entry(n, struct khl_cache_entry, rb);
// TODO: filter neighbours
		/* we want hubs in our cluster, but not our direct neigbours */
		if(likely(e->e.na.s.fam == s_fam) && e->cluster)
			memcpy(&p[res++], &e->e, sizeof(p[0]));
	}

	if(unlikely(res_t = mutex_unlock(&cache.lock))) {
		errno = res_t;
		diedie("gnarf, KHL cache lock stuck, bye!");
	}

	return res;
}

size_t g2_khl_fill_p(struct khl_entry p[], size_t len, int s_fam)
{
	struct khl_cache_entry *e;
	struct rb_node *n;
	size_t res, res_tmp, wrap_arounds;

	if(unlikely(mutex_lock(&cache.lock)))
		return 0;

	for(res = 0, n = rb_first(&cache.tree), wrap_arounds = 0;
	    res < len; n = rb_next(n))
	{
		if(!n) {
			if(unlikely(wrap_arounds++))
				break;
			n = rb_first(&cache.tree);
			if(unlikely(!n))
				break;
		}
		e = rb_entry(n, struct khl_cache_entry, rb);
		if(likely(e->e.na.s.fam == s_fam && !e->cluster))
			memcpy(&p[res++], &e->e, sizeof(p[0]));
	}

	if(unlikely(res_tmp = mutex_unlock(&cache.lock))) {
		errno = res_tmp;
		diedie("gnarf, KHL cache lock stuck, bye!");
	}

	return res;
}

/*@unused@*/
static char const rcsid_khl[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
