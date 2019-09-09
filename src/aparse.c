/*
 * aparse.c
 *
 *  Created on: 2019年8月21日
 *      Author: ueyudiud
 */

#include "aop.h"
#include "afun.h"
#include "agc.h"
#include "avm.h"
#include "ado.h"
#include "alex.h"
#include "aparse.h"
#include "acode.h"
#include "aload.h"

#include <stdarg.h>
#include <string.h>

#define check(l,id) ((l)->ct.t == (id))
#define test(l,id) if (!check(l, id)) error_expected(l, id)
#define testenclose(l,f,t,ln) if (!check(l, t)) error_enclose(l, f, t, ln); else poll(l)
#define checknext(l,id) (check(l, id) ? (poll(l), true) : false)
#define testnext(l,id) { test(l, id); poll(l); }
#define poll aloX_poll
#define foward aloX_forward
#define lerror aloX_error
#define checklimit(l,n,lim,msg) if ((n) > (lim)) lerror(l, msg)

struct alo_Block {
	ablock_t* prev; /* previous block */
	size_t flabel; /* index of first label in this block */
	size_t fjump; /* index of first jump in this block */
	int labelc; /* index of continue */
	int labelb; /* index of jump from break */
	uint16_t nactvar; /* number of local variable at beginning of this block */
	abyte incap; /* true when some local variable in this block is capture */
	abyte loop; /* true when block is loop */
};

static void expr(alexer_t*, aestat_t*);

static anoret lerrorf(alexer_t* lex, astr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	astr s = aloV_pushvfstring(lex->T, fmt, varg);
	va_end(varg);
	lerror(lex, s);
}

static anoret error_expected(alexer_t* lex, int type) {
	lerrorf(lex, "%s expected, got %s", aloX_tkid2str(lex, type), aloX_token2str(lex, &lex->ct));
}

static anoret error_enclose(alexer_t* lex, int l, int r, int line) {
	if (lex->cl == line) {
		error_expected(lex, r);
	}
	else {
		lerrorf(lex, "'%c' expected (to close '%c' at line %d)", l, r, line);
	}
}

static int isending(alexer_t* lex) {
	switch (lex->ct.t) {
	case ')': case ']': case '}':
	case TK_EOF: case TK_ELSE: case TK_CASE:
		return true;
	default:
		return false;
	}
}

static astring_t* check_name(alexer_t* lex) {
	test(lex, TK_IDENT);
	astring_t* s = lex->ct.d.s;
	poll(lex);
	return s;
}

#define OPR_NONE (-1)

static const struct {
	abyte l, r; /* left and right priority */
} priorities[] = {
	{ 12, 12 }, { 12, 12 },                           /* '+', '-' */
	{ 13, 13 }, { 13, 13 }, { 13, 13 }, { 13, 13 },   /* '*', '/', '//', '%' */
	{ 14, 13 },                                       /* '^' */
	{ 11, 11 }, { 11, 11 },                           /* '<<', '>>' */
	{ 10, 10 }, { 9, 9 }, { 8, 8 },                   /* '&', '|', '~' */
	{ 7, 7 },                                         /* '::' */
	{ 6, 6 },                                         /* '..' */
	{ 4, 4 }, { 5, 5 }, { 5, 5 },                     /* '==', '<', '<=' */
	{ 4, 4 }, { 5, 5 }, { 5, 5 },                     /* '!=', '>', '>=' */
	{ 3, 3 }, { 2, 2 },                               /* '&&', '||' */
	{ 1, 1 }                                          /* ',' */
};

static int getbinop(int t) {
	switch (t) {
	case '+': return OPR_ADD;
	case '-': return OPR_SUB;
	case '*': return OPR_MUL;
	case '/': return OPR_DIV;
	case TK_IDIV: return OPR_IDIV;
	case '%': return OPR_MOD;
	case '^': return OPR_POW;
	case TK_SHL: return OPR_SHL;
	case TK_SHR: return OPR_SHR;
	case TK_BCOL: return OPR_CAT;
	case '<': return OPR_LT;
	case '>': return OPR_GT;
	case TK_LE: return OPR_LE;
	case TK_GE: return OPR_GE;
	case TK_EQ: return OPR_EQ;
	case TK_NE: return OPR_NE;
	case '&': return OPR_BAND;
	case '|': return OPR_BOR;
	case '~': return OPR_BXOR;
	case TK_AND: return OPR_AND;
	case TK_OR: return OPR_OR;
	default: return OPR_NONE;
	}
}

static int getunrop(int t) {
	switch (t) {
	case '+': return OPR_PNM;
	case '-': return OPR_UNM;
	case '#': return OPR_LEN;
	case '~': return OPR_BNOT;
	case '!': return OPR_NOT;
	case '$': return OPR_REM;
	default: return OPR_NONE;
	}
}

static void regsym(afstat_t* f, int type, astring_t* name, int index) {
	aloM_chkb(f->l->T, f->d->ss.a, f->d->ss.c, f->d->ss.l, ALO_MAX_BUFSIZE);
	asymbol* symbol = f->d->ss.a + f->d->ss.l++;
	symbol->type = type;
	symbol->name = name;
	symbol->layer = f->layer;
	symbol->index = index;
}

