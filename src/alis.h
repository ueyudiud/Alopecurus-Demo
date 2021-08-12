/*
 * alis.h
 *
 *  Created on: 2019年7月26日
 *      Author: ueyudiud
 */

#ifndef ALIS_H_
#define ALIS_H_

#include "aobj.h"

#define aloI_length(self) ((self)->length)

#define aloI_contains(T,self,key) (aloI_get(T, self, key) != aloO_tnil)
#define aloI_seti(T,self,key,value) { tsetobj(T, aloI_findi(T, self, key), value); aloG_barrierbackt(T, self, value); }
#define aloI_set(T,self,key,value) { tsetobj(T, aloI_find(T, self, key), value); aloG_barrierbackt(T, self, value); }
#define aloI_clear(T,self) { aloE_void(T); (self)->length = 0; }

ALO_IFUN alo_List* aloI_new(alo_State);
ALO_IFUN void aloI_ensure(alo_State, alo_List*, size_t);
ALO_IFUN void aloI_trim(alo_State, alo_List*);
ALO_IFUN const alo_TVal* aloI_geti(alo_List*, a_int);
ALO_IFUN const alo_TVal* aloI_get(alo_State, alo_List*, const alo_TVal*);
ALO_IFUN void aloI_add(alo_State, alo_List*, const alo_TVal*);
ALO_IFUN int aloI_put(alo_State, alo_List*, const alo_TVal*);
ALO_IFUN alo_TVal* aloI_findi(alo_State, alo_List*, a_int);
ALO_IFUN alo_TVal* aloI_find(alo_State, alo_List*, const alo_TVal*);
ALO_IFUN void aloI_removei(alo_State, alo_List*, a_int, alo_TVal*);
ALO_IFUN int aloI_remove(alo_State, alo_List*, const alo_TVal*, alo_TVal*);
ALO_IFUN const alo_TVal* aloI_next(alo_List*, ptrdiff_t*);

#endif /* ALIS_H_ */
