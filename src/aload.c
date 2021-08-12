/*
 * aload.c
 *
 * load prototype from binary data.
 *
 *  Created on: 2019年9月3日
 *      Author: ueyudiud
 */

#define ALOAD_C_
#define ALO_CORE

#include "astr.h"
#include "alis.h"
#include "afun.h"
#include "astate.h"
#include "agc.h"
#include "avm.h"
#include "ado.h"
#include "aload.h"
#include "alo.h"

#include <string.h>

typedef struct {
	alo_State T;
	a_cstr src;
	aibuf_t buf;
} I;

typedef struct {
	I in;
	alo_Proto* p;
} PContext;

void aloZ_delete(alo_State T, alo_Proto* p) {
	if (p == NULL) {
		return;
	}
	for (int i = 0; i < p->nchild; ++i) {
		aloZ_delete(T, p->children[i]);
	}
	aloF_deletep(T, p);
}

static void error(I* in, a_cstr msg) {
	aloV_pushfstring(in->T, "%s: %s precompiled chunk", in->src, msg);
	aloD_throw(in->T, ALO_STERRCOM);
}

#define loadb(in,b,s) ({ if (aloB_iread((in)->T, &(in)->buf, b, s)) error(in, "truncated"); })
#define loada(in,a,l) loadb(in, a, (l) * sizeof((a)[0]))
#define initl(in,a,l) (a = aloM_newa((in)->T, (a)[0], l))
#define loadl(in,a,l) ({ size_t _len = (l); initl(in, a, _len); loada(in, a, _len); })
#define loadr(in,r) loadb(in, &(r), sizeof(r))
#define loadt(in,t) ({ t _block; loadr(in, _block); _block; })
#define checkl(in,l,msg) checkl_(in, l, sizeof(l) - sizeof(char), msg)

static alo_Str* bind(I* in, alo_Str* s) {
	alo_State T = in->T;
	alo_List* list = taslis(T->top - 1);
	tsetstr(T, T->top++, s); /* push string into stack to avoid GC */
	aloI_ensure(T, list, 1);
	tsetstr(T, list->array + list->length++, s);
	T->top--; /* pop stack */
	return s;
}

static uint8_t loadu8(I* in) {
	int ch = aloB_iget((in)->T, &(in)->buf);
	if (ch == EOB)
		error(in, "truncated");
	return ch;
}

static alo_Str* loads(I* in) {
	int c = loadu8(in);
	if (c == 0) {
		return NULL;
	}
	else {
		size_t len;
		if (c < UINT8_MAX) {
			len = c;
		}
		else {
			loadr(in, len);
		}
		alo_Str* s;
		if (--len <= ALO_MAXISTRLEN) {
			char buf[len];
			loada(in, buf, len);
			s = aloS_new(in->T, buf, len);
		}
		else {
			s = aloS_createlng(in->T, len);
			loada(in, s->array, len);
		}
		return bind(in, s);
	}
}

static void loadconsts(I* in, alo_Proto* p) {
	size_t n;
	loadr(in, n);
	initl(in, p->consts, n);
	p->nconst = n;
	for (size_t i = 0; i < n; ++i) {
		alo_TVal* t = p->consts + i;
		switch (loadu8(in)) {
		case CT_NIL:
			tsetnil(t);
			break;
		case CT_FALSE:
			tsetbool(t, false);
			break;
		case CT_TRUE:
			tsetbool(t, true);
			break;
		case CT_INT:
			tsetint(t, loadt(in, a_int));
			break;
		case CT_FLT:
			tsetflt(t, loadt(in, a_float));
			break;
		case CT_XSTR:
			tsetstr(in->T, t, in->T->g->sempty);
			break;
		case CT_SSTR: {
			int len = loadu8(in) + 1;
			if (len < ALO_MAXISTRLEN) {
				char buf[len];
				loada(in, buf, len);
				tsetstr(in->T, t, bind(in, aloS_new(in->T, buf, len)));
				break;
			}
			goto hstr;
		case CT_LSTR:
			len = loadt(in, size_t);
			hstr: {
				alo_Str* s = aloS_createlng(in->T, len);
				loada(in, s->array, len);
				tsetstr(in->T, t, s);
			}
			break;
		}
		default:
			error(in, "unknown constant type in");
		}
	}

}

