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

#define astrsizel(l) (sizeof(astring_t) + ((l) + 1) * sizeof(char))
#define astrsize(o) astrsizel(aloS_len(o))

#define ardsizel(l) (sizeof(arawdata_t) + (l) * sizeof(abyte))
#define ardsize(o) ardsizel((o)->length)

#define aloS_newl(T,s) aloS_new(T, ""s, sizeof(s) / sizeof(char) - 1)

#define aloS_len(o) (ttishstr(o) ? (o)->lnglen : (o)->shtlen)

unsigned aloS_rhash(astr, size_t, uint64_t);
unsigned aloS_hash(astate, astring_t*);
astring_t* aloS_of(astate, astr);
astring_t* aloS_new(astate, const char*, size_t);
astring_t* aloS_newi(astate, const char*, size_t);
astring_t* aloS_createlng(astate, size_t);
arawdata_t* aloS_newr(astate, size_t);
int aloS_hequal(astring_t*, astring_t*);
int aloS_requal(astring_t*, astr, size_t);
int aloS_compare(astring_t*, astring_t*);
void aloS_remove(astate, astring_t*);

void aloS_init(astate);
void aloS_resizecache(astate, size_t);
void aloS_cleancache(astate);

#endif /* ASTR_H_ */
