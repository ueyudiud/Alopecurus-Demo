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

void aloU_init(astate);
astr aloU_getcname(astate, acfun);
void aloU_bind(astate, acfun, astring_t*);
void aloU_clearbuf(astate);

anoret aloU_rterror(astate, astr, ...);

#endif /* ADEBUG_H_ */
