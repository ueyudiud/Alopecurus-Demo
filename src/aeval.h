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

ALO_IFUN void aloV_invoke(alo_State, int);

ALO_IFUN int aloV_binop(alo_State, int, alo_TVal*, const alo_TVal*, const alo_TVal*);
ALO_IFUN int aloV_unrop(alo_State, int, alo_TVal*, const alo_TVal*);
ALO_IFUN int aloV_cmpop(alo_State, int, int*, const alo_TVal*, const alo_TVal*);

#endif /* AEVAL_H_ */
