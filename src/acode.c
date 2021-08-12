/*
 * acode.c
 *
 *  Created on: 2019年8月24日
 *      Author: ueyudiud
 */

#define ACODE_C_
#define ALO_CORE

#include "astate.h"
#include "agc.h"
#include "avm.h"
#include "aeval.h"
#include "alex.h"
#include "acode.h"

#include <math.h>

#define getoffset(a,b) aloE_cast(int, (a) - (b) - 1)

static void fixjmp(afstat_t* f, size_t pc, size_t dest) {
	a_insn* insn = &f->p->code[pc];
	SET_sBx(*insn, getoffset(dest, pc));
	if (GET_i(*insn) == OP_JMP) {
		int lnact = GET_A(*insn);
		if (lnact != f->firsttemp && f->nchild > 0) {
			SET_xA(*insn, true);
		}
		SET_A(*insn, f->firsttemp);
	}
}

static int nextjump(afstat_t* f, int pc) {
	int i = GET_sBx(f->p->code[pc]);
	return i != NO_JUMP ? pc + i + 1 : NO_JUMP;
}

static int isinsnreducible(afstat_t* f) {
	if (f->ncode <= 2)
		return false;
	int i = GET_i(f->p->code[f->ncode - 2]);
	return i != OP_EQ && i != OP_LT && i != OP_LE && GET_A(i) == f->firstlocal;
}

static void jmplist(afstat_t* f, int list, size_t dest) {
	int next;
	do {
		next = nextjump(f, list);
		fixjmp(f, list, dest);
	}
	while ((list = next) != NO_JUMP);
}

static void checkline(afstat_t* f) {
	int line = f->l->pl;
	if (f->lastline != line) {
		aloM_chkb(f->l->T, f->p->lineinfo, f->p->nlineinfo, f->nline, ALO_MAX_BUFSIZE);
		alo_LineInfo* info = f->p->lineinfo + f->nline++;
		info->begin = f->ncode;
		f->lastline = info->line = line; /* settle line number */
	}
}

size_t aloK_insn(afstat_t* f, a_insn insn) {
	if (f->cjump != NO_JUMP) {
		int list = f->cjump;
		if (list != f->ncode - 1 || !isinsnreducible(f) || (list = nextjump(f, list)) != NO_JUMP) {
			jmplist(f, list, f->ncode);
		}
		f->cjump = NO_JUMP;
	}
	checkline(f);
	aloM_chkb(f->l->T, f->p->code, f->p->ncode, f->ncode, ALO_MAX_BUFSIZE);
	size_t pc = f->ncode++;
	f->p->code[pc] = insn;
	return pc;
}

static alo_TVal* newconst(afstat_t* f) {
	aloM_chkbx(f->l->T, f->p->consts, f->p->nconst, f->nconst, ALO_MAX_BUFSIZE, tsetnil);
	return f->p->consts + f->nconst++;
}

static a_insn* getctrl(afstat_t* f, int index) {
	a_insn* i = f->p->code + index;
	switch (GET_i(*i)) {
	case OP_JMP:
		return i - 1;
	case OP_JCZ:
		return i;
	default:
		return aloE_check(false, "illegal control instruction", NULL);
	}
}

#define kfind(i,a,l,t,p,g) for (i = 0; i < (l); i++) { const typeof(a) _e = (a) + i; if (p(_e) && g(_e) == t) { return i; } }
#define loadbool(f,reg,value) aloK_iABx(f, OP_LDC, false, true, reg, value)

static int aloK_kint(afstat_t* f, a_int value) {
	int i;
	kfind(i, f->p->consts, f->nconst, value, tisint, tasint);
	tsetint(newconst(f), value);
	return i;
}

static int aloK_kflt(afstat_t* f, a_float value) {
#if !ALO_STRICT_NUMTYPE
	a_int t;
	if (aloO_flt2int(value, &t, 0)) {
		return aloK_kint(f, t);
	}
#endif
	int i;
	kfind(i, f->p->consts, f->nconst, value, tisflt, tasflt);
	tsetflt(newconst(f), value);
	return i;
}

int aloK_kstr(afstat_t* f, alo_Str* value) {
	int i;
	kfind(i, f->p->consts, f->nconst, value, tisstr, tasstr);
	tsetstr(f->l->T, newconst(f), value);
	return i;
}

void aloK_checkstack(afstat_t* f, int n) {
	int top = f->freelocal + n;
	if (top > f->p->nstack) {
		if (top > aloK_maxstacksize) {
			aloX_error(f->l, "stack size too big.");
		}
		f->p->nstack = top;
	}
}

