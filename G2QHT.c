/*
 * G2QHT.c
 * helper-functions for G2-QHTs
 *
 * Copyright (c) 2006-2010 Jan Seiffert
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

#define WANT_QHT_ZPAD
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "lib/my_pthread.h"
/* debug file */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zlib.h>
/* other */
#include "lib/other.h"
/* Own includes */
#define _G2QHT_C
#define _NEED_G2_P_TYPE
#include "G2QHT.h"
#include "G2Packet.h"
#include "G2ConRegistry.h"
// #include "G2MainServer.h"
#include "lib/log_facility.h"
#include "lib/sec_buffer.h"
#include "lib/my_bitops.h"
#include "lib/my_bitopsm.h"
#include "lib/hzp.h"
#include "lib/atomic.h"
#include "lib/tchar.h"

/* RLE maximum size */
// TODO: take less?
#define QHT_RLE_MAXSIZE (1 << 14) /* 16k */

struct zpad_heap
{
	struct zpad_heap *next;
	/*
	 * length should be aligned to sizeof(void *)
	 * so at least the LSB cann be used as a flag
	 */
	size_t length;
	char data[DYN_ARRAY_LEN];
};

struct scratch
{
	size_t length;
	unsigned char data[DYN_ARRAY_LEN];
};

struct search_hash_buffer
{
	size_t num;
	size_t size;
	uint32_t hashes[DYN_ARRAY_LEN];
};

struct qht_data
{
	struct hzp_free hzp;
	uint8_t data[DYN_ARRAY_LEN];
};

/* Vars */
static pthread_key_t key2qht_zpad;
static pthread_key_t key2qht_scratch1;
static pthread_key_t key2qht_scratch2;

/* Internal Prototypes */
	/* do not remove this proto, our it won't work... */
static void qht_init(void) GCC_ATTR_CONSTRUCT;
static void qht_deinit(void) GCC_ATTR_DESTRUCT;
static void *qht_zpad_alloc(void *, unsigned int, unsigned int) GCC_ATTR_MALLOC;
static void qht_zpad_free(void *, void *);
static inline void qht_zpad_merge(struct zpad_heap *);
static void g2_qht_free_hzp(void *);
#ifdef QHT_DUMP
static void qht_dump_init(void);
static void qht_dump_deinit(void);
static void qht_sdump(void *, size_t);
static void qht_dump(void *, void *, size_t);
#else
# define qht_dump_init()
# define qht_dump_deinit()
# define qht_sdump(x, y) do { } while(0)
# define qht_dump(x, y, z) do { } while(0)
#endif

/* tls thingies */
static void qht_init(void)
{
	qht_dump_init();
	if(pthread_key_create(&key2qht_zpad, free))
		diedie("couldn't create TLS key for qht zpad");

	if(pthread_key_create(&key2qht_scratch1, free))
		diedie("couldn't create TLS key for qht scratch1");

	if(pthread_key_create(&key2qht_scratch2, free))
		diedie("couldn't create TLS key for qht scratch2");
}

static void qht_deinit(void)
{
	qht_dump_deinit();
	pthread_key_delete(key2qht_zpad);
	pthread_key_delete(key2qht_scratch1);
	pthread_key_delete(key2qht_scratch2);
}

/* zpad helper */
/*
 * Poor mans allocator for zlib
 *
 * normaly zlib makes two allocs, one seems to be the internal
 * state (+9k on 32bit *cough*) and the other is the window, norm
 * 32k for 15 bit.
 *
 * So this is a very stupid alloactor to serve these two allocs.
 * But, not to be too dependent on these internals (future zlib
 * versions) it is at least so intelligent it may also handle 3
 * allocs, and frees, maybe ;)
 *
 * And since a deflater is very memory hungry, one time 6k,
 * three (!!!) times 32k * 2 and one time 16k * 4, total over
 * 260k, with a little more space in the pad we also can handle
 * a deflater.
 *
 * !! THE heap for this zpad instance is NOT locked !!
 * Why: it should be thread local to this deflater/inflater!
 */
static void *qht_zpad_alloc(void *opaque, unsigned int num, unsigned int size)
{
	struct zpad *z_pad = opaque;
	struct zpad_heap *zheap, *tmp_zheap;
	size_t bsize, org_len;

	if(unlikely(!z_pad))
		goto err_record;

	/* mildly sanetiy check */
	if(unlikely( num > sizeof(z_pad->pad) + sizeof(z_pad->window)) ||
	   unlikely(size > sizeof(z_pad->pad) + sizeof(z_pad->window)) ||
	   unlikely(size > UINT_MAX / num))
		goto err_record;
	bsize = num * size;

	/* bsize should afterwards have the LSB cleared */
	bsize = ALIGN_SIZE(bsize, sizeof(void *));

	if(unlikely(bsize + sizeof(struct zpad_heap) > sizeof(z_pad->pad) + sizeof(z_pad->window)))
		goto err_record;

	/* start lock */
	zheap = z_pad->pad_free;
	org_len = zheap->length & ~1; /* mask out the free flag */
	/*
	 * normaly we simply allocate at the end, and 'nuff said, but
	 * to be complete...
	 */
	if(unlikely(!(zheap->length & 1) || bsize + sizeof(*zheap) > org_len))
	{
		tmp_zheap = (struct zpad_heap *)z_pad->pad;
		/* walk the heap list */
		do
		{
			size_t tmp_len = tmp_zheap->length & ~1;
			/* if free and big enough */
			if(likely(tmp_zheap->length & 1))
			{
				if(bsize + sizeof(*zheap) <= tmp_len) {
					org_len = tmp_len;
					zheap = tmp_zheap;
					goto insert_in_heap;
				}
				if(tmp_zheap->next && (tmp_zheap->next->length & 1)) {
					qht_zpad_merge(tmp_zheap);
					continue;
				}
			}
			tmp_zheap = tmp_zheap->next;
		} while(tmp_zheap);
		goto err_record;
	}
insert_in_heap:
	zheap->length     = bsize;
	/* Mind the parentheses... */
	tmp_zheap         = (struct zpad_heap*)(zheap->data + bsize);
	if(unlikely((unsigned char *)tmp_zheap < z_pad->pad))
		die("zpad heap probably corrupted, underflow!\n");
	if(unlikely((unsigned char *)tmp_zheap > (z_pad->pad + sizeof(z_pad->pad) + sizeof(z_pad->window) - sizeof(*zheap))))
	{
		logg_posd(LOGF_EMERG, "zpad heap probably corrupted, overflow!\n%p > %p + %zu + %zu - %zu = %p\n",
		          tmp_zheap, z_pad->pad, sizeof(z_pad->pad), sizeof(z_pad->window), sizeof(*zheap),
		          z_pad->pad + sizeof(z_pad->pad) + sizeof(z_pad->window) - sizeof(*zheap));
		exit(EXIT_FAILURE);
	}
	tmp_zheap->length = (org_len - bsize - sizeof(*zheap)) | 1;
	tmp_zheap->next   = zheap->next;
	zheap->next       = tmp_zheap;
	z_pad->pad_free   = tmp_zheap;
/* end lock */

	logg_develd_old("zpad alloc: n: %u\ts: %u\tb: %zu\tp: %p\tzh: %p\tzhn: %p\n",
	                num, size, bsize, z_pad->pad, zheap, tmp_zheap);
	return zheap->data;

err_record:
	logg_develd("zpad alloc failed!! 0x%p\tn: %u\ts: %u\n", opaque, num, size);
	return NULL;
}

static void qht_zpad_free(void *opaque, void *addr)
{
	struct zpad_heap *zheap;

	if(!addr || !opaque)
		return;

	/*
	 * watch the parentheses (/me cuddles his new vim 7), or you
	 * have wrong pointer-arith, and a SIGSEV on 3rd. alloc...
	 */
	zheap = (struct zpad_heap *) ((char *)addr - offsetof(struct zpad_heap, data));
	zheap->length |= 1; // mark free

	logg_develd_old("zpad free: o: %p\ta: %p\tzh: %p\n", opaque, addr, zheap);

	qht_zpad_merge(zheap);
}

static inline void qht_zpad_merge(struct zpad_heap *zheap)
{
	while(zheap->next)
	{
		if(zheap->next->length & 1) { /* if next block is free */
			zheap->length += (zheap->next->length & ~1) + sizeof(struct zpad_heap);
			zheap->next = zheap->next->next;
		}
		else
			zheap = zheap->next;
	}
}

struct zpad *qht_get_zpad(void)
{
	struct zpad *z_pad = pthread_getspecific(key2qht_zpad);

	if(unlikely(!z_pad))
	{
		z_pad = malloc(sizeof(*z_pad));
		if(!z_pad) {
			logg_errno(LOGF_DEVEL, "no z_pad");
			return NULL;
		}
		if(pthread_setspecific(key2qht_zpad, z_pad)) {
			logg_errno(LOGF_CRIT, "qht-zpad key not initilised?");
			free(z_pad);
			return NULL;
		}
	}
	memset(&z_pad->z, 0, sizeof(z_pad->z));
	z_pad->pad_free         = (struct zpad_heap *)z_pad->pad;
	z_pad->pad_free->next   = NULL;
	z_pad->pad_free->length = (sizeof(z_pad->pad) + sizeof(z_pad->window)) | 1; /* set the free bit */
	z_pad->z.zalloc         = qht_zpad_alloc;
	z_pad->z.zfree          = qht_zpad_free;
	z_pad->z.opaque         = z_pad;

	return z_pad;
}

