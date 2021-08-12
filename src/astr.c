/*
 * astr.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#define ASTR_C_
#define ALO_CORE

#include "astr.h"
#include "astate.h"
#include "agc.h"

#include <string.h>

/**
 ** get raw hash from string.
 */
a_hash aloS_rhash(a_cstr src, size_t len, uint64_t seed) {
	a_hash h = seed ^ len;
	uint64_t step = (len >> ALO_STRHASHLIMIT) + 1;
	const char* const end = src + len;
	for (const char* i = src; i < end; i += step)
		h ^= (h << 5) + (h >> 2) + aloE_byte(*i);
	return h;
}

/**
 ** calculate hash or use cache to get hash of string.
 */
a_hash aloS_hash(alo_State T, alo_Str* self) {
	if (!self->fhash) {
		self->hash = aloS_rhash(self->array, aloS_len(self), T->g->seed);
		self->fhash = true;
	}
	return self->hash;
}

/**
 ** create or reuse a string by a '\0' terminated source.
 */
alo_Str* aloS_of(alo_State T, a_cstr value) {
	Gd(T);
	uintmax_t i = aloE_addr(value) % ALO_STRCACHE_N;
	alo_Str** array = G->scache[i];
	for (int j = 0; j < ALO_STRCACHE_M; ++j)
		if (strcmp(array[j]->array, value) == 0) /* equal? */
			return array[j]; /* get string */
	/* shift each element once and drop last element */
	for (int j = ALO_STRCACHE_M - 1; j > 0; --j)
		array[j] = array[j - 1];
	return array[0] = aloS_new(T, value, strlen(value));
}

/**
 ** create raw string object.
 */
static alo_Str* create(alo_State T, int type, size_t len) {
	alo_Str* value = aloE_cast(alo_Str*, aloM_malloc(T, astrsizel(len)));
	value->extra = 0;
	value->flags = 0;
	value->array[len] = '\0'; /* let string ended with '\0' */
	aloG_register(T, value, type);
	return value;
}

#define indexof(t,h) ((h) % (t).capacity)

/**
 ** double intern table capacity.
 */
static void expandtable(alo_State T, aitable_t* table) {
	size_t size = table->capacity;
	size_t nsize = size * 2;
	alo_Str** array = table->array;
	array = aloM_adja(T, array, size, nsize);
	alo_Str **p, **q, *v;
	table->capacity = nsize;
	for (size_t i = 0; i < size; ++i) {
		p = array + i;
		q = p + size;
		*q = NULL;
		while ((v = *p)) {
			if (i == indexof(*table, v->hash)) {
				p = &v->shtlist; /* move forward */
			}
			else {
				*p = v->shtlist; /* remove 'v' from 'p' list */
				/* add 'v' to 'q' list */
				v->shtlist = *q;
				*q = v;
			}
		}
	}
	table->array = array;
}

/**
 ** get or create a interned string object from table.
 */
static alo_Str* intern(alo_State T, const char* src, size_t len) {
	aloE_assert(src, "source should not be null.");
	Gd(T);
	a_hash hash = aloS_rhash(src, len, G->seed);
	alo_Str** s = &G->itable.array[indexof(G->itable, hash)];
	alo_Str* v;
	while ((v = *s)) {
		if (v->hash == hash && aloS_requal(v, src, len)) {
			if (aloG_isdead(G, v)) { /* if string is already dead but not collect yet. */
				v->mark ^= aloG_white; /* resurrect value */
			}
			return v;
		}
		s = &v->shtlist;
	}
	if (G->itable.length > G->itable.capacity * ALO_ITABLE_LOAD_FACTOR) {
		expandtable(T, &G->itable);
	}
	s = &G->itable.array[indexof(G->itable, hash)];

	alo_Str* value = create(T, ALO_VISTR, len);
	value->fhash = true;
	value->hash = hash;
	value->shtlen = len;
	memcpy(value->array, src, len * sizeof(char));
	/* link string to table */
	value->shtlist = *s;
	*s = value;
	G->itable.length++;
	return value;
}

/**
 ** create a long string object.
 */
alo_Str* aloS_createlng(alo_State T, size_t len) {
	aloE_assert(len > ALO_MAXISTRLEN, "too short to store in heap");
	alo_Str* value = create(T, ALO_VHSTR, len);
	value->lnglen = len;
	return value;
}

/**
 ** create a string object in heap.
 */
static alo_Str* heap(alo_State T, const char* src, size_t len) {
	alo_Str* value = aloS_createlng(T, len);
	memcpy(value->array, src, len * sizeof(char));
	return value;
}

