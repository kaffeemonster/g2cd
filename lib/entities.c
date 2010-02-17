/*
 * I was lazy, thanks go out to a Christoph
 */
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "my_bitops.h"

#define UNICODE_MAX 0x10FFFFul

struct utf8_et
{
	const char c[5];
	unsigned char len;
};

struct entity_table
{
	const char abbrev[10];
	unsigned char len;
	struct utf8_et utf;
};

#ifndef str_size
# define str_size(x) (sizeof(x) - 1)
#endif

#define ETE(x, y) { x, str_size(x), { y, str_size(y) } }
static const struct entity_table named_entities[] =
{
	ETE("AElig;",   "Æ"), ETE("Aacute;",  "Á"), ETE("Acirc;",   "Â"), ETE("Agrave;",  "À"),
	ETE("Alpha;",   "Α"), ETE("Aring;",   "Å"), ETE("Atilde;",  "Ã"), ETE("Auml;",    "Ä"),
	ETE("Beta;",    "Β"), ETE("Ccedil;",  "Ç"), ETE("Chi;",     "Χ"), ETE("Dagger;",  "‡"),
	ETE("Delta;",   "Δ"), ETE("ETH;",     "Ð"), ETE("Eacute;",  "É"), ETE("Ecirc;",   "Ê"),
	ETE("Egrave;",  "È"), ETE("Epsilon;", "Ε"), ETE("Eta;",     "Η"), ETE("Euml;",    "Ë"),
	ETE("Gamma;",   "Γ"), ETE("Iacute;",  "Í"), ETE("Icirc;",   "Î"), ETE("Igrave;",  "Ì"),
	ETE("Iota;",    "Ι"), ETE("Iuml;",    "Ï"), ETE("Kappa;",   "Κ"), ETE("Lambda;",  "Λ"),
	ETE("Mu;",      "Μ"), ETE("Ntilde;",  "Ñ"), ETE("Nu;",      "Ν"), ETE("OElig;",   "Œ"),
	ETE("Oacute;",  "Ó"), ETE("Ocirc;",   "Ô"), ETE("Ograve;",  "Ò"), ETE("Omega;",   "Ω"),
	ETE("Omicron;", "Ο"), ETE("Oslash;",  "Ø"), ETE("Otilde;",  "Õ"), ETE("Ouml;",    "Ö"),
	ETE("Phi;",     "Φ"), ETE("Pi;",      "Π"), ETE("Prime;",   "″"), ETE("Psi;",     "Ψ"),
	ETE("Rho;",     "Ρ"), ETE("Scaron;",  "Š"), ETE("Sigma;",   "Σ"), ETE("THORN;",   "Þ"),
	ETE("Tau;",     "Τ"), ETE("Theta;",   "Θ"), ETE("Uacute;",  "Ú"), ETE("Ucirc;",   "Û"),
	ETE("Ugrave;",  "Ù"), ETE("Upsilon;", "Υ"), ETE("Uuml;",    "Ü"), ETE("Xi;",      "Ξ"),
	ETE("Yacute;",  "Ý"), ETE("Yuml;",    "Ÿ"), ETE("Zeta;",    "Ζ"), ETE("aacute;",  "á"),
	ETE("acirc;",   "â"), ETE("acute;",   "´"), ETE("aelig;",   "æ"), ETE("agrave;",  "à"),
	ETE("alefsym;", "ℵ"), ETE("alpha;",   "α"), ETE("amp;",     "&"), ETE("and;",     "∧"),
	ETE("ang;",     "∠"), ETE("apos;",    "'"), ETE("aring;",   "å"), ETE("asymp;",   "≈"),
	ETE("atilde;",  "ã"), ETE("auml;",    "ä"), ETE("bdquo;",   "„"), ETE("beta;",    "β"),
	ETE("brvbar;",  "¦"), ETE("bull;",    "•"), ETE("cap;",     "∩"), ETE("ccedil;",  "ç"),
	ETE("cedil;",   "¸"), ETE("cent;",    "¢"), ETE("chi;",     "χ"), ETE("circ;",    "ˆ"),
	ETE("clubs;",   "♣"), ETE("cong;",    "≅"), ETE("copy;",    "©"), ETE("crarr;",   "↵"),
	ETE("cup;",     "∪"), ETE("curren;",  "¤"), ETE("dArr;",    "⇓"), ETE("dagger;",  "†"),
	ETE("darr;",    "↓"), ETE("deg;",     "°"), ETE("delta;",   "δ"), ETE("diams;",   "♦"),
	ETE("divide;",  "÷"), ETE("eacute;",  "é"), ETE("ecirc;",   "ê"), ETE("egrave;",  "è"),
	ETE("empty;",   "∅"), ETE("emsp;",    " "), ETE("ensp;",    " "), ETE("epsilon;", "ε"),
	ETE("equiv;",   "≡"), ETE("eta;",     "η"), ETE("eth;",     "ð"), ETE("euml;",    "ë"),
	ETE("euro;",    "€"), ETE("exist;",   "∃"), ETE("fnof;",    "ƒ"), ETE("forall;",  "∀"),
	ETE("frac12;",  "½"), ETE("frac14;",  "¼"), ETE("frac34;",  "¾"), ETE("frasl;",   "⁄"),
	ETE("gamma;",   "γ"), ETE("ge;",      "≥"), ETE("gt;",      ">"), ETE("hArr;",    "⇔"),
	ETE("harr;",    "↔"), ETE("hearts;",  "♥"), ETE("hellip;",  "…"), ETE("iacute;",  "í"),
	ETE("icirc;",   "î"), ETE("iexcl;",   "¡"), ETE("igrave;",  "ì"), ETE("image;",   "ℑ"),
	ETE("infin;",   "∞"), ETE("int;",     "∫"), ETE("iota;",    "ι"), ETE("iquest;",  "¿"),
	ETE("isin;",    "∈"), ETE("iuml;",    "ï"), ETE("kappa;",   "κ"), ETE("lArr;",    "⇐"),
	ETE("lambda;",  "λ"), ETE("lang;",    "〈"), ETE("laquo;",   "«"), ETE("larr;",    "←"),
	ETE("lceil;",   "⌈"), ETE("ldquo;",   "“"), ETE("le;",      "≤"), ETE("lfloor;",  "⌊"),
	ETE("lowast;",  "∗"), ETE("loz;",     "◊"), ETE("lrm;",     "\xE2\x80\x8E"), ETE("lsaquo;",  "‹"),
	ETE("lsquo;",   "‘"), ETE("lt;",      "<"), ETE("macr;",    "¯"), ETE("mdash;",   "—"),
	ETE("micro;",   "µ"), ETE("middot;",  "·"), ETE("minus;",   "−"), ETE("mu;",      "μ"),
	ETE("nabla;",   "∇"), ETE("nbsp;",    " "), ETE("ndash;",   "–"), ETE("ne;",      "≠"),
	ETE("ni;",      "∋"), ETE("not;",     "¬"), ETE("notin;",   "∉"), ETE("nsub;",    "⊄"),
	ETE("ntilde;",  "ñ"), ETE("nu;",      "ν"), ETE("oacute;",  "ó"), ETE("ocirc;",   "ô"),
	ETE("oelig;",   "œ"), ETE("ograve;",  "ò"), ETE("oline;",   "‾"), ETE("omega;",   "ω"),
	ETE("omicron;", "ο"), ETE("oplus;",   "⊕"), ETE("or;",      "∨"), ETE("ordf;",    "ª"),
	ETE("ordm;",    "º"), ETE("oslash;",  "ø"), ETE("otilde;",  "õ"), ETE("otimes;",  "⊗"),
	ETE("ouml;",    "ö"), ETE("para;",    "¶"), ETE("part;",    "∂"), ETE("permil;",  "‰"),
	ETE("perp;",    "⊥"), ETE("phi;",     "φ"), ETE("pi;",      "π"), ETE("piv;",     "ϖ"),
	ETE("plusmn;",  "±"), ETE("pound;",   "£"), ETE("prime;",   "′"), ETE("prod;",    "∏"),
	ETE("prop;",    "∝"), ETE("psi;",     "ψ"), ETE("quot;",    "\""), ETE("rArr;",    "⇒"),
	ETE("radic;",   "√"), ETE("rang;",    "〉"), ETE("raquo;",   "»"), ETE("rarr;",    "→"),
	ETE("rceil;",   "⌉"), ETE("rdquo;",   "”"), ETE("real;",    "ℜ"), ETE("reg;",     "®"),
	ETE("rfloor;",  "⌋"), ETE("rho;",     "ρ"), ETE("rlm;",     "\xE2\x80\x8F"), ETE("rsaquo;",  "›"),
	ETE("rsquo;",   "’"), ETE("sbquo;",   "‚"), ETE("scaron;",  "š"), ETE("sdot;",    "⋅"),
	ETE("sect;",    "§"), ETE("shy;",     "\xC2\xAD"), ETE("sigma;",   "σ"), ETE("sigmaf;",  "ς"),
	ETE("sim;",     "∼"), ETE("spades;",  "♠"), ETE("sub;",     "⊂"), ETE("sube;",    "⊆"),
	ETE("sum;",     "∑"), ETE("sup;",     "⊃"), ETE("sup1;",    "¹"), ETE("sup2;",    "²"),
	ETE("sup3;",    "³"), ETE("supe;",    "⊇"), ETE("szlig;",   "ß"), ETE("tau;",     "τ"),
	ETE("there4;",  "∴"), ETE("theta;",   "θ"), ETE("thetasym;","ϑ"), ETE("thinsp;",  " "),
	ETE("thorn;",   "þ"), ETE("tilde;",   "˜"), ETE("times;",   "×"), ETE("trade;",   "™"),
	ETE("uArr;",    "⇑"), ETE("uacute;",  "ú"), ETE("uarr;",    "↑"), ETE("ucirc;",   "û"),
	ETE("ugrave;",  "ù"), ETE("uml;",     "¨"), ETE("upsih;",   "ϒ"), ETE("upsilon;", "υ"),
	ETE("uuml;",    "ü"), ETE("weierp;",  "℘"), ETE("xi;",      "ξ"), ETE("yacute;",  "ý"),
	ETE("yen;",     "¥"), ETE("yuml;",    "ÿ"), ETE("zeta;",    "ζ"), ETE("zwj;",     "\xE2\x80\x8D"),
	ETE("zwnj;",    "\xE2\x80\x8C"),
};