void aloK_incrstack(afstat_t* f, int n) {
	aloE_assert(n >= 0, "stack size should be non-negative number");
	aloK_checkstack(f, n);
	f->freelocal += n;
}

#define hasjump(e) ((e)->lt != (e)->lf)

static void initlabel(afstat_t* f, alabel* label) {
	label->name = NULL;
	label->nactvar = f->firstlocal;
	label->layer = f->layer;
	label->line = f->l->cl;
	label->pc = f->ncode;
}

static alabel* newlabel(afstat_t* f, altable_t* table) {
	aloM_chkb(f->l->T, table->a, table->c, table->l, ALO_MAX_BUFSIZE);
	alabel* label = table->a + table->l++;
	initlabel(f, label);
	return label;
}

int aloK_newlabel(afstat_t* f) {
	return newlabel(f, &f->d->lb) - f->d->lb.a;
}

static void linkcjmp(afstat_t* f, int* p, int jmp) {
	if (jmp != NO_JUMP) { /* only update label when has jump label */
		if (*p == NO_JUMP) {
			*p = jmp;
		}
		else {
			int list, next = *p;
			do {
				list = next;
				next = nextjump(f, list);
			}
			while (next != NO_JUMP);
			fixjmp(f, list, jmp);
		}
	}
}

void aloK_putlabel(afstat_t* f, int pc) {
	if (pc != NO_JUMP) { /* create new label if there exists jumps. */
		/* correct jump index */
		if (pc + 1 == f->ncode) { /* when jump instruction is next to current slot */
			if (isinsnreducible(f)) { /* is instruction reducible? */
				int i = nextjump(f, pc);
				f->ncode -= 1; /* erase previous jump instruction */
				if (i == NO_JUMP) {
					return;
				}
				pc = i;
			}
		}
		linkcjmp(f, &f->cjump, pc); /* link jump list */
	}
}

void aloK_marklabel(afstat_t* f, int index, int list) {
	if (list != NO_JUMP) {
		size_t dest = f->d->lb.a[index].pc;
		int next;
		do {
			next = nextjump(f, list);
			fixjmp(f, list, dest);
		}
		while ((list = next) != NO_JUMP);
	}
}

static int putjump_(afstat_t* f, int prev, a_insn insn) {
	alabel* label = newlabel(f, &f->d->jp);
	if (prev != NO_JUMP) {
		SET_sBx(insn, getoffset(prev, label->pc));
	}
	return label->pc = aloK_insn(f, insn);
}

#define putjump(f,prev,i,cond,xa,a) putjump_(f, prev, CREATE_iAsBx(i, xa, cond, a, NO_JUMP))

int aloK_jumpforward(afstat_t* f, int index) {
	if (f->ncode != 0) {
		switch (GET_i(f->p->code[f->ncode - 1])) {
		case OP_RET: case OP_TCALL: { /* can direct code reach here */
			int jmp = f->cjump;
			f->cjump = NO_JUMP;
			return jmp; /* return current hold index */
		}
		case OP_JMP: { /* if last label is jump out */
			if (index == f->ncode - 1) {
				return index;
			}
			break;
		}
		}
	}

	int jmp = f->cjump;
	f->cjump = NO_JUMP;
	linkcjmp(f, &jmp, index);
	return putjump(f, jmp, OP_JMP, false, false, f->firstlocal);
}

void aloK_jumpbackward(afstat_t* f, int index) {
	alabel* label = f->d->lb.a + index;
	int flag = f->ncode >= 1 && f->cjump == f->ncode - 1;
	if (f->cjump != NO_JUMP) {
		jmplist(f, f->cjump, label->pc);
		f->cjump = NO_JUMP;
	}
	if (!flag) {
		aloK_iAsBx(f, OP_JMP, label->nactvar != f->firstlocal, false, label->nactvar, getoffset(label->pc, f->ncode));
	}
}

void aloK_fixline(afstat_t* f, int line) {
	if (f->lastline != line) {
		alo_LineInfo* infos = f->p->lineinfo;
		aloE_assert(f->nline > 0, "no code to fix line.");
		if (infos[f->nline - 1].begin == f->ncode - 1) { /* is line begin here */
			if (f->nline == 1 || infos[f->nline - 2].line != line) { /* not fit to previous line */
				infos[f->nline - 1].line = line;
			}
			else {
				f->nline -= 1; /* drop previous line information */
			}
		}
		else {
			aloM_chkb(f->l->T, f->p->lineinfo, f->p->nlineinfo, f->nline, ALO_MAX_BUFSIZE);
			alo_LineInfo* info = f->p->lineinfo + f->nline++;
			info->begin = f->ncode - 1;
			info->line = line; /* settle line number */
		}
		f->lastline = line;
	}
}

