/*
 * acode.h
 *
 *  Created on: 2019年8月23日
 *      Author: ueyudiud
 */

#ifndef ACODE_H_
#define ACODE_H_

#include "aop.h"
#include "aparse.h"

/* binary operator */
enum {
	OPR_ADD, OPR_SUB,
	OPR_MUL, OPR_DIV, OPR_IDIV, OPR_MOD,
	OPR_POW,
	OPR_SHL, OPR_SHR,
	OPR_BAND, OPR_BOR, OPR_BXOR,
	OPR_CAT,
	OPR_EQ, OPR_LT, OPR_LE,
	OPR_NE, OPR_GT, OPR_GE,
	OPR_AND, OPR_OR
};

/* unary operator */
enum {
	OPR_PNM, OPR_UNM, OPR_LEN, OPR_BNOT, OPR_NOT, OPR_REM
};

/**
 ** mark for no jump label.
 **/
#define NO_JUMP (-1)

/**
 ** get instruction.
 */
#define getinsn(f,e) ((f)->p->code[(e)->v.g])

#define aloK_iABC(f,i,xA,xB,xC,A,B,C) aloK_insn(f, CREATE_iABC(i, xA, xB, xC, A, B, C))
#define aloK_iABx(f,i,xA,xB,A,Bx) aloK_insn(f, CREATE_iABx(i, xA, xB, A, Bx))
#define aloK_iAsBx(f,i,xA,xC,A,Bx) aloK_insn(f, CREATE_iAsBx(i, xA, xC, A, Bx))

ALO_IFUN size_t aloK_insn(afstat_t*, a_insn);
ALO_IFUN int aloK_kstr(afstat_t*, alo_Str*);
ALO_IFUN void aloK_loadstr(afstat_t*, aestat_t*, alo_Str*);

/**
 ** stack manipulation.
 */

ALO_IFUN void aloK_checkstack(afstat_t*, int);
ALO_IFUN void aloK_incrstack(afstat_t*, int);

/**
 ** process control
 */

ALO_IFUN int aloK_newlabel(afstat_t*);
ALO_IFUN void aloK_putlabel(afstat_t*, int);
ALO_IFUN void aloK_marklabel(afstat_t*, int, int);
ALO_IFUN int aloK_jumpforward(afstat_t*, int);
ALO_IFUN void aloK_jumpbackward(afstat_t*, int);
ALO_IFUN void aloK_fixline(afstat_t*, int);
ALO_IFUN void aloK_return(afstat_t*, int, int);
ALO_IFUN void aloK_gwt(afstat_t*, aestat_t*);
ALO_IFUN void aloK_gwf(afstat_t*, aestat_t*);

/**
 ** data -> reference
 */

ALO_IFUN void aloK_eval(afstat_t*, aestat_t*);
ALO_IFUN void aloK_evalk(afstat_t*, aestat_t*);
ALO_IFUN int aloK_anyR(afstat_t*, aestat_t*);
ALO_IFUN void aloK_anyS(afstat_t*, aestat_t*);
ALO_IFUN void aloK_anyX(afstat_t*, aestat_t*);
ALO_IFUN int aloK_nextR(afstat_t*, aestat_t*);
ALO_IFUN void aloK_member(afstat_t*, aestat_t*, aestat_t*);
ALO_IFUN void aloK_drop(afstat_t*, aestat_t*);
ALO_IFUN int aloK_reuse(afstat_t*, aestat_t*);

/**
 ** evaluation
 */

ALO_IFUN size_t aloK_loadnil(afstat_t*, int, int);
ALO_IFUN void aloK_loadproto(afstat_t*, aestat_t*);
ALO_IFUN void aloK_singleret(afstat_t*, aestat_t*);
ALO_IFUN void aloK_fixedret(afstat_t*, aestat_t*, int);
ALO_IFUN void aloK_multiret(afstat_t*, aestat_t*);
ALO_IFUN void aloK_self(afstat_t*, aestat_t*, alo_Str*);
ALO_IFUN void aloK_unbox(afstat_t*, aestat_t*, int, int);
ALO_IFUN void aloK_boxt(afstat_t*, aestat_t*, int);
ALO_IFUN void aloK_newcol(afstat_t*, aestat_t*, int, size_t);
ALO_IFUN void aloK_newitr(afstat_t*, aestat_t*);
ALO_IFUN void aloK_set(afstat_t*, int, aestat_t*, aestat_t*);
ALO_IFUN void aloK_move(afstat_t*, aestat_t*, int);
ALO_IFUN void aloK_assign(afstat_t*, aestat_t*, aestat_t*);
ALO_IFUN void aloK_prefix(afstat_t*, aestat_t*, int, int);
ALO_IFUN void aloK_infix(afstat_t*, aestat_t*, int);
ALO_IFUN void aloK_suffix(afstat_t*, aestat_t*, aestat_t*, int, int);
ALO_IFUN void aloK_opassign(afstat_t*, aestat_t*, aestat_t*, int, int);

#endif /* ACODE_H_ */
