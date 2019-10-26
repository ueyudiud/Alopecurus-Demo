/*
 * aeval.c
 *
 *  Created on: 2019年8月10日
 *      Author: ueyudiud
 */

#include "aop.h"
#include "aobj.h"
#include "astr.h"
#include "atup.h"
#include "alis.h"
#include "atab.h"
#include "afun.h"
#include "astate.h"
#include "agc.h"
#include "abuf.h"
#include "ameta.h"
#include "adebug.h"
#include "ado.h"
#include "aeval.h"

#include <math.h>

/* cast integer to value for taking native operation */
#define i2x(i) aloE_cast(uint64_t, i)

/* call C math function */
#define mcall(T,f,x...) (aloE_void(T), aloE_flt(f(x)))

/* only take conversion from number to integer */
#if ALO_STRICT_NUMTYPE
#define vm_toint(o,i) (ttisint(o) && (i = tgetint(o), true))
#else
#define vm_toint(o,i) (ttisint(o) ? (i = tgetint(o), true) : ttisflt(o) && aloO_flt2int(tgetflt(o), &i, 0))
#endif

#define vm_ibin(T,op,x,y) (aloE_void(T), aloE_int(i2x(x) op i2x(y)))
#define vm_fbin(T,op,x,y) (aloE_void(T), aloE_flt((x) op (y)))

#define vm_addi(T,x,y) vm_ibin(T, +, x, y)
#define vm_add(T,x,y) vm_fbin(T, +, x, y)

#define vm_subi(T,x,y) vm_ibin(T, -, x, y)
#define vm_sub(T,x,y) vm_fbin(T, -, x, y)

#define vm_muli(T,x,y) vm_ibin(T, *, x, y)
#define vm_mul(T,x,y) vm_fbin(T, *, x, y)

#define vm_div(T,x,y) vm_fbin(T, /, x, y)

/* check two value has same sign */
#define samesg(x,y) (!(((x) ^ (y)) & AINT_MIN))

static aint vm_idiv(astate T, aint a, aint b) {
	switch (b) { /* handle for special cases */
	case -1:
		return -a; /* give negative number to prevent a bug for divided AINT_MIN */
	case 0:
		aloU_rterror(T, "attempt to divide by 0");
		return 0;
	case 1:
		return a;
	}
	aint n = a / b;
	return a != b * n && !samesg(a, b) ? n - 1 : n; /* floor(a/b) */
}

#define vm_idivf(T,a,b) floor(vm_div(T, a, b))

#define vm_pow(T,a,b) mcall(T, pow, a, b)

static aint vm_mod(astate T, aint a, aint b) {
	switch (b) { /* handle for special cases */
	case 1:
	case -1:
		return 0;
	case 0:
		aloU_rterror(T, "attempt to get modulo by 0");
		return 0;
	}
	aint n = a % b;
	return n && !samesg(n, b) ? n + b : n; /* a-floor(a/b)*b */
}

static afloat vm_modf(astate T, afloat a, afloat b) {
	afloat m = fmod(a, b);
	return a * b >= 0 ? /* has same operator? */
			m : m + b;
}

#define NBITS (sizeof(aint) * CHAR_BIT)

static aint vm_shl(astate T, aint a, aint b) {
	return b > 0 ?
			b >=  NBITS ? 0 : aloE_int((uint64_t) a << (uint64_t) b) :
			b <= -NBITS ? 0 : aloE_int((uint64_t) a >> (uint64_t)-b);
}

static aint vm_shr(astate T, aint a, aint b) {
	return b > 0 ?
			b >=  NBITS ? 0 : aloE_int((uint64_t) a >> (uint64_t) b) :
			b <= -NBITS ? 0 : aloE_int((uint64_t) a << (uint64_t)-b);
}

#define vm_band(T,a,b) vm_ibin(T, &, a, b)
#define vm_bor(T,a,b) vm_ibin(T, |, a, b)
#define vm_bxor(T,a,b) vm_ibin(T, ^, a, b)

/**
 ** check cached closure is available prototype.
 */
static aclosure_t* getcached(aproto_t* p, aclosure_t* en, askid_t base) {
	aclosure_t* c = p->cache;
	if (c == NULL)
		return NULL;
	acapinfo_t* infos = p->captures;
	for (int insn = 0; insn < p->ncap; ++insn) {
		if (!ttiscap(c->array + insn)) {
			return NULL;
		}
		atval_t* t = infos[insn].finstack ?
				base + infos[insn].index :
				tgetcap(en->array + infos[insn].index)->p;
		if (t != tgetcap(c->array + insn)->p)
			return NULL;
	}
	return c;
}