static void freereg(afstat_t* f, int index) {
	if (!aloK_iscapture(index) && index >= f->firstlocal) {
		f->freelocal--;
		aloE_xassert(f->freelocal == index);
	}
}

static void freeexp(afstat_t* f, aestat_t* e) {
	if (e->t == E_FIXED) {
		freereg(f, e->v.g);
	}
}

static void newR(afstat_t* f, aestat_t* e) {
	aloK_checkstack(f, 1);
	int index = f->freelocal;
	aloK_move(f, e, index);
	aloE_xassert(f->freelocal == index);
	f->freelocal++;
}

#define move(f,a,xb,b) aloK_iABC(f, OP_MOV, false, xb, false, a, b, 0)

void aloK_eval(afstat_t* f, aestat_t* e) {
	switch (e->t) {
	case E_LOCAL: {
		e->t = E_FIXED;
		e->v.g = aloK_setstack(e->v.g);
		break;
	}
	case E_CAPTURE: {
		e->t = E_FIXED;
		e->v.g = aloK_setcapture(e->v.g);
		break;
	}
	case E_INDEXED: {
		typeof(e->v.d) d = e->v.d;
		if (!d.fk) freereg(f, d.k);
		if (!d.fo) freereg(f, d.o);
		e->t = E_ALLOC;
		e->v.g = aloK_iABC(f, OP_GET, false, d.fo, d.fk, 0, d.o, d.k);
		break;
	}
	case E_CALL: case E_UNBOX: case E_VARARG: {
		aloK_singleret(f, e);
		break;
	}
	case E_CONCAT: {
		e->t = E_ALLOC;
		f->freelocal = e->v.a.s;
		e->v.g = aloK_iABC(f, OP_CAT, false, false, false, 0, e->v.a.s, e->v.a.l - 1);
		break;
	}
	default:
		break;
	}
}

void aloK_evalk(afstat_t* f, aestat_t* e) {
	aloK_eval(f, e);
	int index;
	switch (e->t) {
	case E_NIL: /* special handle for nil */
		return;
	case E_TRUE: case E_FALSE:
		index = e->t == E_TRUE;
		break;
	case E_INTEGER:
		index = aloK_kint(f, e->v.i);
		break;
	case E_FLOAT:
		index = aloK_kflt(f, e->v.f);
		break;
	case E_STRING:
		index = aloK_kstr(f, e->v.s);
		break;
	case E_VOID:
		e->t = E_ALLOC;
		e->v.g = aloK_iABC(f, OP_NEWA, false, false, false, 0, e->v.g + 1, 1);
		return;
	default:
		return;
	}
	if (index >= aloK_fastconstsize) {
		e->t = E_ALLOC;
		e->v.g = aloK_iABx(f, OP_LDC, false, true, 0, index);
	}
	else {
		e->t = E_CONST;
		e->v.g = index;
	}
}

static void fixregaux(afstat_t* f, aestat_t* e, int reg) {
	aloK_evalk(f, e);
	switch (e->t) {
	case E_FIXED:
		if (e->v.g != reg) {
			move(f, reg, false, e->v.g);
		}
		break;
	case E_ALLOC:
		SET_A(getinsn(f, e), reg);
		break;
	case E_CONST:
		move(f, reg, true, e->v.g);
		break;
	case E_NIL:
		aloK_loadnil(f, reg, 1);
		break;
	default:
		aloE_assert(e->t == E_JMP, "illegal type.");
		break;
	}
}

/**
 ** get new temporary register and put expression into it.
 */
int aloK_nextR(afstat_t* f, aestat_t* e) {
	if (!hasjump(e) || !(e->t == E_TRUE || e->t == E_FALSE)) { /* special cases for jump */
		aloK_evalk(f, e);
		freeexp(f, e);
	}
	newR(f, e);
	return e->v.g;
}

void aloK_anyX(afstat_t* f, aestat_t* e) {
	if (e->t == E_FIXED) {
		if (!hasjump(e)) {
			return;
		}
		else if (e->v.g >= f->freelocal) { /* temporary value */
			fixregaux(f, e, e->v.g);
			return;
		}
	}
	else {
		aloK_evalk(f, e);
		if (e->t == E_CONST || e->t == E_FIXED) {
			return;
		}
	}
	freeexp(f, e);
	newR(f, e);
}

