/*
 * agc.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#define AGC_C_
#define ALO_CORE

#include "aop.h"
#include "astr.h"
#include "atup.h"
#include "atab.h"
#include "afun.h"
#include "astate.h"
#include "abuf.h"
#include "adebug.h"
#include "ameta.h"
#include "agc.h"
#include "avm.h"
#include "ado.h"

#include <string.h>

/* the scale of GC multiplier */
#define GCUNIT 100

/**
 ** cost for each GC step.
 */

#define SWEEP_COST (sizeof(alo_Str) / 4)
#define CALLFIN_COST (sizeof(alo_Str) / 2)
#define MAXSWEEPCOUNT (ALO_MINWORK / SWEEP_COST + 1)

#define iscoloring(G) ((G)->gcstep <= GCStepAtomic)
#define issweeping(G) ((G)->gcstep >= GCStepSwpNorm && (G)->gcstep <= GCStepSwpFnz)

/**
 ** color changing macros.
 */

#define white2gray(g) ((g)->mark &= ~aloG_white)
#define gray2black(g) ((g)->mark |= aloG_black)
#define black2gray(g) ((g)->mark &= ~aloG_black)
#define setcolor(g,color) ((g)->mark = ((g)->mark & ~aloG_color) | (color))
#define setwhite(G,g) ((g) ? aloE_void(setcolor(g, aloG_whitebit(G))) : (void) 0)

/* link object to GC list. */
#define linkto(l,g) aloE_void((g)->gclist = l, l = (alo_Obj*) (g))

static alo_Obj** tailof_(alo_Obj** p) {
	while (*p)
		p = &(*p)->gcprev;
	return p;
}

#define tailof(l) tailof_(&(l))

/**
 ** registration functions.
 */
void aloG_register_(alo_State T, alo_Obj* g, a_byte tt) {
	Gd(T);
	g->tt = tt;
	g->mark = G->whitebit;
	g->gcprev = G->gnormal;
	G->gnormal = g;
}

void aloG_fix_(alo_State T, alo_Obj* g) {
	Gd(T);
	aloE_assert(G->gnormal == g, "object should in head of normal object list.");
	setcolor(g, 0); /* make object gray forever */
	g->mark = (g->mark & ~aloG_color) | aloG_fixed; /* fix object */
	/* link object in fixed list. */
	G->gnormal = g->gcprev;
	g->gcprev = G->gfixed;
	G->gfixed = g;
}

void aloG_unfix_(alo_State T, alo_Obj* g) {
	Gd(T);
	aloE_assert(G->gfixed == g, "object should in head of fixed object list.");
	setwhite(G, g); /* make object white again */
	g->mark &= ~aloG_fixed; /* release object */
	/* link object in normal list. */
	G->gfixed = g->gcprev;
	g->gcprev = G->gnormal;
	G->gnormal = g;
}

/**
 ** mark and propagate functions.
 */

#define markpg_(G,g) aloE_check(aloG_iswhite(g), "object should be white", white2gray(g), linkto((G)->gray, g))

#define markpg(G,g) if (aloG_iswhite(g)) markpg_(G, g)

#define marknpg(G,g) if ((g) && aloG_iswhite(g)) markpg_(G, g)

static void markg_(aglobal_t* G, alo_Obj* g) {
	aloE_assert(aloG_iswhite(g), "object should be white.");
	white2gray(g);
	switch (ttpnv(g)) {
	case ALO_TSTR: {
		alo_Str* v = g2s(g);
		G->mtraced += astrsize(v);
		gray2black(g);
		break;
	case ALO_TUSER: {
		alo_User* v = g2r(g);
		if (v->metatable == NULL) {
			G->mtraced += ardsize(v);
			gray2black(g);
		}
		else {
			linkto(G->gray, v);
		}
		break;
	}
	default: {
		linkto(G->gray, g);
		break;
	}
	}
	}
}

#define markg(G,g) if (aloG_iswhite(g)) markg_(G, r2g(g))

#define markng(G,g) if ((g) && aloG_iswhite(g)) markg_(G, r2g(g))

#define markt(G,t) ({ alo_TVal* it = (t); if (tisref(it)) markg(G, tasobj(it)); })