/**
 ** load prototype.
 */
static aclosure_t* aloV_nloadproto(astate T, aproto_t* p, aclosure_t* en, askid_t base) {
	aclosure_t* c = getcached(p, en, base);
	if (c != NULL) {
		return c;
	}
	c = aloF_new(T, p->ncap, p);
	tsetclo(T, T->top++, c);
	trsetvalx(T, &c->delegate, en->delegate);
	acapinfo_t* infos = p->captures;
	for (int i = 0; i < p->ncap; ++i) {
		if (infos[i].finstack) {
			tsetcap(c->array + i, aloF_find(T, base + infos[i].index));
		}
		else {
			tsetobj(T, c->array + i, en->array + infos[i].index); /* move enclosed capture */
		}
	}
	return c;
}

/**
 ** A[B] := C
 */
static int aloV_nset(astate T, const atval_t* A, const atval_t* B, const atval_t* C) {
	switch (ttpnv(A)) {
	case ALO_TLIST:
		aloI_set(T, tgetlis(A), B, C, NULL);
		break;
	case ALO_TTABLE: {
		atable_t* table = tgettab(A);
		aloH_set(T, table, B, C);
		aloG_barrierbackt(T, table, B);
		aloH_markdirty(table);
		break;
	}
	default:
		return false;
	}
	return true;
}

/**
 ** A := remove B[C]
 */
static int aloV_nremove(astate T, atval_t* A, const atval_t* B, const atval_t* C) {
	int s;
	switch (ttpnv(B)) {
	case ALO_TLIST:
		s = aloI_remove(T, tgetlis(B), C, A);
		break;
	case ALO_TTABLE:
		s = aloH_remove(T, tgettab(B), C, A);
		break;
	default:
		return false;
	}
	if (!s) { /* when nothing removed */
		tsetnil(A);
	}
	return true;
}

/**
 ** A := B + C
 */
static int aloV_nadd(astate T, atval_t* A, const atval_t* B, const atval_t* C) {
	if (ttisint(B) && ttisint(C)) {
		tsetint(A, vm_addi(T, tgetint(B), tgetint(C)));
	}
	else if (ttisnum(B) && ttisnum(C)) {
		tsetflt(A, vm_add(T, tgetnum(B), tgetnum(C)));
	}
	else {
		return false;
	}
	return true;
}

/**
 ** A := B - C
 */
static int aloV_nsub(astate T, atval_t* A, const atval_t* B, const atval_t* C) {
	if (ttisint(B) && ttisint(C)) {
		tsetint(A, vm_subi(T, tgetint(B), tgetint(C)));
	}
	else if (ttisnum(B) && ttisnum(C)) {
		tsetflt(A, vm_sub(T, tgetnum(B), tgetnum(C)));
	}
	else {
		return false;
	}
	return true;
}

/**
 ** A := B * C
 */
static int aloV_nmul(astate T, atval_t* A, const atval_t* B, const atval_t* C) {
	if (ttisint(B) && ttisint(C)) {
		tsetint(A, vm_muli(T, tgetint(B), tgetint(C)));
	}
	else if (ttisnum(B) && ttisnum(C)) {
		tsetflt(A, vm_mul(T, tgetnum(B), tgetnum(C)));
	}
	else {
		return false;
	}
	return true;
}

/**
 ** A := B / C
 */
static int aloV_ndiv(astate T, atval_t* A, const atval_t* B, const atval_t* C) {
	if (ttisnum(B) && ttisnum(C)) {
		tsetflt(A, vm_div(T, tgetnum(B), tgetnum(C)));
	}
	else {
		return false;
	}
	return true;
}

/**
 ** A := B // C
 */
static int aloV_nidiv(astate T, atval_t* A, const atval_t* B, const atval_t* C) {
	aint i1, i2;
	if (vm_toint(B, i1) && vm_toint(C, i2)) {
		tsetint(A, vm_idiv(T, i1, i2));
	}
	else if (ttisnum(B) && ttisnum(C)) {
		tsetflt(A, vm_idivf(T, tgetnum(B), tgetnum(C)));
	}
	else {
		return false;
	}
	return true;
}