/**/
static noinline GCC_ATTR_MALLOC unsigned char *qht_get_scratch_intern(size_t length, pthread_key_t scratch_key)
{
	struct scratch *scratch = pthread_getspecific(scratch_key);

	/*
	 * ompf, very unpleasant...
	 * we want the ->data aligned to 16. Since it is a char gcc
	 * alinges it to 1 so effectivly 4/8 with the length in front.
	 * We could tell gcc to align data to 16, but then we get a
	 * nice interference with malloc. It alignes this to 4 ;(
	 * (and i don't understand why... only because it is a huge
	 * alloc? and if i want to put doubles in it?)
	 * And since every malloc could align it to another boundary
	 * we fix it up once and by hand...
	 */
	length += 16;
	if(likely(scratch))
	{
		if(likely(scratch->length >= length))
			return (unsigned char *)ALIGN(scratch->data, 16);
		else
		{
			free(scratch);
			scratch = NULL;
			if(pthread_setspecific(scratch_key, NULL))
				diedie("could not remove qht scratch!");
		}
	}

	scratch = malloc(sizeof(*scratch) + length);

	logg_develd_old("scratch: %p, %lu\n", (void *)scratch, (unsigned long)scratch);

	if(!scratch) {
		logg_errno(LOGF_DEVEL, "no qht scratchram");
		return NULL;
	}
	scratch->length = length;

	if(pthread_setspecific(scratch_key, scratch)) {
		logg_errno(LOGF_CRIT, "qht scratch key not initilised?");
		free(scratch);
		return NULL;
	}

	return (unsigned char *)ALIGN(scratch->data, 16);
}

static inline unsigned char *qht_get_scratch1(size_t length)
{
	return qht_get_scratch_intern(length, key2qht_scratch1);
}

static inline unsigned char *qht_get_scratch2(size_t length)
{
	return qht_get_scratch_intern(length, key2qht_scratch2);
}

static inline void _g2_qht_clean(struct qhtable *to_clean, bool reset_needed)
{
	if(!to_clean)
		return;

	g2_qht_frag_free(to_clean->fragments);

	memset(&to_clean->entries, 0, offsetof(struct qhtable, data) - offsetof(struct qhtable, entries));
	if(reset_needed)
		to_clean->flags.reset_needed = true;
	else if(to_clean->data)
		memset(to_clean->data, ~0, to_clean->data_length);
}

/* helper-funktions */
void g2_qht_clean(struct qhtable *to_clean)
{
	_g2_qht_clean(to_clean, true);
}

static struct qht_data *g2_qht_data_alloc(size_t size)
{
	return malloc(sizeof(struct qht_data) + size);
}

static void g2_qht_data_free(uint8_t *tof)
{
	struct qht_data *d;

	if(!tof)
		return;

	d = container_of(tof, struct qht_data, data);
	hzp_deferfree(&d->hzp, d, free);
}

static void g2_qht_free_hzp(void *qtable)
{
	struct qhtable *to_free;

	if(!qtable)
		return;
	to_free = qtable;
	if(atomic_read(&to_free->refcnt) > 0) {
		logg_develd("qht refcnt race %p %i\n", (void*)to_free, atomic_read(&to_free->refcnt));
		return;
	}
	g2_qht_frag_free(to_free->fragments);
	g2_qht_data_free(to_free->data);
	free(to_free);
}

void g2_qht_put(struct qhtable *to_free)
{
	if(!to_free)
		return;

	if(atomic_dec_test(&to_free->refcnt))
		hzp_deferfree(&to_free->hzp, to_free, g2_qht_free_hzp);
	else if(atomic_read(&to_free->refcnt) < 0)
		logg_develd("qht refcnt below zero! %p %i\n", (void*)to_free, atomic_read(&to_free->refcnt));
}

/*
 * funcs
 */
bool g2_qht_search_prepare(void)
{
	struct search_hash_buffer *shb =
		(struct search_hash_buffer *)qht_get_scratch1(QHT_DEFAULT_BYTES);

	/*
	 * We reuse the scratch pads as a lookaside buffer for
	 * the search hashes.
	 * So no QHT patches or something like that in this thread
	 * while we process a Query.
	 */

	if(!shb)
		return false;

	shb->num  = 0;
	shb->size = ((QHT_DEFAULT_BYTES / (CONREG_LEVEL_COUNT + 1)) - sizeof(*shb)) / sizeof(uint32_t);
	return true;
}

static uint32_t g2_qht_hnumber(uint32_t h, unsigned bits)
{
	uint64_t prod = (uint64_t)h * 0x4F1BBCDCULL;
	uint64_t hash = prod << 32;
	hash >>= 32 + (32 - bits);
	return (uint32_t)hash;
}

static noinline uint32_t g2_qht_search_number_word(const tchar_t *s, size_t start, size_t len)
{
	uint32_t h;
	int b;

	for(h = 0, b = 0, s += start; len; len--, s++)
	{
		int v = *s;
		/*
		 * This is fuckin broken...
		 * Pass around and create winblow UTF-16, like shareaza,
		 * to throw it against an ASCII tolower (here coded explicit
		 * to prevent funny things), like shareaza, and cut away
		 * everthing outside ASCII, like shareaza.
		 * ????
		 * What are they doing?
		 */
		v   = (v < 'A' || v > 'Z' ? v : v + ('a' - 'A')) & 0xFF;
		v <<= b * BITS_PER_CHAR;
		/*
		 * b   = (b + 1) & 3;
		 * and the brokenness continues...
		 * The above is basically an optimized (b + 1) % 8.
		 * The problem is the modulo 8. They count till 7, so
		 * they basically want to shift v at max 7 * 8 = 56 times
		 * to the left. This can not work, assuming int is 32 bit
		 * this would mean they take the first 4 chars, than ingore
		 * 4 chars (shifted to nirvana, xor with 0), than take 4
		 * chars...
		 * The the reason it works: x86 shift take the shift count
		 * modulo 32, so it wraps around again.
		 * But this only works as long as you do not have an arch
		 * which can (only) do 64 bit shifts (x86_64 can still do
		 * 32 bit shifts) or the compiler gets clever and start
		 * to see "Hey, with a shift count of > 32 only 0 remains"
		 * (or plays with some C/C++ rule that shifts larger than
		 * the type are undefined, which would be VERY unpleasant...)
		 * Fix it for good in the code, hoping it is the right thing.
		 */
		b   = (b + 1) % 4;
		h  ^= v;
	}
	/*
	 * Hashes are calced as above in "full length".
	 * They are then cut to hash table length.
	 * We can store the hashes in "full length" and cut them immidiatly
	 * before use, to accomodate for different QHT bit sizes (for every QHT
	 * with the QHT bits property). And this is the way it is intended to
	 * be done.
	 * But this does not work.
	 * We sort the full length hashes (to sort only one time), but after the
	 * cut (due to overflow trunc.) they maybe unsorted again. And we would
	 * have to mix bitfield rle lookups with QHT hash cuts.
	 * All in all we do not support different QHT sizes anyway. We can not
	 * build master tables out of different QHT sizes.
	 * So we can stop burning cycles on every use.
	 * Shareaza is no better here, it also cuts the hashes on generation.
	 */
	return g2_qht_hnumber(h, QHT_DEFAULT_BITS);
}

static noinline uint32_t g2_qht_search_number_word_luc(const unsigned char *s, size_t len)
{
	uint32_t h;
	int b;

	/* see above */
	for(h = 0; len >= sizeof(uint32_t); len -= sizeof(uint32_t), s += sizeof(uint32_t)) {
		uint32_t v = get_unaligned_le32(s);
		h  ^= v;
	}
	for(b = 0; len; len--, s++)
	{
		uint32_t v = *s;
		v <<= b * BITS_PER_CHAR;
		b   = (b + 1) % 4;
		h  ^= v;
	}
	return g2_qht_hnumber(h, QHT_DEFAULT_BITS);
}

noinline void g2_qht_search_add_word(const tchar_t *s, size_t start, size_t len)
{
	struct search_hash_buffer *shb =
		(struct search_hash_buffer *)qht_get_scratch1(QHT_DEFAULT_BYTES);

	if(!shb || shb->num >= shb->size)
		return;
	shb->hashes[shb->num++] = g2_qht_search_number_word(s, start, len);
}

void g2_qht_search_add_hash(uint32_t h)
{
	struct search_hash_buffer *shb =
		(struct search_hash_buffer *)qht_get_scratch1(QHT_DEFAULT_BYTES);

	if(!shb || shb->num >= shb->size)
		return;
	shb->hashes[shb->num++] = g2_qht_hnumber(h, QHT_DEFAULT_BITS);
}

static noinline void g2_qht_search_add_word_luc(const unsigned char *s, size_t len)
{
	struct search_hash_buffer *shb =
		(struct search_hash_buffer *)qht_get_scratch1(QHT_DEFAULT_BYTES);

	if(!shb || shb->num >= shb->size)
		return;
	shb->hashes[shb->num++] = g2_qht_search_number_word_luc(s, len);
}

void g2_qht_search_add_sha1(const unsigned char *h)
{
#define URN_SHA1 "urn:sha1:"
	unsigned char ih[str_size(URN_SHA1) + B32_LEN(20)]; /* base 32 encoding */
	unsigned char *wptr = mempcpy(ih, URN_SHA1, str_size(URN_SHA1));
	wptr = to_base32(wptr, h, 20);
	g2_qht_search_add_word_luc(ih, wptr - ih);
}

void g2_qht_search_add_ttr(const unsigned char *h)
{
#define URN_TTR "urn:tree:tiger/:"
	unsigned char ih[str_size(URN_TTR) + B32_LEN(24)]; /* base 32 encoding */
	unsigned char *wptr = mempcpy(ih, URN_TTR, str_size(URN_TTR));
	wptr = to_base32(wptr, h, 24);
	g2_qht_search_add_word_luc(ih, wptr - ih);
}