void aloG_barrier_(alo_State T, alo_Obj* owner, alo_Obj* value) {
	Gd(T);
	aloE_assert(aloG_isblack(owner) && aloG_iswhite(value), "illegal object state.");
	if (!iscoloring(G)) {
		black2gray(owner);
	}
	else {
		markg_(G, value);
	}
}

void aloG_barrierback_(alo_State T, alo_Obj* owner) {
	Gd(T);
	aloE_assert(aloG_isblack(owner), "illegal object state.");
	black2gray(owner);
	linkto(G->grayagain, owner);
}

static int checkmark(aglobal_t* G, alo_TVal* o) {
	if (!tisref(o)) {
		return false;
	}
	else switch (ttpnv(o)) {
	case ALO_TSTR: { /* string are values */
		markg(G, tasstr(o));
		return false;
	}
	case ALO_TTUPLE: { /* tuple are values */
		markg(G, tastup(o));
		return false;
	}
	case ALO_TUSER: {
		alo_User* v = tgetrdt(o);
		if (strchr(aloT_gmode(G, v->metatable), 'x')) { /* mode 'x' means value type */
			markpg(G, v);
			return false;
		}
		break;
	}
	}
	return aloG_iswhite(tasobj(o));
}

static int scan_wtable(aglobal_t*, atable_t*);

static size_t propagate_list(aglobal_t* G, alo_List* v) {
	marknpg(G, v->metatable);
	if (strchr(aloT_gmode(G, v->metatable), 'v')) { /* weak value reference? */
		if (G->gcstep == GCStepPropagate) {
			black2gray(v);
			linkto(G->grayagain, v);
		}
		else {
			linkto(G->weakv, v);
		}
	}
	else {
		for (size_t i = 0; i < v->length; ++i) {
			markt(G, &v->array[i]);
		}
	}
	return sizeof(alo_List) + v->capacity * sizeof(alo_TVal);
}

static size_t propagate_table(aglobal_t* G, atable_t* v) {
	marknpg(G, v->metatable);
	a_cstr mode = aloT_gmode(G, v->metatable);
	int wk = !!strchr(mode, 'k'), wv = !!strchr(mode, 'v');
	if (wk || wv) { /* weak key or value reference? */
		black2gray(v);
		if (!wk) {
			int clear = false;
			for (size_t i = 0; i < v->capacity; ++i) {
				alo_Entry* entry = v->array + i;
				if (!tisnil(amkey(entry))) {
					markt(G, amkey(entry));
					if (!clear && checkmark(G, amval(entry))) { /* if there is white object, mark it to be cleared */
						clear = true;
					}
				}
			}
			if (G->gcstep == GCStepPropagate) {
				linkto(G->grayagain, v);
			}
			else if (clear) {
				linkto(G->weakv, v);
			}
		}
		else if (!wv) {
			scan_wtable(G, v);
		}
		else {
			linkto(G->weaka, v);
		}
	}
	else for (size_t i = 0; i < v->capacity; ++i) {
		alo_Entry* entry = v->array + i;
		if (!tisnil(amkey(entry))) {
			markt(G, amkey(entry));
			markt(G, amval(entry));
		}
	}
	return sizeof(atable_t) + v->capacity * sizeof(alo_Entry);
}

static size_t propagate_thread(aglobal_t* G, alo_Thread* v) {
	alo_StkId i = v->stack;
	if (i == NULL) { /* if stack is not constructed yet. */
		return sizeof(alo_Thread);
	}
	while (i < v->top) {
		markt(G, i++);
	}
	if (G->gcstep == GCStepAtomic) {
		marknpg(G, v->caller);
		alo_StkId end = v->stack + v->stacksize;
		while (i < end) {
			trsetval(i++, aloO_nil);
		}
	}
	else if (G->gckind == GCKindNormal) {
		aloD_shrinkstack(v);
		aloB_shrink(v);
	}
	return sizeof(alo_Thread) + v->nframe * sizeof(aframe_t) + v->stacksize * sizeof(alo_TVal) + v->memstk.cap;
}

