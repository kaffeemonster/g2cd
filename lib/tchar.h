/*
 * tchar.h
 * header-file for the tchar stuff
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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

#ifndef LIB_TCHAR_H
# define LIB_TCHAR_H

/*
 * The reference impl. for G2 is Shareaza.
 * Our "visual" behavior has to be the same in all main aspects.
 * A major aspect are searches and (calculated) hashes.
 * Esp. hashes have to match, the mathematical result has to be the
 * same if we calculate the hash (as receiver) or Shareaza (as sender).
 *
 * There is only one little problem:
 * Shareaza is "pure" win-software and some windows details have creped
 * into all that math.
 *
 * Like that win uses UTF-16 for its wchar_t. Or that Shareaza tries to
 * split foreign text by some homegrown foo and win character typing.
 *
 * We have to do the same, but we have no win api.
 * So lets pile up some compatibility cruft.
 */

# include <stdbool.h>
# include "other.h"
# include "my_bitops.h"

typedef uint16_t tchar_t;

# define LIB_TCHAR_EXTRN(x) x GCC_ATTR_VIS("hidden")

extern const uint16_t tchar_tolower_base_data[0x1000] GCC_ATTR_VIS("hidden");
extern const uint16_t tchar_c1table_base_data[0x1000] GCC_ATTR_VIS("hidden");
extern const uint16_t tchar_c3table_base_data[0x1000] GCC_ATTR_VIS("hidden");

LIB_TCHAR_EXTRN(size_t tstrlen(const tchar_t *s)) GCC_ATTR_PURE;
LIB_TCHAR_EXTRN(int tstrncmp(const tchar_t *s1, const tchar_t *s2, size_t n));
LIB_TCHAR_EXTRN(tchar_t *tstrchrnul(const tchar_t *s, tchar_t c));
LIB_TCHAR_EXTRN(tchar_t *tstrptolower(tchar_t *s));
LIB_TCHAR_EXTRN(size_t utf8totcs(tchar_t *dst, size_t dl, const char *src, size_t *sl));

static inline tchar_t *tstr_trimleft(tchar_t *s)
{
// TODO: cheapo trim, other whitespace?
	while(' ' == *s)
		s++;
	return s;
}

static inline tchar_t *tstr_trimright(tchar_t *s)
{
	tchar_t *s_orig = s;
	while(*s)
		s++;
	s--;
// TODO: cheapo trim, other whitespace?
	while(s >= s_orig && ' ' == *s)
		*s-- = '\0';
	return s;
}

static inline tchar_t *tstr_trimleftright(tchar_t *s)
{
	tchar_t *s_orig;
// TODO: cheapo trim, other whitespace?
	while(' ' == *s)
		s++;
	s_orig = s;
	while(*s)
		s++;
	s--;
	while(s >= s_orig && ' ' == *s)
		*s-- = '\0';
	return s_orig;
}

#define tstr_size(x) ((sizeof(x)/2)-1)

static inline tchar_t *mempctcpy(tchar_t *dst, const char *src, size_t s)
{
	while(s--)
		*dst++ = *src++;
	return dst;
}

#define strplitctcpy(d, s) mempctcpy((d), (s), str_size(s))
#define tstrplitcpy(d, s) mempcpy((d), (s), tstr_size(s) * 2)

/*
 * the GetStringTypeExW(CT_CTYPE1) info accord. to MSDN
 * classical stuff
 */
enum tchar_ctype1
{
	TC_C1_UPPER   = 0x0001, /* Uppercase */
	TC_C1_LOWER   = 0x0002, /* Lowercase */
	TC_C1_DIGIT   = 0x0004, /* Decimal digits */
	TC_C1_SPACE   = 0x0008, /* Space characters */
	TC_C1_PUNCT   = 0x0010, /* Punctuation */
	TC_C1_CNTRL   = 0x0020, /* Control characters */
	TC_C1_BLANK   = 0x0040, /* Blank characters */
	TC_C1_XDIGIT  = 0x0080, /* Hexadecimal digits */
	TC_C1_ALPHA   = 0x0100, /* Any linguistic character: alphabetical, syllabary, or ideographic */
	TC_C1_DEFINED = 0x0200, /* A defined character, but not one of the other C1_* types */
	TC_C1_ALNUM   = TC_C1_ALPHA|TC_C1_DIGIT,
	TC_C1_PRINT   = ~(TC_C1_CNTRL),
};

