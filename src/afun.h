/*
 * afun.h
 *
 * methods and properties for function value.
 *
 *  Created on: 2019年7月23日
 *      Author: ueyudiud
 */

#ifndef AFUN_H_
#define AFUN_H_

#include "aobj.h"

#define acclsizel(l) (sizeof(alo_CCl) + (l) * sizeof(alo_TVal))
#define acclsize(o) acclsizel((o)->length)

#define aaclsizel(l) (sizeof(alo_ACl) + (l) * sizeof(alo_Capture*))
#define aaclsize(o) aaclsizel((o)->length)

ALO_IFUN alo_ACl* aloF_new(alo_State, size_t);
ALO_IFUN alo_CCl* aloF_newc(alo_State, a_cfun, size_t);
ALO_IFUN alo_Proto* aloF_newp(alo_State);
ALO_IFUN alo_Capture* aloF_envcap(alo_State);
ALO_IFUN alo_Capture* aloF_find(alo_State, alo_StkId);
ALO_IFUN void aloF_close(alo_State, alo_StkId);
ALO_IFUN void aloF_deletep(alo_State, alo_Proto*);

#endif /* AFUN_H_ */
