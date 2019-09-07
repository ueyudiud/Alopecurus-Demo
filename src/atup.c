/*
 * atup.c
 *
 *  Created on: 2019年7月30日
 *      Author: ueyudiud
 */

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
	if (*poff < 0) { /* negative offset means iterating already ended */
		return NULL;
	}
	ptrdiff_t off = *poff;
	if (off == self->length) { /* no element remains? */
		*poff = -1;
		return NULL;
	}
	const atval_t* result = self->array + off++;
	*poff = off;
	return result;
}