void aloK_anyS(afstat_t* f, aestat_t* e) {
	if (e->t == E_FIXED) {
		if (!hasjump(e)) {
			return;
		}
		else if (e->v.g >= f->freelocal) { /* temporary value */
			fixregaux(f, e, e->v.g);
			return;
		}
	}
	else {
		aloK_evalk(f, e);
	}
	freeexp(f, e);
	newR(f, e);
}

/**
 ** put expression into the register
 */
int aloK_anyR(afstat_t* f, aestat_t* e) {
	if (e->t != E_LOCAL) {
		return aloK_nextR(f, e);
	}
	e->t = E_FIXED;
	return e->v.g;
}

/**
 ** get member value.
 */
void aloK_member(afstat_t* f, aestat_t* o, aestat_t* k) {
	if (o->t == E_VARARG) {
		o->t = E_ALLOC;
		if (k->t == E_INTEGER) {
			o->v.g = aloK_iABC(f, OP_SELV, 0, 2, 0, 0, k->v.i, 0);
		}
		else {
			aloK_anyX(f, k);
			o->v.g = aloK_iABC(f, OP_SELV, 0, k->t == E_CONST, 0, 0, k->v.g, 0);
		}
	}
	else {
		aloK_anyX(f, o);
		aloK_anyX(f, k);
		size_t oindex = o->v.g;
		o->v.d.fo = o->t == E_CONST;
		o->v.d.o = oindex;
		if (k->t == E_CONST && k->v.g >= aloK_fastconstsize) {
			aloK_nextR(f, k);
		}
		o->v.d.fk = k->t == E_CONST;
		o->v.d.k = k->v.g;
		o->t = E_INDEXED;
	}
}

/**
 ** remove holding value
 */
void aloK_drop(afstat_t* f, aestat_t* e) {
	switch (e->t) {
	case E_ALLOC:
		newR(f, e);
		goto fixed;
	case E_FIXED:
		fixed:
		freeexp(f, e);
		break;
	case E_INDEXED:
		if (!e->v.d.fk) freereg(f, e->v.d.k);
		if (!e->v.d.fo) freereg(f, e->v.d.o);
		break;
	case E_UNBOX: case E_CALL:
		SET_C(getinsn(f, e), 1); /* set no result */
		break;
	}
}

/**
 ** reused temporary value and put it into register.
 ** the register should on the top of stack.
 */
int aloK_reuse(afstat_t* f, aestat_t* e) {
	return e->t == E_FIXED && f->freelocal == aloK_getstack(e->v.g) ? f->freelocal++ : aloK_nextR(f, e);
}

size_t aloK_loadnil(afstat_t* f, int first, int len) {
	a_insn* prev;
	if (f->ncode > 0 && f->cjump == NO_JUMP) {
		prev = f->p->code + f->ncode - 1;
		if (GET_i(*prev) == OP_LDN) { /* is previous ldn */
			int from1 = first;
			int to1 = first + len;
			int from2 = GET_A(*prev);
			int to2 = from2 + GET_B(*prev);
			if (from1 <= to2 && from2 <= to1) { /* can be connect */
				int from = from1 < from2 ? from1 : from2;
				int to = to1 > to2 ? to1 : to2;
				SET_A(*prev, from);
				SET_B(*prev, to - from);
				return f->ncode - 1;
			}
		}
	}
	/* direct load */
	return aloK_iABC(f, OP_LDN, false, false, false, first, len, 0);
}

void aloK_loadproto(afstat_t* f, aestat_t* e) {
	e->t = E_ALLOC;
	e->lf = e->lt = NO_JUMP;
	e->v.g = aloK_iABx(f, OP_LDP, false, true, 0, f->nchild - 1);
}

/**
 ** return arguments.
 */
void aloK_return(afstat_t* f, int first, int len) {
	aloK_iABC(f, OP_RET, false, false, false, first, len + 1, 0);
}

/**
 ** adjust to only single result.
 */
void aloK_singleret(afstat_t* f, aestat_t* e) {
	switch (e->t) {
	case E_VARARG:
		e->t = E_ALLOC;
		e->v.g = aloK_iABC(f, OP_SELV, 0, 2, false, 0, 0, 0);
		break;
	case E_CALL: {
		a_insn* i = &getinsn(f, e);
		SET_C(*i, 2);
		e->t = E_FIXED;
		e->v.g = GET_A(*i);
		break;
	}
	case E_UNBOX: {
		SET_C(getinsn(f, e), 2);
		e->t = E_ALLOC;
		break;
	}
	default:
		break;
	}
}

