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
#include "astate.h"
#include "aload.h"

typedef struct {
	alo_State T;
	int debug;
	alo_Writer writer;
	void* context;
	jmp_buf* jmp;
} O;

static void savem(O* out, const void* mem, size_t len) {
	if (len > 0) {
		int status = out->writer(out->T, out->context, mem, len);
		if (status) {
			longjmp(*out->jmp, status);
		}
	}
}

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

static void saves(O* out, alo_Str* s) {
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

static void savestr(O* out, alo_Str* s) {
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

static void saveversion(O* out, const aver_t* version) {
	savek(out, aloE_byte(version->major * 16 + version->minor));
}

static void saveconsts(O* out, const alo_Proto* p) {
	size_t n = p->nconst;
	saver(out, n);
	for (size_t i = 0; i < n; ++i) {
		const alo_TVal* t = p->consts + i;
		switch (ttpnv(t)) {
		case ALO_TNIL:
			saveu8(out, CT_NIL);
			break;
		case ALO_TBOOL:
			saveu8(out, tasbool(t) ? CT_TRUE : CT_FALSE);
			break;
		case ALO_TINT:
			saveu8(out, CT_INT);
			savek(out, tasint(t));
			break;
		case ALO_TFLOAT:
			saveu8(out, CT_FLT);
			savek(out, tasflt(t));
			break;
		case ALO_TSTR:
			savestr(out, tasstr(t));
			break;
		default:
			aloE_xassert(false);
		}
	}
}

static void savecaptures(O* out, const alo_Proto* p) {
	saveu16(out, p->ncap);
	for (size_t i = 0; i < p->ncap; ++i) {
		alo_CapInfo* info = p->captures + i;
		saver(out, info->index);
		saveu8(out, info->finstack);
	}
}

static void savefun(O*, const alo_Proto*, alo_Str*);

static void savechildren(O* out, const alo_Proto* p) {
	saver(out, p->nchild);
	for (int i = 0; i < p->nchild; ++i) {
		savefun(out, p->children[i], p->src);
	}
}

static void savedebug(O* out, const alo_Proto* p) {
	saver(out, p->linefdef);
	saver(out, p->lineldef);
	int n;
	n = out->debug ? p->nlineinfo : 0;
	saver(out, n);
	for (int i = 0; i < n; ++i) {
		saver(out, p->lineinfo[i].begin);
		saver(out, p->lineinfo[i].line);
	}
	n = out->debug ? p->nlocvar : 0;
	saver(out, n);
	for (int i = 0; i < n; ++i) {
		saves(out, p->locvars[i].name);
		saver(out, p->locvars[i].start);
		saver(out, p->locvars[i].end);
		saver(out, p->locvars[i].index);
	}
	n = out->debug ? p->ncap : 0;
	saver(out, n);
	for (int i = 0; i < n; ++i) {
		saves(out, p->captures[i].name);
	}
}

static void savefun(O* out, const alo_Proto* p, alo_Str* psrc) {
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
	saveversion(out, out->T->g->version);
	saveu8(out, ALOZ_FORMAT);
	savel(out, ALOZ_DATA);
	savek(out, ALOZ_INT);
	savek(out, ALOZ_FLT);
}

/**
 ** save chunk into binary file.
 */
int aloZ_save(alo_State T, const alo_Proto* p, alo_Writer writer, void* context, int debug) {
	jmp_buf jmp;
	O out = { T, debug, writer, context, &jmp };
	int status = setjmp(jmp);
	if (status == 0) {
		saveheader(&out);
		savefun(&out, p, NULL);
	}
	return status;
}
