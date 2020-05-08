/*
 * aimpl.c
 *
 *  Created on: 2019年7月23日
 *      Author: ueyudiud
 */

#define AIMPL_C_
#define ALO_CORE

#include "astr.h"
#include "atup.h"
#include "alis.h"
#include "atab.h"
#include "afun.h"
#include "astate.h"
#include "abuf.h"
#include "agc.h"
#include "adebug.h"
#include "avm.h"
#include "ado.h"
#include "aeval.h"
#include "aparse.h"
#include "aload.h"
#include "alo.h"

#include <string.h>

void alo_setpanic(astate T, acfun panic) {
	Gd(T);
	G->panic = panic;
}

aalloc alo_getalloc(astate T, void** pctx) {
	Gd(T);
	if (pctx) {
		*pctx = G->context;
	}
	return G->alloc;
}

void alo_setalloc(astate T, aalloc alloc, void* ctx) {
	Gd(T);
	G->alloc = alloc;
	G->context = ctx;
}

void alo_bind(astate T, astr name, acfun handle) {
	aloU_bind(T, handle, aloS_newi(T, name, strlen(name)));
}

const aver_t* alo_version(astate T) {
	return T ? &T->g->version : &aloR_version;
}

#define isinstk(index) ((index) > ALO_GLOBAL_IDNEX)
#define stackoff(top,base) (api_check(T, (top) >= (base), "illegal stack"), aloE_cast(size_t, (top) - (base)))

ssize_t alo_absindex(astate T, ssize_t index) {
	return index < 0 && isinstk(index) ?
			(T->top - (T->frame->fun + 1)) + index : index;
}

static void growstack(astate T, void* context) {
	size_t* psize = aloE_cast(size_t*, context);
	aloD_growstack(T, *psize);
}

int alo_ensure(astate T, size_t size) {
	aframe_t* const frame = T->frame;
	if (stackoff(frame->top, T->top) < size) {
		if (stackoff(T->stack + T->stacksize, T->top) < size) {
			size_t capacity = T->top - T->stack + EXTRA_STACK;
			if (capacity + size > ALO_MAXSTACKSIZE) {
				return false;
			}
			else if (aloD_prun(T, growstack, &size) != ThreadStateRun) { /* try grow stack. */
				return false;
			}
		}
		frame->top = T->top + size; /* adjust top of frame. */
	}
	return true;
}

ssize_t alo_gettop(astate T) {
	return T->top - (T->frame->fun + 1);
}

void alo_settop(astate T, ssize_t index) {
	if (index >= 0) { /* resize stack size */
		api_check(T, index < T->frame->top - (T->frame->fun + 1), "stack overflow");
		askid_t newtop = (T->frame->fun + 1) + index;
		/* set expand stack value to nil */
		for (askid_t i = T->top; i < newtop; ++i) {
			tsetnil(i);
		}
		T->top = newtop;
	}
	else { /* decrease stack size */
		api_check(T, isinstk(index), "illegal stack index");
		api_checkelems(T, -index);
		T->top += index;
	}
}

#define INVALID_ADDR aloE_cast(atval_t*, aloO_tnil)

#define iscapture(index) ((index) < ALO_REGISTRY_INDEX)
#define isvalid(o) ((o) != aloO_tnil)

static atval_t* index2addr(astate T, ssize_t index) {
	if (index >= 0) {
		api_check(T, index <= (T->top - (T->frame->fun + 1)), "stack index out of bound.");
		return T->frame->fun + 1 + index;
	}
	else if (isinstk(index)) {
		api_check(T, T->frame->fun + 1 <= T->top + index, "stack index out of bound.");
		return T->top + index;
	}
	else if (index == ALO_GLOBAL_IDNEX) {
		Gd(T);
		return &G->registry; /* place global registry into stack. */
	}
	else if (index == ALO_REGISTRY_INDEX) {
		return T->frame->env; /* place current registry into stack. */
	}
	else if (index >= ALO_CAPTURE_INDEX(0)) { /* capture mode. */
		askid_t fun = T->frame->fun;
		index -= ALO_CAPTURE_INDEX(0);
		switch (ttype(fun)) {
		case ALO_TACL: {
			aacl_t* v = tgetacl(fun);
			api_check(T, index < v->length, "capture index out of bound.");
			return v->array[index]->p;
		}
		case ALO_TCCL: {
			accl_t* v = tgetccl(fun);
			api_check(T, index < v->length, "capture index out of bound.");
			return v->array + index;
		}
		default: {
			api_check(T, false, "not a closure.");
			return INVALID_ADDR;
		}
		}
	}
	else {
		api_check(T, 0, "illegal stack index.");
		return INVALID_ADDR;
	}
}