static int regloc(afstat_t* f, astring_t* name) {
	aloM_chkb(f->l->T, f->p->locvars, f->p->nlocvar, f->nlocvar, aloK_maxstacksize);
	int index = f->nlocvar++;
	int reg = f->nactvar++;
	f->p->locvars[index] = (alocvar_t) {
		name: name,
		start: f->ncode,
		index: reg
	};
	regsym(f, SYMBOL_LOC, name, index);
	return reg;
}

static void initexp(aestat_t* e, int type) {
	e->t = type;
	e->lf = e->lt = NO_JUMP;
}

#define TONULL(x) *(x) = NULL

static aproto_t* newproto(afstat_t* f) {
	aloM_chkbx(f->l->T, f->p->children, f->p->nchild, f->nchild, ALO_MAX_BUFSIZE, TONULL);
	aproto_t* p = aloF_newp(f->l->T);
	f->p->children[f->nchild++] = p;
	return p;
}

static void initproto(astate T, afstat_t* f) {
	aproto_t* p = f->p;
	p->src = f->l->src;
	p->linefdef = f->l->cl;
	/* built-in constants */
	aloM_newb(T, p->consts, p->nconst, 64);
	p->consts[0] = tnewbool(false);
	p->consts[1] = tnewbool(true);
	f->nconst = 3;
}

static void closeproto(astate T, afstat_t* f) {
	aproto_t* p = f->p;
	p->lineldef = f->l->pl;
	aloM_adjb(T, p->code, p->ncode, f->ncode);
	aloM_adjb(T, p->consts, p->nconst, f->nconst);
	aloM_adjb(T, p->captures, p->ncap, f->ncap);
	aloM_adjb(T, p->locvars, p->nlocvar, f->nlocvar);
	aloM_adjb(T, p->children, p->nchild, f->nchild);
	aloM_adjb(T, p->lineinfo, p->nlineinfo, f->nline);
}

static void enterblock(afstat_t* f, ablock_t* b, int isloop) {
	*b = (ablock_t) {
		loop: isloop,
		nactvar: f->nactvar,
		flabel: f->d->lb.l,
		fjump: f->d->lb.l,
		labelc: isloop ? aloK_newlabel(f) : NO_JUMP,
		labelb: NO_JUMP,
		incap: false,
		prev: f->b
	};
	f->b = b;
}

static void leaveblock(afstat_t* f) {
	ablock_t* b = f->b;
	asymbol* symbols = f->d->ss.a + f->firstlocal;
	for (size_t i = b->nactvar; i < f->nactvar; ++i) {
		if (symbols[i].type != SYMBOL_VAR) {
			f->p->locvars[symbols[i].index].end = f->ncode; /* settle end pc */
		}
	}
	f->nactvar = b->nactvar;
	int njmp = b->fjump;
	alabel* labels = f->d->jp.a;
	for (int i = b->fjump; i < f->d->jp.l; ++i) {
		if (GET_sBx(f->p->code[labels[i].pc]) == NO_JUMP) { /* not settled yet */
			labels[i].nactvar = b->nactvar;
			labels[njmp++] = labels[i];
		}
	}
	f->d->jp.l = njmp;
	f->d->lb.l = b->flabel;
	f->b = b->prev;
}

static void enterfunc(afstat_t* f, afstat_t* parent, ablock_t* b) {
	*f = (afstat_t) {
		l: parent->l,
		p: newproto(parent),
		e: parent,
		b: b,
		d: parent->d,
		cjump: parent->cjump,
		firstlocal: parent->d->ss.l,
		layer: parent->layer + 1
	};
	initproto(f->l->T, f);
	parent->l->f = f;
	enterblock(f, b, false);
}

static void leavefunc(afstat_t* f) {
	leaveblock(f);
	astate T = f->l->T;
	closeproto(T, f);
	f->l->f = f->e; /* move to enclose function */
	aloG_check(T);
}

#define hasmultiarg(e) ((e)->t == E_VARARG || (e)->t == E_UNBOX || (e)->t == E_CALL)

static void istat(alexer_t*);

static int varexpr(alexer_t* lex, aestat_t* e) {
	/* varexpr -> expr { ',' varexpr } */
	int n = 1;
	expr(lex, e);
	while (checknext(lex, ',')) {
		aloK_nextreg(lex->f, e);
		expr(lex, e);
		n++;
	}
	return n;
}

static void primaryexpr(alexer_t* lex, aestat_t* e) {
	/* primaryexpr -> IDENT | LITERAL | '(' [varexpr] ')' */
	switch (lex->ct.t) {
	case TK_IDENT: {
		aloK_field(lex->f, e, check_name(lex));
		break;
	}
	case '(': {
		int line = lex->cl;
		poll(lex);
		if (checknext(lex, ')')) { /* enclosed directly? */
			e->t = E_VOID;
		}
		else {
			int n = varexpr(lex, e);
			if (n > 1) { /* create tuple? */
				aloK_singleret(lex->f, e);
				aloK_boxt(lex->f, e, n);
			}
			testenclose(lex, '(', ')', line);
		}
		break;
	}
	case TK_NIL: {
		initexp(e, E_NIL);
		poll(lex);
		break;
	}
	case TK_FALSE: {
		initexp(e, E_FALSE);
		poll(lex);
		break;
	}
	case TK_TRUE: {
		initexp(e, E_TRUE);
		poll(lex);
		break;
	}
	case TK_INTEGER: {
		initexp(e, E_INTEGER);
		e->v.i = lex->ct.d.i;
		poll(lex);
		break;
	}
	case TK_FLOAT: {
		initexp(e, E_FLOAT);
		e->v.f = lex->ct.d.f;
		poll(lex);
		break;
	}
	case TK_STRING: {
		initexp(e, E_STRING);
		e->v.s = lex->ct.d.s;
		poll(lex);
		break;
	}
	default: {
		lerrorf(lex, "illegal primary expression.");
	}
	}
}