static size_t propagate_acl(aglobal_t* G, alo_ACl* v) {
	marknpg(G, v->proto);
	for (size_t i = 0; i < v->length; ++i) { /* for captured value, handle in special */
		if (v->array[i]) {
			markt(G, v->array[i]->p);
		}
	}
	return aaclsize(v);
}

static size_t propagate_ccl(aglobal_t* G, alo_CCl* v) {
	for (size_t i = 0; i < v->length; ++i) {
		markt(G, &v->array[i]);
	}
	return acclsize(v);
}

static size_t propagate_proto(aglobal_t* G, alo_Proto* v) {
	if (v->cache && aloG_iswhite(v->cache)) {
		v->cache = NULL; /* unlink cache and prototype */
	}
	markng(G, v->name);
	markng(G, v->src);
	int i;
	for (i = 0; i < v->nconst; ++i) { /* mark constants */
		markt(G, &v->consts[i]);
	}
	for (i = 0; i < v->nchild; ++i) { /* mark child prototypes */
		markpg(G, v->children[i]);
	}
	for (i = 0; i < v->nlocvar; ++i) { /* mark local variable name */
		markg(G, v->locvars[i].name);
	}
	for (i = 0; i < v->ncap; ++i) { /* mark capture value name */
		markg(G, v->captures[i].name);
	}
	return sizeof(alo_Proto) +
			v->ncap * sizeof(alo_CapInfo) +
			v->ncode * sizeof(a_insn) +
			v->nconst * sizeof(alo_TVal) +
			v->nchild * sizeof(alo_Proto*) +
			v->nlocvar * sizeof(alo_LocVarInfo) +
			v->nlineinfo * sizeof(alo_LineInfo);
}

static void propagate(aglobal_t* G) {
	alo_Obj* g = G->gray;
	G->gray = g->gclist;
	gray2black(g);
	switch (ttype(g)) {
	case ALO_TTUPLE: {
		alo_Tuple* v = g2a(g);
		for (size_t i = 0; i < v->length; ++i) {
			markt(G, v->array + i);
		}
		G->mtraced += atupsize(v);
		break;
	}
	case ALO_TLIST: {
		G->mtraced += propagate_list(G, g2l(g));
		break;
	}
	case ALO_TTABLE: {
		G->mtraced += propagate_table(G, g2m(g));
		break;
	}
	case ALO_TTHREAD: {
		alo_Thread* v = g2t(g);
		if (G->trun == v) { /* if it is current running thread */
			black2gray(v); /* make thread gray again */
			linkto(G->grayagain, v);
		}
		G->mtraced += propagate_thread(G, v);
		break;
	}
	case ALO_VCCL: {
		G->mtraced += propagate_ccl(G, g2cc(g));
		break;
	}
	case ALO_VACL: {
		G->mtraced += propagate_acl(G, g2ac(g));
		break;
	}
	case ALO_TPROTO: {
		G->mtraced += propagate_proto(G, g2p(g));
		break;
	}
	case ALO_TUSER: {
		alo_User* v = g2r(g);
		marknpg(G, v->metatable);
		G->mtraced += ardsize(v);
		break;
	}
	default: {
		aloE_assert(false, "broken object.");
	}
	}
}

static void propagateall(aglobal_t* G) {
	while (G->gray)
		propagate(G);
}

/**
 ** weak reference marking.
 */

#define tiswhite(o) (tisref(o) && aloG_iswhite(tasobj(o)))
#define delentry(e) aloE_void(tsetnil(amkey(e)), tsetnil(amval(e)))

static int scan_wtable(aglobal_t* G, atable_t* v) {
	int marked = false;
	int clears = false;
	int unknown = false;
	for (size_t i = 0; i < v->capacity; ++i) {
		alo_Entry* entry = &v->array[i];
		if (!tisnil(entry)) {
			if (checkmark(G, amkey(entry))) {
				clears = true;
				if (tiswhite(amval(entry))) {
					unknown = true;
				}
			}
			else if (tiswhite(amval(entry))) {
				markg_(G, tasobj(amval(entry)));
				marked = true;
			}
		}
	}
	if (G->gcstep == GCStepPropagate)
		linkto(G->grayagain, v);
	else if (unknown)
		linkto(G->weakk, v);
	else if (clears)
		linkto(G->weaka, v);
	return marked;
}

