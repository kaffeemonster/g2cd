/*
 * ansi_prng.h
 * Pseudo random number generator according to ANSI X9.31
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
 * $Id: $
 */

#ifndef LIB_ANSI_PRNG_H
# define LIB_ANSI_PRNG_H

# include "other.h"

# define LIB_ANSI_PRNG_EXTRN(x) x GCC_ATTR_VIS("hidden")

# define RAND_BLOCK_BYTE 16

LIB_ANSI_PRNG_EXTRN(void random_bytes_get(void *ptr, size_t len));
LIB_ANSI_PRNG_EXTRN(void random_bytes_rekey(const char data[RAND_BLOCK_BYTE * 2]));
LIB_ANSI_PRNG_EXTRN(void random_bytes_init(const char data[RAND_BLOCK_BYTE * 2]));
#endif
