/*
 * asave.c
 *
 * save prototype to binary data.
 *
 *  Created on: 2019年9月2日
 *      Author: ueyudiud
 */

#define ASAVE_C_
#define ALO_CORE

#include "astr.h"
#include "aload.h"
#include "alo.h"

typedef struct {
	astate T;
	int status;
	awriter writer;
	void* context;
	int debug;
} O;

#define savem(out,m,s) ({ size_t _size = (s); if (!(out)->status && _size > 0) { (out)->status = (out)->writer((out)->T, (out)->context, m, _size); } })
#define savea(out,a,n) savem(out, a, (n) * sizeof((a)[0]))
#define saver(out,r) savem(out, &(r), sizeof(r))
#define savel(out,l) savem(out, ""l, sizeof(l) - sizeof(char))
#define savek(out,k) ({ typeof(k) _const = (k); savem(out, &_const, sizeof(k)); })

static void saveu8(O* out, uint8_t value) {
	saver(out, value);
}

static void saveu16(O* out, uint16_t value) {
	saver(out, value);
}

static void saves(O* out, astring_t* s) {
	if (s == NULL) {
		saveu8(out, 0);
	}
	else {
		size_t len = aloS_len(s) + 1;
		if (len < UINT8_MAX) {
			saveu8(out, len);
		}
		else {
			saveu8(out, UINT8_MAX);
			saver(out, len);
		}
		savea(out, s->array, len - 1);
	}
}

static void savestr(O* out, astring_t* s) {
	size_t n = aloS_len(s);
	if (n == 0) {
		saveu8(out, CT_XSTR);
	}
	else {
		if (n <= UINT8_MAX + 1) {
			saveu8(out, CT_SSTR);
			saveu8(out, n - 1);
		}
		else {
			saveu8(out, CT_LSTR);
			saver(out, n);
		}
		savea(out, s->array, n);
	}
}

static void saveversion(O* out, abyte major, abyte minor) {
	savek(out, aloE_byte(major * 16 + minor));
}

static void saveconsts(O* out, const aproto_t* p) {
	size_t n = p->nconst;
	saver(out, n);
	for (size_t i = 0; i < n; ++i) {
		const atval_t* t = p->consts + i;
		switch (ttpnv(t)) {
		case ALO_TNIL:
			saveu8(out, CT_NIL);
			break;
		case ALO_TBOOL:
			saveu8(out, tgetbool(t) ? CT_TRUE : CT_FALSE);
			break;
		case ALO_TINT:
			saveu8(out, CT_INT);
			savek(out, tgetint(t));
			break;
		case ALO_TFLOAT:
			saveu8(out, CT_FLT);
			savek(out, tgetflt(t));
			break;
		case ALO_TSTRING:
			savestr(out, tgetstr(t));
			break;
		default:
			aloE_xassert(false);
		}
	}
}

static void savecaptures(O* out, const aproto_t* p) {
	saveu16(out, p->ncap);
	for (size_t i = 0; i < p->ncap; ++i) {
		acapinfo_t* info = p->captures + i;
		saver(out, info->index);
		saveu8(out, info->finstack);
	}
}

static void savefun(O*, const aproto_t*, astring_t*);

static void savechildren(O* out, const aproto_t* p) {
	saver(out, p->nchild);
	for (size_t i = 0; i < p->nchild; ++i) {
		savefun(out, p->children[i], p->src);
	}
}

static void savedebug(O* out, const aproto_t* p) {
	saver(out, p->linefdef);
	saver(out, p->lineldef);
	size_t n;
	n = out->debug ? p->nlineinfo : 0;
	saver(out, n);
	for (size_t i = 0; i < n; ++i) {
		saver(out, p->lineinfo[i].begin);
		saver(out, p->lineinfo[i].line);
	}
	n = out->debug ? p->nlocvar : 0;
	saver(out, n);
	for (size_t i = 0; i < n; ++i) {
		saves(out, p->locvars[i].name);
		saver(out, p->locvars[i].start);
		saver(out, p->locvars[i].end);
		saver(out, p->locvars[i].index);
	}
	n = out->debug ? p->ncap : 0;
	saver(out, n);
	for (size_t i = 0; i < n; ++i) {
		saves(out, p->captures[i].name);
	}
}

static void savefun(O* out, const aproto_t* p, astring_t* psrc) {
	saves(out, p->name);
	/* save parameter information */
	saver(out, p->nargs);
	saver(out, p->flags);
	saver(out, p->nstack);
	/* save codes */
	saver(out, p->ncode);
	savea(out, p->code, p->ncode);
	/* save constants */
	saveconsts(out, p);
	/* save captures */
	savecaptures(out, p);
	/* save children */
	savechildren(out, p);
	/* save debug information */
	saves(out, out->debug && p->src != psrc ? p->src : NULL); /* save source file */
	savedebug(out, p);
}

static void saveheader(O* out) {
	savel(out, ALOZ_SIGNATURE);
	saveversion(out, ALO_VERSION_NUM);
	saveu8(out, ALOZ_FORMAT);
	savel(out, ALOZ_DATA);
	savek(out, ALOZ_INT);
	savek(out, ALOZ_FLT);
}

int aloZ_save(astate T, const aproto_t* p, awriter writer, void* context, int debug) {
	O out = { T, 0, writer, context, debug };
	saveheader(&out);
	savefun(&out, p, NULL);
	return out.status;
}

