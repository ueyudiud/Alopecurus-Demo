/*
 * agc.h
 *
 * garbage collection.
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#ifndef AGC_H_
#define AGC_H_

#include "amem.h"

/**
 ** GC constants and helper macros.
 */

#define aloG_white1 0x01
#define aloG_white2 0x02
#define aloG_black  0x04
#define aloG_final  0x08
#define aloG_fixed  0x10

#define aloG_white (aloG_white1| aloG_white2)
#define aloG_color (aloG_white | aloG_black )

#define otherwhite(bit) aloE_byte((bit) ^ aloG_white)

#define aloG_whitebit(G) ((G)->whitebit)
#define aloG_otherbit(G) otherwhite((G)->whitebit)

#define aloG_iswhite(g) ((g)->mark & aloG_white)
#define aloG_isgray(g) (((g)->mark & aloG_color) == 0)
#define aloG_isblack(g) ((g)->mark & aloG_black)
#define aloG_isfinalizable(g) ((g)->mark & aloG_final)
#define aloG_isfixed(g) ((g)->mark & aloG_fixed)
#define aloG_isdead(G,g) ((g)->mark & aloG_otherbit(G))
#define aloG_isalive(G,g) (!aloG_isdead(G, g))

enum {
	GCStepPropagate,
	GCStepAtomic,
	GCStepSwpNorm,
	GCStepSwpFin,
	GCStepSwpFnz,
	GCStepCallFin,
	GCStepPause
};

enum {
	GCKindNormal,
	GCKindEmergency
};

/**
 ** GC functions.
 */

void aloG_register_(astate, agct, abyte);

#define aloG_register(T,g,tt) aloG_register_(T, r2g(g), tt)

void aloG_fix_(astate, agct);

#define aloG_fix(T,g) aloG_fix_(T, r2g(g))

void aloG_unfix_(astate, agct);

#define aloG_unfix(T,g) aloG_unfix_(T, r2g(g))

void aloG_checkfnzobj(astate, agct, atable_t*);

void aloG_barrier_(astate, agct, agct);

#define aloG_barrier(T,owner,value) if (aloG_isblack(owner) && aloG_iswhite(value)) aloG_barrier_(T, r2g(owner), r2g(value))

#define aloG_barriert(T,owner,value) if (ttisref(value)) aloG_barrier(T, owner, tgetref(value))

void aloG_barrierback_(astate, agct);

#define aloG_barrierback(T,owner,value) if (aloG_isblack(owner) && aloG_iswhite(value)) aloG_barrierback_(T, r2g(owner))

#define aloG_barrierbackt(T,owner,value) if (ttisref(value)) aloG_barrierback(T, owner, tgetref(value))

void aloG_step(astate);

#define aloG_xcheck(T,pre,post) if ((T)->g->mdebt > 0) { pre; aloG_step(T); post; }

#define aloG_check(T) aloG_xcheck(T,,)

void aloG_fullgc(astate, int);

void aloG_clear(astate);

#endif /* AGC_H_ */