void alo_copy(astate T, ssize_t fromidx, ssize_t toidx) {
	atval_t* to = index2addr(T, toidx);
	tsetobj(T, to, index2addr(T, fromidx));
	if (iscapture(toidx)) { /* if it is capture */
		aloG_barriert(T, tgetclo(T->frame->fun), to);
	}
}

/**
 ** push value to top of stack.
 */
void alo_push(astate T, ssize_t index) {
	const atval_t* o = index2addr(T, index);
	tsetobj(T, api_incrtop(T), o);
}

/**
 ** pop top value into stack.
 */
void alo_pop(astate T, ssize_t index) {
	atval_t* o = index2addr(T, index);
	tsetobj(T, o, api_decrtop(T));
}

/**
 ** erase one value in stack
 */
void alo_erase(astate T, ssize_t index) {
	api_check(T, isinstk(index), "invalid stack index.");
	askid_t i = index2addr(T, index);
	while (++i < T->top) {
		tsetobj(T, i - 1, i);
	}
	T->top--;
}

/**
 ** insert value at the top of stack into specific index of stack
 */
void alo_insert(astate T, ssize_t index) {
	api_checkslots(T, 1);
	api_check(T, isinstk(index), "invalid stack index.");
	askid_t bot = index2addr(T, index);
	askid_t top = T->top - 1;
	atval_t cache = *top;
	askid_t i = top - 1;
	while (i >= bot) {
		tsetobj(T, i + 1, i);
		i--;
	}
	trsetvalx(T, bot, cache);
}

/**
 ** move stack from one state to another state.
 */
void alo_xmove(astate from, astate to, size_t n) {
	if (from == to)
		return;
	api_checkelems(from, n);
	api_checkslots(to, n);
	api_check(T, from->g == to->g, "two thread from different state.");
	from->top -= n;
	for (size_t i = 0; i < n; ++i) {
		tsetobj(to, to->top++, from->top + i);
	}
}

int alo_isinteger(astate T, ssize_t index) {
	const atval_t* o = index2addr(T, index);
	return ttisint(o) || (ttisflt(o) && aloO_flt2int(tgetflt(o), NULL, 0));
}

int alo_isnumber(astate T, ssize_t index) {
	const atval_t* o = index2addr(T, index);
	return ttisnum(o);
}

int alo_iscfunction(astate T, ssize_t index) {
	const atval_t* o = index2addr(T, index);
	return ttislcf(o) || ttisccl(o);
}

int alo_israwdata(astate T, ssize_t index) {
	const atval_t* o = index2addr(T, index);
	return ttisptr(o) || ttisrdt(o);
}

int alo_typeid(astate T, ssize_t index) {
	askid_t b = T->frame->fun + 1;
	if (isinstk(index) && (index + T->top < b || index + b >= T->top)) {
		return ALO_TUNDEF;
	}
	const atval_t* o = index2addr(T, index);
	return isvalid(o) ? ttpnv(o) : ALO_TUNDEF;
}

astr alo_typename(astate T, ssize_t index) {
	askid_t b = T->frame->fun + 1;
	if (isinstk(index) && (index + T->top < b || index + b >= T->top)) {
		return alo_tpidname(T, ALO_TUNDEF);
	}
	const atval_t* o = index2addr(T, index);
	if (!isvalid(o)) {
		return alo_tpidname(T, ALO_TUNDEF);
	}
	return aloV_typename(T, o);
}

astr alo_tpidname(__attribute__((unused)) astate T, int id) {
	aloE_assert(id >= ALO_TUNDEF && id < ALO_NUMTYPE, "illegal type id.");
	return id == ALO_TUNDEF ? "none" : aloT_typenames[id];
}

int alo_toboolean(astate T, ssize_t index) {
	const atval_t* o = index2addr(T, index);
	return aloV_getbool(o);
}

aint alo_tointegerx(astate T, ssize_t index, int* p) {
	const atval_t* o = index2addr(T, index);
	aint out;
	int isint = aloV_toint(o, out);
	if (!isint)
		out = 0;
	if (p)
		*p = isint;
	return out;
}

afloat alo_tonumberx(astate T, ssize_t index, int* p) {
	const atval_t* o = index2addr(T, index);
	afloat out;
	int isnum = aloV_toflt(o, out);
	if (!isnum)
		out = 0;
	if (p)
		*p = isnum;
	return out;
}

astr alo_tolstring(astate T, ssize_t index, size_t* plen) {
	const atval_t* o = index2addr(T, index);
	astring_t* value = tgetstr(o);
	if (plen) {
		*plen = aloS_len(value);
	}
	return value->array;
}

