/*
 * ado.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#include "astr.h"
#include "afun.h"
#include "abuf.h"
#include "agc.h"
#include "adebug.h"
#include "avm.h"
#include "ado.h"
#include "alo.h"

#include <stdlib.h>

#define iserrorstate(status) ((status) > ThreadStateYield)

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
		else if (newsize > ALO_MAXSTACKSIZE) {
			newsize = ALO_MAXSTACKSIZE;
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

static size_t usedstackcount(astate T) {
	aframe_t* frame = T->frame;
	askid_t lim = T->top;
	while (frame) {
		if (lim < frame->top) {
			lim = frame->top;
		}
		frame = frame->prev;
	}
	return lim - T->stack;
}

static void shrinkframe(astate T) {
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
			aframe_t* next = frame->next->next->next;
			aloM_delo(T, frame->next);
			frame->next = next;
			if (next == NULL)
				break;
			next->prev = frame;
			frame = next;
		}
	}
}

void aloD_shrinkstack(astate T) {
	if (T->memstk.cap >= (ALO_MBUF_SHTLEN * 16 * 2) && T->memstk.len <= T->memstk.cap / 4) { /* memory buffer too big? */
		size_t newcap = T->memstk.cap / 2;
		void* newmem = aloM_realloc(T, T->memstk.buf, T->memstk.cap, newcap);
		aloE_xassert(newmem == T->memstk.buf);
		aloE_void(newmem);
	}
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
int aloD_prun(astate T, apfun fun, void* ctx) {
	uint16_t nccall = T->nccall, nxyield = T->nxyield;
	ambuf_t* mbuf = T->memstk.top; /* store memory buffer */
	ajmp_t label;
	label.prev = T->label;
	label.status = ThreadStateRun;
	T->label = &label;
	if (setjmp(label.buf) == 0) {
		fun(T, ctx);
	}
	T->label = label.prev;
	T->nccall = nccall;
	T->nxyield = nxyield;
	T->memstk.top = mbuf; /* revert memory buffer */
	return label.status;
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

void aloD_hook(astate T, int event, int line) {
#if ALO_RUNTIME_DEBUG
	ahfun hook = T->hook;
	if (hook && T->fallowhook) {
		aframe_t* frame = T->frame;
		ptrdiff_t top = getstkoff(T, T->top);
		ptrdiff_t ftop = getstkoff(T, frame->top);
		aframeinfo_t info;
		info.event = event;
		info.line = line;
		info._frame = frame;
		aloD_checkstack(T, ALO_MINSTACKSIZE);
		T->frame->top = T->top + ALO_MINSTACKSIZE;
		T->fallowhook = false;
		frame->fhook = true;

		hook(T, &info);

		aloE_assert(!T->fallowhook, "hook should not allowed.");
		T->fallowhook = true;
		T->top = putstkoff(T, top);
		frame->top = putstkoff(T, ftop);
		frame->fhook = false;
	}
#endif
}

#define checkstack(T,n,f) aloD_checkstackaux(T, n, ptrdiff_t _off = getstkoff(T, f); aloG_check(T), f = putstkoff(T, _off))

#define nextframe(T) ((T)->frame->next ?: aloR_pushframe(T))

/**
 ** move result to correct place.
 */
static int moveresults(astate T, askid_t dest, askid_t src, int nresult, int expected) {
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
int aloD_rawcall(astate T, askid_t fun, int nresult, int* nactual) {
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
			if (T->hookmask & ALO_HMASKCALL) {
				aloD_hook(T, ALO_HMASKCALL, -1);
			}
			int n = *nactual = caller(T);
			aloE_assert(frame->fun + n <= T->top, "no enough elements in stack.");
			aloD_postcall(T, T->top - n, n);
		}
		return true;
	case ALO_TACL: {
		aproto_t* p = tgetclo(fun)->a.proto;
		askid_t base = fun + 1;
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
		const atval_t* tm = aloT_gettm(T, fun, TM_CALL, true); /* get tagged method */
		if (tm == NULL || !ttisfun(tm)) { /* not a function? */
			aloU_rterror(T, "fail to call object with '%s' type", aloT_typenames[ttpnv(fun)]);
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

int aloD_precall(astate T, askid_t fun, int nresult) {
	int dummy;
	return aloD_rawcall(T, fun, nresult, &dummy);
}

void aloD_postcall(astate T, askid_t ret, int nresult) {
	aframe_t* const frame = T->frame;
#if ALO_RUNTIME_DEBUG
	if (T->hookmask & ALO_HMASKRET) {
		ptrdiff_t r = getstkoff(T, ret);
		aloD_hook(T, ALO_HMASKRET, -1);
		ret = putstkoff(T, r);
	}
#endif
	askid_t res = frame->fun;
	int expected = frame->nresult;
	T->frame = frame->prev; /* back to previous frame */
	aloR_closeframe(T, frame); /* close current frame */
	/* move result to proper place */
	moveresults(T, res, ret, nresult, expected);
}

#define EXTRAERRCALL (ALO_MAXCCALL >> 3)

static void stackerror(astate T) {
	if (T->nccall == ALO_MAXCCALL) {
		aloU_rterror(T, "C stack overflow");
	}
	else if (T->nccall >= ALO_MAXCCALL + EXTRAERRCALL) {
		aloD_throw(T, ThreadStateErrError); /* too many stack while handling error */
	}
}

void aloD_call(astate T, askid_t fun, int nresult) {
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

void aloD_callnoyield(astate T, askid_t fun, int nresult) {
	T->nxyield++;
	aloD_call(T, fun, nresult);
	T->nxyield--;
}

static void postpcc(astate T, int status) {
	aframe_t* frame = T->frame;

	aloE_assert(frame->c.kfun != NULL && T->nxyield == 0, "the continuation is not available.");
	aloE_assert(frame->fypc || status != ThreadStateYield, "error can only be caught by protector.");

	aloD_adjustresult(T, frame->nresult);
	int n = (*frame->c.kfun)(T, ThreadStateYield, frame->c.ctx);
	api_checkelems(T, n);
	aloD_postcall(T, T->top - n, n);
}

/**
 * execute remained continuations.
 */
static void unroll(astate T) {
	aframe_t* frame;
	while ((frame = T->frame) != &T->base_frame) {
		if (frame->falo) { /* is yielded in script? */
			aloV_invoke(T, true); /* call with finished previous */
		}
		else {
			postpcc(T, ThreadStateYield);
		}
	}
}

static void unroll_unsafe(astate T, void* context) {
	int status = *aloE_cast(int*, context);
	postpcc(T, status);
	unroll(T);
}

static aframe_t* rewind_protector(astate T) {
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
static int recover(astate T, int status) {
	/* find yieldable protector */
	aframe_t* frame = rewind_protector(T);
	if (frame == NULL)
		return false;
	askid_t oldtop = frame->top;
	aloF_close(T, frame->top);
	tsetobj(T, oldtop - 1, frame->top - 1);
	T->frame = frame;
	T->top = frame->top;
	T->nxyield = 0;
	return true;
}

static void resume_unsafe(astate T, void* context) {
	int narg = *aloE_cast(int*, context);
	aframe_t* frame = T->frame;
	T->status = ThreadStateRun;
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
				narg = (*frame->c.kfun)(T, ThreadStateYield, frame->c.ctx);
				api_checkelems(T, narg);
			}
			aloD_postcall(T, T->top - narg, narg);
		}
		unroll(T);
	}
}

static int resume_error(astate T, const char* msg, int narg) {
	T->top -= narg;
	aloU_pushmsg(T, msg); /* push message to the top */
	return ThreadStateErrRuntime;
}

int alo_resume(astate T, astate from, int narg) {
	Gd(T);
	api_check(T, from->g == G, "two coroutine from different state.");
	api_check(T, G->trun == from, "the coroutine is not running.");
	if (T == G->tmain) { /* the main thread can not resumed */
		return resume_error(T, "the main coroutine is not resumable.", narg);
	}
	else if (T->status != ThreadStateYield) { /* check coroutine status */
		return resume_error(T, T->status == ThreadStateRun ?
				"cannot resume a non-suspended coroutine." :
				"the coroutine is already dead.", narg);
	}
	else if (T->frame == &T->base_frame && T->top != T->base_frame.fun + 2) { /* check function not called yet */
		return resume_error(T, "the coroutine is already dead.", narg);
	}
	api_checkelems(T, narg);

	int oldnxy = T->nxyield;
	T->nccall = from->nccall + 1;
	if (T->nccall >= ALO_MAXCCALL) {
		stackerror(T);
	}
	T->nxyield = 0;

	T->caller = from;
	G->trun = T;
	int status = aloD_prun(T, resume_unsafe, &narg);
	if (status == ThreadStateRun) { /* does function invoke to the end? */
		T->status = ThreadStateYield; /* yield coroutine */
	}
	else if (status != ThreadStateYield) { /* error occurred? */
		while (iserrorstate(status) && recover(T, status)) { /* try to recover the error by yieldable error protector */
			status = aloD_prun(T, unroll_unsafe, &status);
		}
		if (iserrorstate(status)) { /* is error unrecoverable? */
			T->status = aloE_byte(status);
			T->frame->top = T->top;
		}
		else aloE_assert(status == T->status, "status mismatched.");
	}
	G->trun = from;
	T->nccall--;
	T->nxyield = oldnxy;
	aloE_assert(T->nccall == from->nccall, "C call depth mismatched.");
	return status;
}

void alo_yieldk(astate T, int nres, akfun kfun, void* kctx) {
	aframe_t* frame = T->frame;
	api_checkelems(T, nres);
	Gd(T);
	aloE_void(G);
	aloE_assert(T == G->trun, "the current coroutine is not running.");
	if (T->nxyield > 0) {
		aloU_rterror(T, T->caller != NULL ?
				"attempt to yield across a C-call boundary." :
				"attempt to yield from a outside coroutine.");
	}
	T->status = ThreadStateYield;
	if (frame->falo) { //TODO
		aloU_rterror(T, "not implemented yet.");
	}
	else {
		aloE_assert(T->label, "missing resume label.");
		if ((frame->c.kfun = kfun)) { /* protector present? */
			/* settle protector */
			frame->c.ctx = kctx;
		}
		/* push new frame for 'yield' function */
		frame = nextframe(T);
		frame->nresult = ALO_MULTIRET;
		frame->name = "<yield>";
		frame->c.kfun = NULL;
		frame->c.ctx = NULL;
		frame->fun = T->top - nres;
		frame->top = T->top;
		frame->flags = 0;
		T->label->status = ThreadStateYield;
		longjmp(T->label->buf, 1);
	}
}

int alo_status(astate T) {
	return T->status;
}

int alo_isyieldable(astate T) {
	return T->nxyield == 0;
}
