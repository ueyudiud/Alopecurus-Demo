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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WRITE_ERROR_LEVEL 3

#define NATIVE_SOURCE "<native>"

#define fnname(frame) ((frame)->name ?: "<unknown>")

#if defined(ALO_DEBUG)

#ifndef alo_assert
void alo_assert(astr msg, astr file, int line) {
	fprintf(stderr, "%s:%d %s\n", file, line, msg);
	exit(EXIT_FAILURE);
}
#endif

#ifndef alo_log
void alo_log(astr msg, astr file, int line, ...) {
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

void aloU_init(astate T) {
	Gd(T);
	actable_t* table = &G->ctable;
	aloM_newb(T, table->array, table->capacity, ALO_CTABLE_INITIAL_CAPACITY);
	for (size_t i = 0; i < ALO_CTABLE_INITIAL_CAPACITY; ++i) {
		table->array[i].handle = NULL;
	}
	table->length = 0;
}

static int putcreg(actable_t* table, acfun handle, astr name) {
	size_t hash = aloE_addr(handle) % table->capacity; /* first hash */
	size_t hash0 = hash;
	int i = 1;
	while (table->array[hash].handle) {
		api_check(T, table->array[hash].handle != handle, "C function registered twice.");
		hash = (hash + i * i) % table->capacity; /* rehash */
		i += 1;
		if (hash == hash0) { /* collide to first hash? */
			return true;
		}
	}
	table->array[hash].handle = handle;
	table->array[hash].name = name;
	return false;
}

static astr getcreg(actable_t* table, acfun handle) {
	size_t hash = aloE_addr(handle) % table->capacity; /* first hash */
	size_t i = 0;
	acreg_t* creg;
	while ((creg = table->array + hash)->handle) {
		if (creg->handle == handle) {
			return creg->name;
		}
		hash = (hash * hash + ++i) % table->capacity; /* rehash */
	}
	return "<unknown>";
}

astr aloU_getcname(astate T, acfun handle) {
	Gd(T);
	return getcreg(&G->ctable, handle);
}

static void growbuf(astate T, actable_t* table) {
	acreg_t* oldarray = table->array;
	size_t oldcap = table->capacity;
	size_t newcap = oldcap * 2;
	table->array = aloM_newa(T, acreg_t, newcap);
	table->capacity = newcap;
	for (size_t i = 0; i < newcap; ++i) {
		table->array[i].handle = NULL;
	}
	for (size_t i = 0; i < oldcap; ++i) {
		if (putcreg(table, oldarray[i].handle, oldarray[i].name)) {
			aloU_rterror(T, "failed to put entry into buffer.");
		}
	}
	aloM_dela(T, oldarray, oldcap);

}

void aloU_bind(astate T, acfun handle, astring_t* name) {
	Gd(T);
	actable_t* table = &G->ctable;
	if (table->length >= table->capacity * ALO_CTABLE_LOAD_FACTOR) { /* needs to grow size? */
		growbuf(T, table);
	}
	if (putcreg(table, handle, name->array)) { /* place new entry failed? */
		growbuf(T, table); /* grow buffer size again */
		if (putcreg(table, handle, name->array)) { /* failed again? */
			aloU_rterror(T, "cannot put C function '%s'", name->array);
		}
	}
	table->length++;
}

void aloU_clearbuf(astate T) {
	Gd(T);
	actable_t* table = &G->ctable;
	aloM_delb(T, table->array, table->capacity);
}

alineinfo_t* aloU_lineof(aproto_t* proto, const ainsn_t* pc) {
	alineinfo_t* ls = proto->lineinfo;
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

#define lineof(p,i) ({ alineinfo_t* _info = aloU_lineof(p,i); _info ? _info->line : 0; })

static void write_stacktrace(astate T, ambuf_t* buf, int level) {
	aframe_t* frame = T->frame;
	do {
		astr name = fnname(frame);
		astr src;
		int line;
		if (frame->falo) {
			aproto_t* p = tgetclo(frame->fun)->a.proto;
			src = p->src->array;
			line = lineof(p, frame->a.pc);
		}
		else {
			src = NATIVE_SOURCE;
			line = 0;
		}
		if (line > 0) {
			alo_format(T, aloB_bwrite, buf, "\nat %s (%s:%d)", name, src, line);
		}
		else {
			alo_format(T, aloB_bwrite, buf, "\nat %s (%s)", name, src);
		}
	}
	while ((frame = frame->prev) && --level > 0);
}

anoret aloU_mnotfound(astate T, const atval_t* owner, astr fun) {
	aloU_rterror(T, "method '%s.__%s' not found", aloV_typename(T, owner), fun);
}

static astring_t* aloU_pushvmsg(astate T, astr fmt, va_list varg) {
	aloB_decl(T, buf);
	alo_vformat(T, aloB_bwrite, &buf, fmt, varg);
	write_stacktrace(T, &buf, WRITE_ERROR_LEVEL);
	astring_t* value = aloB_tostr(T, buf);
	aloB_close(T, buf);
	tsetstr(T, T->top++, value); /* push error message to stack. */
	return value;
}

static anoret aloU_vrterror(astate T, astr fmt, va_list varg) {
	aloU_pushvmsg(T, fmt, varg);
	aloD_throw(T, ThreadStateErrRuntime);
}

/**
 ** push error message and stack trace information into top of stack.
 */
astring_t* aloU_pushmsg(astate T, astr fmt, ...) {
	aloG_check(T);
	va_list varg;
	va_start(varg, fmt);
	astring_t* value = aloU_pushvmsg(T, fmt, varg);
	va_end(varg);
	return value;
}

/**
 ** throw an runtime error.
 */
anoret aloU_rterror(astate T, astr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	aloU_vrterror(T, fmt, varg);
}

static void getframeinfo(aframe_t* frame, astr what, aframeinfo_t* info) {
	info->_frame = frame;
	if (strchr(what, 'n')) {
		info->name = fnname(frame);
		info->kind =
				frame->prev == NULL ? "main" :
				frame->mode == FrameModeTail ? "tail" :
						frame->falo ? "alo" : "C";
	}
	if (strchr(what, 's')) {
		if (frame->falo) {
			aproto_t* proto = tgetclo(frame->fun)->a.proto;
			info->src = proto->src->array ?: NATIVE_SOURCE;
			info->linefdef = proto->linefdef;
			info->lineldef = proto->lineldef;
		}
		else {
			info->src = NATIVE_SOURCE;
			info->linefdef = info->lineldef = -1;
		}
	}
	if (strchr(what, 'a')) {
		if (frame->falo) {
			aproto_t* proto = tgetclo(frame->fun)->a.proto;
			info->nargument = proto->nargs;
			info->ncapture = proto->ncap;
			info->vararg = proto->fvararg;
		}
		else {
			info->nargument = 0;
			info->ncapture = ttisccl(frame->fun) ? tgetclo(frame->fun)->length : 0;
			info->vararg = true;
		}
	}
	if (strchr(what, 'l')) {
		if (frame->falo) {
			aproto_t* proto = tgetclo(frame->fun)->a.proto;
			info->line = lineof(proto, frame->a.pc);
		}
		else {
			info->line = -1;
		}
	}
}

void alo_getframe(astate T, astr what, aframeinfo_t* info) {
	getframeinfo(T->frame, what, info);
}

int alo_prevframe(__attribute__((unused)) astate T, astr what, aframeinfo_t* info) {
	aframe_t* frame = aloE_cast(aframe_t*, info->_frame);
	if (frame->prev == NULL) {
		return false;
	}
	getframeinfo(frame->prev, what, info);
	return true;
}

void alo_sethook(astate T, ahfun fun, int mask, __attribute__((unused)) int count) {
	if (fun == NULL || mask == 0) {
		fun = NULL;
		mask = 0;
	}
	T->hook = fun;
	T->hookmask = aloE_byte(mask);
}

ahfun alo_gethook(astate T) {
	return T->hook;
}

int alo_gethookmask(astate T) {
	return T->hookmask;
}
