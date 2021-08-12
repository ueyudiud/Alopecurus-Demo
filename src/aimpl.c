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

void alo_setpanic(alo_State T, a_cfun panic) {
	Gd(T);
	G->panic = panic;
}

alo_Alloc alo_getalloc(alo_State T, void** pctx) {
	Gd(T);
	if (pctx) {
		*pctx = G->context;
	}
	return G->alloc;
}

void alo_setalloc(alo_State T, alo_Alloc alloc, void* ctx) {
	Gd(T);
	G->alloc = alloc;
	G->context = ctx;
}

const aver_t* alo_version(alo_State T) {
	return T ? T->g->version : &aloR_version;
}

#define isinstk(index) ((index) > ALO_GLOBAL_IDNEX)
#define stackoff(top,base) (aloi_check(T, (top) >= (base), "illegal stack"), aloE_cast(size_t, (top) - (base)))

a_isize alo_absindex(alo_State T, a_isize index) {
	return index < 0 && isinstk(index) ?
			(T->top - (T->frame->fun + 1)) + index : index;
}

static void growstack(alo_State T, void* context) {
	size_t* psize = aloE_cast(size_t*, context);
	aloD_growstack(T, *psize);
}

a_bool alo_ensure(alo_State T, a_usize size) {
	aframe_t* const frame = T->frame;
	if (stackoff(frame->top, T->top) < size) {
		if (stackoff(T->stack + T->stacksize, T->top) < size) {
			size_t capacity = T->top - T->stack + EXTRA_STACK;
			if (capacity + size > ALO_MAXSTACKSIZE) {
				return false;
			}
			else if (aloD_prun(T, growstack, &size) != ALO_STOK) { /* try grow stack. */
				return false;
			}
		}
		frame->top = T->top + size; /* adjust top of frame. */
	}
	return true;
}

a_isize alo_gettop(alo_State T) {
	return T->top - (T->frame->fun + 1);
}

void alo_settop(alo_State T, a_isize index) {
	if (index >= 0) { /* resize stack size */
		aloi_check(T, index < T->frame->top - (T->frame->fun + 1), "stack overflow");
		alo_StkId newtop = (T->frame->fun + 1) + index;
		/* set expand stack value to nil */
		for (alo_StkId i = T->top; i < newtop; ++i) {
			tsetnil(i);
		}
		T->top = newtop;
	}
	else { /* decrease stack size */
		aloi_check(T, isinstk(index), "illegal stack index");
		api_checkelems(T, -index);
		T->top += index;
	}
}

#define INVALID_ADDR aloE_cast(alo_TVal*, aloO_tnil)

#define iscapture(index) ((index) < ALO_REGISTRY_INDEX)
#define isvalid(o) ((o) != aloO_tnil)

static alo_TVal* index2addr(alo_State T, a_isize index) {
	if (index >= 0) {
		aloi_check(T, index <= (T->top - (T->frame->fun + 1)), "stack index out of bound.");
		return T->frame->fun + 1 + index;
	}
	else if (isinstk(index)) {
		aloi_check(T, T->frame->fun + 1 <= T->top + index, "stack index out of bound.");
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
		alo_StkId fun = T->frame->fun;
		index -= ALO_CAPTURE_INDEX(0);
		switch (ttype(fun)) {
		case ALO_VACL: {
			alo_ACl* v = tgetacl(fun);
			aloi_check(T, index < v->length, "capture index out of bound.");
			return v->array[index]->p;
		}
		case ALO_VCCL: {
			alo_CCl* v = tgetccl(fun);
			aloi_check(T, index < v->length, "capture index out of bound.");
			return v->array + index;
		}
		default: {
			aloi_check(T, false, "not a closure.");
			return INVALID_ADDR;
		}
		}
	}
	else {
		aloi_check(T, 0, "illegal stack index.");
		return INVALID_ADDR;
	}
}

void alo_copy(alo_State T, a_isize fromidx, a_isize toidx) {
	alo_TVal* to = index2addr(T, toidx);
	tsetobj(T, to, index2addr(T, fromidx));
	if (iscapture(toidx)) { /* if it is capture */
		aloG_barriert(T, tasclo(T->frame->fun), to);
	}
}

