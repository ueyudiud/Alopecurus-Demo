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

ALO_IFUN alist_t* aloI_new(astate);
ALO_IFUN void aloI_ensure(astate, alist_t*, size_t);
ALO_IFUN void aloI_trim(astate, alist_t*);
ALO_IFUN const atval_t* aloI_geti(alist_t*, aint);
ALO_IFUN const atval_t* aloI_get(astate, alist_t*, const atval_t*);
ALO_IFUN void aloI_add(astate, alist_t*, const atval_t*);
ALO_IFUN int aloI_put(astate, alist_t*, const atval_t*);
ALO_IFUN atval_t* aloI_findi(astate, alist_t*, aint);
ALO_IFUN atval_t* aloI_find(astate, alist_t*, const atval_t*);
ALO_IFUN void aloI_removei(astate, alist_t*, aint, atval_t*);
ALO_IFUN int aloI_remove(astate, alist_t*, const atval_t*, atval_t*);
ALO_IFUN const atval_t* aloI_next(alist_t*, ptrdiff_t*);

#endif /* ALIS_H_ */