void g2_qht_search_add_ed2k(const unsigned char *h)
{
#define URN_ED2K "urn:ed2khash:"
	unsigned char ih[str_size(URN_ED2K) + 16 * 2]; /* hex encoding */
	unsigned char *wptr = mempcpy(ih, URN_ED2K, str_size(URN_ED2K));
	wptr = to_base16(wptr, h, 16);
	g2_qht_search_add_word_luc(ih, wptr - ih);
}

void g2_qht_search_add_bth(const unsigned char *h)
{
#define URN_BTH "urn:btih:"
	unsigned char ih[str_size(URN_BTH) + B32_LEN(20)]; /* base 32 encoding */
	unsigned char *wptr = mempcpy(ih, URN_BTH, str_size(URN_BTH));
	wptr = to_base32(wptr, h, 20);
	g2_qht_search_add_word_luc(ih, wptr - ih);
}

void g2_qht_search_add_md5(const unsigned char *h)
{
#define URN_MD5 "urn:md5:"
	unsigned char ih[str_size(URN_MD5) + 16 * 2]; /* hex encoding */
	unsigned char *wptr = mempcpy(ih, URN_MD5, str_size(URN_MD5));
	wptr = to_base16(wptr, h, 16);
	g2_qht_search_add_word_luc(ih, wptr - ih);
}

static unsigned from_base16(unsigned char *h, const tchar_t *str, unsigned num)
{
	unsigned i;
	for(i = 0; i < num; i++)
	{
		unsigned tmp = str[i * 2];
		unsigned char c;
		if((unsigned)(tmp - '0') < 10u)
			c = (tmp - '0') << 4;
		else if((unsigned)(tmp - 'A') < 6u )
			c = (tmp - 'A' + 10u) << 4;
		else if((unsigned)(tmp - 'a') < 6u )
			c = (tmp - 'a' + 10u) << 4;
		else
			return false;
		tmp = str[i * 2 + 1];
		if((unsigned)(tmp - '0') < 10u)
			c |= tmp - '0';
		else if((unsigned)(tmp - 'A') < 6u)
			c |= tmp - 'A' + 10u;
		else if((unsigned)(tmp - 'a') < 6u)
			c |= tmp - 'a' + 10u;
		else
			return false;
		h[i] = c;
	}
	return true;
}

static unsigned from_base32(unsigned char *h, const tchar_t *str, unsigned num)
{
	unsigned b32chars = B32_LEN(num), i = 0, ch = 0, carry = 0;
	int shift = 11;

	do
	{
		unsigned tmp;
		if((tmp = str[ch] - 'A' ) < 26)
			carry |= tmp << shift;
		else if((tmp = str[ch] - 'a') < 26)
			carry |= tmp << shift;
		else if((tmp = str[ch] - '2') < 6)
			carry |= (tmp + 26) << shift;
		else
			return false;
		shift -= 5;
		if(shift < 0)
		{
			shift += 8;
			h[i++] = (unsigned char)(carry >> 8);
			carry <<= 8;
		}
	}
	while(++ch < b32chars)
		/* huh? */;
	h[i] = (unsigned char)(carry >> 8);
	return true;
}

bool g2_qht_search_add_hash_urn(const tchar_t *str, size_t len)
{
	static const struct
	{
		void (*type_add)(const unsigned char *);
		unsigned (*de_enc)(unsigned char *, const tchar_t *, unsigned);
		unsigned char m_len;
		unsigned char s_len;
		unsigned char h_off;
		unsigned char b_cnt;
		tchar_t sig[17];
	} urn_types[] =
	{
#define E(f, e, a, b, c, d) .type_add=(f), .de_enc=(e), .m_len=(a), .s_len=(c), .h_off=(b), .b_cnt=(d)
#define ES(a, b, c) E(g2_qht_search_add_sha1, from_base32, a, b, c, 5*4)
#define ET(a, b, c) E(g2_qht_search_add_ttr , from_base32, a, b, c, 3*8)
#define EE(a, b, c) E(g2_qht_search_add_ed2k, from_base16, a, b, c, 4*4)
#define EM(a, b, c) E(g2_qht_search_add_md5 , from_base16, a, b, c, 4*4)
#define EB32(a, b, c) E(g2_qht_search_add_bth , from_base32, a, b, c, 5*4)
#define EB16(a, b, c) E(g2_qht_search_add_bth , from_base16, a, b, c, 5*4)
		/* sha1 */
		{ES(32+ 9,  9,  9), .sig={'u','r','n',':','s','h','a','1',':','\0'}},
		{ES(32+ 5,  5,  5), .sig={'s','h','a','1',':','\0'}},
		{ES(85   , 13, 13), .sig={'u','r','n',':','b','i','t','p','r','i','n','t',':','\0'}},
		{ES(81   ,  9,  9), .sig={'b','i','t','p','r','i','n','t',':','\0'}},
		/* tiger tree */
		{ET(39+16, 16, 16), .sig={'u','r','n',':','t','r','e','e',':','t','i','g','e','r','/',':','\0'}},
		{ET(39+12, 12, 12), .sig={'t','r','e','e',':','t','i','g','e','r','/',':','\0'}},
		{ET(39+46, 46, 13), .sig={'u','r','n',':','b','i','t','p','r','i','n','t',':','\0'}},
		{ET(39+42, 42,  9), .sig={'b','i','t','p','r','i','n','t',':','\0'}},
		{ET(39+15, 15, 15), .sig={'u','r','n',':','t','r','e','e',':','t','i','g','e','r',':','\0'}},
		{ET(39+11, 11, 11), .sig={'t','r','e','e',':','t','i','g','e','r',':','\0'}},
		{ET(39+11, 11, 11), .sig={'u','r','n',':','t','t','r','o','o','t',':','\0'}},
		/* ed2k */
		{EE(32+13, 13, 13), .sig={'u','r','n',':','e','d','2','k','h','a','s','h',':','\0'}},
		{EE(32+ 5,  5,  5), .sig={'e','d','2','k',':','\0'}},
		{EE(32+ 9,  9,  9), .sig={'u','r','n',':','e','d','2','k',':','\0'}},
		{EE(32+ 9,  9,  9), .sig={'e','d','2','k','h','a','s','h',':','\0'}},
		/* md5 */
		{EM(32+ 8,  8,  8), .sig={'u','r','n',':','m','d','5',':','\0'}},
		{EM(32+ 4,  4,  4), .sig={'m','d','5',':','\0'}},
		/* bth */
		{EB32(32+ 9,  9,  9), .sig={'u','r','n',':','b','t','i','h',':','\0'}},
		{EB32(32+ 5,  5,  5), .sig={'b','t','i','h',':','\0'}},
		{EB16(40+ 9,  9,  9), .sig={'u','r','n',':','b','t','i','h',':','\0'}},
		{EB16(40+ 5,  5,  5), .sig={'b','t','i','h',':','\0'}},
	};
#undef E
#undef ES
#undef ET
#undef EE
#undef EM
#undef EB32
#undef EB16

	unsigned char hash_val[32];
	unsigned i;

	for(i = 0; i < anum(urn_types); i++)
	{
		if(len < urn_types[i].m_len)
			continue;
		if(0 == tstrncmp(str, urn_types[i].sig, urn_types[i].s_len))
		{
			str += urn_types[i].h_off;
			if(urn_types[i].b_cnt != urn_types[i].de_enc(hash_val, str, urn_types[i].b_cnt))
				return false;
			urn_types[i].type_add(hash_val);
			return true;
		}
	}

	return false;
}

/*
 * This function has to produce the same keywords as Shareaza.
 * So no wonder it is quite similar to the Shareaza one.
 *
 * Thank god they are Open Source now, "back then" this would have been
 * a nightmare...
 */
