/*
 * avm.c
 *
 *  Created on: 2019年7月23日
 *      Author: ueyudiud
 */

#define AVM_C_
#define ALO_CORE

#include "aop.h"
#include "astr.h"
#include "atup.h"
#include "alis.h"
#include "atab.h"
#include "afun.h"
#include "abuf.h"
#include "ameta.h"
#include "agc.h"
#include "adebug.h"
#include "avm.h"
#include "ado.h"
#include "aeval.h"

#include <math.h>

/**
 ** push format string to top of stack
 */
a_cstr aloV_pushfstring(alo_State T, a_cstr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	a_cstr s = aloV_pushvfstring(T, fmt, varg);
	va_end(varg);
	return s;
}

/**
 ** push format string to top of stack, the va_list version
 */
a_cstr aloV_pushvfstring(alo_State T, a_cstr fmt, va_list varg) {
	api_checkslots(T, 1);
	aloB_decl(T, buf);
	alo_vformat(T, aloB_bwrite, &buf, fmt, varg);
	alo_Str* value = aloB_tostr(T, buf);
	aloB_close(T, buf);
	tsetstr(T, api_incrtop(T), value);
	return value->array;
}

a_cstr aloV_typename(alo_State T, const alo_TVal* o) {
	const alo_TVal* name = aloT_gettm(T, o, TM_ID, false);
	if (name && tisstr(name)) {
		return tasstr(name)->array;
	}
	return aloT_typenames[ttpnv(o)];
}

/**
 ** try convert a value to integer, rounding with mode:
 ** mode < 0 : floor to integer
 ** mode = 0 : only accept integral value
 ** mode > 0 : ceiling to integer
 */
int aloV_tointx(const alo_TVal* in, a_int* out, int mode) {
	alo_TVal t;
	find:
	switch (ttpnv(in)) {
	case ALO_TINT: {
		*out = tasint(in);
		return true;
	}
	case ALO_TFLOAT: {
		return aloO_flt2int(tasflt(in), out, mode);
	}
	case ALO_TSTR: {
		if (aloO_str2num(tasstr(in)->array, &t)) {
			in = &t;
			goto find;
		}
		return false;
	}
	default:
		return false;
	}
}

/**
 ** try convert a value to float.
 */
int aloV_tofltx(const alo_TVal* in, a_float* out) {
	alo_TVal t;
	find:
	switch (ttpnv(in)) {
	case ALO_TINT: {
		*out = aloE_flt(tasint(in));
		return true;
	}
	case ALO_TFLOAT: {
		*out = tasflt(in);
		return true;
	}
	case ALO_TSTR: {
		if (aloO_str2num(tasstr(in)->array, &t)) {
			in = &t;
			goto find;
		}
		return false;
	}
	default:
		return false;
	}
}

a_hash aloV_hashof(alo_State T, const alo_TVal* o) {
	const alo_TVal* hashf = aloT_fastgetx(T, o, TM_HASH);
	if (hashf) {
		aloT_vmput2(T, hashf, o);
		aloD_callnoyield(T, T->top - 2, 1);
		T->top -= 1;
		if (!tisint(T->top)) {
			aloU_rterror(T, "'__hash' must apply integer value.");
		}
		return tasint(T->top);
	}
	switch (ttpnv(o)) {
	case ALO_TNIL   : return 0;
	case ALO_TBOOL  : return aloO_boolhash(tasbool(o));
	case ALO_TINT   : return aloO_inthash(tasint(o));
	case ALO_TFLOAT : {
		a_int i;
		if (aloO_flt2int(tasflt(o), &i, 0)) {
			return aloO_inthash(i);
		}
		return aloO_flthash(tasflt(o));
	}
	case ALO_TSTR: return aloS_hash(T, tasstr(o));
	case ALO_TTUPLE : return aloA_hash(T, tastup(o));
	default         : return aloE_addr(o->v.p);
	}
}

/**
 ** concatenate string.
 */
alo_Str* aloV_rawcat(alo_State T, size_t len) {
	aloB_decl(T, buf);
	for (size_t i = len; i > 0; i--) {
		aloO_tostring(T, aloB_bwrite, &buf, T->top - i);
	}
	alo_Str* value = aloB_tostr(T, buf);
	aloB_close(T, buf);
	T->top -= len;
	return value;
}

/**
 ** concatenate objects.
 */
void aloV_concat(alo_State T, size_t len) {
	alo_StkId in = T->top - len;
	const alo_TVal* tm = aloT_gettm(T, in, TM_CAT, true);
	if (tm) {
		for (size_t i = len; i > 0; --i) {
			tsetobj(T, in + i, in + i - 1);
		}
		tsetobj(T, in, tm);
		T->top += 1;
		aloD_call(T, in, 1);
	}
	else {
		alo_Str* value = aloV_rawcat(T, len);
		tsetstr(T, T->top++, value);
	}
}


/**
 ** compare between two objects.
 */
