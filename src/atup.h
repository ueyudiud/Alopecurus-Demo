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

#define atupsizel(l) (sizeof(atuple_t) + (l) * sizeof(atval_t))
#define atupsize(o)  atupsizel((o)->length)

atuple_t* aloA_new(astate, size_t, const atval_t*);
const atval_t* aloA_geti(atuple_t*, aint);
const atval_t* aloA_get(astate, atuple_t*, const atval_t*);
const atval_t* aloA_next(atuple_t*, ptrdiff_t*);
ahash_t aloA_hash(astate, atuple_t*);

#endif /* ATUP_H_ */