static tchar_t *make_keywords(const tchar_t *s)
{
	enum script_type
	{
		SC_NONE     = 0,
		SC_NUMERIC  = 1,
		SC_REGULAR  = 2,
		SC_KANJI    = 4,
		SC_HIRAGANA = 8,
		SC_KATAKANA = 16,
	} boundary[2] = {SC_NONE, SC_NONE};
	tchar_t *phrase = (tchar_t *)qht_get_scratch2(QHT_DEFAULT_BYTES);
	tchar_t *t_wptr = phrase + 3, *end_phrase = phrase + (QHT_DEFAULT_BYTES / sizeof(tchar_t)) - 4;
	const tchar_t *wptr = s;
	unsigned pos = 0, prev_word = 0;
	tchar_t p_ch;
	bool negativ = false;

	if(!phrase)
		return NULL;

	/*
	 * since we look back in our generated string
	 * and do not use some cosy warm oop right/left/mid
	 * funcs which safe our ass if the string is to short
	 * to get back 3 chars, we have to fake it...
	 */
	phrase[0] = ' ';
	phrase[1] = ' ';
	phrase[2] = ' ';

	for(; *wptr; pos++, wptr++)
	{
		bool character;
		int distance;

		boundary[0] = boundary[1];
		boundary[1] = SC_NONE;

		boundary[1] |= tiskanji(*wptr) ? SC_KANJI : SC_NONE;
		boundary[1] |= tiskatakana(*wptr) ? SC_KATAKANA : SC_NONE;
		boundary[1] |= tishiragana(*wptr) ? SC_HIRAGANA : SC_NONE;
		boundary[1] |= tischaracter(*wptr) ? SC_REGULAR : SC_NONE;
/*		boundary[1] |= tisdigit(*wptr) ? SC_NUMERIC : SC_NONE; */

		if((boundary[1] & (SC_HIRAGANA | SC_KATAKANA)) == (SC_HIRAGANA | SC_KATAKANA) &&
		   (boundary[0] & (SC_HIRAGANA | SC_KATAKANA)))
			boundary[1] = boundary[0];

		character = !!(boundary[1] & SC_REGULAR); /* || (expression && ('-' == *wptr || '=' == *wptr)); */
		if('-' == *wptr)
			negativ = true;
		else if(' ' == *wptr)
			negativ = false;
		distance = !character ? 1 : 0;

		if(!((!character || boundary[0] != boundary[1]) && pos))
			continue;

		if(pos > prev_word)
		{
			p_ch = *(t_wptr - 2);
			if(boundary[0] &&
			   (' ' == p_ch || '-' == p_ch || '"' == p_ch) &&
			   !tisdigit(*(t_wptr - (pos < 3 ? 1 : 3))))
			{
				/*
				 * Why this "if" is empty, plug from the shareaza code:
				 *
				 * Join two phrases if the previous was a sigle characters word.
				 * idea of joining single characters breaks GDF compatibility completely,
				 * but because Shareaza 2.2 and above are not really following GDF about
				 * word length limit for ASIAN chars, merging is necessary to be done.
				 */
			}
			else if(' ' != *(t_wptr - 1) && character)
			{
				if(('-' != *(t_wptr - 1) || '"' != *(t_wptr - 1) || '"' == *wptr) &&
				   (!negativ || !(boundary[0] & (SC_HIRAGANA | SC_KATAKANA | SC_KANJI)))) {
					if(t_wptr + 1 < end_phrase)
						*t_wptr++ = ' ';
					else
						goto out;
				}
			}

			if('-' == s[pos - 1] && pos > 1)
			{
				if(' ' == *wptr && ' ' == s[pos - 2])
				{
					prev_word += distance + 1;
					continue;
				}
				else
				{
					size_t len = pos - distance - prev_word;
					if(t_wptr + len < end_phrase)
						t_wptr = mempcpy(t_wptr, &s[prev_word], len * sizeof(tchar_t));
					else
						goto out;
				}
			}
			else
			{
				size_t len = pos - prev_word;
				if(t_wptr + len < end_phrase)
					t_wptr = mempcpy(t_wptr, &s[prev_word], len * sizeof(tchar_t));
				else
					goto out;
				if((SC_NONE == boundary[1] && !character) ||
				    ' ' == *wptr ||
				   ((boundary[0] & (SC_HIRAGANA | SC_KATAKANA | SC_KANJI)) && !negativ)) {
					if(t_wptr + 1 < end_phrase)
						*t_wptr++ = ' ';
					else
						goto out;
				}
			}
		}
		prev_word = pos + distance;
	}

	p_ch = *(t_wptr - 2);
	if(boundary[0] && boundary[1] &&
	   (' ' == p_ch || '-' == p_ch || '"' == p_ch))
	{
		/*
		 * Why this "if" is empty, plug from the shareaza code:
		 *
		 * Join two phrases if the previous was a sigle characters word.
		 * idea of joining single characters breaks GDF compatibility completely,
		 * but because Shareaza 2.2 and above are not really following GDF about
		 * word length limit for ASIAN chars, merging is necessary to be done.
		 */
	}
	else if(' ' != *(t_wptr - 1) && boundary[1])
	{
		if(('-' != *(t_wptr - 1) || '"' != *(t_wptr - 1)) && !negativ) {
			if(t_wptr + 1 < end_phrase)
				*t_wptr++ = ' ';
			else
				goto out;
		}
	}
	if(t_wptr + (pos - prev_word) < end_phrase)
		t_wptr = mempcpy(t_wptr, &s[prev_word], (pos - prev_word) * sizeof(tchar_t));

out:
	*t_wptr-- = '\0';
	/* trim left */
	while(' ' == *phrase)
		phrase++;
	/* trim right space and minus */
	while(t_wptr >= phrase)
	{
		if('-' == *t_wptr || ' ' == *t_wptr)
			*t_wptr-- = '\0';
		else
			break;
	}
	return phrase;
}

struct skeyword
{
	struct list_head list;
	uint32_t len;
	tchar_t data[DYN_ARRAY_LEN];
};

static void fill_word_list(struct list_head *word_list, const tchar_t *in, struct zpad *zmem)
{
	bool quote = false, negate = false, space = true;
	const tchar_t *wptr = in, *word_ptr = in;

	for(; *wptr; wptr++)
	{
		if(tischaracter(*wptr))
			space = false;
		else
		{
			if(word_ptr < wptr && !negate)
			{
				bool word, digit, mix;
				tistypemix(word_ptr, wptr - word_ptr, &word, &digit, &mix);
				if(word || mix || (digit && (wptr - word_ptr) > 3))
				{
					struct skeyword *nword;
					nword = qht_zpad_alloc(zmem, 1, sizeof(*nword) + ((wptr - word_ptr) + 1) * sizeof(tchar_t));
					if(nword)
					{
						INIT_LIST_HEAD(&nword->list);
						nword->len = wptr - word_ptr;
						memcpy(nword->data, word_ptr, nword->len * sizeof(tchar_t));
						nword->data[nword->len] = '\0';
						list_add_tail(&nword->list, word_list);
					}
				}
			}

			word_ptr = wptr + 1;
			if('\"' == *wptr) {
				quote = !quote;
				space = true;
			} else if('-' == *wptr && ' ' != wptr[1] && space && !quote) {
				negate = true;
				space = false;
			} else
				space = ' ' == *wptr;

			if(negate && !quote && '-' != *wptr)
				negate = false;
		}
	}

	if(word_ptr < wptr && !negate)
	{
		bool word, digit, mix;
		tistypemix(word_ptr, wptr - word_ptr, &word, &digit, &mix);
		if(word || mix || (digit && (wptr - word_ptr) > 3))
		{
			struct skeyword *nword;
			nword = qht_zpad_alloc(zmem, 1, sizeof(*nword) + ((wptr - word_ptr) + 1) * sizeof(tchar_t));
			if(nword)
			{
				INIT_LIST_HEAD(&nword->list);
				nword->len = wptr - word_ptr;
				memcpy(nword->data, word_ptr, nword->len * sizeof(tchar_t));
				nword->data[nword->len] = '\0';
				list_add_tail(&nword->list, word_list);
			}
		}
	}
}

static noinline bool hash_word_list(struct search_hash_buffer *shb, struct list_head *word_list)
{
	static const tchar_t common_words[][8] =
	{
		/* audio */
		{'m', 'p', '3', '\0'}, {'o', 'g', 'g', '\0'}, {'a', 'c', '3', '\0'},
		/* picture */
		{'j', 'p', 'g', '\0'}, {'g', 'i', 'f', '\0'}, {'p', 'n', 'g', '\0'}, {'b', 'm', 'p', '\0'},
		/* video */
		{'a', 'v', 'i', '\0'}, {'m', 'p', 'g', '\0'}, {'m', 'k', 'v', '\0'}, {'w', 'm', 'v', '\0'},
		{'m', 'o', 'v', '\0'}, {'o', 'g', 'm', '\0'}, {'m', 'p', 'a', '\0'}, {'m', 'p', 'v', '\0'},
		{'m', '2', 'v', '\0'}, {'m', 'p', '2', '\0'}, {'m', 'p', '4', '\0'}, {'d', 'v', 'd', '\0'},
		{'m', 'p', 'e', 'g', '\0'}, {'d', 'i', 'v', 'x', '\0'}, {'x', 'v', 'i', 'd', '\0'},
		/* archive */
		{'e', 'x', 'e', '\0'}, {'z', 'i', 'p', '\0'}, {'r', 'a', 'r', '\0'}, {'i', 's', 'o', '\0'},
		{'b', 'i', 'n', '\0'}, {'c', 'u', 'e', '\0'}, {'i', 'm', 'g', '\0'}, {'l', 'z', 'h', '\0'},
		{'b', 'z', '2', '\0'}, {'r', 'p', 'm', '\0'}, {'d', 'e', 'b', '\0'}, {'7', 'z', '\0'},
		{'a', 'c', 'e', '\0'}, {'a', 'r', 'j', '\0'}, {'l', 'z', 'm', 'a', '\0'},
		/* profanity */
		{'x', 'x', 'x', '\0'}, {'s', 'e', 'x', '\0'}, {'f', 'u', 'c', 'k', '\0'},
		{'t', 'o', 'r', 'r', 'e', 'n', 't', '\0'},
	};
	struct skeyword *lcursor;
	unsigned num_common = 0, num_valid = 0;

	list_for_each_entry(lcursor, word_list, list)
	{
		size_t valid_chars = 0;
		uint32_t w_len = lcursor->len;
		tchar_t f_ch = lcursor->data[0];

		if(!w_len)
			continue;
		if(!tischaracter(f_ch)) /* char valid? */
			continue;
		else if(w_len > 3) /* anything longer than 3 char is OK */
			valid_chars = w_len;
		else if(0x007f >= f_ch) /* basic ascii? */
			valid_chars = w_len;
		else if(0x0080 <= f_ch && 0x07ff >= f_ch) /* simple utf-8 stuff? */
			valid_chars = w_len * 2;
		else if(0x3041 <= f_ch && 0x30fe >= f_ch) /* asian stuff, count with 2 to get 2 chars min  */
			valid_chars = w_len * 2;
		else if(0x0800 <= f_ch) /* other stuff */
			valid_chars = w_len * 3;
		else if(w_len > 2) /* huh? inspect it... */
		{
			bool word, digit, mix;
			tistypemix(lcursor->data, w_len, &word, &digit, &mix);
			if(word || mix)
				valid_chars = w_len;
		}

		if(valid_chars > 2) /* if more than 3 utf-8 chars, everything is fine */
		{
			size_t i;
			bool found = false;
			for(i = 0; i < anum(common_words); i++)
			{
				if(common_words[i][0] == lcursor->data[0] &&
				   0 == tstrncmp(common_words[i], lcursor->data, w_len)) {
					found = true;
					break;
				}
			}
			if(found)
				num_common++;
			else
				num_valid++;
//		printf("found %c nc %u nv %u\n", found ? 't' : 'f', num_common, num_valid);
			shb->hashes[shb->num++] = g2_qht_search_number_word(lcursor->data, 0, w_len);
			if(shb->num >= shb->size)
				break;
		}
	}

	/* if we had a search scheme in the metadata... */
	if(false)
		num_valid += num_common > 1;
	else
		num_valid += num_common > 2;

	return !!num_valid;
}

