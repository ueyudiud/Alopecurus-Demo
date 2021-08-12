/*
 * atup.c
 *
 *  Created on: 2019年7月30日
 *      Author: ueyudiud
 */

#define ATUP_C_
#define ALO_CORE

#include "atup.h"
#include "agc.h"
#include "adebug.h"
#include "avm.h"

/**
 ** create a new tuple value.
 */
alo_Tuple* aloA_new(alo_State T, size_t len, const alo_TVal* src) {
	alo_Tuple* value = aloE_cast(alo_Tuple*, aloM_malloc(T, atupsizel(len)));
	aloG_register(T, value, ALO_TTUPLE);
	value->length = len;
	for (size_t i = 0; i < len; ++i) {
		tsetobj(T, value->array + i, src + i);
	}
	return value;
}

const alo_TVal* aloA_geti(alo_Tuple* self, a_int index) {
	if (index < 0) {
		index += self->length;
	}
	if (index < 0 || index >= self->length)
		return aloO_tnil;
	return self->array + index;
}

const alo_TVal* aloA_get(__attribute__((unused)) alo_State T, alo_Tuple* self, const alo_TVal* index) {
	a_int v;
	if (aloV_toint(index, v)) {
		if (v < 0) {
			v += self->length;
		}
		if (v < 0 || v >= self->length) {
			return aloO_tnil;
		}
		return self->array + v;
	}
	return aloO_tnil;
}

/**
 ** iterate to next element.
 */
const alo_TVal* aloA_next(alo_Tuple* self, ptrdiff_t* poff) {
	aloE_assert(*poff >= -1, "illegal offset.");
	ptrdiff_t off = ++(*poff);
	if (off >= self->length) { /* iterating already ended */
		*poff = self->length;
		return NULL;
	}
	return &self->array[off];
}

/**
 ** get hash code of tuple.
 */
a_hash aloA_hash(alo_State T, alo_Tuple* self) {
	a_hash h = 1;
	for (size_t i = 0; i< self->length; ++i) {
		h = h * 31 + aloV_hashof(T, &self->array[i]);
	}
	return h;
}