static void partialfun(alexer_t*);

static int statements(alexer_t*);

static int funargs(alexer_t* lex, aestat_t* e) {
	int n;
	switch (lex->ct.t) {
	case TK_STRING: { /* funargs -> STRING */
		initexp(e, E_STRING);
		e->v.s = lex->ct.d.s;
		poll(lex);
		n = 1;
		break;
	}
	case '(': { /* funargs -> '(' {varexpr} ')' */
		int line = lex->cl;
		poll(lex);
		if (checknext(lex, ')')) {
			n = 0;
		}
		else {
			n = varexpr(lex, e);
			testenclose(lex, '(', ')', line);
		}
		if (check(lex, '{')) {
			aloK_nextreg(lex->f, e);
			goto func;
		}
		break;
	}
	case '{' : { /* funargs -> '{' (stats | partialfun) '}' */
		n = 0;
		func: {
			afstat_t f;
			ablock_t b;
			enterfunc(&f, lex->f, &b);
			int line = lex->cl;
			poll(lex); /* skip '}' */
			if (check(lex, TK_CASE)) { /* partial function mode */
				partialfun(lex);
			}
			else {
				if (!statements(lex)) {
					aloK_return(lex->f, 0, 0); /* add return statement if no statement exist */
				}
			}
			testenclose(lex, '{', '}', line);
			leavefunc(&f);
			initexp(e, E_ALLOC);
			e->v.g = aloK_iABx(lex->f, OP_LDP, false, true, 0, lex->f->nchild - 1);
			n++;
		}
		break;
	}
	default:
		lerrorf(lex, "illegal function arguments");
		break;
	}
	if (hasmultiarg(e)) {
		aloK_multiret(lex->f, e);
		if (e->t == E_ALLOC) {
			SET_A(getinsn(lex->f, e), lex->f->freelocal++);
		}
		n = ALO_MULTIRET;
	}
	else if (n > 0) {
		aloK_nextreg(lex->f, e);
	}
	return n;
}

static void suffixexpr(alexer_t* lex, aestat_t* e) {
	/* suffixexpr -> primaryexpr { '.' ['?'] IDENT | '[' ['?'] expr ']' | '...' | '->' IDENT [ funargs ] | funargs } */
	primaryexpr(lex, e);
	aestat_t e2;
	afstat_t* f = lex->f;
	int n;
	int line;
	while (true) {
		switch (lex->ct.t) {
		case '.': {
			poll(lex); /* skip '.' */
			initexp(&e2, E_STRING);
			e2.v.s = check_name(lex);
			aloK_member(f, e, &e2);
			break;
		}
		case '[': {
			int line = lex->cl;
			do {
				poll(lex); /* skip '[' or ',' */
				expr(lex, &e2);
				aloK_member(f, e, &e2);
			}
			while (check(lex, ','));
			testenclose(lex, '[', ']', line);
			break;
		}
		case TK_RARR: {
			line = lex->cl;
			poll(lex); /* skip '->' */
			aloK_self(f, e, check_name(lex));
			switch (lex->ct.t) {
			case '(':
				n = funargs(lex, &e2) + 1;
				break;
			default:
				n = 1;
				break;
			}
			goto call;
		}
		case '(': case '{': case TK_STRING: case '\\': {
			line = lex->cl;
			aloK_nextreg(f, e);
			n = funargs(lex, &e2);
			call:
			f->freelocal = e->v.g + 1; /* remove all arguments and only remain one result */
			e->v.g = aloK_iABC(f, OP_CALL, false, false, false, e->v.g, n + 1, 0);
			e->t = E_CALL;
			aloK_fixline(f, line);
			break;
		}
		case TK_TDOT: {
			poll(lex);
			aloK_unbox(f, e, ALO_MULTIRET);
			break;
		}
		default:
			return;
		}
	}
}

static void monexpr(alexer_t* lex, aestat_t* e) {
	/* monexpr -> unrop monexpr | suffixexpr */
	int op = getunrop(lex->ct.t);
	if (op != OPR_NONE) {
		int line = lex->cl;
		poll(lex); /* skip operator */
		monexpr(lex, e);
		aloK_prefix(lex->f, e, op, line);
	}
	else {
		suffixexpr(lex, e);
	}
}

static int subexpr(alexer_t* lex, aestat_t* e, int limit) {
	/* subexpr -> monexpr { binop subexpr }
	 * the tail subexpr level should less than this expression. */
	monexpr(lex, e);
	int op = getbinop(lex->ct.t);
	while (op != OPR_NONE && priorities[op].l > limit) {
		int line = lex->cl;
		poll(lex);
		aloK_infix(lex->f, e, op);
		aestat_t e2;
		int op2 = subexpr(lex, &e2, priorities[op].r);
		aloK_suffix(lex->f, e, &e2, op, line);
		op = op2;
	}
	return op;
}

