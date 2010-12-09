/*
 * G2HeaderFieldsSort.c
 * Automatic sort for the G2 header fields
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "G2HeaderStrings.h"

#define str_size(x) (sizeof(x)-1)
#define anum(x) (sizeof(x)/sizeof(x[0]))

typedef struct
{
	const char *f_name;
	const char *ktxt;
	const char *txt;
} action_string;

typedef struct
{
	unsigned char index, num;
} action_index;

static const action_string h_as00 = {"accept_what",    "ACCEPT_KEY",       ACCEPT_KEY};
static const action_string h_as01 = {"uagent_what",    "UAGENT_KEY",       UAGENT_KEY};
static const action_string h_as02 = {"a_encoding_what","ACCEPT_ENC_KEY",   ACCEPT_ENC_KEY};
static const action_string h_as03 = {"remote_ip_what", "REMOTE_ADR_KEY",   REMOTE_ADR_KEY};
static const action_string h_as04 = {"listen_what",    "LISTEN_ADR_KEY",   LISTEN_ADR_KEY};
static const action_string h_as05 = {"content_what",   "CONTENT_KEY",      CONTENT_KEY};
static const action_string h_as06 = {"c_encoding_what","CONTENT_ENC_KEY",  CONTENT_ENC_KEY};
static const action_string h_as07 = {"ulpeer_what",    "UPEER_KEY",        UPEER_KEY};
static const action_string h_as08 = {"ulpeer_what",    "HUB_KEY",          HUB_KEY};
static const action_string h_as09 = {"empty_action_c", "X_TRY_UPEER_KEY",  X_TRY_UPEER_KEY}; /* maybe parse */
static const action_string h_as10 = {"try_hub_what",   "X_TRY_HUB_KEY",    X_TRY_HUB_KEY};
static const action_string h_as11 = {"listen_what",    "X_MY_ADDR_KEY",    X_MY_ADDR_KEY};
static const action_string h_as12 = {"listen_what",    "X_NODE_KEY",       X_NODE_KEY};
static const action_string h_as13 = {"listen_what",    "NODE_KEY",         NODE_KEY};
static const action_string h_as14 = {"empty_action_c", "UPEER_NEEDED_KEY", UPEER_NEEDED_KEY};
static const action_string h_as15 = {"empty_action_c", "HUB_NEEDED_KEY",   HUB_NEEDED_KEY};
static const action_string h_as16 = {"empty_action_c", "X_VERSION_KEY",    X_VERSION_KEY};
static const action_string h_as17 = {"empty_action_c", "GGEP_KEY",         GGEP_KEY};
static const action_string h_as18 = {"empty_action_c", "PONG_C_KEY",       PONG_C_KEY};
static const action_string h_as19 = {"empty_action_c", "UPTIME_KEY",       UPTIME_KEY};
static const action_string h_as20 = {"empty_action_c", "VEND_MSG_KEY",     VEND_MSG_KEY};
static const action_string h_as21 = {"empty_action_c", "X_LOC1_PREF_KEY",  X_LOC1_PREF_KEY};
static const action_string h_as22 = {"empty_action_c", "X_LOC2_PREF_KEY",  X_LOC2_PREF_KEY};
static const action_string h_as23 = {"empty_action_c", "X_MAX_TTL_KEY",    X_MAX_TTL_KEY};
static const action_string h_as24 = {"empty_action_c", "X_GUESS_KEY",      X_GUESS_KEY};
static const action_string h_as25 = {"empty_action_c", "X_REQUERIES_KEY",  X_REQUERIES_KEY};
static const action_string h_as26 = {"empty_action_c", "X_Q_ROUT_KEY",     X_Q_ROUT_KEY};
static const action_string h_as27 = {"empty_action_c", "X_UQ_ROUT_KEY",    X_UQ_ROUT_KEY};
static const action_string h_as28 = {"empty_action_c", "X_DYN_Q_KEY",      X_DYN_Q_KEY};
static const action_string h_as29 = {"empty_action_c", "X_EXT_PROBES_KEY", X_EXT_PROBES_KEY};
static const action_string h_as30 = {"empty_action_c", "X_DEGREE_KEY",     X_DEGREE_KEY};
static const action_string h_as31 = {"empty_action_c", "X_AUTH_CH_KEY",    X_AUTH_CH_KEY};
static const action_string h_as32 = {"crawler_what",   "CRAWLER_KEY",      CRAWLER_KEY};
static const action_string h_as33 = {"empty_action_c", "BYE_PKT_KEY",      BYE_PKT_KEY};
static const action_string h_as34 = {"empty_action_c", "X_PROBE_Q_KEY",    X_PROBE_Q_KEY};
static const action_string h_as35 = {"empty_action_c", "X_LEAF_MAX_KEY",   X_LEAF_MAX_KEY}; /* maybe use? */

static action_string const *hdr_fields[] =
{
	&h_as00, &h_as01, &h_as02, &h_as03, &h_as04, &h_as05, &h_as06, &h_as07, &h_as08,
	&h_as09, &h_as10, &h_as11, &h_as12, &h_as13, &h_as14, &h_as15, &h_as16, &h_as17,
	&h_as18, &h_as19, &h_as20, &h_as21, &h_as22, &h_as23, &h_as24, &h_as25, &h_as26,
	&h_as27, &h_as28, &h_as29, &h_as30, &h_as31, &h_as32, &h_as33, &h_as34, &h_as35,
};

