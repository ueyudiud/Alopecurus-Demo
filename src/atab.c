/*
 * atab.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#define ATAB_C_
#define ALO_CORE

#include "astr.h"
#include "atab.h"
#include "agc.h"
#include "adebug.h"
#include "avm.h"
#include "ado.h"

#include <math.h>

#define NO_NODE 0

/* empty entry */
#define aloH_empty (alo_Entry) { .value = aloO_nil, { .key = aloO_nil }, .prev = NO_NODE, .next = NO_NODE, .hash = 0 }

#define delety(e) (*(e) = aloH_empty)

#define hasnext(e) ((e)->next != NO_NODE)
#define hasprev(e) ((e)->prev != NO_NODE)

#define getnext(e) aloE_check(hasnext(e), "reach to end of nodes", (e) + (e)->next)
#define getprev(e) aloE_check(hasprev(e), "reach to begin of nodes", (e) - (e)->prev)

static void movety(alo_Entry* dest, alo_Entry* src) {
	aloE_assert(!tisnil(src), "entry should not be empty.");
	int diff = dest - src;
	dest->key = src->key;
	dest->value = src->value;
	if (hasprev(src)) {
		dest->prev = (getprev(src)->next += diff);
	}
	else {
		dest->prev = NO_NODE;
	}
	if (hasnext(src)) {
		dest->next = (getnext(src)->prev -= diff);
	}
	else {
		dest->next = NO_NODE;
	}
	dest->hash = src->hash;
	delety(src);
}

#define isinvalidfltkey(v) (isnan(v))

#define isinvalidkey(o) (tisnil(o) || (tisflt(o) && isinvalidfltkey(tasflt(o))))

/**
 ** create new table value.
 */