acfun alo_tocfunction(astate T, ssize_t index) {
	const atval_t* o = index2addr(T, index);
	switch (ttype(o)) {
	case ALO_TCCL: return tgetclo(o)->c.handle;
	case ALO_TLCF: return tgetlcf(o);
	default      : return NULL;
	}
}

void* alo_torawdata(astate T, ssize_t index) {
	const atval_t* o = index2addr(T, index);
	switch (ttype(o)) {
	case ALO_TPOINTER: return tgetptr(o);
	case ALO_TRAWDATA: return tgetrdt(o)->array;
	default          : return NULL;
	}
}

astate alo_tothread(astate T, ssize_t index) {
	const atval_t* o = index2addr(T, index);
	return tgetthr(o);
}

void* alo_rawptr(astate T, ssize_t index) {
	const atval_t* o = index2addr(T, index);
	switch (ttpnv(o)) {
	case ALO_TTUPLE  : return tgetrptr(o);
	case ALO_TLIST   : return tgetrptr(o);
	case ALO_TTABLE  : return tgetrptr(o);
	case ALO_TTHREAD : return tgetrptr(o);
	case ALO_TLCF    : return tgetrptr(o);
	case ALO_TCCL    : return tgetrptr(o);
	case ALO_TACL    : return tgetrptr(o);
	case ALO_TPOINTER: return tgetrptr(o);
	case ALO_TRAWDATA: return tgetrdt(o)->array;
	default          : return NULL;
	}
}

size_t alo_rawlen(astate T, ssize_t index) {
	const atval_t* o = index2addr(T, index);
	switch (ttype(o)) {
	case ALO_THSTRING: return tgetstr(o)->lnglen;
	case ALO_TISTRING: return tgetstr(o)->shtlen;
	case ALO_TTUPLE  : return tgettup(o)->length;
	case ALO_TLIST   : return tgetlis(o)->length;
	case ALO_TTABLE  : return tgettab(o)->length;
	case ALO_TRAWDATA: return tgetrdt(o)->length;
	default          : return 0;
	}
}

int alo_equal(astate T, ssize_t index1, ssize_t index2) {
	const atval_t* o1 = index2addr(T, index1);
	const atval_t* o2 = index2addr(T, index2);
	return aloV_equal(T, o1, o2);
}

void alo_arith(astate T, int op) {
	switch (op) {
	case ALO_OPADD ... ALO_OPBXOR: { /* binary operation */
		api_checkelems(T, 2);
		if (!aloV_binop(T, op, T->top - 2, T->top - 2, T->top - 1)) {
			if (!aloT_trybin(T, T->top - 2, T->top - 1, op)) {
				aloU_rterror(T, "can not take operation.");
			}
			tsetobj(T, T->top - 2, T->top);
		}
		T->top -= 1;
		break;
	}
	case ALO_OPPOS ... ALO_OPBNOT: { /* unary operation */
		api_checkelems(T, 1);
		if (!aloV_unrop(T, op, T->top - 1, T->top - 1)) {
			if (!aloT_tryunr(T, T->top - 1, op)) {
				aloU_rterror(T, "can not take operation.");
			}
			tsetobj(T, T->top - 1, T->top);
		}
		break;
	}
	}
}

int alo_compare(astate T, ssize_t index1, ssize_t index2, int op) {
	return aloV_compare(T, index2addr(T, index1), index2addr(T, index2), op);
}

void alo_pushnil(astate T) {
	askid_t t = api_incrtop(T);
	tsetnil(t);
}

void alo_pushboolean(astate T, int value) {
	askid_t t = api_incrtop(T);
	tsetbool(t, !!value);
}

void alo_pushinteger(astate T, aint value) {
	askid_t t = api_incrtop(T);
	tsetint(t, value);
}

void alo_pushnumber(astate T, afloat value) {
	askid_t t = api_incrtop(T);
	tsetflt(t, value);
}

astr alo_pushlstring(astate T, const char* src, size_t len) {
	astring_t* v = aloS_new(T, src, len);
	askid_t t = api_incrtop(T);
	tsetstr(T, t, v);
	aloG_check(T);
	return v->array;
}

astr alo_pushstring(astate T, astr value) {
	astring_t* v = value ? aloS_of(T, value) : T->g->sempty;
	askid_t t = api_incrtop(T);
	tsetstr(T, t, v);
	aloG_check(T);
	return v->array;
}

astr alo_pushfstring(astate T, astr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	astr result = alo_pushvfstring(T, fmt, varg);
	va_end(varg);
	return result;
}

