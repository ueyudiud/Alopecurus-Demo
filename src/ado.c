/*
 * ado.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#include "abuf.h"
#include "agc.h"
#include "adebug.h"
#include "avm.h"
#include "ado.h"

#include <stdlib.h>

void aloD_setdebt(aglobal_t* G, ssize_t debt) {
	ssize_t total = G->mbase + G->mdebt;
	G->mbase = total - debt;
	G->mdebt = debt;
}

/* some extra spaces for error handling */
#define ERRORSTACKSIZE (ALO_MAXSTACKSIZE + 100)

void aloD_growstack(astate T, size_t need) {
	size_t oldsize = T->stacksize;
	if (oldsize > ALO_MAXSTACKSIZE) {
		aloD_throw(T, ThreadStateErrError);
	}
	size_t require = aloE_cast(size_t, T->top - T->stack) + need + EXTRA_STACK;
	if (require > ALO_MAXSTACKSIZE) {
		aloD_reallocstack(T, ERRORSTACKSIZE);
		aloU_rterror(T, "stack overflow"); /* throw an error */
	}
	else {
		size_t newsize = oldsize * 2;
		if (newsize < require) {
			newsize = require;
		}
		aloD_reallocstack(T, newsize);
	}
}

static void correct_stack(astate T, ptrdiff_t off) {
	T->top += off;
	acap* cap = T->captures;
	while (cap) {
		cap->p += off;
		cap = cap->prev;
	}
	aframe_t* frame = T->frame;
	while (frame) {
		frame->fun += off;
		frame->top += off;
		if (frame->falo) { /* for Alopecurus frame */
			frame->a.base += off;
		}
		frame = frame->prev;
	}
}

void aloD_reallocstack(astate T, size_t newsize) {
	atval_t* stack = T->stack;
	aloE_assert(newsize <= ALO_MAXSTACKSIZE || newsize == ERRORSTACKSIZE, "invalid stack size.");
	aloM_update(T, T->stack, T->stacksize, newsize, tsetnil);
	correct_stack(T, T->stack - stack);
}

/**
 ** run function in protection.
 */
int aloD_prun(astate T, apfun fun, void* ctx) {
	uint16_t nccall = T->nccall, nxyield = T->nxyield;
	ajmp_t label;
	label.prev = T->label;
	label.status = ThreadStateRun;
	label.target = NULL;
	T->label = &label;
	if (setjmp(label.buf) == 0) {
		fun(T, ctx);
	}
	T->label = label.prev;
	T->nccall = nccall;
	T->nxyield = nxyield;
	return label.status;
}

/**
 ** use string builder in protection.
 */
void aloD_usesb(astate T, void (*fun)(astate, asbuf_t*)) {
	uint16_t nccall = T->nccall;
	uint64_t nyield = T->nxyield++; /* can not call 'yield' when using buffer */
	ajmp_t label;
	label.prev = T->label;
	label.status = ThreadStateRun;
	label.target = NULL;
	aloB_bdecl(buf);
	T->label = &label;
	if (setjmp(label.buf) == 0) {
		fun(T, &buf);
	}
	aloB_bfree(T, buf);
	T->label = label.prev;
	T->nxyield = nyield;
	T->nccall = nccall;
	if (label.status != ThreadStateRun) {
		aloD_throw(T, label.status);
	}
}

/**
 ** throw an error.
 */
anoret aloD_throw(astate T, int status) {
	aloE_assert(status >= ThreadStateErrRuntime, "can only throw error status by this function.");
	if (T->label) {
		T->label->status = status;
		longjmp(T->label->buf, 1);
	}
	else { /* no error handler in current thread */
		T->status = aloE_byte(status);
		if (T->caller) {
			tsetobj(T, T->caller->top++, T->top - 1); /* move error message */
			aloD_throw(T->caller, status); /* handle error function by caller */
		}
		else {
			Gd(T);
			if (G->panic) {
				G->panic(T); /* call panic function, last chance to jump out */
			}
			abort();
		}
	}
}

#define checkstack(T,n,f) aloD_checkstackaux(T, n, ptrdiff_t _off = getstkoff(T, f); aloG_check(T), f = putstkoff(T, _off))

#define nextframe(T) ((T)->frame->next ?: aloR_pushframe(T))

/**
 ** move result to correct place.
 */
