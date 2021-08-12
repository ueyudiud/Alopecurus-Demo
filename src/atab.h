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

ALO_IFUN atable_t* aloH_new(alo_State);
ALO_IFUN int aloH_ensure(alo_State, atable_t*, size_t);
ALO_IFUN void aloH_trim(alo_State, atable_t*);
ALO_IFUN const alo_TVal* aloH_geti(atable_t*, a_int);
ALO_IFUN const alo_TVal* aloH_gets(alo_State, atable_t*, a_cstr, size_t);
ALO_IFUN const alo_TVal* aloH_getis(atable_t*, alo_Str*);
ALO_IFUN const alo_TVal* aloH_getxs(alo_State, atable_t*, alo_Str*);
ALO_IFUN const alo_TVal* aloH_get(alo_State, atable_t*, const alo_TVal*);
ALO_IFUN alo_TVal* aloH_find(alo_State, atable_t*, const alo_TVal*);
ALO_IFUN alo_TVal* aloH_findxset(alo_State, atable_t*, const char*, size_t);
ALO_IFUN alo_TVal* aloH_finds(alo_State, atable_t*, a_cstr, size_t);
ALO_IFUN void aloH_rawrem(alo_State, atable_t*, ptrdiff_t*, alo_TVal*);
ALO_IFUN int aloH_remove(alo_State, atable_t*, const alo_TVal*, alo_TVal*);
ALO_IFUN void aloH_clear(alo_State, atable_t*);
ALO_IFUN const alo_Entry* aloH_next(atable_t*, ptrdiff_t*);

#endif /* ATAB_H_ */
