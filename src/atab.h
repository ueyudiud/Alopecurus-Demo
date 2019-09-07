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

#define amkey(e) (&(e)->key)
#define amval(e) (&(e)->value)

#define aloH_markdirty(self) aloE_void((self)->reserved = 0)

#define aloH_length(self) ((self)->length)

#define aloH_setls(T,self,key,value,out) aloH_sets(T, self, ""key, sizeof(key) / sizeof(char) - 1, value, out)

atable_t* aloH_new(astate);
void aloH_ensure(astate, atable_t*, size_t);
void aloH_trim(astate, atable_t*);
const atval_t* aloH_geti(atable_t*, aint);
const atval_t* aloH_gets(atable_t*, astr, size_t);
const atval_t* aloH_getis(atable_t*, astring_t*);
const atval_t* aloH_getxs(atable_t*, astring_t*);
const atval_t* aloH_get(astate, atable_t*, const atval_t*);
atval_t* aloH_find(astate, atable_t*, const atval_t*);
void aloH_set(astate, atable_t*, const atval_t*, const atval_t*, atval_t*);
astring_t* aloH_sets(astate, atable_t*, astr, size_t, const atval_t*, atval_t*);
void aloH_setxs(astate, atable_t*, astring_t*, const atval_t*, atval_t*);
int aloH_remove(astate, atable_t*, const atval_t*, atval_t*);
const aentry_t* aloH_next(atable_t*, ptrdiff_t*);

#endif /* ATAB_H_ */