static tchar_t *URL_decode(tchar_t *url)
{
	tchar_t *input, *output, c;
	for(input = url, output = url, c = *url; c; c = *++input)
	{
		if('%' == c)
		{
			unsigned char x;
			if(!(input[1] && input[2]))
				break;
			if(!from_base16(&x, &input[1], 1))
				break;
			*output++ = x;
			input += 2;
		}
		else if('+' == c) /* shorthand for space */
			*output++ = ' ';
		else
			*output++ = c;
	}
//TODO: after url decode we have to redecode UTF-8
	*output = '\0';
	return url;
}

static tchar_t *URL_filter(tchar_t *s)
{
	/* basically trimleftright */
	tchar_t *s_orig, c;
// TODO: cheapo trim, other whitespace?
	while(' ' == *s)
		s++;
	s_orig = s;
	for(c = *s; c; c = *++s) {
		if(c < 32)
			*s = '_';
	}
	s--;
	while(s >= s_orig && ' ' == *s)
		*s-- = '\0';
	return s;
}

static tchar_t *parse_magnet(tchar_t *urn, size_t *n_len)
{
	tchar_t *wptr, *i_ptr, *ret_val = NULL;
	bool is_end;

	wptr = i_ptr = urn;
	do
	{
		tchar_t *skey, *svalue;
		size_t val_len;

		i_ptr = tstrchrnul(i_ptr, '&');
		is_end = *i_ptr == '\0';
		*i_ptr++ = '\0';

		skey = wptr;
		svalue = tstrchrnul(wptr, '=');
		if(0 == *svalue)
			goto next_pair;
		*svalue++ = '\0';
		skey   = URL_decode(skey);
		svalue = URL_decode(svalue);
		URL_filter(skey);
		val_len = URL_filter(svalue) - svalue;

		if(!(*skey && *svalue))
			goto next_pair;

		if(('x' == skey[0] && 't' == skey[1]) ||
		   ('x' == skey[0] && 's' == skey[1]) ||
		   ('a' == skey[0] && 's' == skey[1]))
			g2_qht_search_add_hash_urn(svalue, val_len);
		else if('d' == skey[0] && 'n' == skey[1])
			ret_val = svalue, *n_len = val_len;
		else if('k' == skey[0] && 't' == skey[1]) {
			ret_val = svalue, *n_len = val_len;
// TODO: forget all hashes already collected from this query??
		}
		/* we don't care for size */
next_pair:
		wptr = i_ptr;
	} while(!is_end);

	return ret_val;
}

bool g2_qht_search_drive(char *metadata, size_t metadata_len, char *dn, size_t dn_len, void *data, bool had_urn, bool hubs)
{
	struct search_hash_buffer *shb =
		(struct search_hash_buffer *)qht_get_scratch1(QHT_DEFAULT_BYTES);

	if(!shb)
		return false;

	if(!had_urn && ((metadata && metadata_len) || (dn && dn_len)))
	{
		LIST_HEAD(word_list);
		struct zpad *zmem = qht_get_zpad(); /* dipping into all mem rescources.. */
		bool valid;

		if(!zmem)
			goto cont;

		/*
		 * metadata (XML) and dn (Desciptive Search lang) have to be groked
		 * and keywords have to be extracted to build hashes to match against
		 * the QHT.
		 * Keyword extraction basically means: after a XML/DN-foo strip off do
		 * word spliting.
		 *
		 * If we would party like it's 1981 we would strtok and be done with it.
		 *
		 * Unfortunatly it is "28 Years Later", zombies have taken over control
		 * and type in searches in their own language.
		 * And word splitting is a PITA on languages from Asia. Other languages
		 * also have their bumps (e.g. arabic: articels ligature into the word).
		 * Did i mention BIDI, in which direction should we take hashes?
		 *
		 * Since we get our stuff as UTF-8, we can express everything that is
		 * in UNICODE, which should be anything on this Planet (mostly...).
		 * But thats just the transmission. It does not help us at groking
		 * the content.
		 *
		 * Shareaza does the most simple aprouch:
		 * If it is some Asian language, do char by char hashes, else split
		 * like 1981.
		 * But this is all ... basic, what about to try to do better when
		 * faced with hiragana/katakana or esp. hangul instead of
		 * hanzi/kanji/hanji? Or Arabic anyone?
		 * And for this we have to detect Asian language. Shareaza uses
		 * something from the winapi, which seems to be there (as in: if
		 * your win is new enough), always? (in every win locale?)
		 *
		 * Where we hit the next big problem.
		 * There is nothing adequate for this in the Std.-libs. All this
		 * wchar_t foo is still locale dependent. You can put every
		 * character in there, but you can only do an isspace&friends for the
		 * stuff in your locale. To top it off, they didn't feel like poluting
		 * the isfoo() space any more, so no iskanji(). Instead they switched
		 * to a generic iswctype() which needs a lookup key you have to aquire.
		 * But which keys you can generate besides the classic xdigit, alnum
		 * etc. is not defined. *sigh*
		 * And not every key works in every locale. So to get the "jkanji"
		 * key (which may exist, or maybe not) you have to be an a japanese
		 * locale. We are still talking about wchar_t here...
		 * Great tennis!
		 * Switching trough 4 locales (with locking, multithreading is a GNU
		 * extension) for every character, which may not be installed (do you
		 * have the korean locale on your machine?)...
		 *
		 * l10n, fuck yeah!
		 *
		 * UNICODE does define a generic word split algo. (TR-28)
		 * But it is just a glimpse at the problem (only basic splitting).
		 * And for this we need to categorize the characters we get in a
		 * UNICODE sense.
		 * Where the fun stops.
		 *
		 * So other libs?
		 * ICU is the solution, if your problem is to few dependencies (C bindings,
		 * but C++ core, besides: 14 Meg source?? WTF?).
		 * glib is also a heavy dep, and only has some basic unicode stuff.
		 * gnulib has a unicode word splitter since Feb.2009, but GPL 3, thanks FSF!
		 *
		 * And the basic Problem with chinese and friends remains. If you do not
		 * take a dict at hand, you split garbage and try to compensate by processing
		 * power. E.g. computational indexing in chinese, if not using a dict, uses
		 * n-grams meaning to use every combination in a n-wide sliding window.
		 * With n = 2 (because the avarge chinese word has 1.54 chars...) this
		 * is quite successful.
		 *
		 * And there is another problem:
		 * Normalization.
		 * You can express one character sometimes as a direct code point, and
		 * also as a composed construct. For example an angstrom (an 'a' with a
		 * circle at the top) can be expressed as 'ANGSTROM SIGN (U+212B)' or
		 * as a 'LATIN CAPITAL LETTER A WITH RING ABOVE' (U+00C5) (old latin1
		 * coding), wich can be decomposed as 'LATIN CAPITAL LETTER A' (U+0041)
		 * and a 'COMBINING RING ABOVE' (U+030A).
		 * So three different binary representations, for the same letter, which
		 * will yield different hashes. In european languages we have a set of
		 * accents above, wrinckles below, dots somewhere to make this "important",
		 * but for example hangul, thanks to their system, can also be written
		 * either completely decomposed (Jamo) or as composed 'fixed' graphemes.
		 * This will mostly hurt with MacOS, because the APIs there normalize
		 * everthing to decomposed form (filenames, etc. tranfered as one name
		 * to the Mac, get a different hash back).
		 *
		 * At the end of the day this does not help us. We have to create hashes
		 * like Shareaza or it will not blend^Wmatch the hash.
		 * And for this we have to create a bunch of code ourself.
		 * Which is ugly, bug prone and...
		 * ... will have similaryties to Shareaza code and includes defines from
		 * the winapi.
		 *
		 */
		if(metadata && metadata_len)
		{
			/*
			 * XML parsing my ass...
			 * since most clients only sent bullshit, nothing wellform, no chance
			 * to validate it if you do not know/guess whats in there and we
			 * simply take any attr for hashes anyway:
			 * XML is a waste of space, cpu time, dependency
			 *
			 * If really needed -> revision 199 in scm
			 */
			char *w_ptr, *s;
			size_t remain;
			char *txt_tbuf;

			txt_tbuf = (char *)qht_get_scratch2(QHT_DEFAULT_BYTES);
			if(!txt_tbuf)
				goto check_dn;

			remain = metadata_len;
			w_ptr = metadata;

			for(s = my_memchr(w_ptr, '=', remain); s; s = my_memchr(w_ptr, '=', remain))
			{
				tchar_t *str_buf, *wptr, *str_buf_orig;
				size_t len, o_len, tlen;
				char *txt;

				remain -= (s - w_ptr);
				if(!remain)
					break;
				w_ptr = s + 1;
				if('"' != *w_ptr)
					continue;
				if(!memcmp(w_ptr - str_size("Namespace") - 1, "Namespace", str_size("Namespace")))
					continue;
				w_ptr++;
				remain--;
				if(!remain)
					break;
				s = my_memchr(w_ptr, '"', remain);
				if(!s)
					len = remain;
				else
					len = s - w_ptr;
				remain -= len;

				if(len >= QHT_DEFAULT_BYTES)
					break; /* this should not happen... */

				txt = txt_tbuf;
				o_len = len;
				len = decode_html_entities_utf8(txt, w_ptr, len);
				while(*txt && isblank(*txt))
					txt++, len--;
				while(len && isblank(*(txt + len - 1)))
					txt[--len] = '\0';

				logg_develd_old("len: %zu\t\"%s\" \"%.*s\"\n", len, txt, (int)o_len, w_ptr);
				if(len)
				{
					str_buf = qht_zpad_alloc(zmem, len + 10, sizeof(tchar_t));
					if(!str_buf)
						break;
					str_buf_orig = str_buf;

					tlen = utf8totcs(str_buf, len + 9, txt, &len);
					if(!tlen || len) /* no output or input not consumed? */
						break;

					str_buf[tlen] = '\0';
					wptr = make_keywords(str_buf);
					qht_zpad_free(zmem, str_buf_orig);
					if(wptr)
						fill_word_list(&word_list, wptr, zmem);
				}
				if(!remain)
					break;
				w_ptr = s + 1;
				remain--;
				if(!remain)
					break;
			}
		}
check_dn:
		if(dn && dn_len)
		{
			static const tchar_t URN_MAGNET[] = {'m', 'a', 'g', 'n', 'e', 't', ':', '?', '\0'};
			static const tchar_t URN_URN[] = {'u', 'r', 'n', ':', '\0'};
			tchar_t *str_buf = qht_zpad_alloc(zmem, dn_len + 10, sizeof(tchar_t));
			tchar_t *wptr, *str_buf_orig;
			size_t dn_tlen;

			if(!str_buf)
				goto work_list;
			str_buf_orig = str_buf;

			dn_tlen = utf8totcs(str_buf, dn_len + 9, dn, &dn_len);
			if(!dn_tlen || dn_len) /* no output or input not consumed? */
				goto work_list;

			str_buf[dn_tlen] = '\0';
			dn_tlen = tstrptolower(str_buf) - str_buf;
			if(!dn_tlen)
				goto work_list;
			wptr = tstr_trimleft(str_buf);
			dn_tlen -= (size_t)(wptr - str_buf) <= dn_tlen ?
			           (size_t)(wptr - str_buf) : dn_tlen;
			if(!dn_tlen)
				goto work_list;

			if(0 == tstrncmp(str_buf, URN_MAGNET, tstr_size(URN_MAGNET)))
			{
				/* magnet url */
				size_t n_len = 0;
				str_buf += tstr_size(URN_MAGNET);
				dn_tlen -= tstr_size(URN_MAGNET);
				had_urn  = true;
				wptr = parse_magnet(str_buf, &n_len);
				if(wptr) {
					str_buf  = wptr;
					dn_tlen  = n_len;
				} else
					goto work_list;
			}

			while(0 == tstrncmp(str_buf, URN_URN, tstr_size(URN_URN)))
			{
				/* urns */
				size_t dist;
				wptr = tstrchrnul(str_buf, ' ');
				dist = wptr ? ((size_t)(wptr - str_buf) <= dn_tlen ?
				               (size_t)(wptr - str_buf) : dn_tlen) : dn_tlen;
				had_urn = g2_qht_search_add_hash_urn(str_buf, dist);
				if(!had_urn)
					break;

				if(*wptr) {
					wptr = tstr_trimleft(wptr);
					dn_tlen -= dist;
					str_buf  = wptr;
				} else {
					str_buf = wptr;
					dn_tlen = 0;
					break;
				}
			}

			if(dn_tlen)
			{
				wptr = tstr_trimleft(str_buf);
				dn_tlen -= (size_t)(wptr - str_buf) <= dn_tlen ?
				           (size_t)(wptr - str_buf) : dn_tlen;
				str_buf = wptr;
			}
			if(dn_tlen)
			{
				wptr = tstr_trimright(str_buf);
				if(wptr >= str_buf)
					dn_tlen = (wptr - str_buf) + 1;
				else
					dn_tlen = 0;
			}
			if(dn_tlen)
			{
				wptr = make_keywords(str_buf);
				qht_zpad_free(zmem, str_buf_orig);
				if(wptr)
					fill_word_list(&word_list, wptr, zmem);
			}
		}

work_list:
		valid = hash_word_list(shb, &word_list);
//		printf("valid: %c had_urn: %c == %c\n",
//		       valid ? 't' : 'f', had_urn ? 't': 'f',
//		       (!valid && !had_urn) ? 't' : 'f');
		/* refuse query if no "valid" word and no urn */
		if(!valid && !had_urn)
			return false;
	}
cont:
	shb->num = introsort_u32(shb->hashes, shb->num);
	if(!shb->num)
		return false;

	return g2_packet_search_finalize(shb->hashes, shb->num, data, hubs);
}