struct s_key
{
	const char *s;
	size_t len;
};

static int ete_cmp(const void *key, const void *element)
{
	const struct s_key *s = key;
	const struct entity_table *el = element;
	int result;

	result = memcmp(s->s, el->abbrev, s->len < el->len ? s->len : el->len);
#if 0
	printf("mcp l: %zu res: %i cmp: \"%s\" \"%.*s\"\n", s->len, result, el->abbrev,
	       s->len < el->len ? (int)s->len : (int)el->len, s->s);
#endif

	return result;
}

static const struct utf8_et *get_named_entity(const char *name, size_t len)
{
	struct s_key s;
	const struct entity_table *entity;

	s.s = name;
	s.len = len;
//TODO: binary search, nice and dandy, but still needs 6 tries to find an &amp; ...
	entity = bsearch(&s, named_entities,
		sizeof(named_entities) / sizeof(*named_entities),
		sizeof(*named_entities), ete_cmp);

	return entity ? &entity->utf : NULL;
}

static size_t putc_utf8(unsigned long cp, char *buffer)
{
	unsigned char *bytes = (unsigned char *)buffer;

	if(cp <= 0x007Ful)
	{
		bytes[0] = (unsigned char)cp;
		return 1;
	}

	if(cp <= 0x07FFul)
	{
		bytes[1] = (unsigned char)((2u << 6) | (cp & 0x3Fu));
		bytes[0] = (unsigned char)((6u << 5) | (cp >> 6));
		return 2;
	}

	if(cp <= 0xFFFFul)
	{
		bytes[2] = (unsigned char)(( 2u << 6) | ( cp       & 0x3Fu));
		bytes[1] = (unsigned char)(( 2u << 6) | ((cp >> 6) & 0x3Fu));
		bytes[0] = (unsigned char)((14u << 4) |  (cp >> 12));
		return 3;
	}

	if(cp <= 0x10FFFFul)
	{
		bytes[3] = (unsigned char)(( 2u << 6) | ( cp        & 0x3Fu));
		bytes[2] = (unsigned char)(( 2u << 6) | ((cp >>  6) & 0x3Fu));
		bytes[1] = (unsigned char)(( 2u << 6) | ((cp >> 12) & 0x3Fu));
		bytes[0] = (unsigned char)((30u << 3) |  (cp >> 18));
		return 4;
	}

	return 0;
}