/**
 ** remove short string in cache.
 */
void aloS_remove(alo_State T, alo_Str* self) {
	Gd(T);
	alo_Str** s = &G->itable.array[indexof(G->itable, self->hash)];
	alo_Str* v;
	while ((v = *s)) {
		if (v == self) {
			*s = v->shtlist; /* remove string from list */
			G->itable.length--;
			return;
		}
		s = &v->shtlist;
	}
	aloE_assert(false, "string not found.");
}

/**
 ** create a string with specific length.
 */
alo_Str* aloS_new(alo_State T, const char* src, size_t len) {
	return len == 0 ? T->g->sempty :
			len <= ALO_MAXISTRLEN ? intern(T, src, len) : heap(T, src, len);
}

/**
 ** create a fixed string.
 */
alo_Str* aloS_newi(alo_State T, const char* src, size_t len) {
	if (len == 0) { /* empty string? */
		return T->g->sempty;
	}
	alo_Str* s = aloS_new(T, src, len);
	aloG_fix(T, s);
	return s;
}

/**
 ** create new raw data value.
 */
alo_User* aloS_newr(alo_State T, size_t len) {
	alo_User* value = aloE_cast(alo_User*, aloM_malloc(T, ardsizel(len)));
	aloG_register(T, value, ALO_TUSER);
	value->length = len;
	value->metatable = NULL;
	return value;
}

/**
 ** check two heap string are equal.
 */
int aloS_hequal(alo_Str* s1, alo_Str* s2) {
	aloE_assert(tishstr(s1) && tishstr(s2), "value should be heap string.");
	return s1 == s2 ||
			(s1->lnglen == s2->lnglen && memcmp(s1->array, s2->array, s1->lnglen) == 0);
}

/**
 ** check string object and raw string are equal.
 */
int aloS_requal(alo_Str* self, a_cstr src, size_t len) {
	return aloS_len(self) == len && memcmp(self->array, src, len * sizeof(char)) == 0;
}

/**
 ** compare between two string objects.
 */
int aloS_compare(alo_Str* s1, alo_Str* s2) {
	size_t l1 = aloS_len(s1), l2 = aloS_len(s2);
	size_t l = l1 <= l2 ? l1 : l2;
	return memcmp(s1->array, s2->array, l * sizeof(char)) ?:
			l1 < l2 ? -1 :
			l1 > l2 ?  1 : 0;
}

/**
 ** initialize string cache and string constants.
 */
void aloS_init(alo_State T) {
	Gd(T);
	aloS_resizecache(T, ALO_ITABLE_INITIAL_CAPACITY);
	G->sempty = intern(T, "", 0);
	aloG_fix(T, G->sempty);
	/* fill string cache by empty string */
	for (size_t i = 0; i < ALO_STRCACHE_N; ++i)
		for (size_t j = 0; j < ALO_STRCACHE_M; ++j)
			G->scache[i][j] = G->sempty;
	/* before this point, aloS_cleancache would not take any effect */
	/* set up memory error string */
	G->smerrmsg = intern(T, ALO_MEMERRMSG, sizeof(ALO_MEMERRMSG) / sizeof(char) - 1);
	aloG_fix(T, G->smerrmsg);
}

/**
 ** resize short string cache.
 */
void aloS_resizecache(alo_State T, size_t size) {
	Gd(T);
	aitable_t* table = &G->itable;
	if (size > table->capacity) { /* grow table */
		table->array = aloM_adja(T, table->array, table->capacity, size);
		for (size_t i = table->capacity; i < size; table->array[i++] = NULL);
	}
	for (size_t i = 0; i < table->capacity; ++i) {
		alo_Str* n = table->array[i];
		alo_Str* v;
		table->array[i] = NULL;
		while ((v = n)) {
			n = v->shtlist;
			alo_Str** m = table->array + indexof(*table, v->hash);
			v->shtlist = *m;
			*m = v;
		}
	}
	if (size < table->capacity) { /* shrink table */
		table->array = aloM_adja(T, table->array, table->capacity, size);
	}
	table->capacity = size;
}

/**
 ** clean short string cache.
 */
void aloS_cleancache(alo_State T) {
	Gd(T);
	if (T->g->itable.length == 0) { /* if string table is not initialized */
		return;
	}
	for (int i = 0; i < ALO_STRCACHE_N; ++i) {
		for (int j = 0; j < ALO_STRCACHE_M; ++j) {
			if (aloG_iswhite(G->scache[i][j])) { /* will string be collected? */
				G->scache[i][j] = G->sempty; /* restore to empty string. */
			}
		}
	}
}
