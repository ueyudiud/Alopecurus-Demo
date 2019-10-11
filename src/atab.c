/*
 * atab.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#include "astr.h"
#include "atab.h"
#include "agc.h"
#include "adebug.h"
#include "avm.h"

#include <math.h>

#define for_each(x,v) \
	aentry_t* const end = (x)->array + (x)->capacity; \
	for (aentry_t* v = (x)->array; v < end; ++v)

#define clearentry(e) (*(e) = (aentry_t) { { .key = aloO_nil }, .value = aloO_nil })

#define isinvalidfltkey(v) (isnan(v))

#define isinvalidkey(o) (ttisnil(o) || (ttisflt(o) && isinvalidfltkey(tgetflt(o))))

/**
 ** create new table value.
 */
atable_t* aloH_new(astate T) {
	atable_t* value = aloM_newo(T, atable_t);
	aloG_register(T, value, ALO_TTABLE);
	value->reserved = aloE_byte(~0);
	value->array = NULL;
	value->lastfree = NULL;
	value->capacity = 0;
	value->length = 0;
	value->metatable = NULL;
	return value;
}

/**
 ** ensure table can store extra size of entries.
 */
void aloH_ensure(astate T, atable_t* self, size_t size) {
	size_t capacity = self->capacity;
	if (aloM_ensbx(T, self->array, self->capacity, self->length, size, ALO_MAX_BUFSIZE, clearentry)) {
		self->lastfree = self->array + capacity;
	}
}

/**
 ** trim table capacity to length
 */
void aloH_trim(astate T, atable_t* self) {
	if (self->length == self->capacity) { /* fulfilled table needb't be trimmed */
		return;
	}
	else if (self->length > 0) { /* check has elements */
		/* merge table entries into a continuous array */
		aentry_t* const end = self->array + self->length;
		aentry_t* i = self->array;
		while (!ttisnil(i) && i < end) {
			i++;
		}
		if (i < end) { /* if not get all of entries */
			aentry_t* j = i++;
			do {
				while (ttisnil(i)) { /* get next non-null entry */
					i++;
				}
				*(j++) = *(i++); /* move entry */
			}
			while (j < end); /* stop if all of entries are get */
			aloE_assert(j < self->array + self->capacity, "some of entries are missing.");
		}
		aloM_adjb(T, self->array, self->capacity, self->length); /* trim to size */
		self->lastfree = self->array + self->capacity;
	}
	else if (self->capacity > 0) { /* delete array if it is non-null */
		aloM_delb(T, self->array, self->capacity);
		self->lastfree = NULL;
	}
}

/**
 ** get value by integer key.
 */
const atval_t* aloH_geti(atable_t* self, aint key) {
	for_each(self, i) {
		if (ttisint(i) && tgetint(i) == key) {
			return amval(i);
		}
	}
	return aloO_tnil;
}

/**
 ** get value by raw string key.
 */
const atval_t* aloH_gets(atable_t* self, astr src, size_t len) {
	for_each(self, i) {
		if (ttisstr(i) && aloS_requal(tgetstr(i), src, len)) {
			return amval(i);
		}
	}
	return aloO_tnil;
}

/**
 ** get value by interned string key.
 */
const atval_t* aloH_getis(atable_t* self, astring_t* key) {
	aloE_assert(ttisistr(key), "key should be interned string.");
	for_each(self, i) {
		if (ttisistr(i) && tgetstr(i) == key) {
			return amval(i);
		}
	}
	return aloO_tnil;
}

static const atval_t* aloH_geths(atable_t* self, astring_t* key) {
	aloE_assert(ttishstr(key), "key should be heaped string.");
	for_each(self, i) {
		if (ttishstr(i) && aloS_hequal(tgetstr(i), key)) {
			return amval(i);
		}
	}
	return aloO_tnil;
}

/**
 ** get value by string object.
 */
const atval_t* aloH_getxs(atable_t* self, astring_t* key) {
	return ttisistr(key) ? aloH_getis(self, key) : aloH_geths(self, key);
}

static const atval_t* getgeneric(astate T, atable_t* self, const atval_t* key) {
	for_each(self, i) {
		if (aloV_equal(T, amkey(i), key)) {
			return amval(i);
		}
	}
	return aloO_tnil;
}

/**
 ** get value by tagged value.
 */
const atval_t* aloH_get(astate T, atable_t* self, const atval_t* key) {
	switch (ttype(key)) {
	case ALO_TNIL:
		return aloO_tnil;
	case ALO_TINT:
		return aloH_geti(self, tgetint(key));
	case ALO_TFLOAT: {
		if (isinvalidfltkey(tgetflt(key)))
			return aloO_tnil;
		aint i;
		if (aloV_tointx(key, &i, 0)) { /* try cast float to integer value. */
			aloH_geti(self, i);
		}
		break;
	}
	case ALO_THSTRING:
		return aloH_geths(self, tgetstr(key));
	case ALO_TISTRING:
		return aloH_getis(self, tgetstr(key));
	}
	return getgeneric(T, self, key);
}

/**
 ** place new entry in the table.
 */