static void converge_weak(aglobal_t* G) {
	int update;
	alo_Obj* c;
	alo_Obj* n;
	do {
		n = G->weakk;
		G->weakk = NULL;
		update = false;
		while ((c = n)) {
			n = c->gclist;
			if (scan_wtable(G, g2m(c))) {
				propagateall(G);
				update = true;
			}
		}
	}
	while (update);
}

static void clean_weakkey(aglobal_t* G, alo_Obj* list) {
	alo_Obj* c = list;
	while (c) {
		aloE_xassert(tistab(c));
		atable_t* v = g2m(c);
		alo_Entry* e;
		for (e = v->lastfree - 1; e >= v->array; --e) { /* tailed iterate all elements for last free */
			if (checkmark(G, amkey(e))) {
				delentry(e);
				v->lastfree = e;
				v->length--;
			}
		}
		alo_Entry* const end = v->array + v->capacity;
		for (e = v->lastfree; e < end; ++e) {
			if (checkmark(G, amkey(e))) {
				delentry(e);
				v->length--;
			}
		}
		c = c->gclist;
	}
}

static void clean_weakvalue(aglobal_t* G, alo_Obj* list, alo_Obj* end) {
	alo_Obj* c;
	alo_Obj* n = list;
	while ((c = n) != end) {
		n = c->gclist;
		switch (ttpnv(c)) {
		case ALO_TLIST: {
			alo_List* v = g2l(c);
			size_t i, j = 0;
			for (i = 0; i < v->length; ++i) {
				alo_TVal* o = &v->array[i];
				if (checkmark(G, o)) {
					trsetval(o, aloO_nil);
				}
				else {
					tsetobj(NULL, &v->array[j++], o);
				}
			}
			v->length = j;
			break;
		}
		case ALO_TTABLE: {
			atable_t* v = g2m(c);
			alo_Entry* e;
			for (e = v->lastfree - 1; e >= v->array; --e) { /* tailed iterate all elements for last free */
				if (checkmark(G, amval(e))) {
					delentry(e);
					v->lastfree = e;
					v->length--;
				}
			}
			alo_Entry* const end = v->array + v->capacity;
			for (e = v->lastfree; e < end; ++e) {
				if (checkmark(G, amval(e))) {
					delentry(e);
					v->length--;
				}
			}
			break;
		}
		default:
			aloE_xassert(0);
			break;
		}
	}
}

/**
 ** finalize handling
 */

static void splitfnz(aglobal_t* G) {
	alo_Obj* g;
	alo_Obj** fnz = &G->gfnzable;
	alo_Obj** tfz = tailof(G->gtobefnz);
	while ((g = *fnz)) {
		aloE_assert(aloG_isfinalizable(g), "target is not finalizable.");
		if (aloG_iswhite(g)) { /* if object is being finalized */
			*fnz = g->gcprev;
			g->gcprev = NULL;
			*tfz = g;
			tfz = &g->gcprev;
		}
		else {
			fnz = &g->gcprev;
		}
	}
}

static void keeptbfwhite(aglobal_t* G) {
	alo_Obj* g = G->gtobefnz;
	while (g) {
		markg(G, g);
		g = g->gcprev;
	}
}

/**
 ** sweeping functions.
 */

#define isdeadm(o,gb) ((o)->mark & (gb))

/**
 ** sweep object 'g'
 */