astr alo_pushvfstring(astate T, astr fmt, va_list varg) {
	astr result = aloV_pushvfstring(T, fmt, varg);
	aloG_check(T);
	return result;
}

void alo_pushcfunction(astate T, acfun handle, size_t ncapture, int hasenv) {
	api_checkelems(T, ncapture);
	api_check(T, ncapture > 0 || !hasenv, "a closure has environment size must greater than 0.");
	if (ncapture == 0) {
		askid_t t = api_incrtop(T);
		tsetlcf(t, handle);
	}
	else {
		accl_t* c = aloF_newc(T, handle, ncapture);
		c->fenv = hasenv;
		T->top -= ncapture;
		for (size_t i = 0; i < ncapture; ++i) { /* move captures */
			tsetobj(T, c->array + i, T->top + i);
		}
		askid_t t = api_incrtop(T);
		tsetccl(T, t, c);
		aloG_check(T);
	}
}

void alo_pushpointer(astate T, void* value) {
	askid_t t = api_incrtop(T);
	tsetptr(t, value);
}

int alo_pushthread(astate T) {
	askid_t t = api_incrtop(T);
	tsetthr(T, t, T);
	return T == T->g->tmain;
}

void alo_rawcat(astate T, size_t size) {
	astring_t* s;

	if (size == 1 && ttisstr(T->top - 1)) {
		s = tgetstr(T->top - 1);
		T->top -= 1;
	}
	else if (size > 0) {
		api_checkelems(T, size);
		s = aloV_rawcat(T, size);
	}
	else {
		api_checkslots(T, 1);
		s = T->g->sempty;
	}

	askid_t t = api_incrtop(T);
	tsetstr(T, t, s);
}

/**
 ** iterate to next element in collection.
 */
int alo_inext(astate T, ssize_t idown, aitr* poff) {
	api_checkslots(T, 2);
	askid_t o = index2addr(T, idown);
	const atval_t* t;
	switch (ttpnv(o)) {
	case ALO_TTUPLE: {
		t = aloA_next(tgettup(o), &poff->offset);
		goto ielement;
	}
	case ALO_TLIST: {
		t = aloI_next(tgetlis(o), &poff->offset);
		goto ielement;
	}
	case ALO_TTABLE: {
		const aentry_t* e = aloH_next(tgettab(o), &poff->offset);
		if (e) {
			tsetobj(T, T->top    , amkey(e));
			tsetobj(T, T->top + 1, amval(e));
			T->top += 2;
			return ttpnv(amval(e));
		}
		return ALO_TUNDEF;
	}
	default: {
		aloE_assert(false, "object can not be iterated.");
		break;
	}
	ielement: { /* for array like 'next' call */
		if (t) { /* has next element? */
			tsetint(   T->top    , poff->offset);
			tsetobj(T, T->top + 1, t);
			T->top += 2;
			return ttpnv(t);
		}
		break;
	}
	}
	return ALO_TUNDEF;
}

/**
 ** remove current element in collection.
 */
void alo_iremove(astate T, ssize_t idown, aitr* itr) {
	api_checkslots(T, 2);
	askid_t o = index2addr(T, idown);
	switch (ttpnv(o)) {
	case ALO_TTUPLE: {
		aloE_assert(false, "can not remove element from tuple .");
		break;
	}
	case ALO_TLIST: {
		aloI_removei(T, tgetlis(o), itr->offset, NULL);
		break;
	}
	case ALO_TTABLE: {
		aloH_rawrem(T, tgettab(o), &itr->offset, NULL);
		break;
	}
	default: {
		aloE_assert(false, "object can not be iterated.");
		break;
	}
	}

}

int alo_rawget(astate T, ssize_t idown) {
	api_checkelems(T, 1);
	askid_t o = index2addr(T, idown);
	askid_t k = T->top - 1;
	const atval_t* v = aloO_get(T, o, k);
	tsetobj(T, k, v);
	return v != aloO_tnil ? ttpnv(v) : ALO_TUNDEF;
}

int alo_rawgeti(astate T, ssize_t idown, aint key) {
	api_checkslots(T, 1);
	askid_t o = index2addr(T, idown);
	const atval_t* v;
	switch (ttpnv(o)) {
	case ALO_TTUPLE:
		v = aloA_geti(tgettup(o), key);
		break;
	case ALO_TLIST: {
		v = aloI_geti(tgetlis(o), key);
		break;
	}
	case ALO_TTABLE: {
		v = aloH_geti(tgettab(o), key);
		break;
	}
	default: {
		api_check(T, false, "illegal owner for 'rawgeti'");
		return ALO_TUNDEF;
	}
	}
	tsetobj(T, api_incrtop(T), v);
	return v != aloO_tnil ? ttpnv(v) : ALO_TUNDEF;
}