void aloK_fixedret(afstat_t* f, aestat_t* e, int n) {
	switch (e->t) {
	case E_VARARG: {
		if (n != 0) {
			e->t = E_ALLOC;
			e->v.g = aloK_iABC(f, OP_LDV, false, false, false, 0, 0, n + 1);
		}
		else {
			e->t = E_VOID;
		}
		break;
	}
	case E_UNBOX: {
		a_insn* i = &getinsn(f, e);
		SET_C(*i, n + 1);
		e->t = E_ALLOC;
		break;
	}
	case E_CALL: {
		a_insn* i = &getinsn(f, e);
		SET_C(*i, n + 1);
		e->t = E_FIXED;
		e->v.g = GET_A(*i);
		break;
	}
	default:
		break;
	}
}

void aloK_multiret(afstat_t* f, aestat_t* e) {
	switch (e->t) {
	case E_VARARG:
		e->t = E_ALLOC;
		e->v.g = aloK_iABC(f, OP_LDV, 0, 0, false, 0, 0, 0);
		break;
	case E_CALL: {
		e->t = E_FIXED;
		e->v.g = GET_A(getinsn(f, e));
		break;
	}
	case E_UNBOX: {
		e->t = E_ALLOC;
		break;
	}
	default:
		break;
	}
}

void aloK_self(afstat_t* f, aestat_t* e, alo_Str* name) {
	aloK_anyS(f, e);
	freeexp(f, e);
	int index = aloK_kstr(f, name);
	aloK_iABC(f, OP_SELF, false, e->t == E_CONST, true, f->freelocal, e->v.g, index);
	e->t = E_FIXED;
	e->v.g = f->freelocal;
	aloK_incrstack(f, 2);
}

void aloK_unbox(afstat_t* f, aestat_t* e, int narg, int check) {
	aloK_anyS(f, e);
	freereg(f, e->v.g);
	e->v.g = aloK_iABC(f, OP_UNBOX, false, e->v.g == E_CONST, check, 0, e->v.g, narg + 1);
	e->t = E_UNBOX;
}

void aloK_boxt(afstat_t* f, aestat_t* e, int narg) {
	int multi = false;
	if ((e)->t == E_VARARG || (e)->t == E_UNBOX || (e)->t == E_CALL) {
		aloK_multiret(f, e);
		multi = true;
	}
	int i = aloK_nextR(f, e) - narg + 1;
	e->v.g = aloK_iABC(f, OP_NEWA, false, false, false, 0, i, (multi ? ALO_MULTIRET : narg) + 1);
	f->freelocal = i;
	e->t = E_ALLOC;
}

void aloK_newcol(afstat_t* f, aestat_t* e, int op, size_t narg) {
	aloE_assert(op == OP_NEWL || op == OP_NEWM, "invalid new collection operation");
	e->v.g = aloK_iABx(f, op, 0, 0, 0, narg);
	e->t = E_ALLOC;
	e->lf = e->lt = NO_JUMP;
}

void aloK_newitr(afstat_t* f, aestat_t* e) {
	aloK_anyS(f, e);
	int r = e->v.g;
	freereg(f, r);
	aloK_checkstack(f, 1);
	e->v.g = f->freelocal++;
	aloK_iABC(f, OP_ITR, 0, 0, 0, e->v.g, r, 0);
}

void aloK_set(afstat_t* f, int index, aestat_t* k, aestat_t* v) {
	aloK_anyX(f, k);
	aloK_anyX(f, v);
	aloK_iABC(f, OP_SET, false, k->t == E_CONST, v->t == E_CONST, index, k->v.g, v->v.g);
	freeexp(f, v);
	freeexp(f, k);
}

static void flipcond(afstat_t* f, int index) {
	a_insn* i = getctrl(f, index);
	SET_xC(*i, !GET_xC(*i));
}

static int condjmp(afstat_t* f, aestat_t* e, int cond) {
	aloK_anyS(f, e);
	freeexp(f, e);
	return putjump(f, NO_JUMP, OP_JCZ, cond, false, e->v.g);
}

