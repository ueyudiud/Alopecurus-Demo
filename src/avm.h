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

#define aloV_getbool(o) (!(ttisnil(o) || (ttisbool(o) && !tgetbool(o))))

#define aloV_toint(o,v) (ttisint(o) ? ((v) = tgetint(o), true) : aloV_tointx(o, &(v), 0))

#define aloV_toflt(o,v) (ttisnum(o) ? ((v) = tgetnum(o), true) : aloV_tofltx(o, &(v)))

#define aloV_requal(o1,o2) aloV_equal(NULL, o1, o2)

astr aloV_pushfstring(astate, astr, ...);
astr aloV_pushvfstring(astate, astr, va_list);
void aloV_invoke(astate, int);
astr aloV_typename(astate, const atval_t*);
int aloV_tointx(const atval_t*, aint*, int);
int aloV_tofltx(const atval_t*, afloat*);
ahash_t aloV_hashof(astate, const atval_t*);
int aloV_compare(astate, const atval_t*, const atval_t*, int);
int aloV_equal(astate, const atval_t*, const atval_t*);
void aloV_concat(astate, size_t);
void aloV_iterator(astate, const atval_t*, atval_t*);

#endif /* AVM_H_ */
