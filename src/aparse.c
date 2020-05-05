/*
 * aparse.c
 *
 *  Created on: 2019年8月21日
 *      Author: ueyudiud
 */

#define APARSE_C_
#define ALO_CORE

#include "aop.h"
#include "afun.h"
#include "agc.h"
#include "avm.h"
#include "ado.h"
#include "alex.h"
#include "aparse.h"
#include "acode.h"
#include "aload.h"

#include <string.h>

#define check(l,id) ((l)->ct.t == (id))
#define test(l,id) { if (!check(l, id)) error_expected(l, id); }
#define testenclose(l,f,t,ln) { if (!check(l, t)) error_enclose(l, f, t, ln); else poll(l); }
#define checknext(l,id) (check(l, id) ? (poll(l), true) : false)
#define testnext(l,id) { test(l, id); poll(l); }
#define poll aloX_poll
#define forward aloX_forward
#define lerror aloX_error
#define literal(l,s) aloX_getstr(l, ""s, (sizeof(s) / sizeof(char)) - 1)
#define checklimit(l,n,lim,msg) { if ((n) > (lim)) lerror(l, msg); }

struct alo_Block {
	ablock_t* prev; /* previous block */
	astr lname; /* label name */
	size_t flabel; /* index of first label in this block */
	size_t fjump; /* index of first jump in this block */
	size_t fsymbol; /* first symbol index */
	int lcon; /* index of continue */
	int lout; /* index of jump from break */
	uint16_t nactvar; /* last index of local variable at beginning of this block */
	uint16_t nallvar; /* last index of local or temporary variable at beginning of this block */
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
		lerrorf(lex, "'%c' expected (to close '%c' at line %d), got %s", l, r, line, aloX_token2str(lex, &lex->ct));
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

static astring_t* testident(alexer_t* lex) {
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
	case TK_BDOT: return OPR_CAT;
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
		.name = name,
		.start = f->ncode,
		.index = reg
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
	f->nconst = 2;
}

static void closelineinfo(astate T, afstat_t* f) {
	aloM_adjb(T, f->p->lineinfo, f->p->nlineinfo, f->nline + 1); /* close line info array */
	alineinfo_t* info = &f->p->lineinfo[f->nline];
	info->begin = f->ncode;
	info->line = f->l->pl; /* set last line defined of prototype as last line info */
}

static void closeproto(astate T, afstat_t* f) {
	aproto_t* p = f->p;
	p->lineldef = f->l->pl;
	aloM_adjb(T, p->code, p->ncode, f->ncode);
	aloM_adjb(T, p->consts, p->nconst, f->nconst);
	aloM_adjb(T, p->captures, p->ncap, f->ncap);
	aloM_adjb(T, p->locvars, p->nlocvar, f->nlocvar);
	aloM_adjb(T, p->children, p->nchild, f->nchild);
	closelineinfo(T, f);
}

static void enterblock(afstat_t* f, ablock_t* b, int isloop, astr label) {
	*b = (ablock_t) {
		.lname = label,
		.loop = isloop,
		.nactvar = f->nactvar,
		.nallvar = f->freelocal,
		.fsymbol = f->d->ss.l,
		.flabel = f->d->lb.l,
		.fjump = f->d->lb.l,
		.lcon = isloop ? aloK_newlabel(f) : NO_JUMP,
		.lout = NO_JUMP,
		.incap = false,
		.prev = f->b
	};
	f->b = b;
	f->nactvar = f->freelocal; /* adjust top of active variable index */
}

static void leaveblock(afstat_t* f) {
	ablock_t* b = f->b;
	asymbol* symbols = f->d->ss.a + f->firstlocal;
	for (size_t i = b->nallvar; i < f->nactvar; ++i) {
		if (symbols[i].type == SYMBOL_LOC) {
			f->p->locvars[symbols[i].index].end = f->ncode; /* settle end pc */
		}
	}
	f->nactvar = b->nactvar;
	f->freelocal = b->nallvar;
	f->d->ss.l = b->fsymbol;
	int njmp = b->fjump;
	alabel* labels = f->d->jp.a;
	for (size_t i = b->fjump; i < f->d->jp.l; ++i) {
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
		.l = parent->l,
		.p = newproto(parent),
		.e = parent,
		.b = b,
		.d = parent->d,
		.cjump = NO_JUMP,
		.firstlocal = parent->d->ss.l,
		.layer = parent->layer + 1
	};
	initproto(f->l->T, f);
	parent->l->f = f;
	enterblock(f, b, false, NULL);
}

static void leavefunc(afstat_t* f) {
	leaveblock(f);
	astate T = f->l->T;
	closeproto(T, f);
	f->l->f = f->e; /* move to enclose function */
	aloG_check(T);
}

#define hasmultiarg(e) ((e)->t == E_VARARG || (e)->t == E_UNBOX || (e)->t == E_CALL)

static void primaryexpr(alexer_t*, aestat_t*);

static int funargs(alexer_t*, aestat_t*);

static void partialfun(alexer_t*);

static void istat(alexer_t*);

static int stats(alexer_t*);

static int stat(alexer_t*, int);

static void rootstats(alexer_t* lex) {
	if (!stats(lex)) {
		aloK_return(lex->f, 0, 0); /* add return statement if no statement exist */
	}
}

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

static void memberof(alexer_t* lex, aestat_t* e, astring_t* s) {
	aestat_t e2;
	initexp(&e2, E_STRING);
	e2.v.s = s;
	aloK_member(lex->f, e, &e2);
}

static void colargs(alexer_t* lex, aestat_t* e) {
	afstat_t* f = lex->f;
	int line = lex->cl;
	poll(lex);
	if (checknext(lex, ':')) { /* colargs -> ':' */
		aloK_newcol(f, e, OP_NEWM, 0);
	}
	else {
		aloK_newcol(f, e, OP_NEWL, 0);
		if (!check(lex, ']')) {
			int narg = 1;
			size_t i = e->v.g;
			aloK_anyR(f, e);
			size_t t = e->v.g;
			aestat_t e2, e3;
			expr(lex, &e2);
			if (checknext(lex, ':')) { /* colargs -> expr ':' expr { [','|';'] expr ':' expr } */
				SET_i(f->p->code[i], OP_NEWM); /* set to table */
				aloK_anyRK(f, &e2);
				expr(lex, &e3);
				aloK_rawset(f, t, &e2, &e3);
				while (checknext(lex, ',') || checknext(lex, ';') || !isending(lex)) {
					expr(lex, &e2);
					aloK_anyRK(f, &e2);
					testnext(lex, ':');
					expr(lex, &e3);
					aloK_rawset(f, t, &e2, &e3);
					narg++;
				}
			}
			else { /* colargs -> expr { [','|';'] expr } */
				initexp(&e3, E_INTEGER);
				e3.v.i = narg - 1;
				aloK_rawset(f, t, &e3, &e2);
				while (checknext(lex, ',') || checknext(lex, ';') || !isending(lex)) {
					initexp(&e2, E_INTEGER);
					e2.v.i = narg++;
					expr(lex, &e3);
					aloK_rawset(f, t, &e2, &e3);
				}
			}
			SET_Bx(f->p->code[i], narg); /* set collection size */
		}
	}
	testenclose(lex, '[', ']', line);
}

static void primaryexpr(alexer_t* lex, aestat_t* e) {
	afstat_t* f = lex->f;
	switch (lex->ct.t) {
	case TK_IDENT: { /* primaryexpr -> IDENT */
		aloK_field(f, e, testident(lex));
		break;
	}
	case '@': { /* primaryexpr -> '@' [IDENT] */
		aloK_fromreg(f, e, literal(lex, "@"));
		if (poll(lex) == TK_IDENT) {
			memberof(lex, e, testident(lex));
		}
		break;
	}
	case TK_THIS: { /* primaryexpr -> 'this' */
		aloK_fromreg(f, e, lex->T->g->stagnames[TM_THIS]);
		break;
	}
	case TK_NEW: { /* primaryexpr -> 'new' ['@'] IDENT funargs */
		int line = lex->cl;
		int prevfree = f->freelocal;
		int prevact = f->nactvar;
		poll(lex); /* skip 'new' token */
		f->nactvar = prevfree + 1;
		f->freelocal += 1;
		if (checknext(lex, '@')) {
			aloK_fromreg(f, e, literal(lex, "@"));
			memberof(lex, e, testident(lex));
		}
		else {
			aloK_field(f, e, testident(lex));
		}
		aloK_nextreg(f, e);
		aestat_t e2 = *e;
		e2.t = E_LOCAL;
		e2.v.g -= 1;
		aloK_iABC(f, OP_NEW, 0, 0, 0, e->v.g - 1, e->v.g, 0);
		aloK_drop(f, e);
		memberof(lex, e, lex->T->g->stagnames[TM_NEW]);
		f->freelocal += 1;
		aloK_nextreg(f, e);
		aloK_nextreg(f, &e2);
		int n = funargs(lex, e);
		f->nactvar = prevact;
		f->freelocal = prevfree + 1; /* remove all arguments and only remain one result */
		aloK_iABC(f, OP_CALL, false, false, false, prevfree + 1, n != ALO_MULTIRET ? n + 2 : 0, 1);
		e->t = E_LOCAL;
		e->v.g = prevfree;
		aloK_fixline(f, line);
		break;
	}
	case '(': { /* primaryexpr -> '(' [varexpr] ')' */
		int line = lex->cl;
		poll(lex);
		if (checknext(lex, ')')) { /* enclosed directly? */
			initexp(e, E_VOID);
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
	case '[': { /* primaryexpr -> '[' colargs ']' */
		colargs(lex, e);
		break;
	}
	case TK_NIL: { /* primaryexpr -> NIL */
		initexp(e, E_NIL);
		poll(lex);
		break;
	}
	case TK_FALSE: { /* primaryexpr -> FALSE */
		initexp(e, E_FALSE);
		poll(lex);
		break;
	}
	case TK_TRUE: { /* primaryexpr -> TRUE */
		initexp(e, E_TRUE);
		poll(lex);
		break;
	}
	case TK_INTEGER: { /* primaryexpr -> INTEGER */
		initexp(e, E_INTEGER);
		e->v.i = lex->ct.d.i;
		poll(lex);
		break;
	}
	case TK_FLOAT: { /* primaryexpr -> FLOAT */
		initexp(e, E_FLOAT);
		e->v.f = lex->ct.d.f;
		poll(lex);
		break;
	}
	case TK_STRING: { /* primaryexpr -> STRING */
		initexp(e, E_STRING);
		e->v.s = lex->ct.d.s;
		poll(lex);
		break;
	}
	default: {
		lerrorf(lex, "illegal primary expression, got: %s", aloX_token2str(lex, &lex->ct));
	}
	}
}

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
				rootstats(lex);
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
		lerror(lex, "illegal function arguments");
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
	/* suffixexpr -> primaryexpr {suffix} */
	primaryexpr(lex, e);
	aestat_t e2;
	afstat_t* f = lex->f;
	int n;
	int line;
	int lbnil = NO_JUMP;
	while (true) {
		switch (lex->ct.t) {
		case '.': { /* suffix -> '.' IDENT */
			poll(lex); /* skip '.' */
			initexp(&e2, E_STRING);
			e2.v.s = testident(lex);
			aloK_member(f, e, &e2);
			break;
		}
		case '[': { /* suffix -> '[' (expr | '?' expr [':' expr]) ']' */
			int line = lex->cl;
			do {
				poll(lex); /* skip '[' or ',' */
				if (checknext(lex, '?')) {
					int line2 = lex->cl;
					expr(lex, &e2);
					aloK_member(f, e, &e2);
					aloK_gwt(f, e);
					aloK_fixline(f, line2);
					aloK_reuse(f, e);
					int label1 = e->lf;
					e->lf = NO_JUMP;
					int label2 = aloK_jumpforward(f, NO_JUMP);
					aloK_putlabel(f, label1);
					if (checknext(lex, ':')) {
						expr(lex, &e2);
						aloK_move(f, &e2, e->v.g);
					}
					else {
						aloK_loadnil(f, e->v.g, 1);
					}
					aloK_putlabel(f, label2);
				}
				else {
					expr(lex, &e2);
					aloK_member(f, e, &e2);
				}
			}
			while (check(lex, ','));
			testenclose(lex, '[', ']', line);
			break;
		}
		case TK_QCOL: { /* suffix -> '?.' IDENT */
			poll(lex); /* skip '?.' */
			aloK_gwt(f, e);
			aloK_reuse(f, e);
			lbnil = e->lf;
			e->lf = NO_JUMP;
			initexp(&e2, E_STRING);
			e2.v.s = testident(lex);
			aloK_member(f, e, &e2);
			break;
		}
		case TK_RARR: { /* suffix -> '->' IDENT [funargs] */
			line = lex->cl;
			poll(lex); /* skip '->' */
			aloK_self(f, e, testident(lex));
			switch (lex->ct.t) {
			case '(': case '{': case TK_STRING:
				n = funargs(lex, &e2);
				if (n != ALO_MULTIRET) {
					n += 1;
				}
				break;
			default:
				n = 1;
				break;
			}
			goto call;
		}
		case '(': case '{': case TK_STRING: case '\\': { /* suffix -> '->' IDENT [funargs] */
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
		case TK_TDOT: { /* suffix -> '...' */
			poll(lex);
			aloK_unbox(f, e, ALO_MULTIRET);
			break;
		}
		default:
			if (lbnil != NO_JUMP) {
				aloK_nextreg(f, e);
				int lbend = aloK_jumpforward(f, NO_JUMP);
				aloK_putlabel(f, lbnil);
				aloK_loadnil(f, e->v.g, 1);
				aloK_putlabel(f, lbend);
			}
			return;
		}
	}
}


static void funcarg(alexer_t*);

static void lambdaexpr(alexer_t* lex, aestat_t* e) {
	afstat_t f;
	ablock_t b;
	enterfunc(&f, lex->f, &b);
	if (checknext(lex, '{')) {
		/* lambdaexpr -> '\\' '{' stats '}' */
		int line = lex->pl;
		rootstats(lex);
		testenclose(lex, '{', '}', line);
	}
	else {
		/* lambdaexpr -> '\\' funcarg '->' istat */
		funcarg(lex);
		testnext(lex, TK_RARR);
		istat(lex);
	}
	leavefunc(&f);
	initexp(e, E_ALLOC);
	e->v.g = aloK_iABx(lex->f, OP_LDP, false, true, 0, lex->f->nchild - 1);
}

static void monexpr(alexer_t* lex, aestat_t* e) {
	/* monexpr -> unrop monexpr | suffixexpr | lambdaexpr */
	int op = getunrop(lex->ct.t);
	if (op != OPR_NONE) {
		int line = lex->cl;
		poll(lex); /* skip operator */
		monexpr(lex, e);
		aloK_prefix(lex->f, e, op, line);
	}
	else if (checknext(lex, '\\')) {
		lambdaexpr(lex, e);
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

static void triexpr(alexer_t* lex, aestat_t* e) {
	/* triexpr -> subexpr [ '?' [expr] ':' expr ]*/
	subexpr(lex, e, 0);
	if (checknext(lex, '?')) {
		int reg, label1;
		afstat_t* f = lex->f;
		aloK_gwt(f, e);
		label1 = e->lf;
		if (checknext(lex, ':')) {
			if (e->t == E_JMP) {
				lerror(lex, "can not take comparison in '?:' expression.");
			}
			e->lf = NO_JUMP;
			reg = aloK_reuse(f, e);
			aloK_drop(f, e);
		}
		else {
			expr(lex, e);
			reg = aloK_nextreg(f, e);
			aloK_drop(f, e);
			testnext(lex, ':');
		}
		int label2 = aloK_jumpforward(f, NO_JUMP);
		aloK_putlabel(f, label1);
		expr(lex, e);
		aloK_nextreg(f, e);
		aloE_assert(reg == e->v.g, "register not matched.");
		aloE_void(reg);
		aloK_putlabel(f, label2);
	}
}

static void patmatch(alexer_t*, aestat_t*);

static void expr(alexer_t* lex, aestat_t* e) {
	/* expr -> triexpr ['::' patmatch] */
	triexpr(lex, e);
	if (checknext(lex, TK_BCOL)) {
		patmatch(lex, e);
	}
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

struct context_pattern {
	int parent;
	int previous;
};

/* get next case variable */
acasevar_t* nextcv(alexer_t* lex, int type, astring_t* name, struct context_pattern* ctx) {
	afstat_t* f = lex->f;
	*(ctx->parent != NO_CASEVAR ? &f->d->cv.a[ctx->parent].nchild : &f->d->cv.nchild) += 1;
	if (ctx->previous != NO_CASEVAR) {
		f->d->cv.a[ctx->previous].next = f->d->cv.l;
	}
	int index = pushcv(f);
	acasevar_t* var = f->d->cv.a + index;
	var->type = type;
	var->src = var->dest = -1;
	var->name = name;
	var->nchild = 0;
	var->parent = ctx->parent;
	var->prev = ctx->previous;
	var->next = NO_CASEVAR;
	ctx->previous = index;
	return var;
}

/* enter case variable */
void entercv(alexer_t* lex, struct context_pattern* ctx) {
	afstat_t* f = lex->f;
	ctx->previous = NO_CASEVAR;
	ctx->parent = f->d->cv.l - 1;
}

/* leave case variable */
void leavecv(alexer_t* lex, struct context_pattern* ctx) {
	acasevar_t* var = lex->f->d->cv.a + ctx->parent;
	switch (var->type) {
	case CV_UNBOX:
		testenclose(lex, '(', ')', var->line);
		break;
	case CV_UNBOXSEQ:
		testenclose(lex, '[', ']', var->line);
		break;
	default:
		lerror(lex, "unexpected token");
		break;
	}
	ctx->parent = var->parent;
	ctx->previous = var->prev;
}

static void patterns(alexer_t* lex) {
	/* patterns -> pattern {',' patterns} */
	struct context_pattern ctx = { NO_CASEVAR, NO_CASEVAR };
	astring_t *name, *handle;

	do {
		begin:
		switch (lex->ct.t) {
		case '(': { /* pattern -> '(' patterns ')' */
			name = handle = NULL;
			unbox: {
				poll(lex);
				acasevar_t* var = nextcv(lex, CV_UNBOX, name, &ctx);
				if (handle) {
					aloK_field(lex->f, &var->expr, handle);
				}
				else { /* no unboxer, use identical unboxer */
					initexp(&var->expr, E_VOID);
				}
				entercv(lex, &ctx);
			}
			goto begin; /* enter into block, skip ',' check */
		}
		case '[': { /* pattern -> '[' patterns ']' */
			name = handle = NULL;
			unboxseq: {
				poll(lex);
				acasevar_t* var = nextcv(lex, CV_UNBOX, name, &ctx);
				if (handle) {
					aloK_field(lex->f, &var->expr, handle);
				}
				else { /* no unboxer, use identical unboxer */
					initexp(&var->expr, E_VOID);
				}
				entercv(lex, &ctx);
			}
			goto begin; /* enter into block, skip ',' check */
		}
		case TK_NIL: case TK_TRUE: case TK_FALSE: case TK_INTEGER: case TK_FLOAT: { /* pattern -> LITERAL */
			acasevar_t* var = nextcv(lex, CV_MATCH, NULL, &ctx);
			primaryexpr(lex, &var->expr); /* scan match value */
			break;
		}
		case TK_IDENT: {
			name = testident(lex);
			switch (lex->ct.t) {
			case '@':
				handle = poll(lex) == TK_IDENT ? testident(lex) : NULL;
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
				nextcv(lex, CV_NORMAL, name, &ctx);
				break;
			}
			break;
		}
		}
		label:
		if (ctx.parent != NO_CASEVAR) { /* check leave block */
			switch (lex->ct.t) {
			case ')': case ']':
				leavecv(lex, &ctx);
				goto label;
			}
		}
	}
	while (checknext(lex, ','));
}

static void multiput(afstat_t* f, aestat_t* e, int index, int* j, int* fail) {
	acasevar_t* v = f->d->cv.a + index;
	switch (v->type) {
	case CV_MATCH: {
		initexp(e, E_LOCAL);
		e->v.g = v->src;
		aloK_suffix(f, e, &v->expr, OPR_EQ, f->l->cl);
		e->lf = *fail;
		aloK_gwt(f, e);
		e->t = E_TRUE;
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
			size_t k = 0;
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
	case CV_UNBOXSEQ: /* unbox sequence TODO */
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

struct context_caseassign {
	acasevar_t* v;
	aestat_t e;
	int fail;
};

static void putvar(afstat_t* f, int* i, struct context_caseassign* ctx) {
	switch (ctx->v->type) {
	case CV_MATCH:
		ctx->e.lf = ctx->fail;
		aloK_suffix(f, &ctx->e, &ctx->v->expr, OPR_EQ, f->l->cl);
		aloK_gwt(f, &ctx->e);
		ctx->e.t = E_TRUE;
		ctx->fail = ctx->e.lf;
		break;
	default:
		ctx->v->src = aloK_nextreg(f, &ctx->e);
		if (ctx->v->name != NULL) {
			ctx->v->dest = (*i)++;
		}
		break;
	}
	ctx->v = ctx->v->next != NO_CASEVAR ? f->d->cv.a + ctx->v->next : NULL;
}

static int caseassign(alexer_t* lex, int i) {
	afstat_t* f = lex->f;
	struct context_caseassign ctx;
	ctx.fail = NO_JUMP;
	ctx.v = f->d->cv.a;
	int act = f->nactvar;
	size_t narg = 0;

	do {
		expr(lex, &ctx.e);
		narg++;
		if (!checknext(lex, ',')) {
			goto remain;
		}
		if (hasmultiarg(&ctx.e)) {
			aloK_singleret(f, &ctx.e);
		}
		putvar(f, &i, &ctx); /* move variable */
	}
	while (ctx.v);
	aloE_assert(narg == f->d->cv.l, "illegal variable count.");
	do {
		expr(lex, &ctx.e);
		aloK_drop(f, &ctx.e);
	}
	while (checknext(lex, ','));
	goto end;
	remain:
	if (ctx.v->next == NO_CASEVAR) { /* fit last element */
		putvar(f, &i, &ctx);
	}
	else {
		aloE_assert(narg != f->d->cv.l, "argument not matched");
		adjust_assign(f, f->d->cv.l, narg, &ctx.e);
		do putvar(f, &i, &ctx);
		while (ctx.v);
	}
	end:
	mergecasevar(lex->f, &ctx.e, act, i, &ctx.fail);
	return ctx.fail;
}

static void patmatch(alexer_t* lex, aestat_t* e) {
	aloE_void(e);
//	/* patmatch -> 'case' patterns '->' stats ['case' patterns '->' stats] */
//	int line = lex->cl;
//	testnext(lex, '{');
//	afstat_t* f = lex->f;
//	int fail = NO_JUMP, succ = NO_JUMP;
//	aestat_t e1, e2;
//	ablock_t b;
//	if (hasmultiarg(e)) {
//		aloK_boxt(f, e, 1);
//
//	}
//	else {
//		aloK_nextreg(f, e);
//		aestat_t e0 = *e;
//		while (checknext(lex, TK_CASE)) {
//			enterblock(f, &b, false, NULL);
//			patterns(lex);
//			testnext(lex, TK_RARR);
//
//			/* extract variables */
//			int i = 0, j = 0;
//			acasevar_t* v = f->d->cv.a;
//			if (v->next == NO_CASEVAR) { /* only matches one pattern */
//				*e = e0; /* reset data */
//				aloK_nextreg(f, e);
//				if (v->type == CV_MATCH) {
//					aloK_suffix(f, &e1, &v->expr, OPR_EQ, lex->cl);
//					e1.lf = fail;
//					aloK_gwt(f, &e1);
//					fail = e1.lf;
//				}
//				else {
//					v->src = aloK_nextreg(f, &e1);
//					if (v->name != NULL) {
//						v->dest = j++;
//					}
//				}
//				mergecasevar(f, &e1, f->b->nactvar, j, &fail);
//
//				if (check(lex, TK_CASE)) {
//					succ = aloK_jumpforward(f, succ);
//				}
//			}
//			else { /* failed to match pattern directly */
//				fail = aloK_jumpforward(f, fail);
//			}
//		}
//	}
	lerror(lex, "pattern matching not supportedd yet."); /* TODO */
}

static void partialfun(alexer_t* lex) {
	/* partialfun -> 'case' patterns '->' stats ['case' patterns '->' stats] */
	poll(lex); /* skip first 'case' */
	afstat_t* f = lex->f;
	int fail = NO_JUMP, succ = NO_JUMP;
	aestat_t e1, e2;
	ablock_t b;
	do {
		enterblock(f, &b, false, NULL);
		patterns(lex);
		testnext(lex, TK_RARR);
		initexp(&e1, E_VARARG);
		initexp(&e2, E_INTEGER);
		e2.v.g = f->d->cv.nchild;
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
			if (v->type == CV_MATCH) {
				aloK_suffix(f, &e1, &v->expr, OPR_EQ, lex->cl);
				e1.lf = fail;
				aloK_gwt(f, &e1);
				fail = e1.lf;
			}
			else {
				v->src = aloK_nextreg(f, &e1);
				if (v->name != NULL) {
					v->dest = j++;
				}
			}
		}
		while ((i = v->next) != NO_CASEVAR);
		mergecasevar(f, &e1, f->b->nactvar, j, &fail);
		stats(lex);
		if (check(lex, TK_CASE)) {
			succ = aloK_jumpforward(f, succ);
		}
		f->d->cv.l = 0; /* clear buffer */
		leaveblock(f);
		aloE_xassert(f->freelocal == f->nactvar);
		/* jump when fail to match pattern */
		aloK_putlabel(f, fail);
		fail = NO_JUMP;
	}
	while (checknext(lex, TK_CASE));
	aloK_putlabel(f, succ);
	aloK_return(f, 0, 0);
	f->p->fvararg = true;
}

static void funcarg(alexer_t* lex) {
	/* funcarg -> { IDENT [',' IDENT] {',' IDENT '...'} } */
	astring_t* name;
	int n = 0;
	if (check(lex, TK_IDENT)) {
		do {
			name = testident(lex);
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

static void istat(alexer_t* lex) {
	if (check(lex, '{')) { /* istat -> '{' stats '}' */
		int line = lex->cl;
		poll(lex);
		rootstats(lex);
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
				int reg = aloK_putreg(lex->f, &e);
				aloK_return(lex->f, reg, ALO_MULTIRET);
			}
		}
		else {
			int reg = aloK_putreg(lex->f, &e);
			aloK_return(lex->f, reg, 1);
		}
	}
}

static int ifstat(alexer_t* lex) {
	/* ifstat -> 'if' '(' (localexpr | expr) ')' stat { 'else' stat } */
	int line = lex->cl;
	testnext(lex, '(');
	aestat_t e;
	ablock_t b;
	enterblock(lex->f, &b, false, NULL);
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
	if (checknext(lex, TK_ELSE)) {
		if (!ret) {
			b.lout = aloK_jumpforward(lex->f, b.lout);
		}
		aloK_putlabel(lex->f, e.lf);
		ret &= stat(lex, false); /* ELSE block */
	}
	else {
		aloK_putlabel(lex->f, e.lf);
		ret = false;
	}
	leaveblock(lex->f);
	aloK_putlabel(lex->f, b.lout);
	return ret;
}

static void whilestat(alexer_t* lex) {
	/* whilestat -> 'while' [label] '(' expr ')' stat { 'else' stat } */
	astr lname = NULL;
	int line = lex->cl;
	if (checknext(lex, '[')) {
		lname = testident(lex)->array;
		testenclose(lex, '[', ']', line);
		line = lex->cl;
	}
	testnext(lex, '(');
	afstat_t* f = lex->f;
	aestat_t e;
	ablock_t b;
	enterblock(f, &b, true, lname);
	expr(lex, &e);
	aloK_gwt(f, &e);
	testenclose(lex, '(', ')', line);
	stat(lex, false); /* THEN block */
	aloK_jumpbackward(f, b.lcon);
	aloK_fixline(f, line); /* fix line to conditional check */
	leaveblock(f);
	aloK_putlabel(f, e.lf);
	if (checknext(lex, TK_ELSE)) {
		stat(lex, false); /* ELSE block */
	}
	aloK_putlabel(lex->f, b.lout);
}

static void dowhilestat(alexer_t* lex) {
	/* dowhilestat -> 'do' [label] stat 'while' '(' expr ')' { 'else' stat }  */
	astr lname = NULL;
	int line = lex->cl;
	if (checknext(lex, '[')) {
		lname = testident(lex)->array;
		testenclose(lex, '[', ']', line);
	}
	aestat_t e;
	ablock_t b;
	enterblock(lex->f, &b, true, lname);
	stat(lex, false); /* DO block */
	line = lex->cl;
	testnext(lex, TK_WHILE);
	testnext(lex, '(');
	expr(lex, &e);
	testenclose(lex, '(', ')', line);
	aloK_gwf(lex->f, &e);
	aloK_marklabel(lex->f, b.lcon, e.lt);
	leaveblock(lex->f);
	if (checknext(lex, TK_ELSE)) {
		stat(lex, false); /* ELSE block */
	}
	aloK_putlabel(lex->f, b.lout);
}

static void jumpstat(alexer_t* lex, int type) {
	/* breakstat -> 'break' [label] */
	/* continuestat -> 'continue' [label] */
	ablock_t* block;
	if (checknext(lex, '[')) {
		int line = lex->cl;
		astr lname = testident(lex)->array;
		testenclose(lex, '[', ']', line);
		block = lex->f->b;
		while (block) {
			if (block->lname == lname)
				goto find;
			block = block->prev;
		}
		lerrorf(lex, "label '%s' not found.", lname);
	}
	else {
		block = lex->f->b;
		while (block) {
			if (block->loop)
				goto find;
			block = block->prev;
		}
		lerror(lex, "no loop statement can be jumped.");
	}

	find:
	if (type == TK_BREAK) {
		block->lout = aloK_jumpforward(lex->f, block->lout);
	}
	else {
		aloK_jumpbackward(lex->f, block->lcon);
	}
}

static void forstat(alexer_t* lex) {
	/* forstat -> 'for' [lname] '(' IDENT [',' IDENT] '<-' expr ')' stat ['else' stat] */
	int line = lex->cl;
	astr lname = NULL;
	if (checknext(lex, '[')) {
		lname = testident(lex)->array;
		testenclose(lex, '[', ']', line);
		line = lex->cl;
	}
	testnext(lex, '(');
	ablock_t b;
	afstat_t* f = lex->f;
	int index = pushcv(f); /* push case variable */
	f->d->cv.a[index].name = testident(lex);
	while (checknext(lex, ',')) {
		f->d->cv.a[pushcv(f)].name = testident(lex);
	}
	testnext(lex, TK_LARR);
	aestat_t e;
	expr(lex, &e);
	testnext(lex, ')');
	int lastfree = f->freelocal;
	aloK_newitr(f, &e); /* push iterator */
	enterblock(f, &b, true, lname);
	aloE_assert(f->nactvar == f->freelocal, "variable number mismatched");
	int nvar = f->d->cv.l - index;
	aloK_checkstack(f, nvar);
	for (int i = 0; i < nvar; ++i) {
		regloc(f, f->d->cv.a[index + i].name);
	}
	aloK_iABC(f, OP_ICALL, 0, 0, 0, e.v.g, 0, nvar + 1);
	f->freelocal = f->nactvar;
	f->d->cv.l = index;
	int label = aloK_jumpforward(f, NO_JUMP);
	stat(lex, false); /* THEN block */
	aloK_jumpbackward(f, b.lcon);
	aloK_fixline(f, line); /* fix line to conditional check */
	leaveblock(f);
	f->freelocal = lastfree;
	aloK_putlabel(f, label);
	if (checknext(lex, TK_ELSE)) {
		stat(lex, false); /* ELSE block */
	}
	aloK_putlabel(lex->f, b.lout);
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
	/* normstat -> apply | assignment | opassignexpr | remexpr */
	alhsa_t a;
	if (check(lex, '$')) {
		monexpr(lex, &a.e);
		aloK_drop(lex->f, &a.e);
		return;
	}
	int oldfreeloc = lex->f->freelocal;
	expr(lex, &a.e);
	switch (lex->ct.t) {
	case ',': case '=': { /* normstat -> assignment */
		a.p = NULL;
		assignment(lex, &a, 1);
		lex->f->freelocal = oldfreeloc; /* clear extra value */
		break;
	}
	case TK_AADD ... TK_AXOR: { /* normstat -> expr assignop expr */
		int line = lex->cl;
		int type = lex->ct.t;
		poll(lex);
		aestat_t e;
		expr(lex, &e);
		aloK_opassign(lex->f, &a.e, &e, type - TK_AADD + OP_AADD, line);
		break;
	}
	case TK_ABDOT: {
		int line = lex->cl;
		poll(lex);
		aestat_t e;
		expr(lex, &e);
		aloK_opassign(lex->f, &a.e, &e, OP_ACAT, line);
		break;
	}
	default: { /* normstat -> apply */
		if (a.e.t != E_CALL) {
			lerror(lex, "call expression expected.");
		}
		SET_C(getinsn(lex->f, &a.e), 1);
		aloE_assert(lex->f->freelocal == lex->f->nactvar + 1, "unexpected free local variable count.");
		lex->f->freelocal = oldfreeloc; /* clear function result */
		break;
	}
	}
}

static void fistat(alexer_t* lex) {
	/* fistat -> '->' ('{' stats '}' | varexpr) */
	int line = lex->cl;
	testnext(lex, TK_RARR); /* test '->' */
	if (check(lex, '{')) {
		line = lex->cl;
		poll(lex);
		rootstats(lex);
		testenclose(lex, '{', '}', line);
	}
	else {
		afstat_t* f = lex->f;
		aestat_t e;
		int first = f->nactvar;
		int nret = varexpr(lex, &e);
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
		aloK_return(f, first, nret);
		aloK_fixline(f, line);
	}
}

static void defstat(alexer_t* lex) {
	/* defstat -> 'def' IDENT { '(' funcarg ')' } fistat */
	afstat_t* f = lex->f;
	int line = lex->cl;
	astring_t* name = testident(lex);
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
	fistat(lex);
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
	astring_t* name = testident(lex);
	if (check(lex, '(') || check(lex, TK_RARR)) {
		/* localstat -> 'local' IDENT { '(' funcarg ')' } fistat */
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
		fistat(lex);
		leavefunc(&f2);
		aloE_assert(f->freelocal == index, "illegal local variable index");
		aestat_t e;
		aloK_loadproto(f, &e);
		aloK_fixline(lex->f, line); /* fix line to conditional check */
		aloK_move(f, &e, index);
	}
	else {
		/* localstat -> 'local' IDENT [ ',' IDENT ] { '=' varexpr } */
		int index = pushcv(f);
		f->d->cv.a[index].name = name;
		while (checknext(lex, ',')) {
			index = pushcv(f);
			f->d->cv.a[index].name = testident(lex);
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

static int stat(alexer_t* lex, int assign) {
	lex->f->freelocal = lex->f->nactvar;
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
	case TK_FOR: { /* stat -> forstat */
		poll(lex);
		forstat(lex);
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
	case TK_BREAK: case TK_CONTINUE: { /* stat -> breakstat | continuestat */
		int type = lex->ct.t;
		poll(lex); /* skip 'break' or 'continue' */
		jumpstat(lex, type);
		return true;
	}
	case '{': { /* stat -> '{' stats '}' */
		int line = lex->cl;
		poll(lex);
		ablock_t block;
		enterblock(lex->f, &block, false, NULL);
		int flag = stats(lex);
		leaveblock(lex->f);
		testenclose(lex, '{', '}', line);
		return flag;
	}
	default: { /* stat -> normstat */
		normstat(lex);
		return false;
	}
	}
}

static int stats(alexer_t* lex) {
	/* stats -> { stat {';'} } */
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
	for (int i = 0; i < p->nchild; ++i) {
		registerproto(T, p->children[i]);
	}
	aloG_register(T, p, ALO_TPROTO);
}

struct alo_ParseContext {
	alexer_t lex;
	apdata_t data;
};

void pparse(astate T, void* raw) {
	struct alo_ParseContext* ctx = aloE_cast(struct alo_ParseContext*, raw);
	ctx->data.p = aloF_newp(T);
	ablock_t b;
	afstat_t f = { .p = ctx->data.p, .e = NULL, .d = &ctx->data, .cjump = NO_JUMP };
	ctx->lex.f = &f;
	f.l = &ctx->lex;
	poll(&ctx->lex);
	initproto(T, &f);
	enterblock(&f, &b, false, NULL);
	rootstats(&ctx->lex);
	test(&ctx->lex, TK_EOF); /* should compile to end of script */
	leaveblock(&f);
	closeproto(T, &f);
}

void destory_context(astate T, apdata_t* data) {
	aloM_dela(T, data->ss.a, data->ss.c);
	aloM_dela(T, data->jp.a, data->jp.c);
	aloM_dela(T, data->lb.a, data->lb.c);
	aloM_dela(T, data->cv.a, data->cv.c);
}

int aloP_parse(astate T, astr src, aibuf_t* in, aproto_t** out, astring_t** msg) {
	askid_t top = T->top;
	/* open lexer */
	struct alo_ParseContext context;
	context.data = (apdata_t) { };
	aloX_open(T, &context.lex, src, in);

	int status = aloD_prun(T, pparse, &context);
	if (status == ThreadStateRun) {
		registerproto(T, context.data.p);
		*out = context.data.p;
		*msg = NULL;
	}
	else {
		aloZ_delete(T, context.data.p);
		*msg = tgetstr(T->top - 1);
	}
	destory_context(T, &context.data);
	aloX_close(&context.lex);

	T->top = top;
	return status;
}

