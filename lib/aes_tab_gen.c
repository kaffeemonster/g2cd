/*
 * aes_tab_gen.c
 * AES table generation
 *
 * Copyright (c) 2009 Jan Seiffert
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
 * derived from the Linux kernel aes_generic.c which is GPL2:
 *
 * Based on Brian Gladman's code.
 *
 * Linux developers:
 *  Alexander Kjeldaas <astor@fast.no>
 *  Herbert Valerio Riedel <hvr@hvrlab.org>
 *  Kyle McMartin <kyle@debian.org>
 *  Adam J. Richter <adam@yggdrasil.com> (conversion to 2.5 API).
 *
 * ---------------------------------------------------------------------------
 * Copyright (c) 2002, Dr Brian Gladman <brg@gladman.me.uk>, Worcester, UK.
 * All rights reserved.
 *
 * LICENSE TERMS
 *
 * The free distribution and use of this software in both source and binary
 * form is allowed (with or without changes) provided that:
 *
 *   1. distributions of this source code include the above copyright
 *      notice, this list of conditions and the following disclaimer;
 *
 *   2. distributions in binary form include the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other associated materials;
 *
 *   3. the copyright holder's name is not used to endorse products
 *      built using this software without specific written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this product
 * may be distributed under the terms of the GNU General Public License (GPL),
 * in which case the provisions of the GPL apply INSTEAD OF those given above.
 *
 * DISCLAIMER
 *
 * This software is provided 'as is' with no explicit or implied warranties
 * in respect of its properties, including, but not limited to, correctness
 * and/or fitness for purpose.
 * ---------------------------------------------------------------------------
 *
 * $Id: $
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t pow_tab[256];
static uint8_t log_tab[256];
static uint8_t sbx_tab[256];
static uint8_t isb_tab[256];
static uint32_t rco_tab[10];

static uint32_t aes_ft_tab[4][256];
static uint32_t aes_fl_tab[4][256];
static uint32_t aes_it_tab[4][256];
static uint32_t aes_il_tab[4][256];

static inline uint32_t rol32(uint32_t word, unsigned int shift)
{
	return (word << shift) | (word >> (32 - shift));
}

static inline uint8_t f_mult(uint8_t a, uint8_t b)
{
	uint8_t aa = log_tab[a], cc = aa + log_tab[b];

	return pow_tab[cc + (cc < aa ? 1 : 0)];
}

#define ff_mult(a, b)	(a && b ? f_mult(a, b) : 0)

int main(int argc, char *argv[])
{
	size_t ret;
	uint32_t i, t;
	uint8_t p, q;
	FILE *fout;
	char *o_path;
	const char *pref;

	if(argc > 1)
		pref = argv[1];
	else
		pref = "./";
	o_path = malloc(strlen(pref) + strlen("aes_fl_tab.bin") + 1);
	if(!o_path)
		return EXIT_FAILURE;

	/*
	 * log and power tables for GF(2**8) finite field with
	 * 0x011b as modular polynomial - the simplest primitive
	 * root is 0x03, used here to generate the tables
	 */

	for (i = 0, p = 1; i < 256; ++i) {
		pow_tab[i] = (uint8_t) p;
		log_tab[p] = (uint8_t) i;
		p ^= (p << 1) ^ (p & 0x80 ? 0x01b : 0);
	}

	log_tab[1] = 0;

	for (i = 0, p = 1; i < 10; ++i) {
		rco_tab[i] = p;
		p = (p << 1) ^ (p & 0x80 ? 0x01b : 0);
	}

	for (i = 0; i < 256; ++i) {
		p  = (i ? pow_tab[255 - log_tab[i]] : 0);
		q  = ((p >> 7) | (p << 1)) ^ ((p >> 6) | (p << 2));
		p ^= 0x63 ^ q ^ ((q >> 6) | (q << 2));
		sbx_tab[i] = p;
		isb_tab[p] = (uint8_t) i;
	}

	for (i = 0; i < 256; ++i)
	{
		p = sbx_tab[i];

		t = p;
		aes_fl_tab[0][i] = t;
		aes_fl_tab[1][i] = rol32(t, 8);
		aes_fl_tab[2][i] = rol32(t, 16);
		aes_fl_tab[3][i] = rol32(t, 24);

		t = ((uint32_t) ff_mult(2, p)) | ((uint32_t) p << 8) |
		    ((uint32_t) p << 16) | ((uint32_t) ff_mult(3, p) << 24);

		aes_ft_tab[0][i] = t;
		aes_ft_tab[1][i] = rol32(t, 8);
		aes_ft_tab[2][i] = rol32(t, 16);
		aes_ft_tab[3][i] = rol32(t, 24);

		p = isb_tab[i];

		t = p;
		aes_il_tab[0][i] = t;
		aes_il_tab[1][i] = rol32(t, 8);
		aes_il_tab[2][i] = rol32(t, 16);
		aes_il_tab[3][i] = rol32(t, 24);

		t = ((uint32_t) ff_mult(14, p)) |
		    ((uint32_t) ff_mult(9, p) << 8) |
		    ((uint32_t) ff_mult(13, p) << 16) |
		    ((uint32_t) ff_mult(11, p) << 24);

		aes_it_tab[0][i] = t;
		aes_it_tab[1][i] = rol32(t, 8);
		aes_it_tab[2][i] = rol32(t, 16);
		aes_it_tab[3][i] = rol32(t, 24);
	}

	strcat(strcpy(o_path, pref), "aes_ft_tab.bin");
	fout = fopen(o_path, "wb");
	if(!fout)
		return EXIT_FAILURE;
	ret = fwrite(aes_ft_tab, 1, sizeof(aes_ft_tab), fout);
	fclose(fout);
	if(sizeof(aes_ft_tab) != ret) {
		remove(o_path);
		return EXIT_FAILURE;
	}

	strcat(strcpy(o_path, pref), "aes_fl_tab.bin");
	fout = fopen(o_path, "wb");
	if(!fout)
		return EXIT_FAILURE;
	ret = fwrite(aes_fl_tab, 1, sizeof(aes_fl_tab), fout);
	fclose(fout);
	if(sizeof(aes_fl_tab) != ret) {
		remove(o_path);
		return EXIT_FAILURE;
	}

	strcat(strcpy(o_path, pref), "aes_it_tab.bin");
	fout = fopen(o_path, "wb");
	if(!fout)
		return EXIT_FAILURE;
	ret = fwrite(aes_it_tab, 1, sizeof(aes_it_tab), fout);
	fclose(fout);
	if(sizeof(aes_it_tab) != ret) {
		remove(o_path);
		return EXIT_FAILURE;
	}

	strcat(strcpy(o_path, pref), "aes_il_tab.bin");
	fout = fopen(o_path, "wb");
	if(!fout)
		return EXIT_FAILURE;
	ret = fwrite(aes_il_tab, 1, sizeof(aes_il_tab), fout);
	fclose(fout);
	if(sizeof(aes_il_tab) != ret) {
		remove(o_path);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
/* EOF */
