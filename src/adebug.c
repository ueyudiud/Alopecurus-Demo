/*
 * adebug.c
 *
 *  Created on: 2019年7月25日
 *      Author: ueyudiud
 */

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
void alo_assert(astr msg, astr file, int line) {
	fprintf(stderr, "%s:%d %s", file, line, msg);
	exit(EXIT_FAILURE);
}
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
	size_t i = 0;
	while (table->array[hash].handle) {
		if (table->array[hash].handle == handle) { /* hit? */
			return true;
		}
		hash = (hash * hash + ++i) % table->capacity; /* rehash */
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

void aloU_bind(astate T, acfun handle, astring_t* name) {
	Gd(T);
	actable_t* table = &G->ctable;
	if (table->length >= table->capacity * ALO_CTABLE_LOAD_FACTOR) { /* needs to grow size? */
		acreg_t* oldarray = table->array;
		size_t oldcap = table->capacity;
		size_t newcap = oldcap * 2;
		table->array = aloM_newa(T, acreg_t, newcap);
		table->capacity = newcap;
		for (size_t i = 0; i < newcap; ++i) {
			table->array[i].handle = NULL;
		}
		for (size_t i = 0; i < oldcap; ++i) {
			putcreg(table, oldarray[i].handle, oldarray[i].name);
		}
		aloM_dela(T, oldarray, oldcap);
	}
	if (putcreg(table, handle, name->array)) { /* place new entry */
		aloU_rterror(T, "C function with name '%s' already registered.", name->array);
	}
	table->length++;
}

void aloU_clearbuf(astate T) {
	Gd(T);
	actable_t* table = &G->ctable;
	aloM_delb(T, table->array, table->capacity);
}

static int lineof(alineinfo_t* ls, size_t nl, size_t index) {
	if (ls == NULL)
		return 0; /* no line information provided */
	size_t h = nl - 1, l = 0, m;
	while (l + 1 < h) {
		m = (l + h) / 2;
		if (ls[m].begin > index) {
			h = m - 1;
		}
		else if (ls[m].begin < index) {
			l = m;
		}
		else {
			return ls[m].line;
		}
	}
	return ls[l].line;
}

static void write_stacktrace(astate T, asbuf_t* buf, int level) {
	aframe_t* frame = T->frame;
	do {
		astr name = fnname(frame);
		astr src;
		int line;
		if (frame->falo) {
			aproto_t* p = tgetclo(frame->fun)->a.proto;
			src = p->src->array;
			line = lineof(p->lineinfo, p->nlineinfo, frame->a.pc - p->code);
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

/**
 ** throw an runtime error.
 */
anoret aloU_rterror(astate T, astr fmt, ...) {
	aloG_check(T);
	va_list varg;
	va_start(varg, fmt);

	astring_t* value;

	/**
	 ** builder string in protection.
	 */
	void builder(astate T, asbuf_t* buf) {
		alo_vformat(T, aloB_bwrite, buf, fmt, varg);
		write_stacktrace(T, buf, WRITE_ERROR_LEVEL);
		value = aloS_new(T, buf->array, buf->length);
	}

	aloD_usesb(T, builder);
	tsetstr(T, T->top++, value); /* push error message to stack. */

	va_end(varg);
	aloD_throw(T, ThreadStateErrRuntime);
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
			info->line = lineof(proto->lineinfo, proto->nlineinfo, frame->a.pc - proto->code);
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
