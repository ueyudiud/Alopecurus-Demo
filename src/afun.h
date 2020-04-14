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

#define acclsizel(l) (sizeof(aclosure_t) + (l) * sizeof(atval_t))
#define acclsize(o) acclsizel((o)->base.length)

#define aaclsizel(l) (sizeof(aclosure_t) + (l) * sizeof(acap_t))
#define aaclsize(o) aaclsizel((o)->base.length)

ALO_IFUN aacl_t* aloF_new(astate, size_t, aproto_t*);
ALO_IFUN accl_t* aloF_newc(astate, acfun, size_t);
ALO_IFUN aproto_t* aloF_newp(astate);
ALO_IFUN acap_t* aloF_find(astate, askid_t);
ALO_IFUN void aloF_close(astate, askid_t);
ALO_IFUN void aloF_deletep(astate, aproto_t*);

#endif /* AFUN_H_ */