static aentry_t* newentry(astate T, atable_t* self) {
	size_t capacity = self->capacity;
	if (aloM_chkbx(T, self->array, self->capacity, self->length, ALO_MAX_BUFSIZE, clearentry)) {
		self->lastfree = self->array + capacity; /* correct last free pointer */
	}
	else while (!ttisnil(self->lastfree)) {
		self->lastfree++;
	}
	self->length++;
	return self->lastfree++;
}

/**
 ** find a value slot by key, or create a new slot if not exist.
 */
atval_t* aloH_find(astate T, atable_t* self, const atval_t* key) {
	if (isinvalidkey(key)) { /* illegal key? */
		aloU_invalidkey(T);
	}
	for_each(self, i) { /* tried to find key */
		if (aloV_equal(T, amkey(i), key)) { /* key find? */
			return amval(i);
		}
	}
	aentry_t* entry = newentry(T, self);
	tsetobj(T, amkey(entry), key);
	aloE_assert(ttisnil(amval(entry)), "uninitialized entry value should be nil.");
	aloG_barrierbackt(T, self, key);
	return amval(entry);
}

/**
 ** put key-value pair into table and set old value to 'out' if it is not NULL.
 */
void aloH_set(astate T, atable_t* self, const atval_t* key, const atval_t* value, atval_t* out) {
	atval_t* t = aloH_find(T, self, key);
	if (out) {
		tsetobj(T, out, t);
	}
	tsetobj(T, t, value);
	aloG_barrierbackt(T, self, t);
}

/**
 ** put key-value pair into table, string should be '\0'-terminated string.
 */
astring_t* aloH_sets(astate T, atable_t* self, astr ksrc, size_t klen, const atval_t* value, atval_t* out) {
	for_each(self, i) { /* tried to find key */
		if (ttisstr(i) && aloS_requal(tgetstr(i), ksrc, klen)) { /* key find? */
			if (out) {
				tsetobj(T, out, amval(i));
			}
			tsetobj(T, amval(i), value);
			aloG_barrierbackt(T, self, value);
			return tgetstr(i);
		}
	}
	aentry_t* entry = newentry(T, self);
	astring_t* key = aloS_new(T, ksrc, klen);
	tsetstr(T, amkey(entry), key);
	tsetobj(T, amval(entry), value);
	if (out) {
		tsetnil(out);
	}
	aloG_barrierback(T, self, key);
	aloG_barrierbackt(T, self, value);
	return key;
}

void aloH_setxs(astate T, atable_t* self, astring_t* key, const atval_t* value, atval_t* out) {
	if (ttisistr(key)) {
		for_each(self, i) { /* tried to find key */
			if (ttisistr(i) && tgetstr(i) == key) { /* key find? */
				if (out) {
					tsetobj(T, out, amval(i));
				}
				tsetobj(T, amval(i), value);
				aloG_barrierbackt(T, self, value);
				return;
			}
		}
	}
	else {
		for_each(self, i) { /* tried to find key */
			if (ttisistr(i) && aloS_hequal(tgetstr(i), key)) { /* key find? */
				if (out) {
					tsetobj(T, out, amval(i));
				}
				tsetobj(T, amval(i), value);
				aloG_barrierbackt(T, self, value);
				return;
			}
		}
	}
	aentry_t* entry = newentry(T, self);
	tsetstr(T, amkey(entry), key);
	tsetobj(T, amval(entry), value);
	if (out) {
		tsetnil(out);
	}
	aloG_barrierback(T, self, key);
	aloG_barrierbackt(T, self, value);
}

/**
 ** remove entry by entry index.
 */
void aloH_rawrem(astate T, atable_t* self, size_t index, atval_t* out) {
	aloE_assert(0 <= index && index < self->capacity, "table index out of bound.");
	aentry_t* entry = self->array + index;
	aloE_assert(!ttisnil(entry), "no entry exist.");
	if (out) {
		tsetobj(T, out, amval(entry));
	}
	clearentry(entry);
	self->length--;
}

/**
 ** remove entry by key, return 'true' if key founded, or 'false' in the otherwise.
 */
int aloH_remove(astate T, atable_t* self, const atval_t* key, atval_t* out) {
	if (isinvalidkey(key)) {
		return false; /* a invalid key should not exist in table. */
	}
	for_each(self, i) { /* tried to find key */
		if (aloV_equal(T, amkey(i), key)) { /* key find? */
			if (out) {
				tsetobj(T, out, amval(i));
			}
			clearentry(i);
			self->length--;
			return true;
		}
	}
	return false;
}

/**
 ** iterate to next element in table.
 */
const aentry_t* aloH_next(atable_t* self, ptrdiff_t* poff) {
	aloE_assert(*poff >= -1, "illegal offset.");
	ptrdiff_t off = *poff + 1;
	if (off >= self->capacity) { /* iterating already ended */
		 return NULL;
	}
	aentry_t* const end = self->array + self->capacity;
	aentry_t* e = self->array + off;
	while (e < end) { /* find next non-nil key */
		if (!ttisnil(e)) {
			*poff = e - self->array; /* adjust offset */
			return e;
		}
		e += 1;
	}
	/* reach to end? */
	*poff = self->capacity;
	return NULL;
}