int alo_rawgets(astate T, ssize_t idown, astr key) {
	api_checkslots(T, 1);
	askid_t o = index2addr(T, idown);
	api_check(T, ttistab(o), "illegal owner for 'rawgets'");
	const atval_t* v = aloH_gets(T, tgettab(o), key, strlen(key));
	tsetobj(T, api_incrtop(T), v);
	return v != aloO_tnil ? ttpnv(v) : ALO_TUNDEF;
}

int alo_get(astate T, ssize_t idown) {
	askid_t o = index2addr(T, idown);
	askid_t k = T->top - 1;
	const atval_t* v;
	if (ttiscol(o)) {
		v = aloO_get(T, o, k);
	}
	else goto find;
	if (v == aloO_tnil) {
		find:
		v = aloT_index(T, o, k);
		k = T->top - 1; /* adjust index */
		if (v) {
			tsetobj(T, k, v);
			return ttpnv(v);
		}
		else {
			tsetnil(k);
			return ALO_TUNDEF;
		}
	}
	else {
		tsetobj(T, k, v);
		return ttpnv(v);
	}
}

int alo_gets(astate T, ssize_t idown, astr key) {
	api_checkslots(T, 1);
	askid_t o = index2addr(T, idown);
	if (ttistab(o)) {
		const atval_t* v = aloH_gets(T, tgettab(o), key, strlen(key));
		if (v != aloO_tnil) {
			tsetobj(T, api_incrtop(T), v);
			return ttpnv(v);
		}
	}
	tsetstr(T, api_incrtop(T), aloS_of(T, key));
	const atval_t* v = aloT_index(T, o, T->top - 1);
	if (v) {
		tsetobj(T, T->top - 1, v);
		return ttpnv(v);
	}
	else {
		tsetnil(T->top - 1);
		return ALO_TUNDEF;
	}
}

int alo_put(astate T, ssize_t idown) {
	api_checkelems(T, 1);
	askid_t o = index2addr(T, idown);
	api_check(T, ttislis(o), "only list can take 'put' operation.");
	int result = aloI_put(T, tgetlis(o), T->top - 1);
	api_decrtop(T);
	return result;
}

int alo_getmetatable(astate T, ssize_t index) {
	api_checkslots(T, 1);
	askid_t o = index2addr(T, index);
	atable_t* mt = aloT_getmt(o);
	if (mt) {
		tsettab(T, api_incrtop(T), mt);
		return true;
	}
	return false;
}

int alo_getmeta(astate T, ssize_t index, astr name, int lookup) {
	api_checkslots(T, 1);
	astring_t* s = aloS_of(T, name);
	askid_t o = index2addr(T, index);
	const atval_t* tm;
	if (s->ftagname) { /* inner tagged method. */
		if (s->extra < ALO_NUMTM) {
			tm = lookup ? aloT_fastgetx(T, o, s->extra) : aloT_fastget(T, o, s->extra);
		}
		else {
			tm = aloT_gettm(T, o, s->extra, lookup);
		}
		if (tm && !ttisnil(tm)) {
			tsetobj(T, T->top++, tm);
		}
		else {
			return ALO_TNIL;
		}
	}
	else {
		tsetstr(T, api_incrtop(T), s); /* bind string into thread */
		if ((tm = aloT_lookup(T, o, T->top - 1)) && !ttisnil(tm)) {
			tsetobj(T, T->top - 1, tm);
		}
		else {
			api_decrtop(T);
			return ALO_TNIL;
		}
	}
	return ttpnv(tm);
}

int alo_getdelegate(astate T, ssize_t index) {
	api_checkslots(T, 1);
	askid_t o = index2addr(T, index);
	switch (ttype(o)) {
	case ALO_TACL:
		tsetobj(T, api_incrtop(T), tgetacl(o)->array[0]->p);
		return true;
	case ALO_TCCL:
		tsetobj(T, api_incrtop(T), tgetccl(o)->array);
		return true;
	default:
		return false;
	}
}

amem alo_newdata(astate T, size_t size) {
	api_checkslots(T, 1);
	arawdata_t* value = aloS_newr(T, size);
	tsetrdt(T, api_incrtop(T), value);
	aloG_check(T);
	return value->array;
}

void alo_newtuple(astate T, size_t size) {
	api_checkelems(T, size);
	atuple_t* value = aloA_new(T, size, T->top - size);
	T->top -= size;
	tsettup(T, api_incrtop(T), value);
	aloG_check(T);
}

