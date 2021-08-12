/*
 * atup.h
 *
 * methods and properties for tuple value.
 *
 *  Created on: 2019年7月23日
 *      Author: ueyudiud
 */

#ifndef ATUP_H_
#define ATUP_H_

#include "aobj.h"

#define atupsizel(l) (sizeof(alo_Tuple) + (l) * sizeof(alo_TVal))
#define atupsize(o)  atupsizel((o)->length)

ALO_IFUN alo_Tuple* aloA_new(alo_State, size_t, const alo_TVal*);
ALO_IFUN const alo_TVal* aloA_geti(alo_Tuple*, a_int);
ALO_IFUN const alo_TVal* aloA_get(alo_State, alo_Tuple*, const alo_TVal*);
ALO_IFUN const alo_TVal* aloA_next(alo_Tuple*, ptrdiff_t*);
ALO_IFUN a_hash aloA_hash(alo_State, alo_Tuple*);

#endif /* ATUP_H_ */
