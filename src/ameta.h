/*
 * ameta.h
 *
 * meta programming methods.
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#ifndef AMETA_H_
#define AMETA_H_

#include "aobj.h"

#ifdef TM
#undef TM
#endif

/**
 ** all tagged methods.
 */
#define TM_LIST \
	TM(TM_LOOK, look), \
	TM(TM_DLGT, dlgt), \
	TM(TM_GET, get), \
	TM(TM_SET, set), \
	TM(TM_MODE, mode), \
	TM(TM_LEN, len), \
	TM(TM_EQ, eq), \
	TM(TM_HASH, hash), \
	TM(TM_ID, id), \
	TM(TM_ADD, add), \
	TM(TM_SUB, sub), \
	TM(TM_MUL, mul), \
	TM(TM_DIV, div), \
	TM(TM_IDIV, idiv), \
	TM(TM_MOD, mod), \
	TM(TM_POW, pow), \
	TM(TM_SHL, shl), \
	TM(TM_SHR, shr), \
	TM(TM_BAND, band), \
	TM(TM_BOR, bor), \
	TM(TM_BXOR, bxor), \
	TM(TM_AADD, aadd), \
	TM(TM_ASUB, asub), \
	TM(TM_AMUL, amul), \
	TM(TM_ADIV, adiv), \
	TM(TM_AIDIV, aidiv), \
	TM(TM_AMOD, amod), \
	TM(TM_APOW, apow), \
	TM(TM_ASHL, ashl), \
	TM(TM_ASHR, ashr), \
	TM(TM_AAND, aand), \
	TM(TM_AOR, aor), \
	TM(TM_AXOR, axor), \
	TM(TM_PNM, pnm), \
	TM(TM_UNM, unm), \
	TM(TM_BNOT, bnot), \
	TM(TM_LT, lt), \
	TM(TM_LE, le), \
	TM(TM_SLC, slc), \
	TM(TM_REM, rem), \
	TM(TM_SREM, srem), \
	TM(TM_CAT, cat), \
	TM(TM_ACAT, acat), \
	TM(TM_CALL, call), \
	TM(TM_NEW, new), \
	TM(TM_DEL, del), \
	TM(TM_ALLOC, alloc), \
	TM(TM_UNBOX, unbox), \
	TM(TM_ITR, itr), \
	TM(TM_TOSTR, tostr), \
	TM(TM_THIS, this)

/**
 ** number of tag available to use fast get function.
 */
#define ALO_NUMTM CHAR_BIT

/**
 ** number of type tags
 */
#define ALO_NUMTYPE (ALO_TUSER + 1)

/**
 ** tagged method constructor, use for different places to gain specific properties.
 */
#define TM(id,name) id

/**
 ** tagged method ID.
 */
typedef enum {
	TM_LIST,
	TM_N
} atmi;

#undef TM

/* check tagged method not in table */
#define aloT_fastxcontain(t,e) ((t) == NULL || ((t)->reserved & (1 << (e))))

#define aloT_gfastget(G,t,e) (aloT_fastxcontain(t, e) ? NULL : aloT_fastgetaux(t, (G)->stagnames[e], e))

#define aloT_gmode(G,t) ({ const alo_TVal* imode = aloT_gfastget(G, t, TM_MODE); imode && tisstr(imode) ? tasstr(imode)->array : ""; })

#define aloT_vmput1(T,o1) \
	{ tsetobj(T, T->top, o1); T->top += 1; }

#define aloT_vmput2(T,o1,o2) \
	{ tsetobj(T, T->top, o1); tsetobj(T, T->top + 1, o2); T->top += 2; }

#define aloT_vmput3(T,o1,o2,o3) \
	{ tsetobj(T, T->top, o1); tsetobj(T, T->top + 1, o2); tsetobj(T, T->top + 2, o3); T->top += 3; }

#define aloT_vmput4(T,o1,o2,o3,o4) \
	{ tsetobj(T, T->top, o1); tsetobj(T, T->top + 1, o2); tsetobj(T, T->top + 2, o3); tsetobj(T, T->top + 3, o4); T->top += 4; }

#define aloT_getmt(o) ({ atable_t** pmt = aloT_getpmt(o); pmt ? *pmt : NULL; })

ALO_VDEC const a_cstr aloT_typenames[ALO_NUMTYPE];

ALO_IFUN void aloT_init(alo_State);
ALO_IFUN atable_t** aloT_getpmt(const alo_TVal*);
ALO_IFUN const alo_TVal* aloT_gettm(alo_State, const alo_TVal*, atmi, int);
ALO_IFUN const alo_TVal* aloT_fastgetaux(atable_t*, alo_Str*, atmi);
ALO_IFUN const alo_TVal* aloT_fastget(alo_State, const alo_TVal*, atmi);
ALO_IFUN const alo_TVal* aloT_fastgetx(alo_State, const alo_TVal*, atmi);
ALO_IFUN const alo_TVal* aloT_index(alo_State, const alo_TVal*, const alo_TVal*);
ALO_IFUN const alo_TVal* aloT_lookup(alo_State, const alo_TVal*, const alo_TVal*);
ALO_IFUN void aloT_callunr(alo_State, const alo_TVal*, const alo_TVal*, alo_TVal*);
ALO_IFUN void aloT_callbin(alo_State, const alo_TVal*, const alo_TVal*, const alo_TVal*, alo_TVal*);
ALO_IFUN int aloT_callcmp(alo_State, const alo_TVal*, const alo_TVal*, const alo_TVal*);
ALO_IFUN int aloT_tryunr(alo_State, const alo_TVal*, atmi);
ALO_IFUN int aloT_trybin(alo_State, const alo_TVal*, const alo_TVal*, atmi);
ALO_IFUN int aloT_trylen(alo_State, const alo_TVal*, atable_t*);

#endif /* AMETA_H_ */