static int moveresults(astate T, askid_t dest, askid_t src, int nresult, int expected) {
	switch (expected) {
	case 0: /* no result */
		break;
	case 1: /* single result */
		if (nresult == 0)
			tsetnil(dest);
		else
			tsetobj(T, dest, src);
		break;
	case ALO_MULTIRET: /* multi result */
		for (int i = nresult - 1; i >= 0; --i) {
			tsetobj(T, dest + i, src + i);
		}
		T->top = dest + nresult;
		return true;
	default:
		if (nresult >= expected) {
			for (int i = 0; i < expected; ++i) {
				tsetobj(T, dest + i, src + i);
			}
		}
		else {
			int i = 0;
			for (; i < nresult; ++i) {
				tsetobj(T, dest + i, src + i);
			}
			for (; i < expected; ++i) {
				tsetnil(dest + i);
			}
		}
		break;
	}
	T->top = dest + expected;
	return false;
}

/**
 ** prepares function call, if it is C function, call it directly, or wait
 ** for 'aloV_excute' calling function
 */
int aloD_precall(astate T, askid_t fun, int nresult) {
	acfun caller;
	switch (ttype(fun)) {
	case ALO_TLCF:
		caller = tgetlcf(fun);
		goto C;
	case ALO_TCCL:
		caller = tgetclo(fun)->c.handle;
		C: {
			checkstack(T, ALO_MINSTACKSIZE, fun);
			aframe_t* frame = nextframe(T);
			frame->nresult = nresult;
			frame->name = aloU_getcname(T, caller);
			frame->c.kfun = NULL;
			frame->c.ctx = NULL;
			frame->fun = fun;
			frame->top = T->top + ALO_MINSTACKSIZE;
			frame->flags = 0;
			T->frame = frame;
			int n = caller(T);
			aloE_assert(frame->fun + n <= T->top, "no enough elements in stack.");
			aloD_postcall(T, T->top - n, n);
		}
		return true;
	case ALO_TACL: {
		aproto_t* p = tgetclo(fun)->a.proto;
		askid_t base = fun + 1;
		int actual = T->top - base;
		if (p->fvararg) { /* handling for variable arguments */
			int i;
			for (i = 0; i < p->nargs && i < actual; ++i) {
				tsetobj(T, T->top + i, base + i);
			}
			for (; i < p->nargs; ++i) {
				tsetnil(T->top + i);
			}
			base = T->top;
		}
		else {
			for (int i = actual; i < p->nargs; ++i) {
				tsetnil(base + i);
			}
		}
		checkstack(T, p->nstack, base);
		T->top = base + p->nstack;
		aframe_t* frame = nextframe(T);
		frame->falo = true;
		frame->name = p->name ? p->name->array : "<unknown>";
		frame->nresult = nresult;
		frame->fun = fun;
		frame->a.base = base;
		frame->a.pc = p->code;
		T->frame = frame;
		T->top = frame->top = base + p->nstack;
		return false;
	}
	default: { /* not a function, try call meta method */
		/* prepare stack for meta method */
		checkstack(T, 1, fun);
		const atval_t* tm = aloT_gettm(T, fun, TM_CALL, true); /* get tagged method */
		if (tm == NULL || !ttisfun(tm)) { /* not a function? */
			aloU_rterror(T, "fail to call object.");
		}
		for (askid_t p = T->top; p > fun; --p) {
			tsetobj(T, p, p - 1);
		}
		T->top++;
		tsetobj(T, fun, tm);
		return aloD_precall(T, fun, nresult);
	}
	}
}

void aloD_postcall(astate T, askid_t ret, int nresult) {
	aframe_t* const frame = T->frame;
	askid_t res = frame->fun;
	int expected = frame->nresult;
	T->frame = frame->prev; /* back to previous frame */
	aloR_closeframe(T, frame); /* close current frame */
	/* move result to proper place */
	moveresults(T, res, ret, nresult, expected);
}

#define EXTRAERRCALL 100

static void stackerror(astate T) {
	if (T->nccall == ALO_MAXCCALL) {
		aloU_rterror(T, "C stack overflow");
	}
	else if (T->nccall >= ALO_MAXCCALL + EXTRAERRCALL) {
		aloD_throw(T, ThreadStateErrError); /* too many stack while handling error */
	}
}

void aloD_call(astate T, askid_t fun, int nresult) {
	if (++T->nccall >= ALO_MAXCCALL)
		stackerror(T);
	if (!aloD_precall(T, fun, nresult)) {
		aloV_invoke(T, false); /* invoke Alopecurus function */
	}
	T->nccall--;
}

void aloD_callnoyield(astate T, askid_t fun, int nresult) {
	T->nxyield++;
	aloD_call(T, fun, nresult);
	T->nxyield--;
}
