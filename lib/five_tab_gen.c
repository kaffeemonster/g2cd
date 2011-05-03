/*
 * five_tab_gen.c
 * float print helper table generation
 *
 * Copyright (c) 2009-2011 Jan Seiffert
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
 * Floating Point conversion code is some else clever foo,
 * but i forgot who...
 *
 * $Id: $
 */

/*
 * FP-Code was retrieved as a C file without any attribution.
 *
 * All i remember is a paper about efficient floating point
 * conversion. And after long digging a reference impl. from
 * one of the paper authors, or something like that.
 *
 * Whomever it was:
 * "For those about to rock: (fire!) We salute you"
 *
 * And i think it's not that efficient, but avoids the fpu
 * quite succefully ;-)
 */

/*
 * including the target config is not quite right, but
 * we need to know if there is a stdint.h
 */
#include "../config.h"
#undef gid_t
#undef uid_t
#ifdef HAVE_STDINT_H
# include <stdint.h>
#else
# include <inttypes.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BIGSIZE 24
#define MAX_FIVE 325

typedef uint64_t big_digit;

struct big_num
{
	big_digit d[BIGSIZE];
	int l;
};

static struct big_num five_tab[MAX_FIVE];

static int output_tab(const char *of_name);

#define MUL_BIG(x, y, z, k) \
{ \
	big_digit x_mul, low, high; \
	x_mul = (x); \
	low = (x_mul & 0xffffffff) * (y) + (k); \
	high = (x_mul >> 32) * (y) + (low >> 32); \
	(k) = high >> 32; \
	(z) = (low & 0xffffffff) | (high << 32); \
}

int main(int argc, char *argv[])
{
	int n, i, l;
	big_digit *xp, *zp, k;

	if(argc < 2)
		return EXIT_FAILURE;

	five_tab[0].l = l = 0;
	five_tab[0].d[0] = 5;
	for(n = 0; n < MAX_FIVE - 1; n++)
	{
		xp = &five_tab[n].d[0];
		zp = &five_tab[n + 1].d[0];

		for(i = l, k = 0; i >= 0; i--)
			MUL_BIG(*xp++, 5, *zp++, k);

		if(k != 0)
			*zp = k, l++;
		five_tab[n + 1].l = l;
	}

	return output_tab(argv[1]);
}

static int output_tab(const char *of_name)
{
	FILE *fout;
	size_t ret;

	fout = fopen(of_name, "wb");
	if(!fout)
		return EXIT_FAILURE;
	ret = fwrite(five_tab, 1, sizeof(five_tab), fout);
	fclose(fout);
	if(sizeof(five_tab) != ret) {
		remove(of_name);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
/* EOF */