void alo_newlist(astate T, size_t size) {
	api_checkslots(T, 1);
	alist_t* value = aloI_new(T);
	tsetlis(T, api_incrtop(T), value);
	if (size > 0) { /* call 'ensure' function if initialize capacity is not 0 */
		aloI_ensure(T, value, size);
	}
	aloG_check(T);
}

void alo_newtable(astate T, size_t size) {
	api_checkslots(T, 1);
	atable_t* value = aloH_new(T);
	tsettab(T, api_incrtop(T), value);
	if (size > 0) { /* call 'ensure' function if initialize capacity is not 0 */
		aloH_ensure(T, value, size);
	}
	aloG_check(T);
}

astate alo_newthread(astate T) {
	aloG_check(T);
	api_checkslots(T, 1);
	astate thread = aloR_newthread(T);
	tsetthr(T, api_incrtop(T), thread);
	return thread;
}

/**
 ** trim object, and make it takes lower memory cost.
 */
void alo_trim(astate T, ssize_t idown) {
	askid_t o = index2addr(T, idown);
	switch (ttype(o)) {
	case ALO_TLIST:
		aloI_trim(T, tgetlis(o));
		break;
	case ALO_TTABLE:
		aloH_trim(T, tgettab(o));
		break;
	}
}

/**
 ** trim object
 */
void alo_triml(astate T, ssize_t idown, size_t len) {
	askid_t o = index2addr(T, idown);
	alist_t* list = tgetlis(o);
	aloE_assert(len <= list->length, "the length should less than current length.");
	list->length = len;
	aloI_trim(T, list); /* trim list size. */
}

void alo_rawsetx(astate T, ssize_t idown, int drop) {
	api_checkelems(T, 2);
	askid_t o = index2addr(T, idown);
	askid_t k = T->top - 2;
	askid_t v = T->top - 1;
	switch (ttpnv(o)) {
	case ALO_TLIST: {
		alist_t* owner = tgetlis(o);
		atval_t* v1 = aloI_find(T, owner, k);
		aloG_barrierbackt(T, owner, v);
		if (drop) {
			tsetobj(T, k, v1);
		}
		tsetobj(T, v1, v);
		break;
	}
	case ALO_TTABLE: {
		atable_t* owner = tgettab(o);
		atval_t* v1 = aloH_find(T, owner, k);
		aloG_barrierbackt(T, owner, k);
		aloG_barrierbackt(T, owner, v);
		if (drop) {
			tsetobj(T, k, v1);
		}
		tsetobj(T, v1, v);
		aloH_markdirty(owner);
		break;
	}
	default: {
		api_check(T, false, "illegal owner for 'rawset'");
		break;
	}
	}
	T->top -= drop ? 1 : 2;
	aloG_check(T);
}

void alo_rawseti(astate T, ssize_t idown, aint key) {
	api_checkelems(T, 1);
	askid_t o = index2addr(T, idown);
	askid_t v = T->top - 1;
	switch (ttpnv(o)) {
	case ALO_TLIST: {
		aloI_seti(T, tgetlis(o), key, v);
		break;
	}
	case ALO_TTABLE: {
		atable_t* owner = tgettab(o);
		atval_t k = tnewint(key);
		aloH_set(T, owner, &k, v);
		break;
	}
	default: {
		api_check(T, false, "illegal owner for 'rawseti'");
		break;
	}
	}
	T->top -= 1;
	aloG_check(T);
}

void alo_rawsets(astate T, ssize_t idown, astr key) {
	api_checkelems(T, 1);
	askid_t o = index2addr(T, idown);
	askid_t v = T->top - 1;
	api_check(T, ttistab(o), "illegal owner for 'rawsets'");
	atable_t* table = tgettab(o);
	tsetobj(T, aloH_finds(T, table, key, strlen(key)), v);
	aloG_barrierbackt(T, table, v);
	T->top -= 1;
	aloG_check(T);
}

int alo_rawrem(astate T, ssize_t idown) {
	api_checkelems(T, 1);
	askid_t o = index2addr(T, idown);
	askid_t k = T->top - 1;
	int succ;
	switch (ttpnv(o)) {
	case ALO_TLIST: {
		succ = aloI_remove(T, tgetlis(o), k, k);
		break;
	}
	case ALO_TTABLE: {
		succ = aloH_remove(T, tgettab(o), k, k);
		break;
	}
	default: {
		api_check(T, false, "illegal owner for 'remove'");
		succ = false;
		break;
	}
	}
	if (!succ) {
		T->top -= 1;
	}
	aloG_check(T);
	return succ ? ttpnv(k) : ALO_TUNDEF; /* return removed value type or 'none' if nothing is removed */
}

