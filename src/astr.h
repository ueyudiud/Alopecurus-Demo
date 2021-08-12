/*
 * astr.h
 *
 * methods and properties for string and raw data value.
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#ifndef ASTR_H_
#define ASTR_H_

#include "aobj.h"

/* get string size */

#define astrsizel(l) (sizeof(alo_Str) + ((l) + 1) * sizeof(char))
#define astrsize(o) astrsizel(aloS_len(o))

/* get raw data size */

#define ardsizel(l) (sizeof(alo_User) + (l) * sizeof(a_byte))
#define ardsize(o) ardsizel((o)->length)

/* create a string object by string literal */
#define aloS_newl(T,s) aloS_new(T, ""s, sizeof(s) / sizeof(char) - 1)

/**
 ** get string length.
 */
#define aloS_len(o) (tishstr(o) ? (o)->lnglen : (o)->shtlen)

ALO_IFUN a_hash aloS_rhash(a_cstr, size_t, uint64_t);
ALO_IFUN a_hash aloS_hash(alo_State, alo_Str*);
ALO_IFUN alo_Str* aloS_of(alo_State, a_cstr);
ALO_IFUN alo_Str* aloS_new(alo_State, const char*, size_t);
ALO_IFUN alo_Str* aloS_newi(alo_State, const char*, size_t);
ALO_IFUN alo_Str* aloS_createlng(alo_State, size_t);
ALO_IFUN alo_User* aloS_newr(alo_State, size_t);
ALO_IFUN int aloS_hequal(alo_Str*, alo_Str*);
ALO_IFUN int aloS_requal(alo_Str*, a_cstr, size_t);
ALO_IFUN int aloS_compare(alo_Str*, alo_Str*);
ALO_IFUN void aloS_remove(alo_State, alo_Str*);

/**
 ** string cache helper methods
 */

ALO_IFUN void aloS_init(alo_State);
ALO_IFUN void aloS_resizecache(alo_State, size_t);
ALO_IFUN void aloS_cleancache(alo_State);

#endif /* ASTR_H_ */
