/*
 * atab.h
 *
 * methods and properties for table value.
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#ifndef ATAB_H_
#define ATAB_H_

#include "aobj.h"

/* get key in entry */
#define amkey(e) (&(e)->key)
/* get value in entry */
#define amval(e) (&(e)->value)

/**
 ** call when entries are updated.
 */
#define aloH_markdirty(self) aloE_void((self)->reserved = 0)

#define aloH_length(self) ((self)->length)

#define aloH_setls(T,self,key,value,out) aloH_sets(T, self, ""key, sizeof(key) / sizeof(char) - 1, value, out)

/**
 ** put key-value pair into table.
 */
#define aloH_set(T,self,key,value) { tsetobj(T, aloH_find(T, self, key), value); aloG_barrierbackt(T, self, value); }

atable_t* aloH_new(astate);
int aloH_ensure(astate, atable_t*, size_t);
void aloH_trim(astate, atable_t*);
const atval_t* aloH_geti(atable_t*, aint);
const atval_t* aloH_gets(astate, atable_t*, astr, size_t);
const atval_t* aloH_getis(atable_t*, astring_t*);
const atval_t* aloH_getxs(astate, atable_t*, astring_t*);
const atval_t* aloH_get(astate, atable_t*, const atval_t*);
atval_t* aloH_find(astate, atable_t*, const atval_t*);
atval_t* aloH_findxset(astate, atable_t*, const char*, size_t);
void aloH_sets(astate, atable_t*, astr, size_t, const atval_t*, atval_t*);
void aloH_rawrem(astate, atable_t*, ptrdiff_t*, atval_t*);
int aloH_remove(astate, atable_t*, const atval_t*, atval_t*);
const aentry_t* aloH_next(atable_t*, ptrdiff_t*);

#endif /* ATAB_H_ */
