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
#include <string.h>
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_DB
# define DB_DBM_HSEARCH 1
# include <db.h>
#else
# include <ndbm.h>
#endif
/* Own */
#define _G2KHL_C
#include "other.h"
#include "G2KHL.h"
#include "G2MainServer.h"
#include "lib/log_facility.h"
#include "lib/combo_addr.h"

#define KHL_DUMP_IDENT "G2CDKHLDUMP\n"
#define KHL_DUMP_ENDIAN_MARKER 0x12345678
#define KHL_DUMP_VERSION 0

struct gwc
{
	unsigned int q_count;
	unsigned int m_count;
	const char *url;
};

/* increment version on change */
struct khl_entry
{
	union combo_addr na;
};

/* Vars */
static DBM *gwc_db;

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

	strcpy(name, data_root_dir);
	strcat(name, "/");
	strcat(name, gwc_cache_fname);

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
		logg_devel("putting GWC boot url in cache\n");
		boot.q_count = 0;
		boot.m_count = 1;
		boot.url = NULL;
		value.dptr  = (char *) &boot;
		value.dsize = sizeof(boot);
		if(dbm_store(gwc_db, key, value, DBM_REPLACE))
			logg_errno(LOGF_WARN, "not able to put boot GWC into cache");
	}

init_next:
	/* try to read a khl dump */
	if(!server.settings.khl.dump_fname)
		return true;

	name_len  = strlen(data_root_dir);
	name_len += strlen(server.settings.khl.dump_fname);
	name = alloca(name_len + 2);

	strcpy(name, data_root_dir);
	strcat(name, "/");
	strcat(name, server.settings.khl.dump_fname);

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

	buff[str_size(KHL_DUMP_IDENT)] = '\0';
	if(strcmp(buff, KHL_DUMP_IDENT)) {
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

// TODO: read khl entries
out:
	fclose(khl_dump);
	return true;
}

bool g2_khl_tick(void)
{
	bool ret_val = true;

	return ret_val;
}

void g2_khl_end(void)
{
	FILE *khl_dump;
	uint32_t signature[3];
	const char *data_root_dir;
	char *name, *buff;
	size_t name_len;

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

	strcpy(name, data_root_dir);
	strcat(name, server.settings.khl.dump_fname);

	khl_dump = fopen(name, "wb");
	if(!khl_dump) {
		logg_errno(LOGF_INFO, "couldn't open khl dump");
		return;
	}

	fprintf(khl_dump, "%s", KHL_DUMP_IDENT);

	signature[0] = KHL_DUMP_ENDIAN_MARKER;
	signature[1] = sizeof(struct khl_entry);
	signature[2] = KHL_DUMP_VERSION;

	fwrite(signature, sizeof(signature[0]), 3, khl_dump);

// TODO: dump khl entries
	buff = alloca(500);
	fwrite(buff, 1, 500, khl_dump);
}

/*@unused@*/
static char const rcsid_khl[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