struct hub_match_carg
{
	uint32_t *hashes;
	size_t num;
	void *data;
};

static intptr_t hub_match_callback(g2_connection_t *con, void *carg)
{
	struct hub_match_carg *rdata = carg;
	struct qhtable *table;
	struct qht_data *qd;

	/* no point in touching dead connections */
	if(unlikely(con->flags.dismissed))
		return 0;

	do {
		mb();
		hzp_ref(HZP_QHT, table = con->qht);
	} while(table != con->qht);

	if(!table) /* if we have no table, we have to assume a match */
		return g2_packet_hub_qht_match(con, rdata->data);

	if(table->flags.reset_needed) /* table empty, no match possible */
		goto out;

	/* ignore to full QHTs */
	if(table->used >= 1000)
		goto out;

	do {
		mb();
		hzp_ref(HZP_QHTDAT, qd = container_of(table->data, struct qht_data, data));
	} while(qd != container_of(table->data, struct qht_data, data));

	if(!table->data) /* no data? We have to assume a match */
		goto out_match;

	if(COMP_RLE == table->compressed) {
		if(bitfield_lookup(rdata->hashes, rdata->num, table->data, table->data_length))
			goto out_match;
	}
	else
	{
		size_t i;
		for(i = 0; i < rdata->num; i++) {
			uint32_t n = rdata->hashes[i];
			if(n > table->entries)
				break;
			if(!(table->data[n / BITS_PER_CHAR] & (1 << (n % BITS_PER_CHAR))))
				goto out_match;
		}
	}

	hzp_unref(HZP_QHTDAT);
out:
	hzp_unref(HZP_QHT);
	return g2_packet_hub_qht_done(con, rdata->data);
out_match:
	hzp_unref(HZP_QHTDAT);
	hzp_unref(HZP_QHT);
	return g2_packet_hub_qht_match(con, rdata->data);
}

void g2_qht_match_hubs(uint32_t hashes[], size_t num, void *data)
{
	struct hub_match_carg rdata = { hashes, num, data };
	g2_conreg_all_hub(NULL, hub_match_callback, &rdata);
}

void g2_qht_global_search_chain(struct qht_search_walk *qsw, void *arg)
{
	g2_connection_t *con = arg;
	struct qhtable *table;
	struct qht_data *qd;
	size_t entries;

	/* no point in touching dead connections */
	if(unlikely(con->flags.dismissed))
		return;

	do {
		mb();
		hzp_ref(HZP_QHT, table = con->qht);
	} while(table != con->qht);

	if(!table)
		return;

	if(table->flags.reset_needed)
		goto out;

	/* ignore to full QHTs */
	if(table->used >= server.settings.qht.max_promille)
		goto out;

	entries = table->entries;
	do {
		mb();
		hzp_ref(HZP_QHTDAT, qd = container_of(table->data, struct qht_data, data));
	} while(qd != container_of(table->data, struct qht_data, data));

	if(!table->data)
		goto out_unref;

	if(COMP_RLE == table->compressed) {
		if(bitfield_lookup(qsw->hashes, qsw->num, table->data, table->data_length))
			goto out_match;
	}
	else
	{
		size_t i, num = qsw->num;
		uint32_t *hashes = qsw->hashes;
		for(i = 0; i < num; i++) {
			uint32_t n = hashes[i];
			if(n > entries)
				break;
			if(!(qd->data[n / BITS_PER_CHAR] & (1 << (n % BITS_PER_CHAR))))
				goto out_match;
		}
	}

out_unref:
	hzp_unref(HZP_QHTDAT);
out:
	hzp_unref(HZP_QHT);
	return;

out_match:
	hzp_unref(HZP_QHTDAT);
	hzp_unref(HZP_QHT);
	g2_packet_leaf_qht_match(con, qsw->data);
}

bool g2_qht_global_search_bucket(struct qht_search_walk *qsw, struct qhtable *t)
{
	struct qht_data *qd;
	uint32_t *hashes;
	size_t i, j, entries, num;
	bool ret_val = false;

	entries = t->entries;
	do {
		mb();
		hzp_ref(HZP_QHTDAT, qd = container_of(t->data, struct qht_data, data));
	} while(qd != container_of(t->data, struct qht_data, data));

	rmb();
	if(!t->data || COMP_RLE == t->compressed || t->flags.reset_needed)
		goto out_unref;

	num = qsw->num;
	hashes = qsw->hashes;
	/*
	 * condense hashes to matches
	 * Dirty trick:
	 * While setup we made sure there is enough room for
	 * matched hashes at the end CONREG_LEVELS deep.
	 */
	for(i = 0, j = num; i < num; i++)
	{
		uint32_t n = hashes[i];
		if(n > entries)
			break;
		if(unlikely(!(qd->data[n / BITS_PER_CHAR] & (1 << (n % BITS_PER_CHAR)))))
			hashes[j++] = n;
	}
	/* did something match */
	if(j - num)
	{
		qsw->hashes = &hashes[num];
		qsw->num = j - num;
		ret_val = true;
	}

out_unref:
	hzp_unref(HZP_QHTDAT);
	hzp_unref(HZP_QHT);
	return ret_val;
}

