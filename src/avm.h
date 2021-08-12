/*
 * avm.h
 *
 * VM functions.
 *
 *  Created on: 2019年7月23日
 *      Author: ueyudiud
 */

#ifndef AVM_H_
#define AVM_H_

#include "astate.h"

#define aloV_getbool(o) (!(tisnil(o) || (tisbool(o) && !tasbool(o))))

#define aloV_toint(o,v) (tisint(o) ? ((v) = tasint(o), true) : aloV_tointx(o, &(v), 0))

#define aloV_toflt(o,v) (tisnum(o) ? ((v) = ttonum(o), true) : aloV_tofltx(o, &(v)))

#define aloV_requal(o1,o2) aloV_equal(NULL, o1, o2)

ALO_IFUN a_cstr aloV_pushfstring(alo_State, a_cstr, ...);
ALO_IFUN a_cstr aloV_pushvfstring(alo_State, a_cstr, va_list);
ALO_IFUN void aloV_invoke(alo_State, int);
ALO_IFUN a_cstr aloV_typename(alo_State, const alo_TVal*);
ALO_IFUN int aloV_tointx(const alo_TVal*, a_int*, int);
ALO_IFUN int aloV_tofltx(const alo_TVal*, a_float*);
ALO_IFUN a_hash aloV_hashof(alo_State, const alo_TVal*);
ALO_IFUN int aloV_compare(alo_State, const alo_TVal*, const alo_TVal*, int);
ALO_IFUN int aloV_equal(alo_State, const alo_TVal*, const alo_TVal*);
ALO_IFUN size_t aloV_length(alo_State, const alo_TVal*);
ALO_IFUN alo_Str* aloV_rawcat(alo_State, size_t);
ALO_IFUN void aloV_concat(alo_State, size_t);
ALO_IFUN void aloV_iterator(alo_State, const alo_TVal*, alo_TVal*);

#endif /* AVM_H_ */