static void sweepobj(alo_State T, alo_Obj* g) {
	switch (ttype(g)) {
	case ALO_VHSTR: {
		alo_Str* v = g2s(g);
		aloM_free(T, v, astrsizel(v->lnglen));
		break;
	}
	case ALO_VISTR: {
		alo_Str* v = g2s(g);
		aloS_remove(T, v);
		aloM_free(T, v, astrsizel(v->shtlen));
		break;
	}
	case ALO_TTUPLE: {
		alo_Tuple* v = g2a(g);
		aloM_free(T, v, atupsize(v));
		break;
	}
	case ALO_TLIST: {
		alo_List* v = g2l(g);
		aloM_dela(T, v->array, v->capacity);
		aloM_delo(T, v);
		break;
	}
	case ALO_TTABLE: {
		atable_t* v = g2m(g);
		aloM_dela(T, v->array, v->capacity);
		aloM_delo(T, v);
		break;
	}
	case ALO_TTHREAD: {
		aloR_deletethread(T, g2t(g));
		break;
	}
	case ALO_VACL: {
		alo_ACl* v = g2ac(g);
		for (int i = 0; i < v->length; ++i) {
			alo_Capture* c = v->array[i];
			/* decrease capture reference counter */
			if (c && --c->counter == 0 && aloO_isclosed(c)) {
				/* delete capture if no reference remain */
				aloM_delo(T, c);
			}
		}
		aloM_free(T, v, aaclsize(v));
		break;
	}
	case ALO_VCCL: {
		alo_CCl* v = g2cc(g);
		aloM_free(T, v, acclsize(v));
		break;
	}
	case ALO_TUSER: {
		alo_User* v = g2r(g);
		aloM_free(T, v, ardsize(v));
		break;
	}
	case ALO_TPROTO: {
		aloF_deletep(T, g2p(g));
		break;
	}
	default: {
		aloE_assert(false, "invalid object");
		break;
	}
	}
}

static alo_Obj** sweeplist(alo_State T, alo_Obj** list, size_t limit) {
	Gd(T);
	alo_Obj* c;
	int wb = aloG_whitebit(G), gb = aloG_otherbit(G);
	while ((c = *list) && limit-- > 0) {
		if (isdeadm(c, gb)) { /* is object dead? */
			*list = c->gcprev; /* remove object from list */
			sweepobj(T, c); /* sweep object */
		}
		else {
			setcolor(c, wb); /* settle object to white */
			list = &c->gcprev; /* goto next object */
		}
	}
	return *list ? list : NULL;
}

static alo_Obj** sweeptolive(alo_State T, alo_Obj** list) {
	alo_Obj** const old = list;
	do {
		list = sweeplist(T, list, 1);
	}
	while (old == list);
	return list;
}

static void sweepall(alo_State T, alo_Obj** list) {
	alo_Obj* c;
	alo_Obj* n = *list;
	while ((c = n)) {
		n = c->gcprev; /* move to next object */
		sweepobj(T, c); /* sweep object */
	}
	*list = NULL; /* clear values from list. */
}

/**
 ** GC control functions.
 */

static void gcbegin(aglobal_t* G) {
	/* clear GC linked list */
	G->gray = G->grayagain = NULL;
	G->weaka = G->weakk = G->weakv = NULL;
	/* settle root object to white */
	setwhite(G, G->tmain);
	if (tisref(&G->registry)) {
		setwhite(G, tasobj(&G->registry));
	}
	/* mark root. */
	markt(G, &G->registry);
	markpg(G, G->tmain);
}

static size_t gcatomic(alo_State T, aglobal_t* G) {
	aloE_assert(G->weakk == NULL && G->weakv == NULL && G->weaka == NULL, "weak list is not empty.");
	aloE_assert(G->trun == T, "thread is not running thread.");
	size_t work;
	alo_Obj* grayagain = G->grayagain;
	G->mtraced = 0;
	/* propagate all strong accessible references */
	markpg(G, T);
	markpg(G, G->tmain);
	markt(G, &G->registry);
	propagateall(G); /* traverse changes */
	work = G->mtraced; /* count traced memory, do not recount gray again. */
	G->gray = grayagain;
	G->grayagain = NULL; /* make gray again be null. */
	propagateall(G); /* traverse gray again */
	G->mtraced = 0;
	converge_weak(G); /* converge all weak key table to link. */
	work += G->mtraced;
	/* all strong accessible references are marked */
	/* clean weak references with white value */
	clean_weakvalue(G, G->weakv, NULL);
	clean_weakvalue(G, G->weaka, NULL);
	alo_Obj* owv = G->weakv;
	alo_Obj* owa = G->weaka;
	splitfnz(G); /* split white finalizable object 'tobefnz' list */
	keeptbfwhite(G); /* keep 'tobefnz' list objects */
	propagateall(G);
	G->mtraced = 0;
	converge_weak(G);
	work += G->mtraced;
	/* all resurrected references are marked */
	/* clean weak references with white key */
	clean_weakkey(G, G->weakk);
	clean_weakkey(G, G->weaka);
	/* clean weak references with white value */
	clean_weakvalue(G, G->weakv, owv);
	clean_weakvalue(G, G->weaka, owa);
	aloS_cleancache(T);
	G->whitebit = aloG_otherbit(G); /* flip white bit */
	return work;
}

