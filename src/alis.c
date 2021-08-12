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
alo_List* aloI_new(alo_State T) {
	alo_List* value = aloM_newo(T, alo_List);
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
void aloI_ensure(alo_State T, alo_List* self, size_t size) {
	aloM_ensbx(T, self->array, self->capacity, self->length, size, ALO_MAX_BUFSIZE, tsetnil);
}

/**
 ** trim list capacity to length
 */
void aloI_trim(alo_State T, alo_List* self) {
	if (self->length != self->capacity) { /* reallocate array if capacity is longer than length */
		aloM_adjb(T, self->array, self->capacity, self->length);
	}
}

/**
 ** get value by index.
 */
const alo_TVal* aloI_geti(alo_List* self, a_int index) {
	if (index < 0) {
		index += self->length;
	}
	if (index < 0 || index >= aloE_int(self->length))
		return aloO_tnil;
	return self->array + index;
}

const alo_TVal* aloI_get(__attribute__((unused)) alo_State T, alo_List* self, const alo_TVal* index) {
	a_int v;
	return aloV_toint(index, v) ? aloI_geti(self, v) : aloO_tnil;
}

/**
 ** ensure list capacity and return end of list.
 */
static void ensure(alo_State T, alo_List* self) {
	aloM_chkbx(T, self->array, self->capacity, self->length, ALO_MAX_BUFSIZE, tsetnil);
}

/**
 ** add element at the end of list.
 */
void aloI_add(alo_State T, alo_List* self, const alo_TVal* value) {
	ensure(T, self);
	tsetobj(T, self->array + self->length++, value);
}

/**
 ** add element if value is not present.
 */
int aloI_put(alo_State T, alo_List* self, const alo_TVal* value) {
	for (size_t i = 0; i < self->length; ++i) {
		if (aloV_equal(T, self->array + i, value)) { /* match value */
			return false;
		}
	}
	/* value does not exist, add into list */
	aloI_add(T, self, value);
	return true;
}

alo_TVal* aloI_findi(alo_State T, alo_List* self, a_int index) {
	if (index < 0) {
		index += self->length;
	}
	if (index < 0 || index > aloE_int(self->length)) {
		aloU_outofrange(T, index, self->length);
	}
	if (index == aloE_int(self->length)) {
		ensure(T, self);
		return self->array + self->length++;
	}
	return self->array + index;
}

alo_TVal* aloI_find(alo_State T, alo_List* self, const alo_TVal* index) {
	a_int v;
	if (!aloV_toint(index, v)) {
		aloU_invalidkey(T);
	}
	return aloI_findi(T, self, v);
}

void aloI_removei(__attribute__((unused)) alo_State T, alo_List* self, a_int index, alo_TVal* out) {
	aloE_assert(0 <= index && index < aloE_cast(a_int, self->length), "list index out of bound.");
	if (out) {
		tsetobj(T, out, self->array + index);
	}
	for (size_t j = index + 1; j < self->length; ++j) {
		tsetobj(T, self->array + j - 1, self->array + j);
	}
	self->length--;
}

int aloI_remove(alo_State T, alo_List* self, const alo_TVal* index, alo_TVal* out) {
	a_int i;
	if (!aloV_toint(index, i)) {
		return false;
	}
	if (i < 0) {
		i += self->length;
	}
	if (i < 0 || i > aloE_int(self->length)) { /* index out of bound? */
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
const alo_TVal* aloI_next(alo_List* self, ptrdiff_t* poff) {
	aloE_assert(*poff >= -1, "illegal offset.");
	ptrdiff_t off = ++(*poff);
	if (off >= aloE_int(self->length)) { /* iterating already ended */
		*poff = self->length;
		return NULL;
	}
	return self->array + off;
}
