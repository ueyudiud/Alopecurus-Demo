/*
 * aobj.c
 *
 *  Created on: 2019年7月21日
 *      Author: ueyudiud
 */

#define AOBJ_C_
#define ALO_CORE

#include "achr.h"
#include "aobj.h"
#include "astr.h"
#include "atup.h"
#include "alis.h"
#include "atab.h"
#include "ameta.h"
#include "ado.h"
#include "adebug.h"
#include "alo.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

ALO_VDEF const atval_t aloO_nil = tnewnil();

#define ALOO_FLT_MASK 0x932280AB7E9DBE8A

ahash_t aloO_flthash(afloat f) {
	ahash_t* hash = aloE_cast(ahash_t*, &f);
	return *hash ^ ALOO_FLT_MASK;
}

static int str2int(astr in, aint* out) {
	char* s;
	aint v = strtoll(in, &s, 0);
	if (!errno && s && s != in && *s == '\0') {
		*out = v;
		return true;
	}
	return false;
}

static int str2flt(astr in, afloat* out) {
	char* s;
	afloat v = strtod(in, &s);
	if (!errno && s && s != in && *s == '\0') {
		*out = v;
		return true;
	}
	return false;
}

int aloO_str2int(astr in, atval_t* out) {
	aint i;
	if (str2int(in, &i)) {
		tsetint(out, i);
		return true;
	}
	return false;
}

int aloO_str2num(astr in, atval_t* out) {
	aint i; afloat f;
	if (str2int(in, &i)) { /* try to cast to integer. */
		tsetint(out, i);
		return true;
	}
	if (str2flt(in, &f)) { /* try to cast to float. */
		tsetflt(out, f);
		return true;
	}
	return false;
}

int aloO_flt2int(afloat in, aint* out, int mode) {
	afloat x = floor(in);
	if (x != in) {
		if (mode > 0)
			x += 1; /* in ceiling mode */
		else if (mode == 0)
			return false; /* in strict mode */
	}
	if (x < AINT_MIN || x > AINT_MAX) {
		return false;
	}
	if (out) {
		*out = x;
	}
	return true;
}

int alo_format(astate T, awriter writer, void* context, astr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	int r = alo_vformat(T, writer, context, fmt, varg);
	va_end(varg);
	return r;
}

/**
 ** write string to writer with interrupt controlling.
 */
#define write(T,w,c,s,l) { int $state; if ((($state) = (w)(T, c, s, l))) return $state; }

#define UTF8BUFSIZE 8

int alo_vformat(astate T, awriter writer, void* context, astr fmt, va_list varg) {
	char buf[ALO_SPRIBUFLEN];
	astr p = fmt, q;
	while ((q = strchr(p, '%'))) {
		write(T, writer, context, p, q - p);
		switch (q[1]) {
		case 's': { /* a string value */
			astr s = va_arg(varg, astr);
			write(T, writer, context, s, strlen(s));
			break;
		}
		case 'q': { /* a char sequence */
			const char* s = va_arg(varg, char*);
			size_t l = va_arg(varg, size_t);
			write(T, writer, context, s, l);
			break;
		}
		case '"': { /* a escaped string */
			const char* s = va_arg(varg, astr);
			size_t l = va_arg(varg, size_t);
			int state = aloO_escape(T, writer, context, s, l);
			if (state) return state;
			break;
		}
		case 'c': { /* a character in integer */
			char ch = aloE_cast(char, va_arg(varg, int));
			write(T, writer, context, &ch, 1);
			break;
		}
		case 'd': { /* an integer value */
			int i = va_arg(varg, int);
			int len = sprintf(buf, "%d", i);
			write(T, writer, context, buf, len);
			break;
		}
		case 'i': { /* an alo_Integer value */
			aint i = va_arg(varg, aint);
			int len = ALO_SPRIINT(buf, i);
			write(T, writer, context, buf, len);
			break;
		}
		case 'f': { /* an alo_Float value */
			afloat i = va_arg(varg, afloat);
			int len = ALO_SPRIFLT(buf, i);
			write(T, writer, context, buf, len);
			break;
		}
		case 'p': { /* a pointer value */
			void* p = va_arg(varg, void*);
			int len = sprintf(buf, "%p", p);
			write(T, writer, context, buf, len);
			break;
		}
		case '%': {
			static const char ch = '%';
			write(T, writer, context, &ch, 1);
			break;
		}
		case 'u': {
			char buf[UTF8BUFSIZE];
			char* p = buf + (UTF8BUFSIZE - 1); /* get end of buffer */
			uint32_t ch = va_arg(varg, uint32_t); /* in UTF-32 */
			if (ch < 0x80) { /* ASCii */
				*p = aloE_byte(ch);
			}
			else { /* need continuation bytes */
				abyte mfb = 0x3f; /* first byte mask */
				do {
					*(p--) = 0x80 | (ch & 0x3F);
					ch >>= 6;
					mfb >>= 1;
				}
				while (ch > mfb);
				*p = (~mfb << 1 & 0xff) | (ch & mfb); /* get first byte */
			}
			write(T, writer, context, p, buf + UTF8BUFSIZE - p);
			break;
		}
		default: {
			aloU_rterror(T, "invalid format option, got '%%%c'", q[1]);
		}
		}
		p = q + 2;
	}
	if (*p) {
		write(T, writer, context, p, strlen(p));
	}
	return 0;
}