static void sweepstart(alo_State T, aglobal_t* G) {
	G->gcstep = GCStepSwpNorm;
	G->sweeping = sweeplist(T, &G->gnormal, 1);
}

static size_t sweepstep(alo_State T, aglobal_t* G, alo_Obj** nextlist, int nextstep) {
	size_t olddebt = G->mdebt;
	if (G->sweeping) {
		G->sweeping = sweeplist(T, G->sweeping, MAXSWEEPCOUNT);
		G->mestimate -= olddebt - G->mdebt; /* reduce dead memory size in estimated */
		if (G->sweeping)
			return MAXSWEEPCOUNT * SWEEP_COST;
	}
	G->sweeping = nextlist;
	G->gcstep = nextstep;
	return 0;
}

static void callfinimpl(alo_State T, int dothrow) {
	Gd(T);
	alo_Obj* g = G->gtobefnz; /* remove object from 'tobefnz' list */
	G->gtobefnz = g->gcprev;
	g->gcprev = G->gnormal; /* append object to 'normal' list */
	G->gnormal = g;
	if (issweeping(G)) {
		setwhite(G, g);
	}
	aloE_assert(aloG_isfinalizable(g), "target is not finalizable.");
	g->mark &= ~aloG_final; /* remove finalizable bit */
	alo_TVal t = tnewrefx(g);
	const alo_TVal* f = aloT_gettm(T, &t, TM_DEL, true); /* get delete function */
	if (f) {
		int oldgc = G->fgc; /* set GC state */
		int allowhook = T->fallowhook;
		G->fgc = false; /* disable GC during calling finalizer */
		T->fallowhook = false;
		tsetobj(T, T->top++, f);
		tsetref(T, T->top++, g);
		T->frame->ffnz = true; /* start in finalizing state */
		int status = aloD_pcall(T, T->top - 2, 0, ALO_NOERRFUN);
		T->frame->ffnz = false; /* end in finalizing state */
		T->fallowhook = allowhook;
		G->fgc = oldgc; /* restore GC state */
		if (status != ALO_STOK && dothrow) {
			if (status == ALO_STERRRT) {
				const char* msg = tisstr(T->top - 1) ? tasstr(T->top - 1)->array : "no message";
				aloV_pushfstring(T, "error when calling finalizer (%s)", msg);
				status = ALO_STERRFIN;
			}
			aloD_throw(T, status);
		}
	}
}

static void callallfin(alo_State T) {
	Gd(T);
	while (G->gtobefnz) {
		callfinimpl(T, false);
	}
}

static size_t callfin(alo_State T) {
	Gd(T);
	size_t c = G->nfnzobj;
	while (G->gtobefnz && c-- > 0) {
		callfinimpl(T, true);
	}
	size_t called = G->nfnzobj - c; /* count calling finalizer */
	G->nfnzobj = G->gtobefnz ? G->nfnzobj * 2 : 0;
	return called;
}

static size_t gcstep(alo_State T, aglobal_t* G) {
	switch (G->gcstep) {
	case GCStepPause: {
		G->mtraced = 0;
		gcbegin(G);
		G->gcstep = GCStepPropagate;
		return G->mtraced;
	}
	case GCStepPropagate: {
		G->mtraced = 0;
		propagate(G);
		if (G->gray) {
			return G->mtraced;
		}
		G->gcstep = GCStepAtomic;
		return 0;
	}
	case GCStepAtomic: {
		propagateall(G);
		size_t work = gcatomic(T, G);
		sweepstart(T, G);
		G->mestimate = totalmem(G); /* estimate upper bound */
		return work;
	}
	case GCStepSwpNorm: {
		return sweepstep(T, G, &G->gfnzable, GCStepSwpFin);
	}
	case GCStepSwpFin: {
		return sweepstep(T, G, &G->gtobefnz, GCStepSwpFnz);
	}
	case GCStepSwpFnz: {
		return sweepstep(T, G, NULL, GCStepCallFin);
	}
	case GCStepCallFin: {
		if (G->gtobefnz && G->gckind != GCKindEmergency) {
			return callfin(T) * CALLFIN_COST;
		}
		else {
			G->gcstep = GCStepPause;
			return 0;
		}
	}
	default:
		aloE_xassert(0);
		return 0;
	}
}