void alo_rawclr(astate T, ssize_t idown) {
	askid_t o = index2addr(T, idown);
	switch (ttpnv(o)) {
	case ALO_TLIST: {
		aloI_clear(T, tgetlis(o));
		break;
	}
	case ALO_TTABLE: {
		aloH_clear(T, tgettab(o));
		break;
	}
	default: {
		api_check(T, false, "illegal owner for 'clear'");
		break;
	}
	}
}

void alo_add(astate T, ssize_t idown) {
	api_checkelems(T, 1);
	askid_t o = index2addr(T, idown);
	api_check(T, ttislis(o), "illegal owner for 'add'");
	aloI_add(T, tgetlis(o), T->top - 1);
	api_decrtop(T);
}

void alo_setx(astate T, ssize_t idown, int dodrop) {
	api_checkelems(T, 2);
	askid_t o = index2addr(T, idown);
	const atval_t* tm = aloT_fastgetx(T, o, TM_NIDX);
	if (tm) { /* call tagged method */
		tsetobj(T, T->top    , T->top - 2);
		tsetobj(T, T->top + 1, T->top - 1);
		tsetobj(T, T->top - 1, o);
		tsetobj(T, T->top - 2, tm);
		T->top += 2;
		aloD_call(T, T->top - 4, dodrop ? 1 : 0);
	}
	else {
		alo_rawsetx(T, idown, dodrop);
	}
}

int alo_remove(astate T, ssize_t idown) {
	api_checkelems(T, 2);
	askid_t o = index2addr(T, idown);
	const atval_t* tm = aloT_gettm(T, o, TM_REM, true);
	if (tm) { /* call tagged method */
		tsetobj(T, T->top + 1, T->top - 1);
		tsetobj(T, T->top    , o);
		tsetobj(T, T->top - 1, tm);
		T->top += 2;
		ptrdiff_t res = getstkoff(T, T->top - 3);
		aloD_callnoyield(T, o, ALO_MULTIRET);
		askid_t r = putstkoff(T, res);
		return T->top != r ? /* successfully removed value? */
				ttpnv(r) : /* get object type */
				ALO_TUNDEF; /* return 'none' */
	}
	else {
		return alo_rawrem(T, idown);
	}
}

int alo_setmetatable(astate T, ssize_t index) {
	atval_t* o = index2addr(T, index);
	atable_t* mt;
	if (ttisnil(T->top - 1)) {
		mt = NULL;
	}
	else {
		api_check(T, ttistab(T->top - 1), "value is not meta table.");
		mt = tgettab(T->top - 1);
	}
	switch (ttpnv(o)) {
	case ALO_TLIST:
		tgetlis(o)->metatable = mt;
		break;
	case ALO_TTABLE:
		tgettab(o)->metatable = mt;
		break;
	case ALO_TRAWDATA:
		tgetrdt(o)->metatable = mt;
		break;
	default:
		T->top -= 1;
		return false;
	}
	agct g = tgetref(o);
	aloG_barrier(T, g, mt);
	aloG_checkfnzobj(T, g, mt);
	T->top -= 1;
	return true;
}

int alo_setdelegate(astate T, ssize_t index) {
	api_checkelems(T, 1);
	askid_t o = index2addr(T, index);
	const atval_t* t = api_decrtop(T);
	if (ttisnil(t)) {
		t = T->frame->env;
	}
	api_check(T, ttistab(t), "delegate should be table");
	switch (ttype(o)) {
	case ALO_TACL: {
		aacl_t* closure = tgetacl(o);
		tsetobj(T, closure->array[0]->p, t);
		aloG_barriert(T, closure, t);
		return true;
	}
	case ALO_TCCL: {
		accl_t* closure = tgetccl(o);
		tsetobj(T, closure->array, t);
		aloG_barriert(T, closure, t);
		return true;
	}
	default:
		return false;
	}
}

void alo_callk(astate T, int narg, int nres, akfun kfun, void* kctx) {
	api_check(T, kfun == NULL || !T->frame->falo, "can not use continuation inside hooks");
	api_checkelems(T, 1 + narg);
	api_check(T, T->g->trun == T, "thread is not running.");
	api_check(T, T->status == ThreadStateRun, "thread is in non-normal state.");
	askid_t fun = T->top - (narg + 1);
	if (kfun && T->nxyield == 0) { /* is available for continuation? */
		aframe_t* const frame = T->frame;
		frame->c.kfun = kfun;
		frame->c.ctx = kctx;
		aloD_call(T, fun, nres); /* do call */
	}
	else { /* in non-yieldable environment or not yieldable */
		aloD_callnoyield(T, fun, nres); /* do call */
	}
	aloD_adjustresult(T, nres);
}