static void expr(alexer_t* lex, aestat_t* e) {
	subexpr(lex, e, 0);
}

static void semis(alexer_t* lex) {
	/* semis -> {';'} */
	while (check(lex, ';'))
		poll(lex);
}

typedef struct alo_LHS_Assign {
	struct alo_LHS_Assign* p; /* previous assign target */
	aestat_t e; /* current reference */
} alhsa_t;

static void check_conflict(afstat_t* f, alhsa_t* a, aestat_t* e) {
	int extra = f->freelocal;
	int conflict = false;
	int index = e->t == E_CAPTURE ? aloK_setcapture(e->v.g) : aloK_setstack(e->v.g);
	for (; a; a = a->p) { /* check from linked list */
		if (a->e.t == E_INDEXED) {
			if (!a->e.v.d.fo && a->e.v.d.o == index) {
				conflict = true;
				a->e.t = E_LOCAL;
				a->e.v.g = extra;
			}
			if (!a->e.v.d.fk && a->e.v.d.k == index) {
				conflict = true;
				a->e.t = E_LOCAL;
				a->e.v.g = extra;
			}
		}
	}
	if (conflict) {
		/* copy capture/local value to a temporary (in position 'extra') */
		aloK_iABC(f, OP_MOV, false, false, false, extra, index, 0);
		aloK_incrstack(f, 1);
	}
}

static void adjust_assign(afstat_t* f, int nvar, int narg, aestat_t* e) {
	int extra = nvar - narg;
	if (hasmultiarg(e)) {
		extra ++; /* flat last call */
		if (extra < 0) {
			extra = 0;
		}
		aloK_fixedret(f, e, extra);
		if (e->t == E_ALLOC) {
			aloK_nextreg(f, e);
		}
		if (extra > 0) {
			aloK_incrstack(f, extra - 1);
		}
	}
	else {
		if (e->t != E_VOID) {
			aloK_nextreg(f, e);
		}
		if (extra > 0) {
			int reg = f->freelocal;
			aloK_loadnil(f, reg, extra);
			aloK_incrstack(f, extra);
		}
	}
	if (narg > nvar) {
		f->freelocal -= narg - nvar;  /* remove extra values */
	}
}

static int pushcv(afstat_t* f) {
	aloM_chkb(f->l->T, f->d->cv.a, f->d->cv.c, f->d->cv.l, ALO_MAX_BUFSIZE);
	return f->d->cv.l++;
}

static void patterns(alexer_t* lex) {
	/* patterns -> pattern {',' patterns} */
	int parent = NO_CASEVAR, previous = NO_CASEVAR;

	/* get next case variable */
	acasevar_t* nextcv(int type, astring_t* name) {
		afstat_t* f = lex->f;
		*(parent != NO_CASEVAR ? &f->d->cv.a[parent].nchild : &f->d->cv.nchild) += 1;
		if (previous != NO_CASEVAR) {
			f->d->cv.a[previous].next = f->d->cv.l;
		}
		int index = pushcv(f);
		acasevar_t* var = f->d->cv.a + index;
		var->type = type;
		var->src = var->dest = -1;
		var->name = name;
		var->nchild = 0;
		var->parent = parent;
		var->prev = previous;
		var->next = NO_CASEVAR;
		previous = index;
		return var;
	}

	/* enter case variable */
	void entercv() {
		afstat_t* f = lex->f;
		previous = NO_CASEVAR;
		parent = f->d->cv.l - 1;
	}

	/* leave case variable */
	void leavecv() {
		acasevar_t* var = lex->f->d->cv.a + parent;
		switch (var->type) {
		case CV_UNBOX:
			testenclose(lex, '(', ')', var->line);
			break;
		case CV_UNBOXSEQ:
			testenclose(lex, '[', ']', var->line);
			break;
		default:
			lerrorf(lex, "unexpected token");
			break;
		}
		parent = var->parent;
		previous = var->prev;
	}

	astring_t *name, *handle;

	do {
		begin:
		switch (lex->ct.t) {
		case '(': { /* pattern -> '(' patterns ')' */
			name = handle = NULL;
			unbox: {
				poll(lex);
				acasevar_t* var = nextcv(CV_UNBOX, name);
				if (handle) {
					aloK_field(lex->f, &var->expr, handle);
				}
				else { /* no unboxer, use identical unboxer */
					initexp(&var->expr, E_VOID);
				}
				entercv();
			}
			goto begin; /* enter into block, skip ',' check */
		}
		case '[': { /* pattern -> '[' patterns ']' */
			name = handle = NULL;
			unboxseq: {
				poll(lex);
				acasevar_t* var = nextcv(CV_UNBOX, name);
				if (handle) {
					aloK_field(lex->f, &var->expr, handle);
				}
				else { /* no unboxer, use identical unboxer */
					initexp(&var->expr, E_VOID);
				}
				entercv();
			}
			goto begin; /* enter into block, skip ',' check */
		}
		case TK_NIL: case TK_TRUE: case TK_FALSE: case TK_INTEGER: case TK_FLOAT: { /* pattern -> LITERAL */
			acasevar_t* var = nextcv(CV_MATCH, NULL);
			primaryexpr(lex, &var->expr); /* scan match value */
			break;
		}
		case TK_IDENT: {
			name = check_name(lex);
			switch (lex->ct.t) {
			case '@':
				handle = poll(lex) == TK_IDENT ? check_name(lex) : NULL;
				switch (lex->ct.t) {
				case '(': /* pattern -> IDENT '@' {IDENT} '(' patterns ')' */
					goto unbox;
				case '[': /* pattern -> IDENT '@' {IDENT} '[' patterns ']' */
					goto unboxseq;
				default:
					lerror(lex, "unexpected token");
					break;
				}
				break;
			case '(': /* pattern -> IDENT '(' patterns ')' */
				handle = name;
				name = NULL;
				goto unbox;
			case '[': /* pattern -> IDENT '[' patterns ']' */
				handle = name;
				name = NULL;
				goto unboxseq;
			default: /* pattern -> IDENT */
				nextcv(CV_NORMAL, name);
				break;
			}
			break;
		}
		}
		label:
		if (parent != NO_CASEVAR) { /* check leave block */
			switch (lex->ct.t) {
			case ')': case ']':
				leavecv();
				goto label;
			}
		}
	}
	while (checknext(lex, ','));
}