/**
 ** push value to top of stack.
 */
void alo_push(alo_State T, a_isize index) {
	const alo_TVal* o = index2addr(T, index);
	tsetobj(T, api_incrtop(T), o);
}

/**
 ** pop top value into stack.
 */
void alo_pop(alo_State T, a_isize index) {
	alo_TVal* o = index2addr(T, index);
	tsetobj(T, o, api_decrtop(T));
}

/**
 ** erase one value in stack
 */
void alo_erase(alo_State T, a_isize index) {
	aloi_check(T, isinstk(index), "invalid stack index.");
	alo_StkId i = index2addr(T, index);
	while (++i < T->top) {
		tsetobj(T, i - 1, i);
	}
	T->top--;
}

/**
 ** insert value at the top of stack into specific index of stack
 */
void alo_insert(alo_State T, a_isize index) {
	api_checkslots(T, 1);
	aloi_check(T, isinstk(index), "invalid stack index.");
	alo_StkId bot = index2addr(T, index);
	alo_StkId top = T->top - 1;
	alo_TVal cache = *top;
	alo_StkId i = top - 1;
	while (i >= bot) {
		tsetobj(T, i + 1, i);
		i--;
	}
	trsetvalx(T, bot, cache);
}

/**
 ** move stack from one state to another state.
 */
void alo_xmove(alo_State from, alo_State to, a_usize n) {
	if (from == to)
		return;
	api_checkelems(from, n);
	api_checkslots(to, n);
	aloi_check(T, from->g == to->g, "two thread from different state.");
	from->top -= n;
	for (a_usize i = 0; i < n; ++i) {
		tsetobj(to, to->top++, from->top + i);
	}
}

a_bool alo_isinteger(alo_State T, a_isize index) {
	const alo_TVal* o = index2addr(T, index);
	return tisint(o) || (tisflt(o) && aloO_flt2int(tasflt(o), NULL, 0));
}

a_bool alo_isnumber(alo_State T, a_isize index) {
	const alo_TVal* o = index2addr(T, index);
	return tisnum(o);
}

a_bool alo_iscfunction(alo_State T, a_isize index) {
	const alo_TVal* o = index2addr(T, index);
	return tislcf(o) || tisccl(o);
}

a_bool alo_israwdata(alo_State T, a_isize index) {
	const alo_TVal* o = index2addr(T, index);
	return tisptr(o) || tisusr(o);
}

int alo_typeid(alo_State T, a_isize index) {
	alo_StkId b = T->frame->fun + 1;
	if (isinstk(index) && (index + T->top < b || index + b >= T->top)) {
		return ALO_TUNDEF;
	}
	const alo_TVal* o = index2addr(T, index);
	return isvalid(o) ? ttpnv(o) : ALO_TUNDEF;
}

a_cstr alo_typename(alo_State T, a_isize index) {
	alo_StkId b = T->frame->fun + 1;
	if (isinstk(index) && (index + T->top < b || index + b >= T->top)) {
		return alo_tpidname(T, ALO_TUNDEF);
	}
	const alo_TVal* o = index2addr(T, index);
	if (!isvalid(o)) {
		return alo_tpidname(T, ALO_TUNDEF);
	}
	return aloV_typename(T, o);
}

a_cstr alo_tpidname(__attribute__((unused)) alo_State T, int id) {
	aloE_assert(id >= ALO_TUNDEF && id < ALO_NUMTYPE, "illegal type id.");
	return id == ALO_TUNDEF ? "none" : aloT_typenames[id];
}

int alo_toboolean(alo_State T, a_isize index) {
	const alo_TVal* o = index2addr(T, index);
	return aloV_getbool(o);
}

a_int alo_tointegerx(alo_State T, a_isize index, a_bool* p) {
	const alo_TVal* o = index2addr(T, index);
	a_int out;
	int isint = aloV_toint(o, out);
	if (!isint)
		out = 0;
	if (p)
		*p = isint;
	return out;
}

