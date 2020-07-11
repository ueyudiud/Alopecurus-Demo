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
	TM(TM_LKUP, lkup), \
	TM(TM_DLGT, dlgt), \
	TM(TM_IDX, idx), \
	TM(TM_NIDX, nidx), \
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
#define ALO_NUMTYPE (ALO_TRAWDATA + 1)

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

#define aloT_gmode(G,t) ({ const atval_t* imode = aloT_gfastget(G, t, TM_MODE); imode && ttisstr(imode) ? tgetstr(imode)->array : ""; })

#define aloT_vmput1(T,o1) \
	{ tsetobj(T, T->top, o1); T->top += 1; }

#define aloT_vmput2(T,o1,o2) \
	{ tsetobj(T, T->top, o1); tsetobj(T, T->top + 1, o2); T->top += 2; }

#define aloT_vmput3(T,o1,o2,o3) \
	{ tsetobj(T, T->top, o1); tsetobj(T, T->top + 1, o2); tsetobj(T, T->top + 2, o3); T->top += 3; }

#define aloT_vmput4(T,o1,o2,o3,o4) \
	{ tsetobj(T, T->top, o1); tsetobj(T, T->top + 1, o2); tsetobj(T, T->top + 2, o3); tsetobj(T, T->top + 3, o4); T->top += 4; }

#define aloT_getmt(o) ({ atable_t** pmt = aloT_getpmt(o); pmt ? *pmt : NULL; })

ALO_VDEC const astr aloT_typenames[ALO_NUMTYPE];

ALO_IFUN void aloT_init(astate);
ALO_IFUN atable_t** aloT_getpmt(const atval_t*);
ALO_IFUN const atval_t* aloT_gettm(astate, const atval_t*, atmi, int);
ALO_IFUN const atval_t* aloT_fastgetaux(atable_t*, astring_t*, atmi);
ALO_IFUN const atval_t* aloT_fastget(astate, const atval_t*, atmi);
ALO_IFUN const atval_t* aloT_fastgetx(astate, const atval_t*, atmi);
ALO_IFUN const atval_t* aloT_index(astate, const atval_t*, const atval_t*);
ALO_IFUN const atval_t* aloT_lookup(astate, const atval_t*, const atval_t*);
ALO_IFUN void aloT_callunr(astate, const atval_t*, const atval_t*, atval_t*);
ALO_IFUN void aloT_callbin(astate, const atval_t*, const atval_t*, const atval_t*, atval_t*);
ALO_IFUN int aloT_callcmp(astate, const atval_t*, const atval_t*, const atval_t*);
ALO_IFUN int aloT_tryunr(astate, const atval_t*, atmi);
ALO_IFUN int aloT_trybin(astate, const atval_t*, const atval_t*, atmi);
ALO_IFUN int aloT_trylen(astate, const atval_t*, atable_t*);

#endif /* AMETA_H_ */