//static int checkdense(afstat_t* f, acasevar_t* v) {
//	acasevar_t* const a = f->d->cv.a;
//	acasevar_t* const e = a + (v->parent != NO_CASEVAR && a[v->parent].next != NO_CASEVAR ? a[v->parent].next : f->d->cv.l);
//	for (; v < e; ++v) {
//		if (v->type == CV_NORMAL && v->name) {
//			return false;
//		}
//	}
//	return true;
//}

static void multiput(afstat_t* f, aestat_t* e, int index, int* j, int* fail) {
	acasevar_t* v = f->d->cv.a + index;
	switch (v->type) {
	case CV_MATCH: {
		initexp(e, E_LOCAL);
		e->v.g = v->src;
		e->lf = *fail;
		aloK_suffix(f, e, &v->expr, OPR_EQ, f->l->cl);
		aloK_gwt(f, e);
		*fail = e->lf;
		break;
	}
	case CV_UNBOX: { /* unbox */
		int current = f->freelocal;
		if (v->expr.t == E_VOID) { /* unit type: unbox by itself */
			aloK_unbox(f, e, v->nchild);
			e->t = E_ALLOC;
			aloK_nextreg(f, e);
		}
		else {
			aloK_nextreg(f, &v->expr);
			aloK_nextreg(f, e);
			aloK_iABC(f, OP_CALL, false, false, false, v->expr.v.g, 3, 1 + v->nchild);
			f->freelocal -= 1;
		}
		aloE_assert(current + 1 == f->freelocal, "illegal argument count");
		f->freelocal = current + v->nchild;
		if (v->nchild > 0) { /* placce child variables */
			int k = 0;
			int i = index + 1;
			do {
				v = f->d->cv.a + i;
				v->src = current + k++;
				if (v->name) {
					v->dest = (*j)++;
				}
			}
			while ((i = v->next) != NO_CASEVAR);
			aloE_assert(k == f->d->cv.a[index].nchild, "illegal variable count");
			i = index + 1;
			do {
				v = f->d->cv.a + i;
				f->nactvar = f->freelocal;
				multiput(f, e, i, j, fail);
			}
			while ((i = v->next) != NO_CASEVAR);
		}
		break;
	}
	case CV_UNBOXSEQ: /* unbox sequence */
		//TODO
		break;
	}
}

static void adjustcv(afstat_t* f, acasevar_t* const a, int i) {
	acasevar_t* v;
	int j = i;
	do {
		v = a + j;
		if (v->name) {
			if (v->src != v->dest) {
				aloK_iABC(f, OP_MOV, false, false, false, v->dest, v->src, 0);
				v->src = v->dest;
			}
			regloc(f, v->name);
		}
	}
	while ((j = v->next) != NO_CASEVAR);
	j = i;
	do {
		v = a + j;
		if (v->nchild > 0) {
			adjustcv(f, a, j + 1);
		}
	}
	while ((j = v->next) != NO_CASEVAR);
}

static void mergecasevar(afstat_t* f, aestat_t* e, int begin, int i, int* fail) {
	int j = 0;
	do {
		if (f->d->cv.a[j].type != CV_MATCH) {
			f->nactvar = f->freelocal;
			multiput(f, e, j, &i, fail);
		}
	}
	while ((j = f->d->cv.a[j].next) != NO_CASEVAR);

	f->nactvar = begin;
	adjustcv(f, f->d->cv.a, 0);
	f->freelocal = i; /* adjust free local size */
}