static size_t parse_entity(const char *current, char **to, const char **from, size_t len)
{
	const char *end = my_memchr(current, ';', len);
	size_t remaining;

	if(!end)
		return 0;

	remaining = end - current;
	if(remaining < 3)
		return 0;

	if(current[1] == '#')
	{
		char *tail = NULL;
		bool hex = current[2] == 'x' || current[2] == 'X';
		unsigned long cp;

		if(hex && remaining < 4)
			return 0;
		errno = 0;
// TODO: prevent reading behind end
		cp = strtoul(current + (hex ? 3 : 2), &tail, hex ? 16 : 10);

		if(tail == end && !errno && cp <= UNICODE_MAX)
		{
			*to += putc_utf8(cp, *to);
			remaining = end + 1 - current;
			*from = end + 1;
			return remaining;
		}
	}
	else
	{
		const struct utf8_et *entity = get_named_entity(&current[1], remaining);
		if(entity)
		{
			memcpy(*to, entity->c, entity->len);
			*to += entity->len;
			remaining = end + 1 - current;
			*from = end + 1;
			return remaining;
		}
	}

	return 0;
}

size_t decode_html_entities_utf8(char *dest, const char *src, size_t len)
{
	const char *current, *from = src;
	char *to = dest;
	size_t x, remaining = len;

	while(remaining && (current = my_memchr(from, '&', remaining)))
	{
		my_memcpy(to, from, (size_t)(current - from));
		to += current - from;
		remaining -= current - from;
		if(!remaining)
			break;

		x = parse_entity(current, &to, &from, remaining);
		remaining -= x;
		if(x)
			continue;

		from = current;
		*to++ = *from++;
		remaining--;
	}
	if(remaining)
		my_memcpy(to, from, remaining);
	to += remaining;
	*to = '\0';
	return (size_t)(to - dest);
}