static void gctill(alo_State T, aglobal_t* G, int mask) {
	do gcstep(T, G);
	while (!((1 << G->gcstep) & mask));
}

static void setpause(aglobal_t* G) {
	size_t limit = G->mestimate * G->gcpausemul / GCUNIT;
	aloD_setdebt(G, totalmem(G) - limit);
}

void aloG_checkfnzobj(alo_State T, alo_Obj* g, atable_t* mt) {
	Gd(T);
	if (aloG_isfinalizable(g) || /* if object is linked in finalizable-object list */
		aloH_getis(mt, T->g->stagnames[TM_DEL]) == aloO_tnil) { /* or no finalizer exists */
		return;
	}
	if (issweeping(G)) {
		setwhite(G, g);
		if (G->sweeping == &g->gcprev) {
			G->sweeping = sweeptolive(T, G->sweeping);
		}
	}
	/* searching pointing of object */
	alo_Obj** p;
	for (p = &G->gnormal; *p != g; p = &(*p)->gcprev);
	/* link to finalizable object list */
	*p = g->gcprev;
	g->gcprev = G->gfnzable;
	G->gfnzable = g;
	/* mark final bit */
	g->mark |= aloG_final;
}

void aloG_step(alo_State T) {
	Gd(T);
	if (!G->fgc) {
		aloD_setdebt(G, -ALO_MINWORK * 16); /* set a big GC pause */
	}
	else {
		if (G->gcrunning) {
			return; /* stop re-run GC when it is running */
		}
		ssize_t work = (G->mdebt > 0 ? G->mdebt : 0) * G->gcstepmul / GCUNIT + ALO_MINWORK;
		G->gcrunning = true;
		while ((work -= gcstep(T, G)) >= 0 && G->gcstep != GCStepPause);
		G->gcrunning = false;
		if (G->gcstep == GCStepPause) {
			setpause(G);
		}
		else {
			aloD_setdebt(G, work / G->gcstepmul * GCUNIT - ALO_MINWORK);
		}
	}
}

void aloG_fullgc(alo_State T, int kind) {
	Gd(T);
	aloE_assert(kind == GCKindEmergency || !G->gcrunning, "GC already running.");
	G->gckind = aloE_byte(kind);
	G->gcrunning = true;
	if (iscoloring(G)) {
		sweepstart(T, G); /* force to enter into the sweep step. */
	}

	/* do 2 cycle of GC, to ensure all garbage are cycled. */
	gctill(T, G, 1 << GCStepPause);
	gctill(T, G, ~(1 << GCStepPause));
	gctill(T, G, 1 << GCStepCallFin);
	aloE_assert(G->mestimate == totalmem(G), "estimated memory must be correct after a cycle.");
	gctill(T, G, 1 << GCStepPause);

	G->gcrunning = false;
	G->gckind = GCKindNormal;
	setpause(G);
}

void aloG_clear(alo_State T) {
	Gd(T);
	callallfin(T); /* call all 'tobefnz' finalizer */
	G->gtobefnz = G->gfnzable;
	G->gfnzable = NULL;
	callallfin(T); /* call all 'fnzable' finalizer */
	aloE_assert(G->gfnzable == NULL && G->gtobefnz == NULL, "still have finalizable object exists.");
	G->whitebit = 0; /* mark all objects are dead */
	sweepall(T, &G->gnormal);
	sweepall(T, &G->gfixed);
	aloE_assert(G->itable.length == 0, "some of intern string not delete yet.");
	aloM_dela(T, G->itable.array, G->itable.capacity); /* delete intern string table */
}

