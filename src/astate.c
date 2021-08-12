/*
 * astate.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#define ASTATE_C_
#define ALO_CORE

#include "astr.h"
#include "atab.h"
#include "afun.h"
#include "astate.h"
#include "ameta.h"
#include "adebug.h"
#include "agc.h"
#include "ado.h"
#include "alex.h"
#include "alo.h"

#include <time.h>
#include <inttypes.h>

ALO_VDEF const aver_t aloR_version = { ALO_VERSION_NUM };

/**
 ** the real thread allocated for each thread.
 */
typedef struct {
	a_byte extra[ALO_THREAD_EXTRASPACE]; /* extra thread space for user uses */
	alo_Thread state; /* the actual thread object */
} TX;

/* get raw thread pointer from thread object */
#define getraw(T) aloE_cast(TX*, aloE_cast(a_byte*, T) - offsetof(TX, state))

typedef struct {
	TX t;
	aglobal_t g;
} TG;

static void init_stack(alo_State T, alo_Thread* thread) {
	aloM_newb(T, thread->stack, thread->stacksize, ALO_BASESTACKSIZE);
	thread->top = thread->stack + 1; /* reserve for function slot */
	/* initialize base frame stack */
	thread->base_frame.fun = thread->stack;
	thread->base_frame.top = thread->top + ALO_MINSTACKSIZE;
	tsetnil(thread->base_frame.fun); /* no function for base stack */
}

static void init_registry(alo_State T) {
	Gd(T);
	atable_t* registry = aloH_new(T);
	tsettab(T, &G->registry, registry);
	aloH_ensure(T, registry, ALO_GLOBAL_INITIALIZESIZE);
}

static void initialize(alo_State T, __attribute__((unused)) void* context) {
	init_stack(T, T);
	init_registry(T);
	aloS_init(T);
	aloT_init(T);
	aloX_init(T);
	aloi_openthread(T, NULL);
}

static a_hash newseed(alo_State T) {
	/* first random sequence generate */
	uintptr_t pool[4] = {
		aloE_addr(T),
		aloE_addr(pool),
		aloE_addr(alo_newstate),
		aloE_addr(aloO_tnil)
	};
	/* use time as raw seed to generate seed */
	return aloS_rhash(aloE_cast(const char*, pool), sizeof(pool), time(NULL) ^ ALO_MASKSEED);
}

static void preinit(alo_State T, aglobal_t* G) {
	T->status = ALO_STYIELD;
	T->caller = NULL;
	T->g = G;
	T->frame = &T->base_frame;
	T->nccall = 0;
	T->nframe = 1;
	T->nxyield = 0;
	T->label = NULL;
	T->flags = 0;
	T->errfun = 0;
	T->base_frame.name = "<start>";
	T->base_frame.next = T->base_frame.prev = NULL;
	T->base_frame.fun = T->base_frame.top = NULL;
	T->base_frame.flags = 0;
	T->base_frame.env = &G->registry;
	T->base_frame.c.kfun = NULL;
	T->base_frame.c.kctx = 0;
	T->captures = NULL;
	T->stack = NULL;
	T->top = NULL;
	T->stacksize = 0;
	T->hook = NULL;
	T->hookmask = 0;
	T->fallowhook = true;
	T->memstk.top = basembuf(T);
	T->memstk.ptr = NULL;
	T->memstk.cap = 0;
	T->memstk.len = 0;
}