static int strncasecmp_a(const char *a, const char *b, size_t len)
{
	unsigned c1, c2;

	for(; len; len--)
	{
		c1  = (unsigned) *a++;
		c2  = (unsigned) *b++;
		c1 -= c1 >= 'a' && c1 <= 'z' ? 0x20 : 0;
		c2 -= c2 >= 'a' && c2 <= 'z' ? 0x20 : 0;
		if(!(c1 && c2 && c1 == c2))
			return (int)c1 - (int)c2;
	}
	return 0;
}

static int acstr_cmp(const void *a, const void *b)
{
	action_string *const *a_s = a, *const *b_s = b;
	size_t a_len = strlen((*a_s)->txt), b_len = strlen((*b_s)->txt);
	if(a_len - b_len)
		return a_len - b_len;
	return strncasecmp_a((*a_s)->txt, (*b_s)->txt, b_len);
}

int main(int argc, char *argv[])
{
	unsigned i;
	size_t max_f_name = 0, max_ktxt = 0, max_txt = 0, o_len = 0;
	time_t now;
	struct tm *now_tm, a_tm;
	action_index *aci;

	qsort(hdr_fields, anum(hdr_fields), sizeof(hdr_fields[0]), acstr_cmp);

	for(i = 0; i < anum(hdr_fields); i++)
	{
		size_t t = strlen(hdr_fields[i]->f_name);
		max_f_name = max_f_name > t ? max_f_name : t;
		t = strlen(hdr_fields[i]->ktxt);
		max_ktxt = max_ktxt > t ? max_ktxt : t;
		t = strlen(hdr_fields[i]->txt);
		max_txt = max_txt > t ? max_txt : t;
	}
	max_f_name++;
	max_ktxt++;
	aci = calloc(max_txt, sizeof(*aci));
	if(!aci) {
		perror("error allocating index storage");
		return 1;
	}

	for(i = 0; i < anum(hdr_fields); i++)
	{
		size_t t = strlen(hdr_fields[i]->txt);
		if(t != o_len) {
			aci[t-1].index = i;
			o_len = t;
		}
		aci[t-1].num++;
	}

	now = time(NULL);
	if(!(now_tm = gmtime(&now))) {
		now_tm = &a_tm;
		now_tm->tm_year = 110;
	}

	i = 1;
	do
	{
		static const char *df_name = "G2HeaderFields.h";
		const char *f_name = argc > i ? argv[i] : df_name;
		FILE *out;

		if('-' == f_name[0] && '\0' == f_name[1])
			f_name = df_name;

		if(f_name == df_name)
			out = stdout;
		else
		{
			out = fopen(f_name, "w");
			if(!out)
				continue;
		}

		fprintf(out,
			"/*\n"
			" * %s\n"
			" * automatic sorted header fields\n"
			" *\n"
			" * Copyright (c) 2004-%u Jan Seiffert\n"
			" *\n"
			" * This file is part of g2cd.\n"
			" *\n"
			" * g2cd is free software; you can redistribute it and/or modify\n"
			" * it under the terms of the GNU General Public License as\n"
			" * published by the Free Software Foundation, either version 3\n"
			" * of the License, or (at your option) any later version.\n"
			" *\n"
			" * g2cd is distributed in the hope that it will be useful, but\n"
			" * WITHOUT ANY WARRANTY; without even the implied warranty of\n"
			" * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
			" * GNU General Public License for more details.\n"
			" *\n"
			" * You should have received a copy of the GNU General Public\n"
			" * License along with g2cd.\n"
			" * If not, see <http://www.gnu.org/licenses/>.\n"
			" *\n"
			" */\n"
			"\n",
			f_name,
			now_tm->tm_year + 1900
		);

		fputs("/* This file is generated automatically, do not edit */\n\n"
		      "const action_index KNOWN_HEADER_FIELDS_INDEX[LONGEST_HEADER_FIELD] GCC_ATTR_VIS(\"hidden\") =\n{\n", out);
		for(i = 0; i < max_txt; i++)
		{
			fprintf(out, "\t{%3u, %2u}, /* %2u */\n", aci[i].index, aci[i].num, i+1);
		}

		fputs("};\n\nconst action_string KNOWN_HEADER_FIELDS[] GCC_ATTR_VIS(\"hidden\") =\n{\n", out);
		for(i = 0; i < anum(hdr_fields); i++)
		{
			fprintf(out, "\t{%*s, str_size(%*s), %*s }, /* %s */\n",
			       (int)max_f_name, hdr_fields[i]->f_name, (int)max_ktxt, hdr_fields[i]->ktxt,
			       (int)max_ktxt, hdr_fields[i]->ktxt, hdr_fields[i]->txt);
		}
		fputs("};\n\n/* EOF */\n", out);
		if(f_name != df_name)
			fclose(out);
	} while(i++ < argc);
	return 0;
}
