/*
 * alis.c
 *
 *  Created on: 2019年7月26日
 *      Author: ueyudiud
 */

#include "alis.h"
#include "agc.h"
#include "adebug.h"
#include "avm.h"

/**
 ** create new list value.
 */
alist_t* aloI_new(astate T) {
	alist_t* value = aloM_newo(T, alist_t);
	aloG_register(T, value, ALO_TLIST);
	value->array = NULL;
	value->capacity = 0;
	value->length = 0;
	value->metatable = NULL;
	return value;
}

/**
 ** ensure list can store extra size of elements.
 */
void aloI_ensure(astate T, alist_t* self, size_t size) {
	aloM_ensbx(T, self->array, self->capacity, self->length, size, ALO_MAX_BUFSIZE, tsetnil);
}

/**
 ** trim list capacity to length
 */
void aloI_trim(astate T, alist_t* self) {
	if (self->length != self->capacity) { /* reallocate array if capacity is longer than length */
		aloM_adjb(T, self->array, self->capacity, self->length);
	}
}

/**
 ** get value by index.
 */
const atval_t* aloI_geti(alist_t* self, aint index) {
	if (index < 0) {
		index += self->length;
	}
	if (index < 0 || index >= self->length)
		return aloO_tnil;
	return self->array + index;
}

const atval_t* aloI_get(__attribute__((unused)) astate T, alist_t* self, const atval_t* index) {
	aint v;
	return aloV_toint(index, v) ? aloI_geti(self, v) : aloO_tnil;
}

/**
 ** add element at the end of list.
 */
void aloI_add(astate T, alist_t* self, const atval_t* value) {
	aloM_chkbx(T, self->array, self->capacity, self->length, ALO_MAX_BUFSIZE, tsetnil);
	tsetobj(T, self->array + self->length++, value);
}

/**
 ** add element if value is not present.
 */
int aloI_put(astate T, alist_t* self, const atval_t* value) {
	for (size_t i = 0; i < self->length; ++i) {
		if (aloV_equal(T, self->array + i, value)) { /* match value */
			return false;
		}
	}
	/* value does not exist, add into list */
	aloI_add(T, self, value);
	return true;
}

/**
 ** add all elements at the end of list.
 */
void aloI_addall(astate T, alist_t* self, const atval_t* src, size_t len) {
	aloI_ensure(T, self, len);
	atval_t* t = self->array + self->length;
	for (size_t i = 0; i < len; ++i) {
		tsetobj(T, t + i, src + i);
	}
	self->length += len;
}

void aloI_seti(astate T, alist_t* self, aint index, const atval_t* value, atval_t* out) {
	if (index < 0) {
		index += self->length;
	}
	if (index < 0 || index > self->length) {
		aloU_outofrange(T, index, self->length);
	}
	if (index == self->length) {
		aloI_add(T, self, value);
		if (out) {
			tsetnil(out);
		}
	}
	else {
		if (out) {
			tsetobj(T, out, self->array + index);
		}
		tsetobj(T, self->array + index, value);
	}
	aloG_barrierbackt(T, self, value);
}

void aloI_set(astate T, alist_t* self, const atval_t* index, const atval_t* value, atval_t* out) {
	aint v;
	if (!aloV_toint(index, v)) {
		aloU_invalidkey(T);
	}
	aloI_seti(T, self, v, value, out);
}

int aloI_remove(astate T, alist_t* self, const atval_t* index, atval_t* out) {
	aint i;
	if (!aloV_toint(index, i)) {
		return false;
	}
	if (i < 0) {
		i += self->length;
	}
	if (i < 0 || i > self->length) { /* index out of bound? */
		return false;
	}
	else {
		if (out) {
			tsetobj(T, out, self->array + i);
		}
		for (size_t j = i + 1; j < self->length; ++j) {
			tsetobj(T, self->array + j - 1, self->array + j);
		}
		self->length--;
		return true;
	}
}

/**
 ** iterate to next element.
 */
const atval_t* aloI_next(alist_t* self, ptrdiff_t* poff) {
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
