/*
 * avm.c
 *
 *  Created on: 2019年7月23日
 *      Author: ueyudiud
 */

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

#include <stdarg.h>
#include <math.h>

/**
 ** push format string to top of stack
 */
astr aloV_pushfstring(astate T, astr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	astr s = aloV_pushvfstring(T, fmt, varg);
	va_end(varg);
	return s;
}

/**
 ** push format string to top of stack, the va_list version
 */
astr aloV_pushvfstring(astate T, astr fmt, va_list varg) {
	api_checkslots(T, 1);
	astring_t* value;

	void builder(astate T, asbuf_t* buf) {
		alo_vformat(T, aloB_bwrite, buf, fmt, varg);
		value = aloS_new(T, buf->array, buf->length);
	}

	aloD_usesb(T, builder);

	tsetstr(T, api_incrtop(T), value);
	return value->array;
}

static void concat_unsafe(astate T, asbuf_t* buf, askid_t from) {
	for (askid_t i = from; i < T->top; ++i) {
		aloO_tostring(T, aloB_bwrite, buf, i);
	}
}

/**
 ** try convert a value to integer, rounding with mode:
 ** mode < 0 : floor to integer
 ** mode = 0 : only accept integral value
 ** mode > 0 : ceiling to integer
 */
int aloV_tointx(const atval_t* in, aint* out, int mode) {
	atval_t t;
	find:
	switch (ttpnv(in)) {
	case ALO_TINT: {
		*out = tgetint(in);
		return true;
	}
	case ALO_TFLOAT: {
		return aloO_flt2int(tgetflt(in), out, mode);
	}
	case ALO_TSTRING: {
		if (aloO_str2num(tgetstr(in)->array, &t)) {
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
int aloV_tofltx(const atval_t* in, afloat* out) {
	atval_t t;
	find:
	switch (ttpnv(in)) {
	case ALO_TINT: {
		*out = aloE_flt(tgetint(in));
		return true;
	}
	case ALO_TFLOAT: {
		*out = tgetflt(in);
		return true;
	}
	case ALO_TSTRING: {
		if (aloO_str2num(tgetstr(in)->array, &t)) {
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
 ** concat objects.
 */
void aloV_concat(astate T, size_t len) {
	askid_t in = T->top - len;
	const atval_t* tm = aloT_gettm(T, in, TM_CAT, true);
	if (tm) {
		for (size_t i = len; i > 0; --i) {
			tsetobj(T, in + i, in + i - 1);
		}
		tsetobj(T, in, tm);
		T->top += 1;
		aloD_call(T, in, 1);
	}
	else {
		void builder(astate T, asbuf_t* buf) {
			concat_unsafe(T, buf, in);
			tsetstr(T, in, aloS_new(T, buf->array, buf->length));
			T->top = in + 1;
		}

		aloD_usesb(T, builder);
	}
}


/**
 ** compare between two objects.
 */
int aloV_compare(astate T, const atval_t* t1, const atval_t* t2, int op) {
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
	const atval_t* tm = aloT_gettm(T, t1, op - ALO_OPLT + TM_LT, true);
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
int aloV_equal(astate T, const atval_t* t1, const atval_t* t2) {
	if (ttype(t1) == ttype(t2)) {
		switch (ttype(t1)) {
		case ALO_TNIL    : return true;
		case ALO_TBOOL   : return tgetbool(t1) == tgetbool(t2);
		case ALO_TINT    : return tgetint(t1) == tgetint(t2);
		case ALO_TFLOAT  : return tgetflt(t1) == tgetflt(t2);
		case ALO_TPOINTER: return tgetptr(t1) == tgetptr(t2);
		case ALO_THSTRING: return aloS_hequal(tgetstr(t1), tgetstr(t2));
		case ALO_TISTRING: return tgetstr(t1) == tgetstr(t2);
		case ALO_TTUPLE  : {
			if (tgettup(t1) == tgettup(t2))
				return true;
			break;
		}
		case ALO_TLIST   : {
			if (tgetlis(t1) == tgetlis(t2))
				return true;
			break;
		}
		case ALO_TTABLE  : {
			if (tgettab(t1) == tgettab(t2))
				return true;
			break;
		}
		case ALO_TRAWDATA: {
			if (tgetrdt(t1) == tgetrdt(t2))
				return true;
			break;
		}
		case ALO_TLCF    : return tgetlcf(t1) == tgetlcf(t2);
		case ALO_TCCL    : return tgetclo(t1) == tgetclo(t2);
		case ALO_TACL    : return tgetclo(t1) == tgetclo(t2);
		case ALO_TTHREAD : return tgetthr(t1) == tgetthr(t2);
		default:
			aloE_xassert(false);
			return false;
		}
		if (T == NULL) { /* if no environment for meta method (equivalent to raw equal stop here), return false */
			return false;
		}
		/* call meta method */
		const atval_t* meta = aloT_fastgetx(T, t1, TM_EQ);
		return meta && aloT_callcmp(T, meta, t1, t2);
	}
	else {
		if (ttisnum(t1) && ttisnum(t2)) {
			aint v1, v2;
			return aloV_toint(t1, v1) && aloV_toint(t2, v2) && v1 == v2;
		}
		return false;
	}
}
