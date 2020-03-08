/*
 * adebug.h
 *
 *  Created on: 2019年7月25日
 *      Author: ueyudiud
 */

#ifndef ADEBUG_H_
#define ADEBUG_H_

#include "aobj.h"

#define aloU_fnotfound(T,fun) aloU_rterror(T, "function '%s' not found", fun)
#define aloU_outofrange(T,idx,len) aloU_rterror(T, "index out of bound");
#define aloU_invalidkey(T) aloU_rterror(T, "invalid key")

ALO_IFUN void aloU_init(astate);
ALO_IFUN astr aloU_getcname(astate, acfun);
ALO_IFUN alineinfo_t* aloU_lineof(aproto_t*, const ainsn_t*);
ALO_IFUN void aloU_bind(astate, acfun, astring_t*);
ALO_IFUN void aloU_clearbuf(astate);

ALO_IFUN anoret aloU_mnotfound(astate, const atval_t*, astr);
ALO_IFUN astring_t* aloU_pushmsg(astate, astr, ...);
ALO_IFUN anoret aloU_rterror(astate, astr, ...);

#endif /* ADEBUG_H_ */