a_float alo_tonumberx(alo_State T, a_isize index, a_bool* p) {
	const alo_TVal* o = index2addr(T, index);
	a_float out;
	int isnum = aloV_toflt(o, out);
	if (!isnum)
		out = 0;
	if (p)
		*p = isnum;
	return out;
}

a_cstr alo_tolstring(alo_State T, a_isize index, a_usize* plen) {
	const alo_TVal* o = index2addr(T, index);
	alo_Str* value = tasstr(o);
	if (plen) {
		*plen = aloS_len(value);
	}
	return value->array;
}

a_cfun alo_tocfunction(alo_State T, a_isize index) {
	const alo_TVal* o = index2addr(T, index);
	switch (ttype(o)) {
	case ALO_VCCL: return tasclo(o)->c.handle;
	case ALO_VLCF: return taslcf(o);
	default      : return NULL;
	}
}

void* alo_torawdata(alo_State T, a_isize index) {
	const alo_TVal* o = index2addr(T, index);
	switch (ttype(o)) {
	case ALO_TPTR: return tasptr(o);
	case ALO_TUSER: return tgetrdt(o)->array;
	default          : return NULL;
	}
}

alo_State alo_tothread(alo_State T, a_isize index) {
	const alo_TVal* o = index2addr(T, index);
	return tgetthr(o);
}

void* alo_rawptr(alo_State T, a_isize index) {
	const alo_TVal* o = index2addr(T, index);
	switch (ttpnv(o)) {
	case ALO_TTUPLE  : return tgetrptr(o);
	case ALO_TLIST   : return tgetrptr(o);
	case ALO_TTABLE  : return tgetrptr(o);
	case ALO_TTHREAD : return tgetrptr(o);
	case ALO_VLCF    : return tgetrptr(o);
	case ALO_VCCL    : return tgetrptr(o);
	case ALO_VACL    : return tgetrptr(o);
	case ALO_TPTR: return tgetrptr(o);
	case ALO_TUSER: return tgetrdt(o)->array;
	default          : return NULL;
	}
}

size_t alo_rawlen(alo_State T, a_isize index) {
	const alo_TVal* o = index2addr(T, index);
	switch (ttype(o)) {
	case ALO_VHSTR: return tasstr(o)->lnglen;
	case ALO_VISTR: return tasstr(o)->shtlen;
	case ALO_TTUPLE  : return tastup(o)->length;
	case ALO_TLIST   : return taslis(o)->length;
	case ALO_TTABLE  : return tastab(o)->length;
	case ALO_TUSER: return tgetrdt(o)->length;
	default          : return 0;
	}
}

size_t alo_len(alo_State T, a_isize index) {
	const alo_TVal* o = index2addr(T, index);
	return aloV_length(T, o);
}

a_bool alo_equal(alo_State T, a_isize index1, a_isize index2) {
	const alo_TVal* o1 = index2addr(T, index1);
	const alo_TVal* o2 = index2addr(T, index2);
	return aloV_equal(T, o1, o2);
}

