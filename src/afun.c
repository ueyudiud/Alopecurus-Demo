/*
 * afun.c
 *
 *  Created on: 2019年7月26日
 *      Author: ueyudiud
 */

#define AFUN_C_
#define ALO_CORE

#include "astate.h"
#include "ameta.h"
#include "agc.h"
#include "afun.h"

/**
 ** create uninitialized Alopecurus closure.
 */
alo_ACl* aloF_new(alo_State T, size_t size) {
	alo_ACl* value = aloE_cast(alo_ACl*, aloM_malloc(T, aaclsizel(size)));
	aloG_register(T, value, ALO_VACL);
	value->length = aloE_byte(size);
	value->proto = NULL;
	/* clean delegate and captures */
	for (size_t i = 0; i < size; value->array[i++] = NULL);
	return value;
}

/**
 ** create new C closure.
 */
alo_CCl* aloF_newc(alo_State T, a_cfun handle, size_t size) {
	alo_CCl* value = aloE_cast(alo_CCl*, aloM_malloc(T, acclsizel(size)));
	aloG_register(T, value, ALO_VCCL);
	value->handle = handle;
	value->length = aloE_byte(size);
	value->fenv = false;
	return value;
}

/**
 ** create new prototype (not register to GC instantly).
 */
alo_Proto* aloF_newp(alo_State T) {
	alo_Proto* value = aloM_newo(T, alo_Proto);
	value->nargs = 0;
	value->nstack = 0;
	value->flags = 0;
	value->ncap = 0;
	value->ncode = 0;
	value->nconst = 0;
	value->nlineinfo = 0;
	value->nchild = 0;
	value->nlocvar = 0;
	value->name = T->g->sempty;
	value->linefdef = -1;
	value->lineldef = -1;
	value->consts = NULL;
	value->code = NULL;
	value->lineinfo = NULL;
	value->locvars = NULL;
	value->captures = NULL;
	value->children = NULL;
	value->cache = NULL;
	value->src = NULL;
	return value;
}

/**
 ** create a environment capture.
 */
alo_Capture* aloF_envcap(alo_State T) {
	alo_Capture* cap = aloM_newo(T, alo_Capture);
	cap->counter = 1;
	cap->p = &cap->slot;
	tsetobj(T, cap->p, T->frame->env);
	return cap;
}

/**
 ** find capture in specific stack slot.
 */
alo_Capture* aloF_find(alo_State T, alo_StkId id) {
	alo_Capture** p = &T->captures;
	alo_Capture* c;
	while ((c = *p) && c->p >= id) { /* try to find capture already created */
		if (c->p == id) { /* find capture? */
			c->counter++; /* increase reference count */
			return c;
		}
		p = &c->prev; /* move to previous capture */
	}
	c = aloM_newo(T, alo_Capture); /* capture not exist, create a new one */
	c->counter = 1;
	c->mark = 0; /* settle capture untouched yet. */
	c->p = id;
	/* link capture to list. */
	c->prev = *p;
	*p = c;
	return c;
}

void aloF_close(alo_State T, alo_StkId id) {
	alo_Capture* c;
	alo_Capture* n = T->captures;
	while ((c = n) && c->p >= id) {
		n = c->prev; /* move to previous */
		if (c->counter > 0) { /* is still has reference of capture? */
			/* close capture */
			tsetobj(T, &c->slot, c->p);
			c->p = &c->slot;
		}
		else {
			aloM_delo(T, c); /* delete capture */
		}
	}
	T->captures = c;
}

/**
 ** delete prototype.
 */
void aloF_deletep(alo_State T, alo_Proto* proto) {
	aloM_dela(T, proto->captures, proto->ncap);
	aloM_dela(T, proto->code, proto->ncode);
	aloM_dela(T, proto->consts, proto->nconst);
	aloM_dela(T, proto->children, proto->nchild);
	aloM_dela(T, proto->locvars, proto->nlocvar);
	aloM_dela(T, proto->lineinfo, proto->nlineinfo);
	aloM_delo(T, proto);
}
