/*
 * aeval.h
 *
 * Alopecurus function evaluation for interpreter.
 *
 *  Created on: 2019年8月10日
 *      Author: ueyudiud
 */

#ifndef AEVAL_H_
#define AEVAL_H_

#include "avm.h"

ALO_IFUN void aloV_invoke(astate, int);

ALO_IFUN int aloV_binop(astate, int, atval_t*, const atval_t*, const atval_t*);
ALO_IFUN int aloV_unrop(astate, int, atval_t*, const atval_t*);
ALO_IFUN int aloV_cmpop(astate, int, int*, const atval_t*, const atval_t*);

#endif /* AEVAL_H_ */