void alo_arith(alo_State T, int op) {
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

a_bool alo_compare(alo_State T, a_isize index1, a_isize index2, int op) {
	const alo_TVal* o1 = index2addr(T, index1);
	const alo_TVal* o2 = index2addr(T, index2);
	return aloV_compare(T, o1, o2, op);
}

void alo_pushnil(alo_State T) {
	alo_StkId t = api_incrtop(T);
	tsetnil(t);
}

void alo_pushboolean(alo_State T, a_bool value) {
	alo_StkId t = api_incrtop(T);
	tsetbool(t, !!value);
}

void alo_pushinteger(alo_State T, a_int value) {
	alo_StkId t = api_incrtop(T);
	tsetint(t, value);
}

void alo_pushnumber(alo_State T, a_float value) {
	alo_StkId t = api_incrtop(T);
	tsetflt(t, value);
}

a_cstr alo_pushlstring(alo_State T, const char* src, size_t len) {
	alo_Str* v = aloS_new(T, src, len);
	alo_StkId t = api_incrtop(T);
	tsetstr(T, t, v);
	aloG_check(T);
	return v->array;
}

a_cstr alo_pushstring(alo_State T, a_cstr value) {
	alo_Str* v = value ? aloS_of(T, value) : T->g->sempty;
	alo_StkId t = api_incrtop(T);
	tsetstr(T, t, v);
	aloG_check(T);
	return v->array;
}

a_cstr alo_pushfstring(alo_State T, a_cstr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	a_cstr result = alo_pushvfstring(T, fmt, varg);
	va_end(varg);
	return result;
}

a_cstr alo_pushvfstring(alo_State T, a_cstr fmt, va_list varg) {
	a_cstr result = aloV_pushvfstring(T, fmt, varg);
	aloG_check(T);
	return result;
}

void alo_pushcfunction(alo_State T, a_cfun handle, a_usize ncapture, int hasenv) {
	api_checkelems(T, ncapture);
	aloi_check(T, ncapture > 0 || !hasenv, "a closure has environment size must greater than 0.");
	if (ncapture == 0) {
		alo_StkId t = api_incrtop(T);
		tsetlcf(t, handle);
	}
	else {
		alo_CCl* c = aloF_newc(T, handle, ncapture);
		c->fenv = hasenv;
		T->top -= ncapture;
		for (size_t i = 0; i < ncapture; ++i) { /* move captures */
			tsetobj(T, c->array + i, T->top + i);
		}
		alo_StkId t = api_incrtop(T);
		tsetccl(T, t, c);
		aloG_check(T);
	}
}

void alo_pushpointer(alo_State T, void* value) {
	alo_StkId t = api_incrtop(T);
	tsetptr(t, value);
}

int alo_pushthread(alo_State T) {
	alo_StkId t = api_incrtop(T);
	tsetthr(T, t, T);
	return T == T->g->tmain;
}

void alo_rawcat(alo_State T, size_t size) {
	alo_Str* s;

	if (size == 1 && tisstr(T->top - 1)) {
		s = tasstr(T->top - 1);
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

	alo_StkId t = api_incrtop(T);
	tsetstr(T, t, s);
}

/**
 ** iterate to next element in collection.
 */
int alo_inext(alo_State T, a_isize idown, a_itr* poff) {
	api_checkslots(T, 2);
	alo_StkId o = index2addr(T, idown);
	const alo_TVal* t;
	switch (ttpnv(o)) {
	case ALO_TTUPLE: {
		t = aloA_next(tastup(o), &poff->offset);
		goto ielement;
	}
	case ALO_TLIST: {
		t = aloI_next(taslis(o), &poff->offset);
		goto ielement;
	}
	case ALO_TTABLE: {
		const alo_Entry* e = aloH_next(tastab(o), &poff->offset);
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
void alo_iremove(alo_State T, a_isize idown, a_itr* itr) {
	api_checkslots(T, 2);
	alo_StkId o = index2addr(T, idown);
	switch (ttpnv(o)) {
	case ALO_TTUPLE: {
		aloE_assert(false, "can not remove element from tuple .");
		break;
	}
	case ALO_TLIST: {
		aloI_removei(T, taslis(o), itr->offset, NULL);
		break;
	}
	case ALO_TTABLE: {
		aloH_rawrem(T, tastab(o), &itr->offset, NULL);
		break;
	}
	default: {
		aloE_assert(false, "object can not be iterated.");
		break;
	}
	}

}

int alo_rawget(alo_State T, a_isize idown) {
	api_checkelems(T, 1);
	alo_StkId o = index2addr(T, idown);
	alo_StkId k = T->top - 1;
	const alo_TVal* v = aloO_get(T, o, k);
	tsetobj(T, k, v);
	return v != aloO_tnil ? ttpnv(v) : ALO_TUNDEF;
}

int alo_rawgeti(alo_State T, a_isize idown, a_int key) {
	api_checkslots(T, 1);
	alo_StkId o = index2addr(T, idown);
	const alo_TVal* v;
	switch (ttpnv(o)) {
	case ALO_TTUPLE:
		v = aloA_geti(tastup(o), key);
		break;
	case ALO_TLIST: {
		v = aloI_geti(taslis(o), key);
		break;
	}
	case ALO_TTABLE: {
		v = aloH_geti(tastab(o), key);
		break;
	}
	default: {
		aloi_check(T, false, "illegal owner for 'rawgeti'");
		return ALO_TUNDEF;
	}
	}
	tsetobj(T, api_incrtop(T), v);
	return v != aloO_tnil ? ttpnv(v) : ALO_TUNDEF;
}

int alo_rawgets(alo_State T, a_isize idown, a_cstr key) {
	api_checkslots(T, 1);
	alo_StkId o = index2addr(T, idown);
	aloi_check(T, tistab(o), "illegal owner for 'rawgets'");
	const alo_TVal* v = aloH_gets(T, tastab(o), key, strlen(key));
	tsetobj(T, api_incrtop(T), v);
	return v != aloO_tnil ? ttpnv(v) : ALO_TUNDEF;
}

int alo_get(alo_State T, a_isize idown) {
	alo_StkId o = index2addr(T, idown);
	alo_StkId k = T->top - 1;
	const alo_TVal* v;
	if (tiscol(o)) {
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

int alo_gets(alo_State T, a_isize idown, a_cstr key) {
	api_checkslots(T, 1);
	alo_StkId o = index2addr(T, idown);
	if (tistab(o)) {
		const alo_TVal* v = aloH_gets(T, tastab(o), key, strlen(key));
		if (v != aloO_tnil) {
			tsetobj(T, api_incrtop(T), v);
			return ttpnv(v);
		}
	}
	tsetstr(T, api_incrtop(T), aloS_of(T, key));
	const alo_TVal* v = aloT_index(T, o, T->top - 1);
	if (v) {
		tsetobj(T, T->top - 1, v);
		return ttpnv(v);
	}
	else {
		tsetnil(T->top - 1);
		return ALO_TUNDEF;
	}
}

int alo_put(alo_State T, a_isize idown) {
	api_checkelems(T, 1);
	alo_StkId o = index2addr(T, idown);
	aloi_check(T, tislis(o), "only list can take 'put' operation.");
	int result = aloI_put(T, taslis(o), T->top - 1);
	api_decrtop(T);
	return result;
}

int alo_getmetatable(alo_State T, a_isize index) {
	api_checkslots(T, 1);
	alo_StkId o = index2addr(T, index);
	atable_t* mt = aloT_getmt(o);
	if (mt) {
		tsettab(T, api_incrtop(T), mt);
		return true;
	}
	return false;
}

int alo_getmeta(alo_State T, a_isize index, a_cstr name, int lookup) {
	api_checkslots(T, 1);
	alo_Str* s = aloS_of(T, name);
	alo_StkId o = index2addr(T, index);
	const alo_TVal* tm;
	if (s->ftagname) { /* inner tagged method. */
		if (s->extra < ALO_NUMTM) {
			tm = lookup ? aloT_fastgetx(T, o, s->extra) : aloT_fastget(T, o, s->extra);
		}
		else {
			tm = aloT_gettm(T, o, s->extra, lookup);
		}
		if (tm && !tisnil(tm)) {
			tsetobj(T, T->top++, tm);
		}
		else {
			return ALO_TNIL;
		}
	}
	else {
		tsetstr(T, api_incrtop(T), s); /* bind string into thread */
		if ((tm = aloT_lookup(T, o, T->top - 1)) && !tisnil(tm)) {
			tsetobj(T, T->top - 1, tm);
		}
		else {
			api_decrtop(T);
			return ALO_TNIL;
		}
	}
	return ttpnv(tm);
}

int alo_getdelegate(alo_State T, a_isize index) {
	api_checkslots(T, 1);
	alo_StkId o = index2addr(T, index);
	switch (ttype(o)) {
	case ALO_VACL:
		tsetobj(T, api_incrtop(T), tgetacl(o)->array[0]->p);
		return true;
	case ALO_VCCL:
		tsetobj(T, api_incrtop(T), tgetccl(o)->array);
		return true;
	default:
		return false;
	}
}

a_mem alo_newdata(alo_State T, a_usize size) {
	api_checkslots(T, 1);
	alo_User* value = aloS_newr(T, size);
	tsetrdt(T, api_incrtop(T), value);
	aloG_check(T);
	return value->array;
}

void alo_newtuple(alo_State T, a_usize size) {
	api_checkelems(T, size);
	alo_Tuple* value = aloA_new(T, size, T->top - size);
	T->top -= size;
	tsettup(T, api_incrtop(T), value);
	aloG_check(T);
}

void alo_newlist(alo_State T, a_usize size) {
	api_checkslots(T, 1);
	alo_List* value = aloI_new(T);
	tsetlis(T, api_incrtop(T), value);
	if (size > 0) { /* call 'ensure' function if initialize capacity is not 0 */
		aloI_ensure(T, value, size);
	}
	aloG_check(T);
}

void alo_newtable(alo_State T, a_usize size) {
	api_checkslots(T, 1);
	atable_t* value = aloH_new(T);
	tsettab(T, api_incrtop(T), value);
	if (size > 0) { /* call 'ensure' function if initialize capacity is not 0 */
		aloH_ensure(T, value, size);
	}
	aloG_check(T);
}

/**
 ** give a hint of how many size of extra elements will be put into the collection.
 */
void alo_sizehint(alo_State T, a_isize index, a_usize size) {
	if (size == 0) /* skip check when no extra elements */
		return;
	alo_TVal* t = index2addr(T, index);
	switch (ttpnv(t)) {
	case ALO_TLIST:
		aloI_ensure(T, taslis(t), size);
		break;
	case ALO_TTABLE:
		aloH_ensure(T, tastab(t), size);
		break;
	default:
		aloi_check(T, false, "invalid collection.");
		break;
	}
}

/**
 ** trim object, and make it takes lower memory cost.
 */
void alo_trim(alo_State T, a_isize idown) {
	alo_StkId o = index2addr(T, idown);
	switch (ttype(o)) {
	case ALO_TLIST:
		aloI_trim(T, taslis(o));
		break;
	case ALO_TTABLE:
		aloH_trim(T, tastab(o));
		break;
	}
}

/**
 ** trim object
 */
void alo_triml(alo_State T, a_isize idown, size_t len) {
	alo_StkId o = index2addr(T, idown);
	alo_List* list = taslis(o);
	aloE_assert(len <= list->length, "the length should less than current length.");
	list->length = len;
	aloI_trim(T, list); /* trim list size. */
}

void alo_rawsetx(alo_State T, a_isize idown, int drop) {
	api_checkelems(T, 2);
	alo_StkId o = index2addr(T, idown);
	alo_StkId k = T->top - 2;
	alo_StkId v = T->top - 1;
	switch (ttpnv(o)) {
	case ALO_TLIST: {
		alo_List* owner = taslis(o);
		alo_TVal* v1 = aloI_find(T, owner, k);
		aloG_barrierbackt(T, owner, v);
		if (drop) {
			tsetobj(T, k, v1);
		}
		tsetobj(T, v1, v);
		break;
	}
	case ALO_TTABLE: {
		atable_t* owner = tastab(o);
		alo_TVal* v1 = aloH_find(T, owner, k);
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
		aloi_check(T, false, "illegal owner for 'rawset'");
		break;
	}
	}
	T->top -= drop ? 1 : 2;
	aloG_check(T);
}

void alo_rawseti(alo_State T, a_isize idown, a_int key) {
	api_checkelems(T, 1);
	alo_StkId o = index2addr(T, idown);
	alo_StkId v = T->top - 1;
	switch (ttpnv(o)) {
	case ALO_TLIST: {
		aloI_seti(T, taslis(o), key, v);
		break;
	}
	case ALO_TTABLE: {
		atable_t* owner = tastab(o);
		alo_TVal k = tnewint(key);
		aloH_set(T, owner, &k, v);
		break;
	}
	default: {
		aloi_check(T, false, "illegal owner for 'rawseti'");
		break;
	}
	}
	T->top -= 1;
	aloG_check(T);
}

void alo_rawsets(alo_State T, a_isize idown, a_cstr key) {
	api_checkelems(T, 1);
	alo_StkId o = index2addr(T, idown);
	alo_StkId v = T->top - 1;
	aloi_check(T, tistab(o), "illegal owner for 'rawsets'");
	atable_t* table = tastab(o);
	tsetobj(T, aloH_finds(T, table, key, strlen(key)), v);
	aloG_barrierbackt(T, table, v);
	T->top -= 1;
	aloG_check(T);
}

int alo_rawrem(alo_State T, a_isize idown) {
	api_checkelems(T, 1);
	alo_StkId o = index2addr(T, idown);
	alo_StkId k = T->top - 1;
	int succ;
	switch (ttpnv(o)) {
	case ALO_TLIST: {
		succ = aloI_remove(T, taslis(o), k, k);
		break;
	}
	case ALO_TTABLE: {
		succ = aloH_remove(T, tastab(o), k, k);
		break;
	}
	default: {
		aloi_check(T, false, "illegal owner for 'remove'");
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

void alo_rawclr(alo_State T, a_isize idown) {
	alo_StkId o = index2addr(T, idown);
	switch (ttpnv(o)) {
	case ALO_TLIST: {
		aloI_clear(T, taslis(o));
		break;
	}
	case ALO_TTABLE: {
		aloH_clear(T, tastab(o));
		break;
	}
	default: {
		aloi_check(T, false, "illegal owner for 'clear'");
		break;
	}
	}
}

void alo_add(alo_State T, a_isize idown) {
	api_checkelems(T, 1);
	alo_StkId o = index2addr(T, idown);
	aloi_check(T, tislis(o), "illegal owner for 'add'");
	aloI_add(T, taslis(o), T->top - 1);
	api_decrtop(T);
}

void alo_setx(alo_State T, a_isize idown, int dodrop) {
	api_checkelems(T, 2);
	alo_StkId o = index2addr(T, idown);
	const alo_TVal* tm = aloT_fastgetx(T, o, TM_SET);
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

int alo_remove(alo_State T, a_isize idown) {
	api_checkelems(T, 2);
	alo_StkId o = index2addr(T, idown);
	const alo_TVal* tm = aloT_gettm(T, o, TM_REM, true);
	if (tm) { /* call tagged method */
		tsetobj(T, T->top + 1, T->top - 1);
		tsetobj(T, T->top    , o);
		tsetobj(T, T->top - 1, tm);
		T->top += 2;
		ptrdiff_t res = getstkoff(T, T->top - 3);
		aloD_callnoyield(T, o, ALO_MULTIRET);
		alo_StkId r = putstkoff(T, res);
		return T->top != r ? /* successfully removed value? */
				ttpnv(r) : /* get object type */
				ALO_TUNDEF; /* return 'none' */
	}
	else {
		return alo_rawrem(T, idown);
	}
}

int alo_setmetatable(alo_State T, a_isize index) {
	alo_TVal* o = index2addr(T, index);
	atable_t* mt;
	if (tisnil(T->top - 1)) {
		mt = NULL;
	}
	else {
		aloi_check(T, tistab(T->top - 1), "value is not meta table.");
		mt = tastab(T->top - 1);
	}
	atable_t** pmt = aloT_getpmt(o);
	if (pmt == NULL) {
		T->top -= 1;
		return false;
	}
	*pmt = mt;
	alo_Obj* g = tasobj(o);
	aloG_barrier(T, g, mt);
	aloG_checkfnzobj(T, g, mt);
	T->top -= 1;
	return true;
}

int alo_setdelegate(alo_State T, a_isize index) {
	api_checkelems(T, 1);
	alo_StkId o = index2addr(T, index);
	const alo_TVal* t = api_decrtop(T);
	if (tisnil(t)) {
		t = T->frame->env;
	}
	aloi_check(T, tistab(t), "delegate should be table");
	switch (ttype(o)) {
	case ALO_VACL: {
		alo_ACl* closure = tgetacl(o);
		tsetobj(T, closure->array[0]->p, t);
		aloG_barriert(T, closure, t);
		return true;
	}
	case ALO_VCCL: {
		alo_CCl* closure = tgetccl(o);
		tsetobj(T, closure->array, t);
		aloG_barriert(T, closure, t);
		return true;
	}
	default:
		return false;
	}
}

void alo_callk(alo_State T, int narg, int nres, a_kfun kfun, a_kctx kctx) {
	aloi_check(T, kfun == NULL || !T->frame->falo, "can not use continuation inside hooks");
	api_checkelems(T, 1 + narg);
	aloi_check(T, T->g->trun == T, "thread is not running.");
	aloi_check(T, T->status == ALO_STOK, "thread is in non-normal state.");
	alo_StkId fun = T->top - (narg + 1);
	if (kfun && T->nxyield == 0) { /* is available for continuation? */
		aframe_t* const frame = T->frame;
		frame->c.kfun = kfun;
		frame->c.kctx = kctx;
		aloD_call(T, fun, nres); /* do call */
	}
	else { /* in non-yieldable environment or not yieldable */
		aloD_callnoyield(T, fun, nres); /* do call */
	}
	aloD_adjustresult(T, nres);
}

int alo_pcallk(alo_State T, int narg, int nres, ssize_t errfun, a_kfun kfun, a_kctx kctx) {
	aloi_check(T, kfun == NULL || !T->frame->falo, "can not use continuation inside hooks");
	api_checkelems(T, 1 + narg);
	aloi_check(T, T->g->trun == T, "thread is not running.");
	aloi_check(T, T->status == ALO_STOK, "thread is in non-normal state.");
	aloi_check(T, errfun == ALO_NOERRFUN || errfun == ALO_LASTERRFUN || isinstk(errfun),
			"illegal error function index.");
	aframe_t* const frame = T->frame;
	alo_StkId fun = T->top - (narg + 1);
	ptrdiff_t ef = errfun == ALO_NOERRFUN ? 0 :
			errfun == ALO_LASTERRFUN ? T->errfun :
			getstkoff(T, index2addr(T, errfun));
	int status;
	if (kfun == NULL || T->nxyield > 0) {
		status = aloD_pcall(T, fun, nres, ef); /* call function in protection */
	}
	else {
		frame->c.kfun = kfun;
		frame->c.kctx = kctx;
		frame->c.oef = T->errfun;
		T->errfun = ef;
		frame->fypc = true;
		aloD_call(T, fun, nres); /* call function in protection */
		frame->fypc = false;
		T->errfun = frame->c.oef;
		status = ALO_STOK;
	}
	aloD_adjustresult(T, nres);
	return status;
}

/**
 ** create a new closure and push into top of stack, used to avoid memory error.
 */
static void pnewclosure(alo_State T, void* context) {
	alo_ACl** p = aloE_cast(alo_ACl**, context);
	*p = aloF_new(T, 1);
	tsetacl(T, api_incrtop(T), *p); /* put closure into stack to avoid GC */
}

int alo_compile(alo_State T, a_cstr name, a_cstr src, alo_Reader reader, void* context) {
	alo_ACl* c = NULL;
	int status = aloD_prun(T, pnewclosure, &c);
	if (status != ALO_STOK) {
		return status;
	}
	alo_Str* s;
	aibuf_t buf;
	aloB_iopen(&buf, reader, context);
	status = aloP_parse(T, src, &buf, &c->proto, &s);
	if (status == ALO_STOK) { /* compile success */
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

int alo_load(alo_State T, a_cstr src, alo_Reader reader, void* context) {
	alo_ACl* c = aloF_new(T, 0);
	tsetacl(T, api_incrtop(T), c);
	int status = aloZ_load(T, &c->proto, src, reader, context);
	if (status != ALO_STOK) {
		api_decrtop(T);
	}
	return status;
}

int alo_save(alo_State T, alo_Writer writer, void* context, int debug) {
	alo_Closure* c = tasclo(T->top - 1);
	aloE_assert(tisacl(c), "not a Alopecurus closure");
	return aloZ_save(T, c->a.proto, writer, context, debug);
}

a_none alo_error(alo_State T) {
	api_checkelems(T, 1);
	aloU_error(T, ALO_STERRRT);
}

a_none alo_throw(alo_State T) {
	api_checkelems(T, 1);
	aloD_throw(T, ALO_STERRRT);
}

a_usize alo_gcconf(alo_State T, int what, a_usize data) {
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

void alo_fullgc(alo_State T) {
	aloG_fullgc(T, GCKindNormal);
}

void alo_checkgc(alo_State T) {
	aloG_check(T);
}