/**
 ** A := B % C
 */
static int aloV_nmod(astate T, atval_t* A, const atval_t* B, const atval_t* C) {
	aint i1, i2;
	if (vm_toint(B, i1) && (vm_toint(C, i2) && (ttisint(C) || i2 != 0))) {
		tsetint(A, vm_mod(T, i1, i2));
	}
	else if (ttisnum(B) && ttisnum(C)) {
		tsetflt(A, vm_modf(T, tgetnum(B), tgetnum(C)));
	}
	else {
		return false;
	}
	return true;
}

/**
 ** A := B ^ C
 */
static int aloV_npow(astate T, atval_t* A, const atval_t* B, const atval_t* C) {
	if (ttisnum(B) && ttisnum(C)) {
		tsetflt(A, vm_pow(T, tgetnum(B), tgetnum(C)));
	}
	else {
		return false;
	}
	return true;
}

/**
 ** A := B << C
 */
static int aloV_nshl(astate T, atval_t* A, const atval_t* B, const atval_t* C) {
	aint i1, i2;
	if (vm_toint(B, i1) && vm_toint(C, i2)) {
		tsetint(A, vm_shl(T, i1, i2));
	}
	else {
		return false;
	}
	return true;
}

/**
 ** A := B >> C
 */
static int aloV_nshr(astate T, atval_t* A, const atval_t* B, const atval_t* C) {
	aint i1, i2;
	if (vm_toint(B, i1) && vm_toint(C, i2)) {
		tsetint(A, vm_shr(T, i1, i2));
	}
	else {
		return false;
	}
	return true;
}

/**
 ** A := B & C
 */
static int aloV_nband(astate T, atval_t* A, const atval_t* B, const atval_t* C) {
	aint i1, i2;
	if (vm_toint(B, i1) && vm_toint(C, i2)) {
		tsetint(A, vm_band(T, i1, i2));
	}
	else {
		return false;
	}
	return true;
}

/**
 ** A := B | C
 */
static int aloV_nbor(astate T, atval_t* A, const atval_t* B, const atval_t* C) {
	aint i1, i2;
	if (vm_toint(B, i1) && vm_toint(C, i2)) {
		tsetint(A, vm_bor(T, i1, i2));
	}
	else {
		return false;
	}
	return true;
}

/**
 ** A := B ~ C
 */
static int aloV_nbxor(astate T, atval_t* A, const atval_t* B, const atval_t* C) {
	aint i1, i2;
	if (vm_toint(B, i1) && vm_toint(C, i2)) {
		tsetint(A, vm_bxor(T, i1, i2));
	}
	else {
		return false;
	}
	return true;
}

int (* const aloV_nbinary[])(astate, atval_t*, const atval_t*, const atval_t*) = {
	aloV_nadd,
	aloV_nsub,
	aloV_nmul,
	aloV_ndiv,
	aloV_nidiv,
	aloV_nmod,
	aloV_npow,
	aloV_nshl,
	aloV_nshr,
	aloV_nband,
	aloV_nbor,
	aloV_nbxor
};

static int aloV_npnm(astate T, atval_t* A, const atval_t* B) {
	if (ttisnum(B)) {
		tsetobj(NULL, A, B);
	}
	else {
		return false;
	}
	return true;
}

static int aloV_nunm(astate T, atval_t* A, const atval_t* B) {
	if (ttisint(B)) {
		tsetint(A, -tgetint(B));
	}
	else if (ttisflt(B)) {
		tsetflt(A, -tgetflt(B));
	}
	else {
		return false;
	}
	return true;
}

static int aloV_nlen(astate T, atval_t* A, const atval_t* B) {
	return false;
}

static int aloV_nbnot(astate T, atval_t* A, const atval_t* B) {
	aint i;
	if (vm_toint(B, i)) {
		tsetint(A, ~i);
	}
	else {
		return false;
	}
	return true;
}

int (* const aloV_nunary[])(astate, atval_t*, const atval_t*) = {
	aloV_npnm,
	aloV_nunm,
	aloV_nlen,
	aloV_nbnot
};

int aloV_neq(astate T, int* A, const atval_t* B, const atval_t* C) {
	*A = aloV_equal(NULL, B, C); /* use 'aloV_equal' to compare */
	return true;
}