int alo_pcallk(astate T, int narg, int nres, ssize_t errfun, akfun kfun, void* kctx) {
	api_check(T, kfun == NULL || !T->frame->falo, "can not use continuation inside hooks");
	api_checkelems(T, 1 + narg);
	api_check(T, T->g->trun == T, "thread is not running.");
	api_check(T, T->status == ThreadStateRun, "thread is in non-normal state.");
	api_check(T, errfun == ALO_NOERRFUN || errfun == ALO_LASTERRFUN || isinstk(errfun),
			"illegal error function index.");
	aframe_t* const frame = T->frame;
	askid_t fun = T->top - (narg + 1);
	ptrdiff_t ef = errfun == ALO_NOERRFUN ? 0 :
			errfun == ALO_LASTERRFUN ? T->errfun :
			getstkoff(T, index2addr(T, errfun));
	int status;
	if (kfun == NULL || T->nxyield > 0) {
		status = aloD_pcall(T, fun, nres, ef); /* call function in protection */
	}
	else {
		frame->c.kfun = kfun;
		frame->c.ctx = kctx;
		frame->c.oef = T->errfun;
		T->errfun = ef;
		frame->fypc = true;
		aloD_call(T, fun, nres); /* call function in protection */
		frame->fypc = false;
		T->errfun = frame->c.oef;
		status = ThreadStateRun;
	}
	aloD_adjustresult(T, nres);
	return status;
}

/**
 ** create a new closure and push into top of stack, used to avoid memory error.
 */
static void pnewclosure(astate T, void* context) {
	aacl_t** p = aloE_cast(aacl_t**, context);
	*p = aloF_new(T, 1);
	tsetacl(T, api_incrtop(T), *p); /* put closure into stack to avoid GC */
}

int alo_compile(astate T, astr name, astr src, areader reader, void* context) {
	aacl_t* c = NULL;
	int status = aloD_prun(T, pnewclosure, &c);
	if (status != ThreadStateRun) {
		return status;
	}
	astring_t* s;
	aibuf_t buf;
	aloB_iopen(&buf, reader, context);
	status = aloP_parse(T, src, &buf, &c->proto, &s);
	if (status == ThreadStateRun) { /* compile success */
		aloE_assert(c->proto->ncap == 1, "top prototype should only have 1 capture.");
		c->proto->name = aloS_of(T, name);
		c->array[0] = aloF_envcap(T);
	}
	else { /* failed */
		tsetstr(T, T->top - 1, s); /* move error message into top of stack */
	}
	aloG_check(T);
	return status;
}

int alo_load(astate T, astr src, areader reader, void* context) {
	aacl_t* c = aloF_new(T, 0);
	tsetacl(T, api_incrtop(T), c);
	int status = aloZ_load(T, &c->proto, src, reader, context);
	if (status != ThreadStateRun) {
		api_decrtop(T);
	}
	return status;
}

int alo_save(astate T, awriter writer, void* context, int debug) {
	aclosure_t* c = tgetclo(T->top - 1);
	aloE_assert(ttisacl(c), "not a Alopecurus closure");
	return aloZ_save(T, c->a.proto, writer, context, debug);
}

anoret alo_error(astate T) {
	api_checkelems(T, 1);
	aloU_error(T, ThreadStateErrRuntime);
}

anoret alo_throw(astate T) {
	api_checkelems(T, 1);
	aloD_throw(T, ThreadStateErrRuntime);
}

size_t alo_gcconf(astate T, int what, size_t data) {
	Gd(T);
	size_t result = 0;
	switch (what) {
	case ALO_GCRUNNING: {
		result = G->fgc;
		break;
	}
	case ALO_GCSTOP: {
		G->fgc = false;
		break;
	}
	case ALO_GCRERUN: {
		G->fgc = true;
		aloD_setdebt(G, 0);
		break;
	}
	case ALO_GCUSED: {
		result = totalmem(G);
		break;
	}
	case ALO_GCSTEPMUL: {
		result = G->gcstepmul;
		G->gcstepmul = data;
		break;
	}
	case ALO_GCPAUSEMUL: {
		result = G->gcpausemul;
		G->gcpausemul = data;
		break;
	}
	default: {
		aloE_assert(false, "unknown GC configuration option.");
		break;
	}
	}
	return result;
}

void alo_fullgc(astate T) {
	aloG_fullgc(T, GCKindNormal);
}

void alo_checkgc(astate T) {
	aloG_check(T);
}