int aloO_tostring(astate T, awriter writer, void* context, const atval_t* o) {
	switch (ttype(o)) {
	case ALO_TBOOL:
		return alo_format(T, writer, context, "%s", tgetbool(o) ? "true" : "false");
	case ALO_TINT:
		return alo_format(T, writer, context, "%i", tgetint(o));
	case ALO_TFLOAT:
		return alo_format(T, writer, context, "%f", tgetflt(o));
	case ALO_TISTRING: {
		astring_t* s = tgetstr(o);
		write(T, writer, context, s->array, s->shtlen);
		break;
	}
	case ALO_THSTRING: {
		astring_t* s = tgetstr(o);
		write(T, writer, context, s->array, s->lnglen);
		break;
	}
	default: {
		if (aloT_tryunr(T, o, TM_TOSTR)) {
			if (!ttisstr(T->top)) {
				aloU_rterror(T, "'__tostr' must return string value");
			}
			astring_t* s = tgetstr(T->top);
			write(T, writer, context, s->array, aloS_len(s));
		}
		else {
			aloU_rterror(T, "fail to transform object to string.");
		}
		break;
	}
	}
	return 0;
}

#define MAX_ESCAPE_LEN 4

static const char xdigits[] = "0123456789ABCDEF";

static size_t efill(char* out, int ch) {
	switch (ch) {
	case '\a': out[0] = '\\'; out[1] = 'a'; return 2;
	case '\b': out[0] = '\\'; out[1] = 'b'; return 2;
	case '\f': out[0] = '\\'; out[1] = 'f'; return 2;
	case '\n': out[0] = '\\'; out[1] = 'n'; return 2;
	case '\r': out[0] = '\\'; out[1] = 'r'; return 2;
	case '\t': out[0] = '\\'; out[1] = 't'; return 2;
	case '\'': out[0] = '\\'; out[1] = '\''; return 2;
	case '\"': out[0] = '\\'; out[1] = '\"'; return 2;
	case '\\': out[0] = '\\'; out[1] = '\\'; return 2;
	default:
		if (!aisprint(ch)) {
			if (ch < 8) {
				out[0] = '\\';
				out[1] = '0' + ch;
				return 2;
			}
			else if (ch < 64) {
				out[0] = '\\';
				out[1] = '0' + (ch >> 3 & 0x7);
				out[2] = '0' + (ch      & 0x7);
				return 3;
			}
			else {
				out[0] = '\\';
				out[1] = 'x';
				out[2] = xdigits[ch >> 4 & 0xF];
				out[3] = xdigits[ch & 0xF];
				return 4;
			}
		}
		break;
	}
	out[0] = ch;
	return 1;
}

int aloO_escape(astate T, awriter writer, void* context, const char* src, size_t len) {
	const char* const end = src + len;
	char buf[256];
	char* const btp = buf + (256 - MAX_ESCAPE_LEN); /* buffer top limit */
	while (src != end) {
		char* p = buf;
		while (p <= btp) {
			p += efill(p, aloE_byte(*(src++))); /* fill to buffer */
			if (src == end) {
				break;
			}
		}
		write(T, writer, context, buf, p - buf);
	}
	return 0;
}

const atval_t* aloO_get(astate T, const atval_t* o, const atval_t* k) {
	switch (ttpnv(o)) {
	case ALO_TTUPLE:
		return aloA_get(T, tgettup(o), k);
	case ALO_TLIST:
		return aloI_get(T, tgetlis(o), k);
	case ALO_TTABLE:
		return aloH_get(T, tgettab(o), k);
	default:
		aloi_check(T, false, "illegal owner for 'get'");
		return aloO_tnil;
	}
}