static int caseassign(alexer_t* lex, int i) {
	afstat_t* f = lex->f;
	int fail = NO_JUMP;
	int act = f->nactvar;
	acasevar_t* v = f->d->cv.a;
	aestat_t e;
	int narg = 0;

	void putvar() {
		switch (v->type) {
		case CV_MATCH:
			e.lf = fail;
			aloK_suffix(f, &e, &v->expr, OPR_EQ, lex->cl);
			aloK_gwt(f, &e);
			fail = e.lf;
			break;
		default:
			v->src = aloK_nextreg(f, &e);
			if (v->name != NULL) {
				v->dest = i++;
			}
			break;
		}
		v = v->next != NO_CASEVAR ? f->d->cv.a + v->next : NULL;
	}

	do {
		expr(lex, &e);
		narg++;
		if (!checknext(lex, ',')) {
			goto remain;
		}
		if (hasmultiarg(&e)) {
			aloK_singleret(f, &e);
		}
		putvar(); /* move variable */
	}
	while (v);
	aloE_assert(narg == f->d->cv.l, "illegal variable count.");
	do {
		expr(lex, &e);
		aloK_drop(f, &e);
	}
	while (checknext(lex, ','));
	goto end;
	remain:
	if (v->next == NO_CASEVAR) { /* fit last element */
		putvar();
	}
	else {
		aloE_assert(narg != f->d->cv.l, "argument not matched");
		adjust_assign(f, f->d->cv.l, narg, &e);
		do putvar();
		while (v);
	}
	end:
	mergecasevar(lex->f, &e, act, i, &fail);
	return fail;
}

static void partialfun(alexer_t* lex) {
	/* partialfun -> 'case' patterns '->' stats ['case' patterns '->' stats] */
	poll(lex); /* skip first 'case' */
	afstat_t* f = lex->f;
	int fail = NO_JUMP, succ = NO_JUMP;
	aestat_t e1, e2;
	ablock_t b;
	do {
		enterblock(f, &b, false);
		patterns(lex);
		testnext(lex, TK_RARR);
		initexp(&e1, E_VARARG);
		initexp(&e2, E_INTEGER);
		e2.v.g = f->d->cv.l;
		/* check length fitness */
		aloK_prefix(f, &e1, OPR_LEN, lex->cl);
		aloK_suffix(f, &e1, &e2, OPR_EQ, lex->cl);
		e1.lf = fail;
		aloK_gwt(f, &e1);
		fail = e1.lf;
		/* extract variables */
		int i = 0, j = 0;
		acasevar_t* v;
		do {
			v = f->d->cv.a + i;
			initexp(&e1, E_ALLOC);
			e1.v.g = aloK_iABC(f, OP_SELV, false, 2, false, 0, j, 0);
			switch (v->type) {
			case CV_MATCH:
				aloK_suffix(f, &e1, &v->expr, OPR_EQ, lex->cl);
				e1.lf = fail;
				aloK_gwt(f, &e1);
				fail = e1.lf;
				break;
			default:
				v->src = aloK_nextreg(f, &e1);
				if (v->name != NULL) {
					v->dest = j++;
				}
				break;
			}
		}
		while ((i = v->next) != NO_CASEVAR);
		mergecasevar(f, &e1, f->b->nactvar, j, &fail);
		statements(lex);
		if (check(lex, TK_CASE)) {
			succ = aloK_jumpforward(f, succ);
		}
		lex->f->d->cv.l = 0; /* clear buffer */
		leaveblock(f);
		f->freelocal = f->nactvar;
		/* jump when fail to match pattern */
		aloK_putlabel(f, fail);
		fail = NO_JUMP;
	}
	while (checknext(lex, TK_CASE));
	aloK_putlabel(f, succ);
	aloK_return(f, 0, 0);
}

static void funcarg(alexer_t* lex) {
	/* funcarg -> { IDENT [',' IDENT] {',' IDENT '...'} } */
	astring_t* name;
	int n = 0;
	if (check(lex, TK_IDENT)) {
		do {
			name = check_name(lex);
			if (checknext(lex, TK_TDOT)) {
				regsym(lex->f, SYMBOL_VAR, name, 0);
				lex->f->p->fvararg = true;
				break;
			}
			n++;
			regloc(lex->f, name);
		}
		while (checknext(lex, ','));
	}
	lex->f->p->nargs = n;
	aloK_incrstack(lex->f, n);
}

static void istat(alexer_t* lex) {
	if (check(lex, '{')) { /* istat -> '{' stats '}' */
		int line = lex->cl;
		poll(lex);
		if (!statements(lex)) {
			aloK_return(lex->f, 0, 0); /* add return statement if no statement exist */
		}
		testenclose(lex, '{', '}', line);
	}
	else { /* istat -> expr */
		aestat_t e;
		expr(lex, &e);
		if (hasmultiarg(&e)) {
			if (e.t == E_CALL) {
				SET_i(getinsn(lex->f, &e), OP_TCALL);
			}
			else {
				aloK_multiret(lex->f, &e);
				aloK_return(lex->f, aloK_putreg(lex->f, &e), ALO_MULTIRET);
			}
		}
		else {
			aloK_return(lex->f, aloK_putreg(lex->f, &e), 1);
		}
	}
}

static void retstat(alexer_t* lex) {
	/* retstat -> 'return' (varexpr | '!') [';'] */
	int line = lex->pl;
	int first, nret;
	if (isending(lex) || check(lex, ';')) {
		first = nret = 0;
	}
	else {
		afstat_t* f = lex->f;
		aestat_t e;
		first = f->nactvar;
		nret = varexpr(lex, &e);
		if (hasmultiarg(&e)) {
			if (e.t == E_CALL && nret == 1) {
				SET_i(getinsn(f, &e), OP_TCALL);
				return;
			}
			aloK_multiret(f, &e);
			nret = ALO_MULTIRET;
		}
		else {
			if (nret == 1) {
				first = aloK_putreg(f, &e);
			}
			else {
				aloK_nextreg(f, &e);
				aloE_assert(f->freelocal - first == nret, "illegal argument count");
			}
		}
	}
	aloK_return(lex->f, first, nret);
	aloK_fixline(lex->f, line);
	checknext(lex, ';');
}