void aloK_gwt(afstat_t* f, aestat_t* e) {
	aloK_eval(f, e);
	int label;
	switch (e->t) {
	case E_JMP: {
		label = e->v.g;
		break;
	}
	case E_TRUE: case E_FLOAT: case E_INTEGER: case E_STRING: {
		label = NO_JUMP;
		break;
	}
	case E_NIL: case E_FALSE: {
		label = aloK_jumpforward(f, NO_JUMP);
		break;
	}
	default: {
		label = condjmp(f, e, false);
		break;
	}
	}
	linkcjmp(f, &e->lf, label);
	aloK_putlabel(f, e->lt);
	e->lt = NO_JUMP;
}

void aloK_gwf(afstat_t* f, aestat_t* e) {
	aloK_eval(f, e);
	int label;
	switch (e->t) {
	case E_JMP: {
		flipcond(f, e->v.g);
		label = e->v.g;
		break;
	}
	case E_NIL: case E_FALSE: {
		label = NO_JUMP;
		break;
	}
	case E_TRUE: case E_FLOAT: case E_INTEGER: case E_STRING: {
		label = aloK_jumpforward(f, NO_JUMP);
		break;
	}
	default: {
		label = condjmp(f, e, true);
		break;
	}
	}
	linkcjmp(f, &e->lt, label);
	aloK_putlabel(f, e->lf);
	e->lf = NO_JUMP;
}

void aloK_move(afstat_t* f, aestat_t* e, int reg) {
	int end = NO_JUMP;
	if (e->t == E_JMP) {
		linkcjmp(f, &e->lf, e->v.g);
		goto next;
	}
	else if (hasjump(e)) {
		switch (e->t) {
		case E_TRUE:
			aloK_putlabel(f, e->lt);
			e->lt = NO_JUMP;
			loadbool(f, reg, true);
			end = aloK_jumpforward(f, NO_JUMP);
			goto next;
		case E_FALSE:
			aloK_putlabel(f, e->lf);
			e->lf = NO_JUMP;
			loadbool(f, reg, false);
			end = aloK_jumpforward(f, NO_JUMP);
			goto next;
		}
	}
	fixregaux(f, e, reg);
	if (hasjump(e)) {
		next:
		if (e->lt != NO_JUMP || e->t == E_JMP) {
			aloK_putlabel(f, e->lt);
			loadbool(f, reg, true);
			if (e->lf != NO_JUMP)
				end = aloK_jumpforward(f, end);
		}
		if (e->lf != NO_JUMP) {
			aloK_putlabel(f, e->lf);
			loadbool(f, reg, false);
		}
		aloK_putlabel(f, end);
	}
	e->lf = e->lt = NO_JUMP;
	e->v.g = reg;
	e->t = E_FIXED;
}

void aloK_assign(afstat_t* f, aestat_t* r, aestat_t* v) {
	int index;
	switch (r->t) {
	case E_LOCAL: {
		index = aloK_setstack(r->v.g);
		aloK_move(f, v, index);
		r->t = E_FIXED;
		break;
	}
	case E_CAPTURE: {
		index = aloK_setcapture(r->v.g);
		aloK_move(f, v, index);
		r->t = E_FIXED;
		r->v.g = index;
		break;
	}
	case E_INDEXED: {
		typeof(r->v.d) d = r->v.d;
		aloK_anyS(f, v);
		aloK_iABC(f, OP_SET, d.fo, d.fk, v->t == E_CONST, d.o, d.k, v->v.g);
		r->t = E_VOID;
		break;
	}
	default: {
		aloX_error(f->l, "can not take assignment");
		break;
	}
	}
	freeexp(f, v);
}

/**==============================================================*
 **                     constant folding                         *
 **==============================================================*/

static int tonumber(aestat_t* e, alo_TVal* o) {
	switch (e->t) {
	case E_INTEGER:
		tsetint(o, e->v.i);
		return true;
	case E_FLOAT:
		tsetflt(o, e->v.f);
		return true;
	}
	return false;
}

static int toconst(aestat_t* e, alo_TVal* o) {
	switch (e->t) {
	case E_NIL:
		tsetnil(o);
		return true;
	case E_TRUE: case E_FALSE:
		tsetbool(o, e->t == E_TRUE);
		return true;
	case E_INTEGER:
		tsetint(o, e->v.i);
		return true;
	case E_FLOAT:
		tsetflt(o, e->v.f);
		return true;
	case E_STRING:
		tsetstr(NULL, o, e->v.s);
		return true;
	}
	return false;
}