int aloV_nlt(astate T, int* A, const atval_t* B, const atval_t* C) {
	if (ttisint(B) && ttisint(C)) {
		*A = tgetint(B) < tgetint(C);
	}
	else if (ttisnum(B) && ttisnum(C)) {
		*A = tgetnum(B) < tgetnum(C);
	}
	else if (ttisstr(B) && ttisstr(C)) {
		*A = aloS_compare(tgetstr(B), tgetstr(C)) < 0;
	}
	else {
		return false;
	}
	return true;
}

int aloV_nle(astate T, int* A, const atval_t* B, const atval_t* C) {
	if (ttisint(B) && ttisint(C)) {
		*A = tgetint(B) <= tgetint(C);
	}
	else if (ttisnum(B) && ttisnum(C)) {
		*A = tgetnum(B) <= tgetnum(C);
	}
	else if (ttisstr(B) && ttisstr(C)) {
		*A = aloS_compare(tgetstr(B), tgetstr(C)) <= 0;
	}
	else {
		return false;
	}
	return true;
}

int (* const aloV_ncompare[])(astate, int*, const atval_t*, const atval_t*) = {
	aloV_neq,
	aloV_nlt,
	aloV_nle
};

int aloV_binop(astate T, int op, atval_t* out, const atval_t* in1, const atval_t* in2) {
	return aloV_nbinary[aloE_check(op >= ALO_OPADD && op <= ALO_OPBXOR, "illegal operation", op - ALO_OPADD)](T, out, in1, in2);
}

int aloV_unrop(astate T, int op, atval_t* out, const atval_t* in1) {
	return aloV_nunary[aloE_check(op >= ALO_OPPOS && op <= ALO_OPBNOT, "illegal operation", op - ALO_OPPOS)](T, out, in1);
}

int aloV_cmpop(astate T, int op, int* out, const atval_t* in1, const atval_t* in2) {
	return aloV_ncompare[aloE_check(op >= ALO_OPEQ && op <= ALO_OPLE, "illegal operation", op - ALO_OPEQ)](T, out, in1, in2);
}

#define checkGC(T,t) aloG_xcheck(T, (T)->top = (t), (T)->top = frame->top)

/**
 ** invoke Alopecurus function.
 */
