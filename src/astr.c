/*
 * astr.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#include "astr.h"
#include "astate.h"
#include "agc.h"

#include <string.h>

/**
 ** get raw hash from string.
 */
ahash_t aloS_rhash(astr src, size_t len, uint64_t seed) {
	ahash_t h = seed ^ len;
	uint64_t step = (len >> ALO_STRHASHLIMIT) + 1;
	const char* const end = src + len;
	for (const char* i = src; i < end; i += step)
		h ^= (h << 5) + (h >> 2) + aloE_byte(*i);
	return h;
}

/**
 ** calculate hash or use cache to get hash of string.
 */
ahash_t aloS_hash(astate T, astring_t* self) {
	if (!self->fhash) {
		self->hash = aloS_rhash(self->array, aloS_len(self), T->g->seed);
		self->fhash = true;
	}
	return self->hash;
}

/**
 ** create or reuse a string by a '\0' terminated source.
 */
astring_t* aloS_of(astate T, astr value) {
	Gd(T);
	uintmax_t i = aloE_addr(value) % ALO_STRCACHE_N;
	astring_t** array = G->scache[i];
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
static astring_t* create(astate T, int type, size_t len) {
	astring_t* value = aloE_cast(astring_t*, aloM_malloc(T, astrsizel(len)));
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
static void expandtable(astate T, aitable_t* table) {
	size_t size = table->capacity;
	size_t nsize = size * 2;
	astring_t** array = table->array;
	array = aloM_adja(T, array, size, nsize);
	astring_t **p, **q, *v;
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
static astring_t* intern(astate T, const char* src, size_t len) {
	aloE_assert(src, "source should not be null.");
	Gd(T);
	ahash_t hash = aloS_rhash(src, len, G->seed);
	astring_t** s = &G->itable.array[indexof(G->itable, hash)];
	astring_t* v;
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

	astring_t* value = create(T, ALO_TISTRING, len);
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
astring_t* aloS_createlng(astate T, size_t len) {
	aloE_assert(len > ALO_MAXISTRLEN, "too short to store in heap");
	astring_t* value = create(T, ALO_THSTRING, len);
	value->lnglen = len;
	return value;
}

/**
 ** create a string object in heap.
 */
static astring_t* heap(astate T, const char* src, size_t len) {
	astring_t* value = aloS_createlng(T, len);
	memcpy(value->array, src, len * sizeof(char));
	return value;
}

/**
 ** remove short string in cache.
 */
void aloS_remove(astate T, astring_t* self) {
	Gd(T);
	astring_t** s = &G->itable.array[indexof(G->itable, self->hash)];
	astring_t* v;
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
astring_t* aloS_new(astate T, const char* src, size_t len) {
	return len == 0 ? T->g->sempty :
			len <= ALO_MAXISTRLEN ? intern(T, src, len) : heap(T, src, len);
}

/**
 ** create a fixed string.
 */
astring_t* aloS_newi(astate T, const char* src, size_t len) {
	if (len == 0) { /* empty string? */
		return T->g->sempty;
	}
	astring_t* s = aloS_new(T, src, len);
	aloG_fix(T, s);
	return s;
}

/**
 ** create new raw data value.
 */
arawdata_t* aloS_newr(astate T, size_t len) {
	arawdata_t* value = aloE_cast(arawdata_t*, aloM_malloc(T, ardsizel(len)));
	aloG_register(T, value, ALO_TRAWDATA);
	value->length = len;
	value->metatable = NULL;
	return value;
}

/**
 ** check two heap string are equal.
 */
int aloS_hequal(astring_t* s1, astring_t* s2) {
	aloE_assert(ttishstr(s1) && ttishstr(s2), "value should be heap string.");
	return s1 == s2 ||
			(s1->lnglen == s2->lnglen && memcmp(s1->array, s2->array, s1->lnglen) == 0);
}

/**
 ** check string object and raw string are equal.
 */
int aloS_requal(astring_t* self, astr src, size_t len) {
	return aloS_len(self) == len && memcmp(self->array, src, len * sizeof(char)) == 0;
}

/**
 ** compare between two string objects.
 */
int aloS_compare(astring_t* s1, astring_t* s2) {
	size_t l1 = aloS_len(s1), l2 = aloS_len(s2);
	size_t l = l1 <= l2 ? l1 : l2;
	return memcmp(s1->array, s2->array, l * sizeof(char)) ?:
			l1 < l2 ? -1 :
			l1 > l2 ?  1 : 0;
}

/**
 ** initialize string cache and string constants.
 */
void aloS_init(astate T) {
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
void aloS_resizecache(astate T, size_t size) {
	Gd(T);
	aitable_t* table = &G->itable;
	if (size > table->capacity) { /* grow table */
		table->array = aloM_adja(T, table->array, table->capacity, size);
		for (size_t i = table->capacity; i < size; table->array[i++] = NULL);
	}
	for (size_t i = 0; i < table->capacity; ++i) {
		astring_t* n = table->array[i];
		astring_t* v;
		table->array[i] = NULL;
		while ((v = n)) {
			n = v->shtlist;
			astring_t** m = table->array + indexof(*table, v->hash);
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
void aloS_cleancache(astate T) {
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