static int fromconst(aestat_t* e, const alo_TVal* o) {
	switch (ttpnv(o)) {
	case ALO_TNIL:
		e->t = E_NIL;
		break;
	case ALO_TBOOL:
		e->t = tasbool(o) ? E_TRUE : E_FALSE;
		break;
	case ALO_TINT:
		e->t = E_INTEGER;
		e->v.i = tasint(o);
		break;
	case ALO_TFLOAT: {
		a_float value = tasflt(o);
		if (isnan(value))
			return false;
		e->t = E_FLOAT;
		e->v.f = value;
		break;
	}
	default:
		return false;
	}
	return true;
}

static int foldunary(afstat_t* f, aestat_t* e, int op) {
	alo_TVal o1;
	return tonumber(e, &o1) && aloV_unrop(f->l->T, op, &o1, &o1) && fromconst(e, &o1);
}

static int foldbinary(afstat_t* f, aestat_t* e1, aestat_t* e2, int op) {
	alo_TVal o1, o2;
	return tonumber(e1, &o1) && tonumber(e2, &o2) &&
			aloV_binop(f->l->T, op, &o1, &o1, &o2) && fromconst(e1, &o1);
}

static int foldcompare(afstat_t* f, aestat_t* e1, aestat_t* e2, int op) {
	alo_TVal o1, o2;
	if (toconst(e1, &o1) && toconst(e2, &o2) && aloV_cmpop(f->l->T, op, &o1.v.b, &o1, &o2)) {
		e1->t = o1.v.b ? E_TRUE : E_FALSE;
		return true;
	}
	return false;
}

/**==============================================================*/

static void codenot(afstat_t* f, aestat_t* e) {
	switch (e->t) {
	case E_TRUE: case E_INTEGER: case E_FLOAT: case E_STRING:
		e->t = E_FALSE;
		break;
	case E_FALSE: case E_NIL:
		e->t = E_TRUE;
		break;
	case E_JMP:
		flipcond(f, e->v.g);
		break;
	default: {
		aloK_anyS(f, e);
		freeexp(f, e);
		e->t = E_JMP;
		e->v.g = putjump(f, NO_JUMP, OP_JCZ, true, false, e->v.g);
		break;
	}
	}
	{ int t = e->lf; e->lf = e->lt; e->lt = t; } /* swap label */
}

void aloK_prefix(afstat_t* f, aestat_t* e, int op, int line) {
	switch (op) {
	case OPR_BNOT: case OPR_PNM: case OPR_UNM:
		if (foldunary(f, e, op + ALO_OPPOS - OPR_PNM))
			break;
		goto normal;
	case OPR_LEN:
		if (e->t == E_VARARG) { /* variable argument length */
			e->t = E_ALLOC;
			e->v.g = aloK_iABC(f, OP_SELV, false, 3, false, 0, 0, 0);
			break;
		}
		normal:
		aloK_anyS(f, e);
		freeexp(f, e);
		e->v.g = aloK_iABC(f, op + OP_PNM - OPR_PNM, false, e->t == E_CONST, false, 0, e->v.g, 0);
		e->t = E_ALLOC;
		aloK_fixline(f, line);
		break;
	case OPR_NOT:
		codenot(f, e);
		break;
	case OPR_REM:
		if (e->t != E_INDEXED) {
			aloX_error(f->l, "only can remove value from collection");
		}
		e->v.g = aloK_iABC(f, OP_REM, false, e->v.d.fo, e->v.d.fk, 0, e->v.d.o, e->v.d.k);
		e->t = E_ALLOC;
		aloK_fixline(f, line);
		break;
	}
}

void aloK_infix(afstat_t* f, aestat_t* e, int op) {
	switch (op) {
	case OPR_AND:
		aloK_gwt(f, e);
		e->t = E_TRUE;
		break;
	case OPR_OR:
		aloK_gwf(f, e);
		e->t = E_FALSE;
		break;
	case OPR_ADD ... OPR_BXOR:
	case OPR_EQ ... OPR_GE:
		aloK_eval(f, e);
		if (e->t == E_ALLOC) {
			newR(f, e);
		}
		break;
	case OPR_CAT:
		if (e->t != E_CONCAT) {
			aloK_nextR(f, e);
		}
		break;
	}
}

static int cmpjmp(afstat_t* f, aestat_t* l, aestat_t* r, int op, int cond) {
	aloK_insn(f, CREATE_iABC(op, l->t == E_CONST, r->t == E_CONST, cond, l->v.g, r->v.g, 0));
	return putjump(f, l->lf, OP_JMP, false, false, f->firstlocal);
}

