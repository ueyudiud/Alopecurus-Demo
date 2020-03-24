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
atuple_t* aloA_new(astate T, size_t len, const atval_t* src) {
	atuple_t* value = aloE_cast(atuple_t*, aloM_malloc(T, atupsizel(len)));
	aloG_register(T, value, ALO_TTUPLE);
	value->length = len;
	for (size_t i = 0; i < len; ++i) {
		tsetobj(T, value->array + i, src + i);
	}
	return value;
}

const atval_t* aloA_geti(atuple_t* self, aint index) {
	if (index < 0) {
		index += self->length;
	}
	if (index < 0 || index >= self->length)
		return aloO_tnil;
	return self->array + index;
}

const atval_t* aloA_get(astate T, atuple_t* self, const atval_t* index) {
	aint v;
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
const atval_t* aloA_next(atuple_t* self, ptrdiff_t* poff) {
	aloE_assert(*poff >= -1, "illegal offset.");
	ptrdiff_t off = ++(*poff);
	if (off >= self->length) { /* iterating already ended */
		*poff = self->length;
		return NULL;
	}
	return self->array + off;
}

/**
 ** get hash code of tuple.
 */
ahash_t aloA_hash(astate T, atuple_t* self) {
	ahash_t h = 1;
	for (size_t i = 0; i< self->length; ++i) {
		h = h * 31 + aloV_hashof(T, &self->array[i]);
	}
	return h;
}