/*
 * the GetStringTypeExW(CT_CTYPE2) info accord. to MSDN
 * direction stuff
 */
enum tchar_ctype2
{
	/* Strong */
	TC_C2_LEFTTORIGHT      = 0x0001, /* Left to right */
	TC_C2_RIGHTTOLEFT      = 0x0002, /* Right to left */
	/* Weak */
	TC_C2_EUROPENUMBER     = 0x0003, /* European number, European digit */
	TC_C2_EUROPESEPARATOR  = 0x0004, /* European numeric separator */
	TC_C2_EUROPETERMINATOR = 0x0005, /* European numeric terminator */
	TC_C2_ARABICNUMBER     = 0x0006, /* Arabic number */
	TC_C2_COMMONSEPARATOR  = 0x0007, /* Common numeric separator */
	/* Neutral */
	TC_C2_BLOCKSEPARATOR   = 0x0008, /* Block separator */
	TC_C2_SEGMENTSEPARATOR = 0x0009, /* Segment separator */
	TC_C2_WHITESPACE       = 0x000A, /* White space */
	TC_C2_OTHERNEUTRAL     = 0x000B, /* Other neutrals */
	TC_C2_NOTAPPLICABLE    = 0x0000, /* No implicit directionality (for example, control codes) */
};

/*
 * the GetStringTypeExW(CT_CTYPE3) info accord. to MSDN
 * script and misc stuff
 */
enum tchar_ctype3
{
	TC_C3_NOTAPPLICABLE = 0x0000, /* Not applicable */
	TC_C3_NONSPACING    = 0x0001, /* Nonspacing mark */
	TC_C3_DIACRITIC     = 0x0002, /* Diacritic nonspacing mark */
	TC_C3_VOWELMARK     = 0x0004, /* Vowel nonspacing mark */
	TC_C3_SYMBOL        = 0x0008, /* Symbol */
	TC_C3_KATAKANA      = 0x0010, /* Katakana character */
	TC_C3_HIRAGANA      = 0x0020, /* Hiragana character */
	TC_C3_HALFWIDTH     = 0x0040, /* Half-width (narrow) character */
	TC_C3_FULLWIDTH     = 0x0080, /* Full-width (wide) character */
	TC_C3_IDEOGRAPH     = 0x0100, /* Ideographic character */
	TC_C3_KASHIDA       = 0x0200, /* Arabic Kashida character */
	TC_C3_LEXICAL       = 0x0400, /* Punctuation which is counted as part of the word
	                                 (Kashida, hyphen, feminine/masculine ordinal indicators,
	                                 equal sign, and so forth) */
	TC_C3_ALPHA         = 0x8000, /* All linguistic characters
	                                 (alphabetical, syllabary, and ideographic) */
	TC_C3_HIGHSURROGATE = 0x0800, /* Windows Vista and later: High surrogate code unit */
	TC_C3_LOWSURROGATE  = 0x1000, /* Windows Vista and later: Low surrogate code unit */
};


static inline enum tchar_ctype1 tctype1(tchar_t c)
{
	return tchar_c1table_base_data[c];
}

static inline enum tchar_ctype3 tctype3(tchar_t c)
{
	return tchar_c3table_base_data[c];
}

static inline bool tisdigit(tchar_t c)
{
	return !!(tchar_c1table_base_data[c] & TC_C1_DIGIT);
}
static inline bool tishiragana(tchar_t c)
{
	return !!(tchar_c3table_base_data[c] & TC_C3_HIRAGANA);
}
static inline bool tiskatakana(tchar_t c)
{
	return !!(tchar_c3table_base_data[c] & TC_C3_KATAKANA);
}
static inline bool tiskanji(tchar_t c)
{
	return !!(tchar_c3table_base_data[c] & TC_C3_IDEOGRAPH);
}

static inline bool tischaracter(tchar_t c)
{
	enum tchar_ctype3 c_type = tchar_c3table_base_data[c];
	return   (c_type & TC_C3_ALPHA) ||
			 (((c_type & TC_C3_KATAKANA) || (c_type & TC_C3_HIRAGANA)) && !(c_type & TC_C3_SYMBOL)) ||
			   (c_type & TC_C3_IDEOGRAPH) ||
			    tisdigit(c);
}

LIB_TCHAR_EXTRN(void tistypemix(const tchar_t *s, size_t len, bool *word, bool *digit, bool *mix));

#endif