void g2_qht_match_leafs(uint32_t hashes[], size_t num, void *data)
{
	struct qht_data *qd;
	struct qhtable *master_qht;
	size_t i, j, entries;

	master_qht = g2_qht_global_get_hzp();
	if(!master_qht)
		return;

	/* should not happen */
	if(master_qht->flags.reset_needed)
		goto out;

	entries = master_qht->entries;
	do {
		mb();
		hzp_ref(HZP_QHTDAT, qd = container_of(master_qht->data, struct qht_data, data));
	} while(qd != container_of(master_qht->data, struct qht_data, data));

	if(!master_qht->data || COMP_RLE == master_qht->compressed)
		goto out_unref;

	/* condense hashes to matches */
	for(i = 0, j = 0; i < num; i++)
	{
		uint32_t n = hashes[i];
		if(n > entries)
			break;
		if(!(qd->data[n / BITS_PER_CHAR] & (1 << (n % BITS_PER_CHAR))))
			hashes[j++] = n;
	}
	num = j;
	/* let the master qht go */
	hzp_unref(HZP_QHTDAT);
	hzp_unref(HZP_QHT);

	if(num) {
		struct qht_search_walk qsw = {.hashes = hashes, .num = num, .data = data};
		g2_qht_global_search(&qsw);
	}
	return;

out_unref:
	hzp_unref(HZP_QHTDAT);
out:
	hzp_unref(HZP_QHT);
}

static bool qht_compress_table(struct qhtable *table, uint8_t *data, size_t qht_size)
{
	uint8_t *ndata = qht_get_scratch1(QHT_DEFAULT_BYTES); /* try to not realloc scratch */
	ssize_t res;
	struct qht_data *t_data;
	void *t_x = NULL;
	enum g2_qht_comp tcomp;

	res = bitfield_encode(ndata, QHT_RLE_MAXSIZE, data, qht_size);
	if(res < 0)
	{
		logg_develd_old("QHT %p could not be compressed!\n", table);
		tcomp = COMP_NONE;
		res = qht_size;
		ndata = data;
	}
	else
	{
		logg_develd_old("QHT %p compressed - 1:%u.%u\n", table,
		            (unsigned)(qht_size / res),
		            (unsigned)(((qht_size % res) * 1000) / res));
		tcomp = COMP_RLE;
	}

	t_data = g2_qht_data_alloc(res);
	if(!t_data) {
		logg_develd("could not allocate memory for %s compressable QHT: %zu\n",
		            COMP_NONE == tcomp ? "not" : "", res);
		return false;
	}
	my_memcpy(t_data->data, ndata, res);
	if(table->data) {
		t_x = atomic_px(t_x, (atomicptr_t *)&table->data);
		g2_qht_data_free(t_x);
	}
	table->compressed = tcomp;
	table->data_length = res;
	t_x = atomic_px(t_data->data, (atomicptr_t *)&table->data);
	g2_qht_data_free(t_x);
	return true;
}

const char *g2_qht_patch(struct qhtable *table, struct qht_fragment *frag)
{
	const char *ret_val = NULL;
	unsigned char *target_ptr;
	size_t pos, qht_size;

	if(!table || !frag)
		return NULL;

	qht_size = DIV_ROUNDUP(table->entries, BITS_PER_CHAR);

	if(atomic_read(&table->refcnt) > 1)
		logg_develd("WARNING: patch called on table with refcnt > 1!! %p %i\n", (void*)table, atomic_read(&table->refcnt));

	logg_develd_old("qlen: %zu\tflen: %zu\n", table->data_length, frag->length);

/*	if(table->flags.reset_needed)
		logg_develd("reset_needed qht-table passed: %p\n", (void *) table);*/

// TODO: prevent hub QHTs getting compressed

	if(COMP_RLE == table->compressed || table->flags.reset_needed)
	{
		target_ptr = qht_get_scratch2(qht_size);
		if(!target_ptr)
			return NULL;
		if(COMP_RLE == table->compressed && !table->flags.reset_needed) {
			ssize_t res = bitfield_decode(target_ptr, qht_size, table->data, table->data_length);
			if(res < 0) {
				logg_develd("failed to de-rle QHT: %zi\n", res);
				return NULL;
			}
		}
	}
	else
		target_ptr = table->data;

	switch(frag->compressed)
	{
	case COMP_DEFLATE:
		{
			int res;
			unsigned char *tmp_ptr = qht_get_scratch1(qht_size);
			struct zpad *zpad = qht_get_zpad();

			if(!(tmp_ptr && zpad)) {
				logg_devel("patch mem failed\n");
				break;
			}

			/* fix allocations... done, TLS rocks ;) */
			zpad->z.next_out  = tmp_ptr;
			zpad->z.avail_out = qht_size;
			zpad->z.next_in   = frag->data;
			zpad->z.avail_in  = frag->length;
			frag = frag->next;

			if(Z_OK != inflateInit(&zpad->z)) {
				inflateEnd(&zpad->z);
				logg_devel("failure initiliasing patch decompressor\n");
				break;
			}
			do
			{
				res = inflate(&zpad->z, Z_SYNC_FLUSH);
				if(Z_OK == res && !zpad->z.avail_in)
				{
					if(!frag)
						break;
					zpad->z.next_in  = frag->data;
					zpad->z.avail_in = frag->length;
					frag = frag->next;
				} else if(Z_STREAM_END != res) {
					logg_develd("failure decompressing patch: %i\n", res);
					break;
				}
			} while(Z_STREAM_END != res);
			inflateEnd(&zpad->z);
			if(Z_STREAM_END != res) {
				logg_develd("failure decompressing patch: %i\n", res);
				break;
			}

			/* apply it */
			if(table->flags.reset_needed)
				memneg(target_ptr, tmp_ptr, qht_size);
			else
				memxor(target_ptr, tmp_ptr, qht_size);
			table->used = table->entries - mempopcnt(target_ptr, qht_size);
			logg_develd_old("QHT e: %zu\tu: %zu\tp: %zu\n", table->entries, table->used, (table->used * 1000) / table->entries);
			table->used = (table->used * 1000) / table->entries;
			qht_dump(target_ptr, tmp_ptr, qht_size);
			if(server.settings.qht.compress_internal &&
			   (COMP_RLE == table->compressed || table->flags.reset_needed))
				if(!qht_compress_table(table, target_ptr, qht_size))
					break;
			table->flags.reset_needed = false;
			ret_val = "compressed patch applied";
		}
		break;
	case COMP_NONE:
		/* size for no compression case is already checked */
		for(pos = 0; frag; frag = frag->next)
		{
			if(table->flags.reset_needed)
				memneg(target_ptr + pos, frag->data, frag->length);
			else
				memxor(target_ptr + pos, frag->data, frag->length);
			qht_dump(target_ptr + pos, frag->data, frag->length);
			pos += frag->length;
		}

		table->used = table->entries - mempopcnt(target_ptr, qht_size);
		logg_develd_old("QHT e: %zu\tu: %zu\tp: %zu\n", table->entries, table->used, (table->used * 1000) / table->entries);
		table->used = (table->used * 1000) / table->entries;
		if(server.settings.qht.compress_internal &&
		   (COMP_RLE == table->compressed || table->flags.reset_needed))
			if(!qht_compress_table(table, target_ptr, qht_size))
				break;
		table->flags.reset_needed = false;
		ret_val = "patch applied";
		break;
	default:
		logg_develd("funky compression: %i\n", frag->compressed);
	}

	table->time_stamp = local_time_now;
	wmb();

	return ret_val;
}

void g2_qht_aggregate(struct qhtable *to, struct qhtable *from)
{
	size_t qht_size;

	rmb();
	qht_size = DIV_ROUNDUP(to->entries, BITS_PER_CHAR);

	qht_size = qht_size <= DIV_ROUNDUP(from->entries, BITS_PER_CHAR) ? qht_size :
	           DIV_ROUNDUP(from->entries, BITS_PER_CHAR);

	if(to->flags.reset_needed || COMP_RLE != from->compressed)
	{
		uint8_t *from_ptr;
		if(COMP_RLE == from->compressed)
		{
			ssize_t res;
			from_ptr = qht_get_scratch1(qht_size);
			if(!from_ptr)
				return;
			res = bitfield_decode(from_ptr, qht_size, from->data, from->data_length);
			if(res < 0)
				logg_develd("failed to de-rle QHT %p: %zi\n", from, res);
				/* means we have not enough space, but we can use the bytes... */
		}
		else
			from_ptr = from->data;

		if(to->flags.reset_needed) {
			my_memcpy(to->data, from_ptr, qht_size);
			to->flags.reset_needed = false;
		} else
			memand(to->data, from_ptr, qht_size);
	}
	else
	{
		ssize_t res;
		res = bitfield_and(to->data, qht_size, from->data, from->data_length);
		if(res < 0)
			logg_develd("failed to de-rle_and QHT %p: %zi\n", from, res);
			/* means we have not enough space, but we can use the bytes... */
	}
}

struct qht_fragment *g2_qht_frag_alloc(size_t len)
{
	struct qht_fragment *ret_val;

	ret_val = malloc(sizeof(*ret_val) + len);
	if(ret_val) {
		ret_val->next   = NULL;
		ret_val->length = len;
	}
	return ret_val;
}

void g2_qht_frag_free(struct qht_fragment *to_free)
{
	if(!to_free)
		return;
	g2_qht_frag_free(to_free->next);
	free(to_free);
}

struct qht_fragment *g2_qht_diff_get_frag(const struct qhtable *org, const struct qhtable *new)
{
	struct qht_fragment *ret_val = NULL, *n = NULL, *t;
	struct zpad *zpad;
	unsigned char *tmp_ptr;
	size_t len, pos, frag_len, frags = 0;
	int res;

	if(unlikely(!new))
		return NULL;

// TODO: we assume that fragment generating QHTs are not compressed (master qht)

	len = org ? org->data_length : new->data_length;
	len = new->data_length < len ? new->data_length : len;