int aloV_compare(alo_State T, const alo_TVal* t1, const alo_TVal* t2, int op) {
	switch (op) {
	case ALO_OPEQ:
		return aloV_equal(T, t1, t2);
	case ALO_OPLT: case ALO_OPLE: {
		int flag;
		if (aloV_cmpop(T, op, &flag, t1, t2)) {
			return flag;
		}
		break;
	}
	default:
		aloE_xassert(false);
	}
	const alo_TVal* tm = aloT_gettm(T, t1, op - ALO_OPLT + TM_LT, true);
	if (tm == NULL) {
		aloU_rterror(T, "can not compare between two objects");
	}
	T->frame->mode = FrameModeCompare;
	int result = aloT_callcmp(T, tm, t1, t2);
	T->frame->mode = FrameModeNormal;
	return result;
}

/**
 ** check two objects are equal.
 */
int aloV_equal(alo_State T, const alo_TVal* t1, const alo_TVal* t2) {
	if (ttype(t1) == ttype(t2)) {
		switch (ttype(t1)) {
		case ALO_TNIL    : return true;
		case ALO_TBOOL   : return tasbool(t1) == tasbool(t2);
		case ALO_TINT    : return tasint(t1) == tasint(t2);
		case ALO_TFLOAT  : return tasflt(t1) == tasflt(t2);
		case ALO_TPTR: return tasptr(t1) == tasptr(t2);
		case ALO_VHSTR: return aloS_hequal(tasstr(t1), tasstr(t2));
		case ALO_VISTR: return tasstr(t1) == tasstr(t2);
		case ALO_VLCF    : return taslcf(t1) == taslcf(t2);
		default          : {
			if (tasobj(t1) == tasobj(t2)) /* if two reference are equal, return true */
				return true;
			if (T == NULL) /* if no environment for meta method (equivalent to raw equal stop here), return false */
				return false;
			/* call meta method */
			const alo_TVal* meta = aloT_fastgetx(T, t1, TM_EQ);
			return meta && aloT_callcmp(T, meta, t1, t2);
		}
		}
	}
	else if (tisnum(t1) && tisnum(t2)) { /* check number */
		a_int v1, v2;
		return aloV_toint(t1, v1) && aloV_toint(t2, v2) && v1 == v2;
	}
	return false;
}

/**
 ** get length of object, the result value is integer expected.
 */
size_t aloV_length(alo_State T, const alo_TVal* t) {
	switch (ttype(t)) {
	case ALO_VHSTR:
		return tasstr(t)->lnglen;
	case ALO_VISTR:
		return tasstr(t)->shtlen;
	case ALO_TLIST  : {
		alo_List* v = taslis(t);
		if (aloT_trylen(T, t, v->metatable))
			break;
		return v->length;
	}
	case ALO_TTABLE : {
		atable_t* v = tastab(t);
		if (aloT_trylen(T, t, v->metatable))
			break;
		return v->length;
	}
	default: {
		if (!aloT_trylen(T, t, aloT_getmt(t)))
			aloU_mnotfound(T, t, "len");
		break;
	}
	}
	if (!tisint(T->top))
		aloU_rterror(T, "length should be integer value.");
	return tasint(T->top);
}

static int tuple_iterator(alo_State T) {
	alo_CCl* c = tgetccl(T->frame->fun);
	const alo_TVal* result = aloA_next(tastup(c->array), aloE_cast(ptrdiff_t*, &trefint(c->array + 1)));
	if (result) {
		tsetobj(T, api_incrtop(T), result);
		return 1;
	}
	return 0;
}

static int list_iterator(alo_State T) {
	alo_CCl* c = tgetccl(T->frame->fun);
	const alo_TVal* result = aloI_next(taslis(c->array), aloE_cast(ptrdiff_t*, &trefint(c->array + 1)));
	if (result) {
		tsetobj(T, api_incrtop(T), result);
		return 1;
	}
	return 0;
}

static int table_iterator(alo_State T) {
	alo_CCl* c = tgetccl(T->frame->fun);
	const alo_Entry* result = aloH_next(tastab(c->array), aloE_cast(ptrdiff_t*, &trefint(c->array + 1)));
	if (result) {
		tsetobj(T, api_incrtop(T), amkey(result));
		tsetobj(T, api_incrtop(T), amval(result));
		return 2;
	}
	return 0;
}

/**
 ** create an base iterator by object.
 */
void aloV_iterator(alo_State T, const alo_TVal* in, alo_TVal* out) {
	a_cfun handle;
	switch (ttpnv(in)) {
	case ALO_TTUPLE: handle = tuple_iterator; break;
	case ALO_TLIST : handle = list_iterator;  break;
	case ALO_TTABLE: handle = table_iterator; break;
	case ALO_TFUNC:
		tsetobj(T, out, in);
		return;
	default:
		aloU_rterror(T, "the value is not iterable.");
	}
	alo_CCl* c = aloF_newc(T, handle, 2);
	tsetobj(T, c->array, in);
	tsetint(c->array + 1, ALO_ITERATE_BEGIN);
	tsetccl(T, out, c);
}
