/*
 * astate.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

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

const aver_t aloR_version = { ALO_VERSION_NUM };

typedef struct {
	athread_t t;
	aglobal_t g;
} TG;

static void init_stack(astate T, athread_t* thread) {
	aloM_clsb(thread->stack, thread->stacksize);
	aloM_newb(T, thread->stack, thread->stacksize, ALO_BASESTACKSIZE);
	thread->top = thread->stack + 1;
	/* initialize base frame stack */
	thread->base_frame.fun = thread->stack;
	thread->base_frame.top = thread->top + ALO_MINSTACKSIZE;
	tsetnil(thread->base_frame.fun); /* no function for base stack */
}

static void init_registry(astate T) {
	Gd(T);
	atable_t* registry = aloH_new(T);
	G->registry = tnewref(registry, ALO_TTABLE);
	aloH_ensure(T, registry, ALO_GLOBAL_INITIALIZESIZE);
}

static void initialize(astate T, __attribute__((unused)) void* context) {
	init_stack(T, T);
	init_registry(T);
	aloS_init(T);
	aloT_init(T);
	aloX_init(T);
	aloU_init(T);
}

astate alo_newstate(aalloc alloc, void* ctx) {
	TG* tg = aloE_cast(TG*, alloc(ctx, NULL, 0, sizeof(TG)));
	if (tg == NULL)
		return NULL;
	athread_t* T = &tg->t;
	aglobal_t* G = &tg->g;
	G->alloc = alloc;
	G->context = ctx;
	G->seed = time(NULL) ^ ALO_MASKSEED;
	G->mbase = sizeof(TG);
	G->mdebt = 0;
	G->mestimate = sizeof(TG);
	G->mtraced = 0;
	G->gnormal = G->gfixed = G->gfnzable = G->gtobefnz = NULL;
	G->gcstepmul = ALO_GCSTEPMUL;
	G->gcpausemul = ALO_GCPAUSEMUL;
	G->panic = NULL;
	G->tmain = T;
	G->trun = T;
	G->smerrmsg = NULL;
	G->sempty = NULL;
	G->version = aloR_version;
	G->whitebit = aloG_white1;
	G->gcstep = GCStepPause;
	G->gckind = GCKindNormal;
	G->gcrunning = false;
	tsetnil(&G->registry); /* registry = nil */
	G->flags = 0;
	G->itable.length = 0;
	G->itable.capacity = 0;
	G->itable.array = NULL;
	G->ctable.length = 0;
	G->ctable.capacity = 0;
	G->ctable.array = NULL;
	T->gcprev = NULL;
	T->tt = ALO_TTHREAD;
	T->mark = aloG_white1;
	T->status = ThreadStateRun;
	T->caller = NULL;
	T->g = G;
	T->frame = &T->base_frame;
	T->berrno = 0;
	T->nccall = 0;
	T->nframe = 1;
	T->nxyield = 0;
	T->label = NULL;
	T->flags = 0;
	T->base_frame.name = "<start>";
	T->base_frame.next = T->base_frame.prev = NULL;
	T->base_frame.fun = T->base_frame.top = NULL;
	T->base_frame.flags = 0;
	T->base_frame.c.kfun = NULL;
	T->base_frame.c.ctx = NULL;
	T->captures = NULL;

	/* run initializer with memory allocation, if failed, delete thread directly */
	if (aloD_prun(T, initialize, NULL) != ThreadStateRun) {
		alo_deletestate(T);
		return NULL;
	}

	G->fgc = true; /* enable GC */
	return T;
}

/**
 ** close frame and free all references in the frame.
 */
void aloR_closeframe(astate T, aframe_t* frame) {
	frame->flags = 0; /* clear frame flags, the frame will regard as a C functions calling frame now. */
	/* make old context unreachable */
	frame->c.kfun = NULL;
}

static void destory_thread(astate T, athread_t* v, int close) {
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
		acap *c, *n = T->captures;
		while ((c = n)) {
			n = c->prev;
			aloM_delo(T, c); /* delete capture */
		}
	}
	aloM_delb(T, v->stack, v->stacksize); /* clear stack */
}

void alo_deletestate(astate rawT) {
	Gd(rawT);
	astate T = G->tmain;
	aloG_clear(T); /* clean all objects */
	destory_thread(T, T, false);
	aloE_assert(totalmem(G) == sizeof(TG), "memory leaked");
	G->alloc(G->context, T, sizeof(TG), 0);
}

/**
 ** delete thread.
 */
void aloR_deletethread(astate T, athread_t* v) {
	destory_thread(T, v, true);
	aloM_delo(T, v);
}

/**
 ** get top of frames.
 */
aframe_t* aloR_topframe(astate T) {
	aframe_t* frame = T->frame;
	while (frame->next) {
		frame = frame->next;
	}
	return frame;
}

aframe_t* aloR_pushframe(astate T) {
	aframe_t* oldframe = T->frame;
	aloE_assert(oldframe->next == NULL, "not reach top of frame yet.");
	aframe_t* newframe = aloM_newo(T, aframe_t);
	newframe->prev = oldframe;
	oldframe->next = newframe;
	newframe->next = NULL;
	T->nframe++;
	return newframe;
}