void aloV_invoke(astate T, int dofinish) {
	aframe_t* frame = T->frame;
	const atval_t* consts;
	atval_t* capture;
	askid_t base;
	aproto_t* proto;
	ainsn_t** ppc;

	inline askid_t R(abyte mode, uint16_t index) {
		return base + index;
	}

	inline const atval_t* K(uint16_t index) {
		return consts + index;
	}

	atval_t* S(abyte mode, uint16_t index) {
		if (aloK_iscapture(index)) {
			atval_t* o = capture + aloK_getcapture(index);
			return ttiscap(o) ? tgetcap(o)->p : o;
		}
		else {
			return base + aloK_getstack(index);
		}
	}

	const atval_t* X(abyte mode, uint16_t index) {
		return mode ? K(index) : S(mode, index);
	}

	invoke: {
		aloE_assert(frame == T->frame, "frame should be current frame.");
		aclosure_t* c = tgetclo(frame->fun);
		ppc = &frame->a.pc;
		capture = &c->delegate;
		proto = c->a.proto;
		consts = proto->consts;
		base = frame->a.base;

		frame->fact = true; /* start active */
	}

	ainsn_t I;
	const atval_t *tb, *tc, *tm;

	if (dofinish) {
		dofinish = false;
		goto finish;
	}

/* macros for main interpreting */
#define pc (*ppc)

#define insn GET_i(I)
#define x GET_x(I)
#define xA GET_xA(I)
#define xB GET_xB(I)
#define xC GET_xC(I)
#define yA GET_A(I)
#define yB GET_B(I)
#define yC GET_C(I)
#define A xA, yA
#define B xB, yB
#define C xC, yC
#define Bx GET_Bx(I)
#define sBx GET_sBx(I)
#define Ax GET_Ax(I)
#define protect(x) ({ x; base = frame->a.base; })

	normal:
	while (true) {
		I = *pc;
		pc += 1;
		switch (insn) { /* get instruction */
		case OP_MOV: {
			tsetobj(T, S(A), X(B));
			break;
		}
		case OP_LDC: {
			tsetobj(T, S(A), K(Bx));
			break;
		}
		case OP_LDN: {
			for (int n = 0; n < yB; tsetnil(R(A + n)), ++n);
			break;
		}
		case OP_LDP: {
			aproto_t* p = proto->children[Bx];
			aclosure_t* c = aloV_nloadproto(T, p, tgetclo(frame->fun), base);
			tsetclo(T, S(A), c);
			T->top = frame->top; /* adjust top */
			checkGC(T, T->top);
			break;
		}
		case OP_LDV: {
			size_t nargs = yB - 1;
			int flag = nargs < 0;
			if (flag) {
				nargs = base - (frame->fun + 1) - proto->nargs;
				protect(aloD_checkstack(T, nargs));
			}
			askid_t dest = R(A);
			askid_t src = frame->fun + 1 + proto->nargs;
			for (size_t i = 0; i < nargs; ++i) {
				tsetobj(T, dest + i, src + i);
			}
			if (flag) {
				T->top = dest + nargs;
			}
			break;
		}
		case OP_SELV: {
			aint id;
			size_t n = base - (frame->fun + 1) - proto->nargs;
			switch (xB) {
			case 0: case 1: {
				if (vm_toint(X(B), id)) {
					goto indexvar;
				}
				else {
					aloU_invalidkey(T);
				}
				break;
			}
			case 2: { /* integer constant */
				id = yB;
				indexvar: {
					if (id >= n) {
						aloU_outofrange(T, id, n);
					}
					tsetobj(T, S(A), (frame->fun + 1) + proto->nargs + id);
				}
				break;
			}
			case 3: { /* length of variables */
				tsetint(S(A), n);
				break;
			}
			}
			break;
		}
		case OP_NEWA: {
			askid_t s = R(B);
			size_t n;
			if (xC) {
				n = T->top - s;
				if (yC && yC - 1 != n) {
					break;
				}
				T->top = frame->top;
			}
			else {
				n = yC;
			}
			tsettup(T, S(A), aloA_new(T, n, s));
			checkGC(T, s);
			break;
		}
		case OP_NEWL: {
			alist_t* v = aloI_new(T);
			tsetlis(T, S(A), v);
			if (Bx > 0) {
				aloI_ensure(T, v, Bx);
			}
			checkGC(T, T->top);
			break;
		}
		case OP_NEWM: {
			atable_t* v = aloH_new(T);
			tsettab(T, S(A), v);
			if (Bx > 0) {
				aloH_ensure(T, v, Bx);
			}
			checkGC(T, T->top);
			break;
		}
		case OP_UNBOX: {
			tb = X(B);
			protect(tm = aloT_gettm(T, tb, TM_UNBOX, true));
			if (tm) {
				tsetobj(T, R(A    ), tm);
				tsetobj(T, R(A + 1), tb);
				T->top = R(A + 2);
				aloD_call(T, R(A), ALO_MULTIRET);
				goto finish;
			}
			else {
				if (!ttistup(tb)) {
					goto notfound;
				}
				atuple_t* v = tgettup(tb);
				int n = yC - 1;
				if (n == ALO_MULTIRET) {
					askid_t s = R(A);
					if (s + v->length > T->top) {
						protect(aloD_growstack(T, s + v->length - T->top));
					}
					for (int i = 0; i < v->length; ++i) {
						tsetobj(T, R(A + i), v->array + i);
					}
					T->top = R(A + v->length);
				}
				else {
					int i = 0;
					for (i = 0; i < v->length && i < n; ++i) {
						tsetobj(T, R(A + i), v->array + i);
					}
					for (; i < n; ++i) {
						tsetnil(R(A + i));
					}
				}
			}
			break;
		}
		case OP_GET: {
			const atval_t* tv;
			tb = X(B);
			tc = X(C);
			int f = ttiscol(tb);
			if (f) {
				tv = aloO_get(T, tb, tc);
				if (tv != aloO_tnil) {
					tsetobj(T, S(A), tv);
					break;
				}
			}
			protect(tv = aloT_index(T, tb, tc));
			if (tv == NULL) {
				if (!f) {
					goto notfound;
				}
				tv = aloO_tnil;
			}
			tsetobj(T, S(A), tv);
			break;
		}
		case OP_SET: {
			tb = X(B);
			protect(tm = aloT_fastget(T, tb, TM_NIDX));
			if (tm) {
				aloT_vmput4(T, tm, S(A), tb, X(C));
				aloD_call(T, T->top - 4, 0);
			}
			else if (!aloV_nset(T, S(A), tb, X(C))) {
				goto notfound;
			}
			break;
		}
		case OP_REM: {
			tb = X(B);
			protect(tm = aloT_gettm(T, tb, TM_REM, true));
			if (tm) {
				aloT_vmput3(T, tm, tb, X(C));
				aloD_call(T, T->top - 3, 1);
				tsetobj(T, S(A), --T->top);
			}
			else if (!aloV_nremove(T, S(A), tb, X(C))) {
				goto notfound;
			}
			break;
		}
		case OP_SELF: {
			protect(tm = aloT_lookup(T, X(B), X(C))); /* lookup member function */
			if (tm == NULL) {
				aloU_rterror(T, "unable to look up field.");
			}
			tsetobj(T, R(A + 1), X(B));
			tsetobj(T, R(A    ), tm);
			break;
		}
		case OP_ADD ... OP_BXOR: {
			int op = insn - OP_ADD;
			tb = X(B);
			tc = X(C);
			if (!(aloV_nbinary[op])(T, S(A), tb, tc)) {
				if (!aloT_trybin(T, tb, tc, op + TM_ADD)) {
					goto notfound;
				}
				tsetobj(T, S(A), T->top);
			}
			break;
		}
		case OP_PNM: case OP_UNM: case OP_BNOT: {
			int op = insn - OP_PNM;
			tb = X(B);
			if (!(aloV_nunary[op])(T, S(A), tb)) {
				if (!aloT_tryunr(T, tb, op + TM_PNM)) {
					goto notfound;
				}
				tsetobj(T, S(A), T->top);
			}
			break;
		}
		case OP_LEN: {
			tb = X(B);
			if (aloT_tryunr(T, tb, TM_LEN)) {
				goto finish;
			}
			else {
				size_t n;
				switch (ttpnv(tb)) {
				case ALO_TSTRING: n = aloS_len(tgetstr(tb)); break;
				case ALO_TTUPLE : n = tgettup(tb)->length; break;
				case ALO_TLIST  : n = tgetlis(tb)->length; break;
				case ALO_TTABLE : n = tgettab(tb)->length; break;
				default: goto notfound;
				}
				tsetint(S(A), n);
			}
			break;
		}
		case OP_AADD ... OP_ASHR: {
			int op = insn - OP_AADD;
			atval_t* s = S(A);
			tb = X(B);
			if (!(aloV_nbinary[op])(T, s, s, tb)) {
				if (!aloT_trybin(T, s, tb, op + TM_AADD)) {
					goto notfound;
				}
				goto finish;
			}
			break;
		}
		case OP_CAT: {
			askid_t r = R(B);
			size_t n = yC - 1;
			if (n == ALO_MULTIRET) {
				n = T->top - r;
			}
			else {
				n += 2;
				T->top = r + n;
			}
			aloV_concat(T, n);
			checkGC(T, T->top);
			goto finish;
		}
		case OP_ACAT: {
			atval_t* s = S(A);
			tb = X(B);
			int f;
			protect(f = aloT_trybin(T, s, tb, TM_ACAT));
			if (f) {
				goto finish;
			}
			else if (ttisstr(s)) {
				astring_t* h = tgetstr(s);

				void builder(astate T, asbuf_t* buf) {
					aloB_bwrite(T, buf, h->array, aloS_len(h));
					aloO_tostring(T, aloB_bwrite, buf, tb);
					h = aloS_new(T, buf->array, buf->length);
				}

				aloD_usesb(T, builder);
				tsetstr(T, s, h);
			}
			break;
		}
		case OP_JMP: {
			if (xA) {
				aloF_close(T, R(A));
			}
			pc += sBx;
			break;
		}
		case OP_JCZ: {
			if (aloV_getbool(X(A)) == xC) {
				pc += sBx;
			}
			break;
		}
		case OP_EQ: {
			int flag;
			protect(flag = aloV_equal(T, X(A), X(B)));
			if (flag == xC) {
				pc += 1;
			}
			break;
		}
		case OP_LT: {
			int flag;
			protect(flag = aloV_compare(T, X(A), X(B), ALO_OPLT));
			if (flag == xC) {
				pc += 1;
			}
			break;
		}
		case OP_LE: {
			int flag;
			protect(flag = aloV_compare(T, X(A), X(B), ALO_OPLE));
			if (flag == xC) {
				pc += 1;
			}
			break;
		}
		case OP_CALL: {
			askid_t r = R(A);
			int nresult = yC - 1;
			if (yB != 0) {
				T->top = r + yB;
			}
			if (aloD_precall(T, r, nresult)) {
				if (nresult >= 0) {
					T->top = frame->top; /* adjust results */
				}
			}
			else { /* can be execute by this function */
				frame = T->frame;
				goto invoke; /* execute it */
			}
			protect();
			break;
		}
		case OP_TCALL: { /* tail call function */
			aloF_close(T, base); /* close captures */
			askid_t r = R(A);
			askid_t f = frame->fun;
			int narg = yB - 1; /* get argument count. */
			if (narg == ALO_MULTIRET) {
				narg = (T->top - r) - 1;
			}
			if (!ttisfun(r)) { /* meta helper for call */
				const atval_t* tm = aloT_gettm(T, r, TM_CALL, true); /* get tagged method */
				if (tm == NULL || !ttisfun(tm)) { /* not a function? */
					aloU_rterror(T, "fail to call object.");
				}
				tsetobj(T, --r, tm);
				narg++;
			}

			/* move arguments */
			for (size_t n = 0; n <= narg; ++n) {
				tsetobj(T, f + n, r + n);
			}
			T->top = f + narg + 1;

			/* get number of results */
			int nres = frame->nresult;

			/* move to previous frame and close current frame */
			T->frame = frame = frame->prev;
			frame->mode = FrameModeTail; /* set in tail call mode */
			aloR_closeframe(T, frame->next);

			if (aloD_precall(T, f, nres)) {
				/* called by C function */
				if (frame->falo && frame->fact) { /* still in Alopecurus frame? */
					goto invoke;
				}
				return;
			}
			else { /* can be execute by this function */
				frame = T->frame;
				goto invoke; /* execute it */
			}
			return;
		}
		case OP_RET: {
			aloF_close(T, base); /* close captures */
			askid_t r = R(A);
			int nres = yB - 1;
			if (nres == ALO_MULTIRET) {
				nres = T->top - r;
			}
			aloD_postcall(T, r, nres);
			if (T->frame->falo) { /* can be execute by this function */
				frame = T->frame;
				if (frame->fact) { /* is frame actived */
					dofinish = true; /* mark calling end */
					goto invoke; /* execute it */
				}
			}
			return;
		}
		default: {
			aloE_assert(false, "unknown opcode.");
			break;
		}
		}
	}

	/**
	 ** called when meta call for opcode not found, throw an error.
	 */
	notfound:
	aloU_mnotfound(T, R(A), aloK_opname[insn]);

	/**
	 ** finish block, used for call after 'yield'
	 */
	finish:
	protect(I = pc[-1]); /* do protect */
	switch (GET_i(I)) {
	case OP_UNBOX: {
		int succ;
		int n = T->top - R(A);
		if (xC) {
			succ = yC <= n;
		}
		else {
			succ = yC == n;
			T->top = frame->top;
		}
		pc += succ;
		break;
	}
	case OP_CALL:
		if (yC) {
			T->top = frame->top;
		}
		break;
	case OP_GET: case OP_REM: case OP_ADD: case OP_SUB: case OP_MUL:
	case OP_DIV: case OP_IDIV: case OP_MOD: case OP_POW: case OP_SHL:
	case OP_SHR: case OP_BAND: case OP_BOR: case OP_BXOR: case OP_PNM:
	case OP_UNM: case OP_LEN: case OP_BNOT:
	case OP_AADD: case OP_ASUB: case OP_AMUL: case OP_ADIV: case OP_AIDIV:
	case OP_AMOD: case OP_APOW: case OP_ASHL: case OP_ASHR: case OP_AAND:
	case OP_AOR: case OP_AXOR:
		tsetobj(T, S(A), --T->top);
		break;
	case OP_CAT: case OP_ACAT:
		tsetobj(T, S(A), T->top - 1);
		T->top = frame->top; /* restore top of frame */
		break;
	case OP_SET:
		break;
	case OP_EQ: case OP_LT: case OP_LE:
		if (tgetbool(--T->top) == xC) {
			pc += 1;
		}
		break;
	default:
		aloE_xassert(false);
	}
	goto normal;
}