static int stat(alexer_t*, int);

static int ifstat(alexer_t* lex) {
	/* ifstat -> 'if' '(' (localexpr | expr) ')' stat { 'else' stat } */
	int line = lex->cl;
	testnext(lex, '(');
	aestat_t e;
	ablock_t b;
	enterblock(lex->f, &b, false);
	if (checknext(lex, TK_LOCAL)) { /* localexpr -> 'local' patterns '=' caseassign */
		int n = lex->f->nactvar;
		patterns(lex);
		testnext(lex, '='); /* assignment */
		initexp(&e, E_VOID);
		e.lf = caseassign(lex, n);
	}
	else { /* if statement */
		expr(lex, &e);
		aloK_gwt(lex->f, &e);
	}
	testenclose(lex, '(', ')', line);
	int ret = stat(lex, false); /* THEN block */
	int index = aloK_jumpforward(lex->f, e.lt);
	leaveblock(lex->f);
	aloK_putlabel(lex->f, e.lf);
	if (checknext(lex, TK_ELSE)) {
		ret &= stat(lex, false); /* ELSE block */
	}
	else {
		ret = false;
	}
	aloK_putlabel(lex->f, index);
	return ret;
}

static void whilestat(alexer_t* lex) {
	/* whilestat -> 'while' '(' expr ')' stat { 'else' stat } */
	int line = lex->cl;
	testnext(lex, '(');
	aestat_t e;
	ablock_t b;
	enterblock(lex->f, &b, true);
	expr(lex, &e);
	aloK_gwt(lex->f, &e);
	testenclose(lex, '(', ')', line);
	stat(lex, false); /* THEN block */
	aloK_jumpbackward(lex->f, b.labelc);
	aloK_fixline(lex->f, line); /* fix line to conditional check */
	leaveblock(lex->f);
	aloK_putlabel(lex->f, e.lf);
	if (checknext(lex, TK_ELSE)) {
		stat(lex, false); /* ELSE block */
	}
	aloK_putlabel(lex->f, b.labelb);
}

static void dowhilestat(alexer_t* lex) {
	/* dowhilestat -> 'do' stat 'while' '(' expr ')' { 'else' stat }  */
	aestat_t e;
	ablock_t b;
	enterblock(lex->f, &b, true);
	stat(lex, false); /* DO block */
	int line = lex->cl;
	testnext(lex, TK_WHILE);
	testnext(lex, '(');
	expr(lex, &e);
	testenclose(lex, '(', ')', line);
	aloK_gwf(lex->f, &e);
	aloK_marklabel(lex->f, b.labelc, e.lt);
	leaveblock(lex->f);
	if (checknext(lex, TK_ELSE)) {
		stat(lex, false); /* ELSE block */
	}
	aloK_putlabel(lex->f, b.labelb);
}

static void assignment(alexer_t* lex, alhsa_t* a, int nvar) {
	aestat_t e;
	if (checknext(lex, ',')) {
		checklimit(lex, nvar + lex->T->nccall, ALO_MAXCCALL, "C call levels over limit");
		/* assignment -> ',' suffixexpr assignment */
		alhsa_t a1;
		a1.p = a;
		suffixexpr(lex, &a1.e);
		if (a1.e.t != E_INDEXED) {
			check_conflict(lex->f, a, &a1.e);
		}
		assignment(lex, &a1, nvar + 1);
	}
	else {
		/* assignment -> '=' varexpr */
		testnext(lex, '=');
		int narg = varexpr(lex, &e);
		if (narg != nvar) {
			adjust_assign(lex->f, nvar, narg, &e);
		}
		else {
			aloK_singleret(lex->f, &e); /* close last expression */
			aloK_assign(lex->f, &a->e, &e);
			return;
		}
	}
	initexp(&e, E_FIXED);
	e.v.g = lex->f->freelocal - 1;
	aloK_assign(lex->f, &a->e, &e);
}

static void normstat(alexer_t* lex) {
	/* normstat -> apply | assignment | remexpr */
	alhsa_t a;
	if (check(lex, '$')) {
		monexpr(lex, &a.e);
		aloK_drop(lex->f, &a.e);
		return;
	}
	int oldfreeloc = lex->f->freelocal;
	expr(lex, &a.e);
	if (check(lex, ',') || check(lex, '=')) { /* normstat -> assignment */
		a.p = NULL;
		assignment(lex, &a, 1);
		lex->f->freelocal = oldfreeloc; /* clear extra value */
	}
	else { /* normstat -> apply */
		if (a.e.t != E_CALL) {
			lerror(lex, "call expression expected.");
		}
		SET_C(getinsn(lex->f, &a.e), 1);
	}
}

static void defstat(alexer_t* lex) {
	/* defstat -> 'def' IDENT { '(' funcarg ')' } '->' istat */
	afstat_t* f = lex->f;
	int line = lex->cl;
	astring_t* name = check_name(lex);
	afstat_t f2;
	ablock_t b;
	enterfunc(&f2, f, &b);
	f2.p->name = name; /* bind function name */
	if (check(lex, '(')) {
		int line = lex->cl;
		poll(lex);
		funcarg(lex);
		testenclose(lex, '(', ')', line);
	}
	testnext(lex, TK_RARR);
	istat(lex);
	leavefunc(&f2);
	aestat_t e1, e2;
	aloK_loadproto(f, &e1);
	aloK_fixline(lex->f, line); /* fix line to function */
	aloK_fromreg(f, &e2, name);
	aloK_assign(f, &e2, &e1);
	aloK_fixline(lex->f, line); /* fix line to function */
	aloK_drop(f, &e2);
}

