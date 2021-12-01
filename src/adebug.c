/*
 * adebug.c
 *
 *  Created on: 2019年7月25日
 *      Author: ueyudiud
 */

#define ADEBUG_C_
#define ALO_CORE

#include "aop.h"
#include "astr.h"
#include "astate.h"
#include "abuf.h"
#include "agc.h"
#include "adebug.h"
#include "avm.h"
#include "ado.h"
#include "alo.h"

#include <string.h>

#define WRITE_ERROR_LEVEL 3

#define NATIVE_SOURCE "<native>"

#define fnname(frame) ((frame)->name ?: "<unknown>")

#if defined(ALO_DEBUG) || defined(ALOI_DEBUG)

#include <stdio.h>
#include <stdlib.h>

#ifndef alo_assert
void alo_assert(a_cstr msg, a_cstr file, int line) {
	fprintf(stderr, "%s:%d %s\n", file, line, msg);
	exit(EXIT_FAILURE);
}
#endif

#ifndef alo_log
void alo_log(a_cstr msg, a_cstr file, int line, ...) {
	va_list varg;
	char buf[256];
	va_start(varg, line);
	vsnprintf(buf, sizeof(buf) / sizeof(char), msg, varg);
	va_end(varg);
	printf("%s:%d %s\n", file, line, buf);
	fflush(stdout);
}
#endif

#endif

alo_LineInfo* aloU_lineof(alo_Proto* proto, const a_insn* pc) {
	alo_LineInfo* ls = proto->lineinfo;
	if (ls == NULL)
		return NULL; /* no line information provided */
	ptrdiff_t index = pc - proto->code;
	aloE_assert(index >= 0, "illegal code.");
	size_t h = proto->nlineinfo - 1, l = 0, m;
	while (l + 1 < h) {
		m = (l + h) / 2;
		if (ls[m].begin > index) {
			h = m - 1;
		}
		else if (ls[m].begin < index) {
			l = m;
		}
		else {
			return ls + m;
		}
	}
	return ls + l;
}

#define lineof(p,i) ({ alo_LineInfo* _info = aloU_lineof(p,i); _info ? _info->line : 0; })

a_none aloU_error(alo_State T, int status) {
	if (T->errfun) { /* error handling function exists, handle the error */
		alo_StkId fun = putstkoff(T, T->errfun);
		tsetobj(T, T->top, T->top - 1); /* move error message */
		tsetobj(T, T->top - 1, fun); /* move error handling function */
		T->top++;
		aloD_callnoyield(T, T->top - 2, 1);
	}
	aloD_throw(T, status);
}

/**
 ** throw an runtime error.
 */
a_none aloU_rterror(alo_State T, a_cstr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	aloG_check(T); /* GC checking point */
	aloB_decl(T, buf);
	alo_vformat(T, aloB_bwrite, &buf, fmt, varg); /* format error message */
	alo_Str* value = aloB_tostr(T, buf);
	tsetstr(T, T->top++, value); /* push error message to stack. */
	aloU_error(T, ALO_STERRRT);
	/* unreachable code */
	aloB_close(T, buf);
	va_end(varg);
}

/**
 ** throw an error error.
 */
a_none aloU_ererror(alo_State T, a_cstr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	aloB_decl(T, buf);
	alo_vformat(T, aloB_bwrite, &buf, fmt, varg);
	alo_Str* value = aloB_tostr(T, buf);
	tsetstr(T, T->top++, value); /* push error message to stack. */
	aloD_throw(T, ALO_STERRERR);
	/* unreachable code */
	aloB_close(T, buf);
	va_end(varg);
}

#if defined(ALOI_DEBUG)
static int isvalidframe(alo_State T, void* raw) {
	aframe_t* frame;
	for (frame = T->frame; frame; frame = frame->prev) {
		if (frame == raw)
			return true;
	}
	return frame->prev != NULL;
}
#endif

static void getinfo(aframe_t* frame, a_cstr what, alo_Closure* cl, alo_DbgInfo* i) {
	i->_frame = frame;
	for (; *what; ++what) {
		switch (*what) {
		case 'n': /* name information */
			i->name = fnname(frame);
			i->kind =
					frame->prev == NULL ? "main" : /* no function called */
					frame->falo ? "alo" : "C";
			break;
		case 's': /* source information */
			if (cl && tisacl(cl)) {
				alo_Proto* proto = cl->a.proto;
				i->src = proto->src->array ?: NATIVE_SOURCE;
				i->linefdef = proto->linefdef;
				i->lineldef = proto->lineldef;
			}
			else {
				i->src = NATIVE_SOURCE; /* source name is not available for C function. */
				i->linefdef = i->lineldef = -1;
			}
			break;
		case 'a': /* argument information */
			if (cl && tisacl(cl)) {
				alo_Proto* proto = cl->a.proto;
				i->nargument = proto->nargs;
				i->ncapture = proto->ncap;
				i->vararg = proto->fvararg;
			}
			else {
				i->nargument = 0;
				i->ncapture = tisccl(frame->fun) ? tasclo(frame->fun)->length : 0;
				i->vararg = true; /* C function always allows variable arguments */
			}
			break;
		case 'l': /* line information */
			i->line = frame->falo ? lineof(tasclo(frame->fun)->a.proto, frame->a.pc - 1) : -1;
			break;
		case 'c': /* call information */
			i->istailc = frame->mode == FrameModeTail;
			i->isfinc = frame->mode = FrameModeFianlize;
			break;
		}
	}
}

int alo_getinfo(alo_State T, int from, a_cstr what, alo_DbgInfo* info) {
	aframe_t* frame = T->frame;
	alo_TVal* fun;
	switch (from) {
	case ALO_INFPREV: { /* previous frame */
		aloi_check(T, isvalidframe(T, info->_frame), "invalid stack frame.");
		frame = aloE_cast(aframe_t*, info->_frame)->prev;
		if (frame->prev == NULL)
			return false;
		fun = frame->fun;
		break;
	}
	case ALO_INFSTACK: { /* function in stack */
		api_checkelems(T, 1);
		fun = T->frame->top - 1;
		if (!tasclo(fun))
			return false;
		break;
	}
	case ALO_INFCURR: {
		if (frame->prev == NULL)
			return false;
		fun = frame->fun;
		break;
	}
	default: {
		fun = NULL;
		aloi_check(T, false, "invalid 'from' for alo_getinfo.");
	}
	}
	getinfo(frame, what, tisclo(fun) ? tasclo(fun) : NULL, info);
	return true;
}

void alo_sethook(alo_State T, ahfun fun, int mask) {
	if (fun == NULL || mask == 0) {
		fun = NULL;
		mask = 0;
	}
	T->hook = fun;
	T->hookmask = aloE_byte(mask);
}

ahfun alo_gethook(alo_State T) {
	return T->hook;
}

int alo_gethookmask(alo_State T) {
	return T->hookmask;
}
