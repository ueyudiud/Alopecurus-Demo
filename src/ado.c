/*
 * ado.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#define ADO_C_
#define ALO_CORE

#include "astr.h"
#include "afun.h"
#include "abuf.h"
#include "agc.h"
#include "adebug.h"
#include "avm.h"
#include "ado.h"
#include "alo.h"

#include <stdlib.h>

#define iserrorstate(status) ((status) > ALO_STYIELD)

void aloD_setdebt(aglobal_t* G, ssize_t debt) {
	ssize_t total = G->mbase + G->mdebt;
	G->mbase = total - debt;
	G->mdebt = debt;
}

/* some extra spaces for error handling */
#define ERRORSTACKSIZE (ALO_MAXSTACKSIZE + 100)

void aloD_growstack(alo_State T, size_t need) {
	size_t oldsize = T->stacksize;
	if (oldsize > ALO_MAXSTACKSIZE) {
		aloD_throw(T, ALO_STERRERR);
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
		else if (newsize > ALO_MAXSTACKSIZE) {
			newsize = ALO_MAXSTACKSIZE;
		}
		aloD_reallocstack(T, newsize);
	}
}

static void correct_stack(alo_State T, ptrdiff_t off) {
	T->top += off;
	alo_Capture* cap = T->captures;
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

void aloD_reallocstack(alo_State T, size_t newsize) {
	alo_TVal* stack = T->stack;
	aloE_assert(newsize <= ALO_MAXSTACKSIZE || newsize == ERRORSTACKSIZE, "invalid stack size.");
	aloM_update(T, T->stack, T->stacksize, newsize, tsetnil);
	correct_stack(T, T->stack - stack);
}

static size_t usedstackcount(alo_State T) {
	aframe_t* frame = T->frame;
	alo_StkId lim = T->top;
	while (frame) {
		if (lim < frame->top) {
			lim = frame->top;
		}
		frame = frame->prev;
	}
	return lim - T->stack;
}

static void shrinkframe(alo_State T) {
	if (T->stacksize > ALO_MAXSTACKSIZE) { /* had been handling stack overflow? */
		/* delete all frame above */
		aframe_t* frame = T->frame->next;
		T->frame->next = NULL;
		while (frame) {
			aframe_t* next = frame->next;
			aloM_delo(T, frame);
			frame = next;
		}
	}
	else {
		/* shrink to half of frame */
		aframe_t* frame = T->frame;
		while (frame->next && frame->next->next) { /* has two extra frame? */
			aframe_t* next = frame->next->next;
			aloM_delo(T, frame->next);
			frame->next = next;
			if (next == NULL)
				break;
			next->prev = frame;
			frame = next;
		}
	}
}

void aloD_shrinkstack(alo_State T) {
	shrinkframe(T);
	size_t inuse = usedstackcount(T);
	size_t estimate = inuse + (inuse / 8) + 1 + 2 * EXTRA_STACK;
	if (estimate > ALO_MAXSTACKSIZE) {
		estimate = ALO_MAXSTACKSIZE; /* respect stack limit */
	}
	if (inuse <= (ALO_MAXSTACKSIZE - EXTRA_STACK) && estimate < T->stacksize) {
		aloD_reallocstack(T, estimate);
	}
	else {
		adjuststack(T, estimate);
	}
}

/**
 ** run function in protection.
 */
int aloD_prun(alo_State T, apfun fun, void* ctx) {
	/* store current thread state */
	uint16_t nccall = T->nccall;
	ambuf_t* buf = T->memstk.top; /* store memory buffer */
	/* initialize jump label */
	ajmp_t label;
	label.prev = T->label;
	T->label = &label;
	/* run protected function */
	int status = aloi_try(T, label, fun(T, ctx));
	/* restore old thread state */
	T->label = label.prev;
	T->nccall = nccall;
	T->memstk.top = buf; /* revert memory buffer */
	return status;
}

/**
 ** throw an error.
 */
a_none aloD_throw(alo_State T, int status) {
	aloE_assert(status >= ALO_STERRRT, "can only throw error status by this function.");
	rethrow:
	if (T->label) {
		aloi_throw(T, *T->label, status);
	}
	else { /* no error handler in current thread */
		T->status = aloE_byte(status); /* set coroutine status */
		if (T->caller) {
			tsetobj(T, T->caller->top++, T->top - 1); /* move error message */
			alo_State Tcaller = T->caller;
			T->caller = NULL; /* remove caller */
			T = Tcaller;
			goto rethrow; /* handle error function by caller */
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

void aloD_hook(alo_State T, int event, int line) {
	ahfun hook = T->hook;
	if (hook && T->fallowhook) {
		aframe_t* frame = T->frame;
		ptrdiff_t top = getstkoff(T, T->top);
		ptrdiff_t ftop = getstkoff(T, frame->top);
		alo_DbgInfo info;
		info.event = event;
		info.line = line;
		info._frame = frame;
		aloD_checkstack(T, ALO_MINSTACKSIZE);
		T->frame->top = T->top + ALO_MINSTACKSIZE;
		T->fallowhook = false;
		frame->fhook = true;

		hook(T, &info); /* call hook function */

		aloE_assert(!T->fallowhook, "hook should not allowed.");
		T->fallowhook = true;
		T->top = putstkoff(T, top);
		frame->top = putstkoff(T, ftop);
		frame->fhook = false;
	}
}

#define checkstack(T,n,f) aloD_checkstackaux(T, n, ptrdiff_t _off = getstkoff(T, f); aloG_check(T), f = putstkoff(T, _off))

#define nextframe(T) ((T)->frame->next ?: aloR_pushframe(T))

/**
 ** move result to correct place.
 */
static int moveresults(alo_State T, alo_StkId dest, alo_StkId src, int nresult, int expected) {
	aloE_assert(dest <= src, "illegal stack structure.");
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
		for (int i = 0; i < nresult; ++i) {
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
int aloD_rawcall(alo_State T, alo_StkId fun, int nresult, int* nactual) {
	a_cfun caller;
	alo_TVal* env;
	switch (ttype(fun)) {
	case ALO_VLCF: {
		caller = taslcf(fun);
		env = T->frame->env;
		goto C;
	}
	case ALO_VCCL: {
		alo_CCl* c = tgetccl(fun);
		caller = c->handle;
		env = c->fenv ? c->array : T->frame->env;
		goto C;
	}
		C:
		checkstack(T, ALO_MINSTACKSIZE, fun);
		aframe_t* frame = nextframe(T);
		frame->nresult = nresult;
		frame->name = "<native>";
		frame->c.kfun = NULL;
		frame->c.kctx = 0;
		frame->c.oef = 0;
		frame->fun = fun;
		frame->top = T->top + ALO_MINSTACKSIZE;
		frame->flags = 0;
		frame->env = env;
		T->frame = frame;
		if (T->hookmask & ALO_HMASKCALL) {
			aloD_hook(T, ALO_HMASKCALL, -1);
		}
		int n = *nactual = caller(T);
		aloE_assert(frame->fun + n <= T->top, "no enough elements in stack.");
		aloD_postcall(T, T->top - n, n);
		return true;
	case ALO_VACL: {
		alo_ACl* c = tgetacl(fun);
		alo_Proto* p = c->proto;
		alo_StkId base = fun + 1;
		int actual = T->top - base;
		int off = 0;
		if (p->fvararg) { /* handling for variable arguments */
			int i;
			for (i = 0; i < p->nargs && i < actual; ++i) {
				tsetobj(T, T->top + i, base + i);
			}
			for (; i < p->nargs; ++i) {
				tsetnil(T->top + i);
			}
			base = T->top;
			off = actual + 1;
		}
		else {
			for (int i = actual; i < p->nargs; ++i) {
				tsetnil(base + i);
			}
			off = 1;
		}
		checkstack(T, p->nstack, base);
		T->top = base + p->nstack;
		aframe_t* frame = nextframe(T);
		frame->falo = true;
		frame->name = p->name ? p->name->array : "<unknown>";
		frame->nresult = nresult;
		frame->fun = base - off;
		frame->a.base = base;
		frame->a.pc = p->code;
		frame->env = c->array[0]->p;
		T->frame = frame;
		T->top = frame->top = base + p->nstack;
		if (T->hookmask & ALO_HMASKCALL) {
			aloD_hook(T, ALO_HMASKCALL, -1);
		}
		return false;
	}
	default: { /* not a function, try call meta method */
		/* prepare stack for meta method */
		checkstack(T, 1, fun);
		const alo_TVal* tm = aloT_gettm(T, fun, TM_CALL, true); /* get tagged method */
		if (tm == NULL || !tisfun(tm)) { /* not a function? */
			aloU_rterror(T, "fail to call object with '%s' type", aloT_typenames[ttpnv(fun)]);
		}
		for (alo_StkId p = T->top; p > fun; --p) {
			tsetobj(T, p, p - 1);
		}
		T->top++;
		tsetobj(T, fun, tm);
		return aloD_precall(T, fun, nresult);
	}
	}
}

int aloD_precall(alo_State T, alo_StkId fun, int nresult) {
	int dummy;
	return aloD_rawcall(T, fun, nresult, &dummy);
}

void aloD_postcall(alo_State T, alo_StkId ret, int nresult) {
	aframe_t* const frame = T->frame;
	if (T->hookmask & ALO_HMASKRET) {
		ptrdiff_t r = getstkoff(T, ret);
		aloD_hook(T, ALO_HMASKRET, -1);
		ret = putstkoff(T, r);
	}
	alo_StkId res = frame->fun;
	int expected = frame->nresult;
	T->frame = frame->prev; /* back to previous frame */
	aloR_closeframe(T, frame); /* close current frame */
	/* move result to proper place */
	moveresults(T, res, ret, nresult, expected);
}

#define EXTRAERRCALL (ALO_MAXCCALL >> 3)

static void stackerror(alo_State T) {
	if (T->nccall == ALO_MAXCCALL) {
		aloU_rterror(T, "C stack overflow");
	}
	else if (T->nccall >= ALO_MAXCCALL + EXTRAERRCALL) {
		aloU_ererror(T, "error in error handling: C stack overflow"); /* too many stack while handling error */
	}
}

void aloD_call(alo_State T, alo_StkId fun, int nresult) {
	if (T->nccall >= ALO_MAXCCALL)
		stackerror(T);
	T->nccall++;
	aframe_t* frame = T->frame;
	if (!aloD_precall(T, fun, nresult)) {
		int oldact = frame->fact;
		frame->fact = false;
		aloV_invoke(T, false); /* invoke Alopecurus function */
		frame->fact = oldact;
	}
	T->nccall--;
}

void aloD_callnoyield(alo_State T, alo_StkId fun, int nresult) {
	T->nxyield++;
	aloD_call(T, fun, nresult);
	T->nxyield--;
}

/* protected call information */
struct PCI {
	alo_StkId fun;
	int nres;
};

static void pcall_unsafe(alo_State T, void* context) {
	struct PCI* pci = aloE_cast(struct PCI*, context);
	aloD_callnoyield(T, pci->fun, pci->nres);
}

int aloD_pcall(alo_State T, alo_StkId fun, int nres, ptrdiff_t ef) {
	/* store current frame state */
	aframe_t* const frame = T->frame;
	uint16_t nxyield = T->nxyield;
	a_byte allowhook = T->fallowhook;
	ptrdiff_t oldef = T->errfun;
	ptrdiff_t oldtop = getstkoff(T, fun);
	T->errfun = ef;
	int status = aloD_prun(T, pcall_unsafe, &(struct PCI) { fun, nres });
	if (status != ALO_STOK) {
		alo_StkId top = putstkoff(T, oldtop);
		aloF_close(T, top);
		tsetobj(T, top++, T->top - 1);
		T->top = top;
		T->frame = frame;
		T->fallowhook = allowhook;
		T->nxyield = nxyield;
		aloD_shrinkstack(T);
	}
	else {
		aloE_assert(T->frame == frame, "frame changed");
	}
	T->errfun = oldef;
	return status;
}

static void postpcc(alo_State T, int status) {
	aframe_t* frame = T->frame;

	aloE_assert(frame->c.kfun != NULL && T->nxyield == 0, "the continuation is not available.");
	aloE_assert(frame->fypc || status != ALO_STYIELD, "error can only be caught by protector.");

	aloD_adjustresult(T, frame->nresult);
	int n = (*frame->c.kfun)(T, status, frame->c.kctx);
	api_checkelems(T, n);
	aloD_postcall(T, T->top - n, n);
}

/**
 * execute remained continuations.
 */
static void unroll(alo_State T) {
	aframe_t* frame;
	while ((frame = T->frame) != &T->base_frame) {
		if (frame->falo) { /* is yielded in script? */
			aloV_invoke(T, true); /* call with finished previous */
		}
		else {
			postpcc(T, ALO_STYIELD);
		}
	}
}

static void unroll_unsafe(alo_State T, void* context) {
	int status = *aloE_cast(int*, context);
	postpcc(T, status);
	unroll(T);
}

static aframe_t* rewind_protector(alo_State T) {
	aframe_t* frame = T->frame;
	do {
		if (frame->fypc) {
			return frame;
		}
	}
	while ((frame = frame->prev));
	return NULL;
}

/**
 * attempt to recover the function in error by protector.
 */
static int recover(alo_State T) {
	/* find yieldable protector */
	aframe_t* frame = rewind_protector(T);
	if (frame == NULL)
		return false;
	alo_StkId oldtop = frame->top;
	aloF_close(T, frame->top);
	tsetobj(T, oldtop - 1, frame->top - 1);
	T->frame = frame;
	T->top = frame->top;
	T->nxyield = 0;
	return true;
}

static void resume_unsafe(alo_State T, void* context) {
	int narg = *aloE_cast(int*, context);
	aframe_t* frame = T->frame;
	if (T->frame == &T->base_frame) {
		if (!aloD_precall(T, T->top - narg - 1, ALO_MULTIRET))
			aloV_invoke(T, false);
	}
	else {
		if (frame->falo) { /* is yielded in script? */
			aloV_invoke(T, true); /* call with finished previous */
		}
		else {
			if (frame->c.kfun) { /* does frame have a continuation? */
				narg = (*frame->c.kfun)(T, ALO_STYIELD, frame->c.kctx);
				api_checkelems(T, narg);
			}
			aloD_postcall(T, T->top - narg, narg);
		}
		unroll(T);
	}
}

#define resume_error(T,msg,narg) ({ T->top -= (narg); tsetstr(T, T->top, aloS_newl(T, msg)); T->top++; -1; })

int alo_resume(alo_State T, alo_State from, int narg) {
	Gd(T);
	aloi_check(T, from->g == G, "two coroutine from different state.");
	aloi_check(T, G->trun == from, "the coroutine is not running.");

	if (T == G->tmain) { /* the main thread can not resumed */
		return resume_error(from, "the main coroutine is not resumable.", narg);
	}
	else if (T->status != ALO_STYIELD) { /* check coroutine status */
		if (T->status == ALO_STOK)
			return resume_error(from, "cannot resume a non-suspended coroutine.", narg);
		else
			goto dead;
	}
	else if (T->frame == &T->base_frame && T->top != T->base_frame.fun + 2) { /* check function not called yet */
		dead:
		return resume_error(from, "the coroutine is already dead.", narg);
	}
	api_checkelems(T, narg);

	int oldnxy = T->nxyield;
	T->nccall = from->nccall + 1;
	if (T->nccall >= ALO_MAXCCALL) {
		stackerror(T);
	}
	G->trun = T;
	T->caller = from;
	T->nxyield = 0;
	T->status = ALO_STOK;
	aloi_resumethread(T, narg); /* call user's resume action */
	int status = aloD_prun(T, resume_unsafe, &narg);
	if (status != ALO_STOK) { /* does function invoke to the end? */
		if (status != ALO_STYIELD) { /* error occurred? */
			while (iserrorstate(status) && recover(T)) { /* try to recover the error by yieldable error protector */
				status = aloD_prun(T, unroll_unsafe, &status);
			}
			if (iserrorstate(status)) { /* is error unrecoverable? */
				T->frame->top = T->top;
			}
			else aloE_assert(status == T->status, "status mismatched.");
		}
	}
	T->status = status == ALO_STOK ? ALO_STYIELD : aloE_byte(status); /* mark thread status */
	G->trun = from;
	T->nccall--;
	T->nxyield = oldnxy;
	aloE_assert(T->nccall == from->nccall, "C call depth mismatched.");
	return status;
}

void alo_yieldk(alo_State T, int nres, a_kfun kfun, a_kctx kctx) {
	aframe_t* frame = T->frame;
	api_checkelems(T, nres);
	aloE_assert(T == T->g->trun, "the current coroutine is not running.");
	if (T->nxyield > 0) {
		aloU_rterror(T, T->caller != NULL ?
				"attempt to yield across a C-call boundary." :
				"attempt to yield from a outside coroutine.");
	}
	T->status = ALO_STYIELD;
	aloi_yieldthread(T, nres); /* call user's yield procedure */
	/* user yield failed, call default yield procedure */
	if (frame->falo) {
		aloU_rterror(T, "can not yield in hook.");
	}
	aloE_assert(T->label, "missing resume label.");
	if ((frame->c.kfun = kfun)) { /* protector present? */
		/* settle protector */
		frame->c.kctx = kctx;
		frame->fypc = true;
		/* push new frame for yield procedure */
		frame = nextframe(T);
		frame->nresult = ALO_MULTIRET;
		frame->name = "<yield>";
		frame->flags = 0;
		T->frame = frame;
	}
	frame->c.kfun = NULL;
	frame->c.kctx = 0;
	frame->c.oef = 0;
	frame->fun = T->top - nres;
	frame->top = T->top;
	T->caller = NULL; /* remove caller */
	aloi_yield(T, *T->label); /* yield coroutine */
}

int alo_status(alo_State T) {
	return T->status;
}

int alo_isyieldable(alo_State T) {
	return T->nxyield == 0;
}