	switch(server.settings.qht.compression)
	{
	default:
		logg_develd("funky compression: %i\n", server.settings.qht.compression);
	case COMP_NONE:
		pos = 0;
		do
		{
			frag_len = len > (NORM_BUFF_CAPACITY - QHT_PATCH_HEADER_BYTES) ?
			           NORM_BUFF_CAPACITY : len + QHT_PATCH_HEADER_BYTES;
			t = g2_qht_frag_alloc(frag_len);
			if(!t)
				break;
			t->nr = ++frags;
			tmp_ptr = t->data + QHT_PATCH_HEADER_BYTES;
			frag_len -= QHT_PATCH_HEADER_BYTES;
			len -= frag_len;

			if(!org)
				memneg(tmp_ptr, new->data + pos, frag_len);
			else
				memxorcpy(tmp_ptr, org->data + pos, new->data + pos, frag_len);
			qht_sdump(tmp_ptr, frag_len);
			pos += frag_len;
			t->next = n;
			n = t;
		} while(len);
		if(n)
		{
			t = n->next;
			n->next = NULL;
			ret_val = n;
			ret_val->count = frags;
			ret_val->compressed = COMP_NONE;

			while(t)
			{
				n = t;
				t = t->next;
				n->next = ret_val;
				ret_val = n;
				ret_val->count = frags;
				ret_val->compressed = COMP_NONE;
			}
		}
		else if(t)
		{
//TODO: Free something
		}
		break;
	case COMP_DEFLATE:
		tmp_ptr = qht_get_scratch1(len);
		zpad = qht_get_zpad();

		if(!(tmp_ptr && zpad)) {
			logg_devel("patch mem failed\n");
			break;
		}

		if(!org)
			memneg(tmp_ptr, new->data, len);
		else
			memxorcpy(tmp_ptr, org->data, new->data, len);
		qht_sdump(tmp_ptr, len);

		zpad->z.next_in = tmp_ptr;
		zpad->z.avail_in = len;

		if(Z_OK != deflateInit(&zpad->z, Z_DEFAULT_COMPRESSION)) {
			deflateEnd(&zpad->z);
			logg_devel("failure initiliasing patch compressor\n");
			break;
		}
		do
		{
			t = g2_qht_frag_alloc(NORM_BUFF_CAPACITY);
			if(!t)
				break;
			t->nr = ++frags;

			zpad->z.next_out = t->data + QHT_PATCH_HEADER_BYTES;
			zpad->z.avail_out = t->length - QHT_PATCH_HEADER_BYTES;
			res = deflate(&zpad->z, Z_FINISH);
			if(Z_OK == res || Z_STREAM_END == res) {
				t->length -= zpad->z.avail_out;
				t->next = n;
				n = t;
			} else {
				logg_develd("failure compressing patch: %i\n", res);
				break;
			}
		} while(Z_STREAM_END != res);
		deflateEnd(&zpad->z);
		if(n)
		{
			t = n->next;
			n->next = NULL;
			ret_val = n;
			ret_val->count = frags;
			ret_val->compressed = COMP_DEFLATE;

			while(t)
			{
				n = t;
				t = t->next;
				n->next = ret_val;
				ret_val = n;
				ret_val->count = frags;
				ret_val->compressed = COMP_DEFLATE;
			}
		}
		else if(t)
		{
//TODO: Free something
		}
	}

	return ret_val;
}

int g2_qht_add_frag(struct qhtable *ttable, struct qht_fragment *frag, uint8_t *data)
{
	struct qht_fragment **t, *n;
	size_t total_len, legal_len;

	if(!ttable)
		return -1;

	if(frag->compressed != COMP_NONE && frag->compressed != COMP_DEFLATE) {
		logg_develd("unknown compression %i in qht fragment\n", frag->compressed);
		return -1;
	}

	t = &ttable->fragments;
	n = *t;
	if(n)
	{
		if(frag->count != n->count) {
			logg_devel("fragment-count changed!\n");
			return -1;
		}
		if(frag->compressed != n->compressed) {
			logg_devel("compression changed!\n");
			return -1;
		}
	} else if(1 != frag->nr) {
		logg_devel("fragment not starting at one?\n");
		return -1;
	}

	total_len = 0;
	for(; n; n = (*t)->next, t = &(*t)->next)
		total_len += n->length;

	if(n && frag->nr != (n->nr + 1)) {
		logg_devel("fragment missed\n");
		return -1;
	}

	legal_len = DIV_ROUNDUP(ttable->entries, BITS_PER_CHAR);
	/*
	 * stop this, there are some large QHTs after compress
	 * (prop. Hubs), but don't allow data to grow, compression
	 * should at least yield +/- 0.
	 * TODO: If the patch is compressed, we are just guessing a max length
	 *
	if(frag->compressed)
		legal_len = (legal_len * 6) / 8;
	 */
	/*
	 * ATM check is sane, we are checking the real sizes of the
	 * really recieved data, not user supplied data...
	 */
	if((total_len + frag->length) > legal_len) {
		logg_devel("patch to big\n");
		return -1;
	}

	my_memcpy(frag->data, data, frag->length);
	*t = frag;

	logg_develd_old("nr: %u\tcount: %u\n", frag->nr, frag->count);
	return frag->nr >= frag->count;
}

bool g2_qht_reset(struct qhtable **ttable, uint32_t qht_ent, bool try_compress)
{
	size_t w_size, bits;
	struct qhtable *tmp_table;

	if(!ttable) {
		logg_devel("called with NULL ttable?\n");
		return true;
	}

	w_size = DIV_ROUNDUP((size_t)qht_ent, BITS_PER_CHAR);
	if(!qht_ent || w_size > MAX_BYTES_QHT) {
		logg_devel("illegal number of elements\n");
		return true;
	}

	/* qht_ent is zero-checked, we need the power, not the index, so -1 */
	bits = flsst(qht_ent) - 1;
	if((1 << bits) ^ qht_ent)
		logg_develd("TODO: %lu bits, %lu entries, can handle this, but hashes will be bogus\n",
			(unsigned long) bits, (unsigned long) qht_ent);

	/*
	 * set the master table NULL, someone could traverse it,
	 * while we process it
	 */
	tmp_table = *ttable;
	*ttable = NULL;
	wmb();
	if(!tmp_table)
	{
		tmp_table = malloc(sizeof(*tmp_table));
		if(!tmp_table) {
			logg_errno(LOGF_DEBUG, "allocating qh-table");
			return true;
		}
		atomic_set(&tmp_table->refcnt, 1);
		tmp_table->fragments = NULL;
		tmp_table->data = NULL;
		tmp_table->data_length = 0;
	}
// TODO: new data is not initialised
	/* try with this early reset_needed. membar? */
	tmp_table->flags.reset_needed = true;
	wmb();
	if(!try_compress)
	{
		if(tmp_table->data_length < w_size)
		{
			struct qht_data *ttable_data;

			if(atomic_read(&tmp_table->refcnt) > 1)
				logg_develd("WARNING: reset on qht with refcnt > 1!! %p %i\n", (void*)tmp_table, atomic_read(&tmp_table->refcnt));

			ttable_data = g2_qht_data_alloc(w_size);
			if(ttable_data) {
				uint8_t *ptr = tmp_table->data;
				rmb();
				tmp_table->data = ttable_data->data;
				tmp_table->data_length = w_size;
				g2_qht_data_free(ptr);
			} else {
				logg_errno(LOGF_DEBUG, "reallocating qh-table data");
				qht_ent = tmp_table->entries;
				/* control flow will resurect old table, only wiped */
			}
		}
	} else if(tmp_table->data) {
		void *ptr  = tmp_table->data;
		rmb();
		tmp_table->data = NULL;
		tmp_table->data_length = 0;
		g2_qht_data_free(ptr);
	}

	_g2_qht_clean(tmp_table, true);
	tmp_table->entries = qht_ent;
	tmp_table->bits = bits;
	tmp_table->time_stamp = local_time_now;
	/* bring back the table */
	wmb();
	*ttable = tmp_table;
	return false;
}

#ifdef QHT_DUMP
static int qht_tdump_fd, qht_pdump_fd, qht_sdump_fd;
static void qht_dump_init(void)
{
	char tmp_nam[sizeof("./G2QHTtdump.bin") + 12];
	my_snprintf(tmp_nam, sizeof(tmp_nam), "./G2QHTtdump%lu.bin", (unsigned long)getpid());
	if(0 > (qht_tdump_fd = open(tmp_nam, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)))
		logg_errno(LOGF_ERR, "opening QHT-table-file");
	my_snprintf(tmp_nam, sizeof(tmp_nam), "./G2QHTpdump%lu.bin", (unsigned long)getpid());
	if(0 > (qht_pdump_fd = open(tmp_nam, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)))
		logg_errno(LOGF_ERR, "opening QHT-patch-file");
	my_snprintf(tmp_nam, sizeof(tmp_nam), "./G2QHTsdump%lu.bin", (unsigned long)getpid());
	if(0 > (qht_sdump_fd = open(tmp_nam, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)))
		logg_errno(LOGF_ERR, "opening QHT-spatch-file");
}

static void qht_dump_deinit(void)
{
	if(0 <= qht_tdump_fd)
		close(qht_tdump_fd);
	if(0 <= qht_pdump_fd)
		close(qht_pdump_fd);
	if(0 <= qht_sdump_fd)
		close(qht_sdump_fd);
}

static void qht_sdump(void *patch, size_t len)
{
	if(len != (size_t)write(qht_sdump_fd, patch, len)) {
		/* it's a debug dump ... */
	}
}

static void qht_dump(void *table, void *patch, size_t len)
{
	if(len != (size_t)write(qht_tdump_fd, table, len)) {
		/* ... cool down ... */
	}
	if(len != (size_t)write(qht_pdump_fd, patch, len)) {
		/* ... everything is fine! */
	}
}
#endif

static char const rcsid_q[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