static void localstat(alexer_t* lex) {
	afstat_t* f = lex->f;
	int line = lex->cl;
	astring_t* name = check_name(lex);
	if (check(lex, '(') || check(lex, TK_RARR)) {
		/* localstat -> 'local' IDENT { '(' funcarg ')' } '->' istat */
		int index = regloc(f, name); /* add local name */
		afstat_t f2;
		ablock_t b;
		enterfunc(&f2, f, &b);
		f2.p->name = name; /* bind function name */
		if (check(lex, '(')) {
			int line = lex->cl;
			poll(lex);
			funcarg(lex);
			testenclose(lex, '(', ')', line);
		}
		testnext(lex, TK_RARR);
		istat(lex);
		leavefunc(&f2);
		aloE_assert(f->freelocal == index, "illegal local variable index");
		aestat_t e;
		aloK_loadproto(f, &e);
		aloK_fixline(lex->f, line); /* fix line to conditional check */
		aloK_anyR(f, &e);
	}
	else {
		/* localstat -> 'local' IDENT [ ',' IDENT ] { '=' varexpr } */
		int index = pushcv(f);
		f->d->cv.a[index].name = name;
		while (checknext(lex, ',')) {
			index = pushcv(f);
			f->d->cv.a[index].name = check_name(lex);
		}
		int narg;
		aestat_t e;
		if (checknext(lex, '=')) {
			narg = varexpr(lex, &e);
		}
		else {
			narg = 0;
			initexp(&e, E_VOID);
		}
		int nvar = f->d->cv.l;
		adjust_assign(f, nvar, narg, &e);
		for (int i = 0; i < nvar; ++i) {
			regloc(f, f->d->cv.a[i].name); /* register local variables */
		}
		f->d->cv.l = 0;
	}
}

static int statements(alexer_t*);

static int stat(alexer_t* lex, int assign) {
	switch (lex->ct.t) {
	case ';': { /* stat -> ';' */
		poll(lex); /* skip ';' */
		return false;
	}
	case TK_IF: { /* stat -> ifstat */
		poll(lex);
		return ifstat(lex);
	}
	case TK_WHILE: { /* stat -> whilestat */
		poll(lex);
		whilestat(lex);
		return false;
	}
	case TK_DO: { /* stat -> dowhilestat */
		poll(lex);
		dowhilestat(lex);
		return false;
	}
	case TK_LOCAL: { /* stat -> localstat */
		if (!assign) {
			lerror(lex, "cannot put local statement here");
		}
		poll(lex);
		localstat(lex);
		return false;
	}
	case TK_DEF: { /* stat -> defstat */
		poll(lex);
		defstat(lex);
		return false;
	}
	case TK_RETURN: { /* stat -> retstat */
		poll(lex); /* skip 'return' */
		retstat(lex);
		return true;
	}
	case '{': { /* stat -> '{' stats '}' */
		int line = lex->cl;
		poll(lex);
		int flag = statements(lex);
		testenclose(lex, '{', '}', line);
		return flag;
	}
	default: { /* stat -> normstat */
		normstat(lex);
		return false;
	}
	}
}

static int statements(alexer_t* lex) {
	/* statements -> { statement [';'] } */
	while (!isending(lex)) {
		if (stat(lex, true)) { /* if scanned ending statement */
			semis(lex);
			if (!isending(lex)) {
				lerror(lex, "statement should be placed after return statement.");
			}
			return true;
		}
	}
	return false;
}

static void registerproto(astate T, aproto_t* p) {
	for (size_t i = 0; i < p->nchild; ++i) {
		registerproto(T, p->children[i]);
	}
	aloG_register(T, p, ALO_TPROTO);
}

int aloP_parse(astate T, astr src, aibuf_t* in, aproto_t** out) {
	/* open lexer */
	alexer_t lex;
	aloX_open(T, &lex, src, in);

	apdata_t data = { };

	void pparse(astate T, __attribute__((unused)) void* context) {
		data.p = aloF_newp(T);
		ablock_t b;
		afstat_t f = { p: data.p, e: NULL, d: &data, cjump: NO_JUMP };
		lex.f = &f;
		f.l = &lex;
		poll(&lex);
		initproto(T, &f);
		enterblock(&f, &b, false);
		if (!statements(&lex)) {
			aloK_return(&f, 0, 0);
		}
		leaveblock(&f);
		closeproto(T, &f);
	}

	int status = aloD_prun(T, pparse, NULL);
	*out = !status ? data.p : NULL;

	aloM_dela(T, data.ss.a, data.ss.c);
	aloM_dela(T, data.jp.a, data.jp.c);
	aloM_dela(T, data.lb.a, data.lb.c);
	aloM_dela(T, data.cv.a, data.cv.c);
	aloX_close(&lex);
	if (status) {
		aloZ_delete(T, data.p);
	}
	else {
		registerproto(T, data.p);
	}

	return status;
}