alo_State alo_newstate(alo_Alloc alloc, void* ctx) {
	TG* tg = aloE_cast(TG*, alloc(ctx, NULL, 0, sizeof(TG)));
	if (tg == NULL)
		return NULL;
	alo_Thread* T = &tg->t.state;
	aglobal_t* G = &tg->g;
	G->alloc = alloc;
	G->context = ctx;
	G->seed = newseed(T);
	G->mbase = sizeof(TG);
	G->mdebt = 0;
	G->mestimate = sizeof(TG);
	G->mtraced = 0;
	G->gnormal = G->gfixed = G->gfnzable = G->gtobefnz = NULL;
	G->gcstepmul = ALO_DEFAULT_GCSTEPMUL;
	G->gcpausemul = ALO_DEFAULT_GCPAUSEMUL;
	G->panic = NULL;
	G->tmain = T;
	G->trun = T;
	G->smerrmsg = NULL;
	G->sempty = NULL;
	G->version = &aloR_version;
	G->whitebit = aloG_white1;
	G->gcstep = GCStepPause;
	G->gckind = GCKindNormal;
	G->gcrunning = false;
	tsetnil(&G->registry); /* registry = nil */
	G->flags = 0;
	G->itable.length = 0;
	G->itable.capacity = 0;
	G->itable.array = NULL;
	T->gcprev = NULL;
	T->tt = ALO_TTHREAD;
	T->mark = aloG_white1;
	preinit(T, G);
	/* run initializer with memory allocation, if failed, delete thread directly */
	if (aloD_prun(T, initialize, NULL) != ALO_STOK) {
		alo_deletestate(T);
		return NULL;
	}

	aloE_log("new alo state initialized, seed: %"PRId64, G->seed);

	G->fgc = true; /* enable GC */
	T->status = ALO_STOK;
	return T;
}

alo_State alo_newthread(alo_State T) {
	Gd(T);
	aloG_check(T);
	api_checkslots(T, 1);
	alo_Thread* thread = &aloM_newo(T, TX)->state;
	preinit(thread, G);
	aloG_register(T, thread, ALO_TTHREAD);
	tsetthr(T, api_incrtop(T), thread);
	aloG_barrierback(T, T, thread);
	init_stack(T, thread);
	aloi_openthread(thread, T);
	return thread;
}

/**
 ** close frame and free all references in the frame.
 */
void aloR_closeframe(__attribute__((unused)) alo_State T, aframe_t* frame) {
	frame->flags = 0; /* clear frame flags, the frame will regard as a C functions calling frame now. */
	/* make old context unreachable */
	frame->c.kfun = NULL;
}

static void destory_thread(alo_State T, alo_Thread* v, int close) {
	aframe_t* frame = aloR_topframe(v);
	while (frame->prev) { /* true if frame is not base frame */
		aframe_t* current = frame;
		frame = frame->prev; /* move to previous frame */
		aloR_closeframe(T, current);
		aloM_delo(T, current); /* delete current frame */
	}
	if (close) {
		aloF_close(T, T->stack);
	}
	else {
		alo_Capture *c, *n = T->captures;
		while ((c = n)) {
			n = c->prev;
			aloM_delo(T, c); /* delete capture */
		}
	}
	aloM_delb(T, v->stack, v->stacksize); /* clear stack */
	aloM_free(T, v->memstk.ptr, v->memstk.cap);
}

void alo_deletestate(alo_State rawT) {
	Gd(rawT);
	alo_State T = G->tmain;
	aloi_closethread(T);
	aloG_clear(T); /* clean all objects */
	destory_thread(T, T, false);
	aloE_assert(totalmem(G) == sizeof(TG), "memory leaked");
	G->alloc(G->context, getraw(T), sizeof(TG), 0);
}

/**
 ** delete thread.
 */
void aloR_deletethread(alo_State T, alo_Thread* v) {
	aloi_closethread(T);
	destory_thread(T, v, true);
	aloM_delo(T, getraw(v));
}

/**
 ** get top of frames.
 */
aframe_t* aloR_topframe(alo_State T) {
	aframe_t* frame = T->frame;
	while (frame->next) {
		frame = frame->next;
	}
	return frame;
}

aframe_t* aloR_pushframe(alo_State T) {
	aframe_t* oldframe = T->frame;
	aloE_assert(oldframe->next == NULL, "not reach top of frame yet.");
	aframe_t* newframe = aloM_newo(T, aframe_t);
	newframe->prev = oldframe;
	oldframe->next = newframe;
	newframe->next = NULL;
	T->nframe++;
	return newframe;
}