void aloK_suffix(afstat_t* f, aestat_t* l, aestat_t* r, int op, int line) {
	int label;
	switch (op) {
	case OPR_AND:
		aloE_assert(l->lt == NO_JUMP, "illegal jump label");
		aloK_gwt(f, r);
		r->t = E_TRUE;
		label = l->lf;
		*l = *r;
		linkcjmp(f, &l->lf, label);
		break;
	case OPR_OR:
		aloE_assert(l->lf == NO_JUMP, "illegal jump label");
		aloK_gwf(f, r);
		r->t = E_FALSE;
		label = l->lt;
		*l = *r;
		linkcjmp(f, &l->lt, label);
		break;
	case OPR_ADD ... OPR_BXOR:
		if (foldbinary(f, l, r, op + ALO_OPADD - OPR_ADD)) {
			break;
		}
		aloK_anyX(f, l);
		aloK_anyX(f, r);
		freeexp(f, r);
		freeexp(f, l);
		l->v.g = aloK_iABC(f, op + OP_ADD - OPR_ADD, false, l->t == E_CONST, r->t == E_CONST, 0, l->v.g, r->v.g);
		l->t = E_ALLOC;
		aloK_fixline(f, line);
		break;
	case OPR_CAT: {
		if (l->t == E_CONCAT) {
			aloK_nextR(f, r);
			l->v.a.l += 1;
		}
		else {
			aloK_nextR(f, r);
			l->v.a.s = l->v.g;
			l->v.a.l = 2;
			l->t = E_CONCAT;
		}
		break;
	}
	case OPR_EQ: case OPR_LT: case OPR_LE:
		if (foldcompare(f, l, r, op + ALO_OPEQ - OPR_EQ)) {
			break;
		}
		aloK_anyX(f, l);
		aloK_anyX(f, r);
		freeexp(f, r);
		freeexp(f, l);
		l->v.g = cmpjmp(f, l, r, op + OP_EQ - OPR_EQ, true);
		l->t = E_JMP;
		aloK_fixline(f, line);
		break;
	case OPR_NE:
		if (foldcompare(f, l, r, ALO_OPEQ)) {
			l->t ^= (E_TRUE ^ E_FALSE); /* flip value */
			break;
		}
		aloK_anyX(f, l);
		aloK_anyX(f, r);
		freeexp(f, r);
		freeexp(f, l);
		l->v.g = cmpjmp(f, l, r, OP_EQ, false);
		l->t = E_JMP;
		aloK_fixline(f, line);
		break;
	case OPR_GT: case OPR_GE:
		if (foldcompare(f, r, l, op + ALO_OPLT - OPR_GT)) {
			break;
		}
		aloK_anyX(f, l);
		aloK_anyX(f, r);
		freeexp(f, r);
		freeexp(f, l);
		l->v.g = cmpjmp(f, r, l, op + OP_LT - OPR_GT, true);
		l->t = E_JMP;
		aloK_fixline(f, line);
		break;
	}
}

void aloK_opassign(afstat_t* f, aestat_t* l, aestat_t* r, int op, int line) {
	int index;
	switch (l->t) {
	case E_LOCAL: {
		index = aloK_setstack(l->v.g);
		aloK_anyX(f, r);
		aloK_fixline(f, line);
		aloK_iABC(f, op, false, r->t == E_CONST, false, index, r->v.g, 0);
		freeexp(f, r);
		break;
	}
	case E_CAPTURE: {
		index = aloK_setcapture(l->v.g);
		aloK_anyX(f, r);
		aloK_fixline(f, line);
		aloK_iABC(f, op, false, r->t == E_CONST, false, index, r->v.g, 0);
		freeexp(f, r);
		break;
	}
	case E_INDEXED: {
		typeof(l->v.d) d = l->v.d;
		/* get value by index function */
		aestat_t e;
		e.lf = e.lt = NO_JUMP;
		e.t = E_ALLOC;
		e.v.g = aloK_iABC(f, OP_GET, false, d.fo, d.fk, 0, d.o, d.k);
		newR(f, &e);
		aloK_anyX(f, r);
		aloK_fixline(f, line);
		/* take operation for value */
		aloK_iABC(f, op, false, r->t == E_CONST, false, e.v.g, r->v.g, 0);
		/* place back to owner */
		aloK_iABC(f, OP_SET, d.fo, d.fk, false, d.o, d.k, e.v.g);
		freeexp(f, r);
		freereg(f, e.v.g);
		break;
	}
	default: {
		aloX_error(f->l, "can not take assignment");
		break;
	}
	}
	l->t = E_VOID;
}