atable_t* aloH_new(alo_State T) {
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

static void resizetable(alo_State, atable_t*, size_t);

/**
 ** aloH_ensure table can store extra size of entries.
 */
int aloH_ensure(alo_State T, atable_t* self, size_t size) {
	size_t require = self->length + size;
	if (require > self->capacity) {
		resizetable(T, self, aloM_adjsize(T, self->capacity, require, ALO_MAX_BUFSIZE));
		return true;
	}
	return false;
}

/**
 ** get head entry by hash.
 */
#define headety(table,hash) ((table)->array + (hash) % (table)->capacity)

/**
 ** get entry that no key occupy it.
 */
static alo_Entry* newety(atable_t* self) {
	while (self->lastfree >= self->array) {
		if (tisnil(self->lastfree)) {
			return self->lastfree--;
		}
		self->lastfree--;
	}
	return NULL;
}

/**
 ** add entry to table and link to previous node.
 */
static alo_Entry* addety(atable_t* self, alo_Entry* prev) {
	alo_Entry* entry = newety(self);
	int diff = entry - prev;
	if (hasnext(prev)) {
		getnext(prev)->prev -= diff;
		entry->next = prev->next - diff;
	}
	entry->prev = prev->next = diff;
	return entry;
}

static alo_TVal* putety(alo_State T, atable_t* self, const alo_TVal* key, a_hash hash) {
	alo_Entry* entry = headety(self, hash); /* first hashing */
	if (!tisnil(entry)) { /* blocked by other element */
		if (!hasprev(entry)) { /* check element is head of hash list */
			entry = addety(self, entry);
		}
		else {
			movety(newety(self), entry);
		}
	}
	aloE_assert(tisnil(amkey(entry)) && tisnil(amval(entry)), "uninitialized entry should be empty.");
	tsetobj(T, amkey(entry), key);
	aloE_void(T);
	entry->hash = hash;
	return amval(entry);
}

static void resizetable(alo_State T, atable_t* self, size_t newsize) {
	size_t oldsize = self->capacity;
	alo_Entry* oldarray = self->array;
	alo_Entry* newarray = aloM_newa(T, alo_Entry, newsize);
	for (size_t i = 0; i < newsize; delety(&newarray[i++]));
	self->array = newarray;
	self->capacity = newsize;
	self->lastfree = newarray + newsize - 1;
	if (oldsize > 0) {
		for (size_t i = 0; i < oldsize; ++i) {
			alo_Entry* entry = &oldarray[i];
			if (!tisnil(entry)) {
				alo_TVal* slot = putety(T, self, amkey(entry), entry->hash);
				tsetobj(T, slot, amval(entry));
			}
		}
		aloM_dela(T, oldarray, oldsize);
	}
}

/**
 ** trim table capacity to length
 */
void aloH_trim(alo_State T, atable_t* self) {
	if (self->length == 0) {
		if (self->capacity > 0) { /* delete array if it is non-null */
			aloM_delb(T, self->array, self->capacity);
			self->lastfree = NULL;
		}
	}
	else if (self->length < aloE_cast(size_t, self->capacity / 4)) {
		resizetable(T, self, self->capacity / 2); /* shrink table to half capacity */
	}
}

#define anyof(_self,_hash,_cond) \
	if ((_self)->length > 0) { /* element is present */ \
		alo_Entry* _entry = headety(_self, _hash); \
		if (!hasprev(_entry)) \
		do if (_cond) return amval(_entry); while (hasnext(_entry) && (_entry = getnext(_entry), true)); \
	}

/**
 ** get value by integer key.
 */
const alo_TVal* aloH_geti(atable_t* self, a_int key) {
	anyof(self, aloO_inthash(key), tisint(_entry) && tasint(_entry) == key);
	return aloO_tnil;
}

static alo_TVal* finds(atable_t* self, a_cstr ksrc, size_t klen, a_hash hash) {
	anyof(self, hash, tisstr(_entry) && aloS_requal(tasstr(_entry), ksrc, klen));
	return NULL;
}

/**
 ** get value by raw string key.
 */
const alo_TVal* aloH_gets(alo_State T, atable_t* self, a_cstr src, size_t len) {
	return finds(self, src, len, aloS_rhash(src, len, T->g->seed)) ?: aloO_tnil;
}

/**
 ** get value by interned string key.
 */
const alo_TVal* aloH_getis(atable_t* self, alo_Str* key) {
	aloE_assert(tisistr(key), "key should be interned string.");
	anyof(self, key->hash, tisstr(_entry) && tasstr(_entry) == key);
	return aloO_tnil;
}

static const alo_TVal* aloH_geths(__attribute__((unused)) alo_State T, atable_t* self, alo_Str* key) {
	aloE_assert(tishstr(key), "key should be heaped string.");
	anyof(self, key->hash, tisstr(_entry) && aloS_hequal(tasstr(_entry), key));
	return aloO_tnil;
}

/**
 ** get value by string object.
 */
const alo_TVal* aloH_getxs(alo_State T, atable_t* self, alo_Str* key) {
	return tisistr(key) ? aloH_getis(self, key) : aloH_geths(T, self, key);
}

static const alo_TVal* getgeneric(alo_State T, atable_t* self, const alo_TVal* key) {
	anyof(self, aloV_hashof(T, key), aloV_equal(T, amkey(_entry), key));
	return aloO_tnil;
}

/**
 ** get value by tagged value.
 */
const alo_TVal* aloH_get(alo_State T, atable_t* self, const alo_TVal* key) {
	switch (ttype(key)) {
	case ALO_TNIL:
		return aloO_tnil;
	case ALO_TINT:
		return aloH_geti(self, tasint(key));
	case ALO_TFLOAT: {
		if (isinvalidfltkey(tasflt(key)))
			return aloO_tnil;
		a_int i;
		if (aloV_tointx(key, &i, 0)) { /* try cast float to integer value. */
			return aloH_geti(self, i);
		}
		break;
	}
	case ALO_VHSTR:
		return aloH_geths(T, self, tasstr(key));
	case ALO_VISTR:
		return aloH_getis(self, tasstr(key));
	}
	return getgeneric(T, self, key);
}

/**
 ** find a value slot by key, or create a new slot if not exist.
 */
alo_TVal* aloH_find(alo_State T, atable_t* self, const alo_TVal* key) {
	alo_TVal aux;
	switch (ttpnv(key)) {
	case ALO_TNIL: {
		aloU_invalidkey(T);
		break;
	}
	case ALO_TFLOAT: {
		a_float v = tasflt(key);
		if (aloO_flt2int(v, &aux.v.i, 0)) {
			aux.tt = ALO_TINT;
			key = &aux;
		}
		else if (isnan(v)) {
			aloU_invalidkey(T);
		}
		break;
	}
	}
	const alo_TVal* value = aloH_get(T, self, key);
	if (value != aloO_tnil) {
		return aloE_cast(alo_TVal*, value);
	}
	aloH_ensure(T, self, 1);
	self->length ++;
	alo_TVal* slot = putety(T, self, key, aloV_hashof(T, key));
	aloG_barrierbackt(T, self, key);
	return slot;
}

/**
 ** find string from string set.
 */
alo_TVal* aloH_findxset(alo_State T, atable_t* self, const char* ksrc, size_t klen) {
	a_hash hash = aloS_rhash(ksrc, klen, T->g->seed);
	alo_TVal* slot = finds(self, ksrc, klen, hash);
	if (slot != NULL) {
		return slot;
	}
	aloH_ensure(T, self, 1);
	alo_Str* str = aloS_new(T, ksrc, klen);
	alo_TVal key = tnewstr(str);
	slot = putety(T, self, &key, hash);
	tsetstr(T, slot, str);
	aloG_barrierback(T, self, str);
	aloH_markdirty(self);
	self->length ++;
	return slot;
}

/**
 ** put key-value pair into table, string should be '\0'-terminated string.
 */
alo_TVal* aloH_finds(alo_State T, atable_t* self, a_cstr src, size_t len) {
	a_hash hash = aloS_rhash(src, len, T->g->seed);
	alo_TVal* slot = finds(self, src, len, hash);
	if (slot == NULL) {
		aloH_ensure(T, self, 1);
		alo_Str* str = aloS_new(T, src, len);
		/* force set string hash */
		str->fhash = true;
		str->hash = hash;
		alo_TVal key = tnewstr(str);
		slot = putety(T, self, &key, hash);
		self->length ++;
		if (str->ftagname && str->extra < ALO_NUMTM) { /* check key is TM name that able to fast get value */
			/* correct TM data */
			self->reserved &= ~(1 << str->extra);
		}
		aloG_barrierback(T, self, str);
	}

	return slot;
}

static int remety(atable_t* self, alo_Entry* entry) {
	aloE_assert(!tisnil(entry), "no entry exist.");
	if (hasprev(entry)) { /* if entry is not head */
		alo_Entry* prev = getprev(entry);
		if (hasnext(entry)) {
			alo_Entry* next = getnext(entry);
			prev->next = next->prev = prev - next;
		}
		else {
			prev->next = NO_NODE;
		}
	}
	else if (hasnext(entry)) { /* if their are more than one entry */
		movety(entry, getnext(entry)); /* move next entry to current slot for use hashing to get it */
		return true;
	}
	delety(entry);
	if (self->lastfree < entry) { /* if 'hole' is after the last free slot */
		self->lastfree = entry; /* settle last free slot here */
	}
	return false;
}

/**
 ** remove entry by entry index.
 */
void aloH_rawrem(alo_State T, atable_t* self, ptrdiff_t* pindex, alo_TVal* out) {
	aloE_void(T);
	size_t index = *pindex;
	aloE_assert(index < self->capacity, "table index out of bound.");
	alo_Entry* entry = self->array + index;
	aloE_assert(!tisnil(entry), "no entry exist.");
	if (out) {
		tsetobj(T, out, amval(entry));
	}
	if (remety(self, entry)) {
		(*pindex)--;
	}
	self->length--;
}

/**
 ** remove entry by key, return 'true' if key founded, or 'false' in the otherwise.
 */
int aloH_remove(alo_State T, atable_t* self, const alo_TVal* key, alo_TVal* out) {
	if (self->capacity == 0) {
		return false; /* no element exists in empty table */
	}
	else if (isinvalidkey(key)) {
		return false; /* a invalid key should not exist in table. */
	}
	a_hash hash = aloV_hashof(T, key);
	alo_Entry* entry = headety(self, hash);
	if (tisnil(entry) || hasprev(entry)) {
		return false;
	}
	else if (entry->hash == hash && aloV_equal(T, amkey(entry), key)) { /* found entry in right place */
		goto find;
	}
	else while (hasnext(entry)) {
		entry = getnext(entry);
		if (entry->hash == hash && aloV_equal(T, amkey(entry), key)) {
			goto find;
		}
	}
	return false;

	find:
	if (out) {
		tsetobj(T, out, amval(entry));
	}
	remety(self, entry);
	self->length--;
	return true;
}

/**
 ** clear all values in table.
 */
void aloH_clear(__attribute__((unused)) alo_State T, atable_t* self) {
	alo_Entry* const end = self->array + self->capacity;
	for (alo_Entry* e = self->array; e < end; ++e) {
		delety(e);
	}
	self->length = 0;
	self->lastfree = self->array + self->capacity - 1;
	self->reserved = aloE_byte(~0);
}

/**
 ** iterate to next element in table.
 */
const alo_Entry* aloH_next(atable_t* self, ptrdiff_t* poff) {
	aloE_assert(*poff >= -1, "illegal offset.");
	size_t off = *poff + 1;
	if (off >= self->capacity) { /* iterating already ended */
		 return NULL;
	}
	alo_Entry* const end = self->array + self->capacity;
	alo_Entry* e = self->array + off;
	while (e < end) { /* find next non-nil key */
		if (!tisnil(e)) {
			*poff = e - self->array; /* adjust offset */
			return e;
		}
		e += 1;
	}
	/* reach to end? */
	*poff = self->capacity;
	return NULL;
}
