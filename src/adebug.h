/*
 * adebug.h
 *
 *  Created on: 2019年7月25日
 *      Author: ueyudiud
 */

#ifndef ADEBUG_H_
#define ADEBUG_H_

#include "aobj.h"

ALO_IFUN alo_LineInfo* aloU_lineof(alo_Proto*, const a_insn*);
ALO_IFUN void aloU_clearbuf(alo_State);

ALO_IFUN a_none aloU_error(alo_State, int);
ALO_IFUN a_none aloU_rterror(alo_State, a_cstr, ...);
ALO_IFUN a_none aloU_ererror(alo_State, a_cstr, ...);

#define aloU_fnotfound(T,fun) aloU_rterror(T, "function '%s' not found", fun)
#define aloU_mnotfound(T,o,n) aloU_rterror(T, "method '%s.__%s' not found", aloV_typename(T, o), n);
#define aloU_outofrange(T,idx,len) aloU_rterror(T, "index out of bound, len: %i, index: %i", aloE_int(len), aloE_int(idx));
#define aloU_invalidkey(T) aloU_rterror(T, "invalid key")
#define aloU_szoutoflim(T) aloU_rterror(T, "size out of limit")

#endif /* ADEBUG_H_ */