static void loadcaptures(I* in, alo_Proto* p) {
	uint16_t n = loadt(in, uint16_t);
	initl(in, p->captures, n);
	p->ncap = n;
	for (int i = 0; i < n; ++i) {
		alo_CapInfo* info = p->captures + i;
		loadr(in, info->index);
		info->finstack = loadu8(in);
	}
}

static void loadfun(I*, alo_Proto**, alo_Str*);

static void loadchildren(I* in, alo_Proto* p) {
	size_t n;
	loadr(in, n);
	initl(in, p->children, n);
	p->nchild = n;
	/* initialize children array */
	for (size_t i = 0; i < n; p->children[i++] = NULL);
	for (size_t i = 0; i < n; ++i) {
		loadfun(in, p->children + i, p->src);
	}
}

static void loaddebug(I* in, alo_Proto* p) {
	loadr(in, p->linefdef);
	loadr(in, p->lineldef);
	size_t n;
	loadr(in, n);
	initl(in, p->lineinfo, n);
	p->nlineinfo = n;
	for (size_t i = 0; i < n; ++i) {
		loadr(in, p->lineinfo[i].begin);
		loadr(in, p->lineinfo[i].line);
	}
	loadr(in, n);
	initl(in, p->locvars, n);
	p->nlocvar = n;
	for (size_t i = 0; i < n; ++i) {
		p->locvars[i].name = loads(in);
		loadr(in, p->locvars[i].start);
		loadr(in, p->locvars[i].end);
		loadr(in, p->locvars[i].index);
	}
	loadr(in, n); /* capture array has already loaded */
	for (size_t i = 0; i < n; ++i) {
		p->captures[i].name = loads(in);
	}
}

static void loadfun(I* in, alo_Proto** slot, alo_Str* src) {
	alo_Proto* p = *slot = aloF_newp(in->T);
	p->name = loads(in);
	/* load parameter information */
	loadr(in, p->nargs);
	loadr(in, p->flags);
	loadr(in, p->nstack);
	/* load codes */
	loadr(in, p->ncode);
	loadl(in, p->code, p->ncode);
	/* load constants */
	loadconsts(in, p);
	/* load captures */
	loadcaptures(in, p);
	/* load children */
	loadchildren(in, p);
	/* load debug information */
	p->src = loads(in) ?: src; /* load source file */
	loaddebug(in, p);
}

static void checkl_(I* in, const void* src, size_t len, a_cstr msg) {
	char buf[len];
	loadb(in, buf, len);
	if (memcmp(buf, src, len) != 0) {
		error(in, msg);
	}
}

static void checkversion(I* in) {
	a_byte version = loadu8(in);
	a_byte major = version / 16;
	a_byte minor = version % 16;
	const aver_t* current = in->T->g->version;
	if (major != current->major || minor < current->minor) {
		error(in, "version mismatch in");
	}
}

static void checkhead(I* in) {
	checkl(in, ALOZ_SIGNATURE, "not a");
	checkversion(in);
	if (loadu8(in) != ALOZ_FORMAT) {
		error(in, "format mismatch in");
	}
	checkl(in, ALOZ_DATA, "corrupted");
	if (loadt(in, a_int) != ALOZ_INT) {
		error(in, "endianness mismatch in");
	}
	if (loadt(in, a_float) != ALOZ_FLT) {
		error(in, "float format mismatch in");
	}
}

static void pload(alo_State T, void* rcontext) {
	PContext* context = aloE_cast(PContext*, rcontext);
	checkhead(&context->in);
	loadfun(&context->in, &context->p, bind(&context->in, aloS_of(T, context->in.src)));
}

/**
 ** load chunk from binary file.
 */
int aloZ_load(alo_State T, alo_Proto** p, a_cstr src, alo_Reader reader, void* context) {
	PContext c = { { T, src, { NULL, 0, reader, context } }, NULL };
	tsetlis(T, T->top, aloI_new(T)); /* create reference for avoiding GC */
	T->top ++;
	int status = aloD_prun(T, pload, &c);
	if (status == ALO_STOK) {
		*p = c.p;
		aloG_register(T, c.p, ALO_TPROTO); /* register prototype */
	}
	else {
		aloZ_delete(T, c.p);
		*p = NULL;
	}
	T->top--;
	return status;
}
