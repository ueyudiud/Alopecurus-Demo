/*
 * adebug.h
 *
 *  Created on: 2019年7月25日
 *      Author: ueyudiud
 */

#ifndef ADEBUG_H_
#define ADEBUG_H_

#include "aobj.h"

ALO_IFUN void aloU_init(astate);
ALO_IFUN astr aloU_getcname(astate, acfun);
ALO_IFUN alineinfo_t* aloU_lineof(aproto_t*, const ainsn_t*);
ALO_IFUN void aloU_bind(astate, acfun, astring_t*);
ALO_IFUN void aloU_clearbuf(astate);

ALO_IFUN anoret aloU_error(astate, int);
ALO_IFUN anoret aloU_rterror(astate, astr, ...);
ALO_IFUN anoret aloU_ererror(astate, astr, ...);

#define aloU_fnotfound(T,fun) aloU_rterror(T, "function '%s' not found", fun)
#define aloU_mnotfound(T,o,n) aloU_rterror(T, "method '%s.__%s' not found", aloV_typename(T, o), n);
#define aloU_outofrange(T,idx,len) aloU_rterror(T, "index out of bound, len: %i, index: %i", aloE_int(len), aloE_int(idx));
#define aloU_invalidkey(T) aloU_rterror(T, "invalid key")
#define aloU_szoutoflim(T) aloU_rterror(T, "size out of limit")

#endif /* ADEBUG_H_ */
