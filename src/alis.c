/*
 * alis.c
 *
 *  Created on: 2019年7月26日
 *      Author: ueyudiud
 */

#define ALIS_C_
#define ALO_CORE

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
 ** ensure list capacity and return end of list.
 */
static void ensure(astate T, alist_t* self) {
	aloM_chkbx(T, self->array, self->capacity, self->length, ALO_MAX_BUFSIZE, tsetnil);
}

/**
 ** add element at the end of list.
 */
void aloI_add(astate T, alist_t* self, const atval_t* value) {
	ensure(T, self);
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

atval_t* aloI_findi(astate T, alist_t* self, aint index) {
	if (index < 0) {
		index += self->length;
	}
	if (index < 0 || index > self->length) {
		aloU_outofrange(T, index, self->length);
	}
	if (index == self->length) {
		ensure(T, self);
		return self->array + self->length++;
	}
	return self->array + index;
}

atval_t* aloI_find(astate T, alist_t* self, const atval_t* index) {
	aint v;
	if (!aloV_toint(index, v)) {
		aloU_invalidkey(T);
	}
	return aloI_findi(T, self, v);
}

void aloI_removei(astate T, alist_t* self, aint index, atval_t* out) {
	aloE_assert(0 <= index && index < self->length, "list index out of bound.");
	if (out) {
		tsetobj(T, out, self->array + index);
	}
	for (size_t j = index + 1; j < self->length; ++j) {
		tsetobj(T, self->array + j - 1, self->array + j);
	}
	self->length--;
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
		aloI_removei(T, self, i, out);
		return true;
	}
}

/**
 ** iterate to next element.
 */
const atval_t* aloI_next(alist_t* self, ptrdiff_t* poff) {
	aloE_assert(*poff >= -1, "illegal offset.");
	ptrdiff_t off = ++(*poff);
	if (off >= self->length) { /* iterating already ended */
		*poff = self->length;
		return NULL;
	}
	return self->array + off;
}
