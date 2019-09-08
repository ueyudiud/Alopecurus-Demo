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

size_t aloK_insn(afstat_t*, ainsn_t);
int aloK_kstr(afstat_t*, astring_t*);

/**
 ** stack manipulation.
 */

void aloK_checkstack(afstat_t*, int);
void aloK_incrstack(afstat_t*, int);

/**
 ** process control
 */

int aloK_newlabel(afstat_t*);
void aloK_putlabel(afstat_t*, int);
void aloK_marklabel(afstat_t*, int, int);
int aloK_jumpforward(afstat_t*, int);
void aloK_jumpbackward(afstat_t*, int);
void aloK_fixline(afstat_t*, int);

/**
 ** data -> reference
 */

void aloK_eval(afstat_t*, aestat_t*);
void aloK_evalk(afstat_t*, aestat_t*);
void aloK_anyR(afstat_t*, aestat_t*);
int aloK_nextreg(afstat_t*, aestat_t*);
void aloK_member(afstat_t*, aestat_t*, aestat_t*);
void aloK_fromreg(afstat_t*, aestat_t*, astring_t*);
void aloK_field(afstat_t*, aestat_t*, astring_t*);
void aloK_drop(afstat_t*, aestat_t*);

/**
 ** evaluation
 */

size_t aloK_loadnil(afstat_t*, int, int);
void aloK_loadproto(afstat_t*, aestat_t*);
void aloK_return(afstat_t*, int, int);
int aloK_putreg(afstat_t*, aestat_t*);
void aloK_singleret(afstat_t*, aestat_t*);
void aloK_fixedret(afstat_t*, aestat_t*, int);
void aloK_multiret(afstat_t*, aestat_t*);
void aloK_self(afstat_t*, aestat_t*, astring_t*);
void aloK_unbox(afstat_t*, aestat_t*, int);
void aloK_boxt(afstat_t*, aestat_t*, int);
void aloK_gwt(afstat_t*, aestat_t*);
void aloK_gwf(afstat_t*, aestat_t*);
void aloK_move(afstat_t*, aestat_t*, int);
void aloK_assign(afstat_t*, aestat_t*, aestat_t*);
void aloK_prefix(afstat_t*, aestat_t*, int);
void aloK_infix(afstat_t*, aestat_t*, int);
void aloK_suffix(afstat_t*, aestat_t*, aestat_t*, int, int);

#endif /* ACODE_H_ */
