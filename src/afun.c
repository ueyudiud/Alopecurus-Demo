/*
 * afun.c
 *
 *  Created on: 2019年7月26日
 *      Author: ueyudiud
 */

#define AFUN_C_
#define ALO_CORE

#include "afun.h"
#include "astate.h"
#include "ameta.h"
#include "agc.h"

/**
 ** create uninitialized Alopecurus closure.
 */
aclosure_t* aloF_new(astate T, size_t size, aproto_t* proto) {
	aclosure_t* value = aloE_cast(aclosure_t*, aloM_malloc(T, aclosizel(size)));
	aloG_register(T, value, ALO_TACL);
	value->length = aloE_byte(size);
	value->a.proto = proto;
	/* clean delegate and captures */
	tsetnil(&value->delegate);
	for (size_t i = 0; i < size; tsetnil(value->array + i++));
	return value;
}

/**
 ** create new C closure.
 */
aclosure_t* aloF_newc(astate T, acfun handle, size_t size) {
	aclosure_t* value = aloE_cast(aclosure_t*, aloM_malloc(T, aclosizel(size)));
	aloG_register(T, value, ALO_TCCL);
	value->c.handle = handle;
	value->length = aloE_byte(size);
	tsetobj(T, &value->delegate, aloT_getreg(T));
	return value;
}

/**
 ** create new prototype (not register to GC instantly).
 */
aproto_t* aloF_newp(astate T) {
	aproto_t* value = aloM_newo(T, aproto_t);
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

atval_t* aloF_get(aclosure_t* self, size_t index) {
	aloE_assert(index < self->length, "index out of bound.");
	atval_t* slot = self->array + index;
	return ttiscap(slot) ? tgetcap(slot)->p : slot;
}

acap* aloF_find(astate T, askid_t id) {
	acap** p = &T->captures;
	acap* c;
	while ((c = *p) && c->p >= id) { /* try to find capture already created */
		if (c->p == id) { /* find capture? */
			c->counter++; /* increase reference count */
			return c;
		}
		p = &c->prev; /* move to previous capture */
	}
	c = aloM_newo(T, acap); /* capture not exist, create a new one */
	c->counter = 1;
	c->mark = 0; /* settle capture untouched yet. */
	c->p = id;
	/* link capture to list. */
	c->prev = *p;
	*p = c;
	return c;
}

void aloF_close(astate T, askid_t id) {
	acap* c;
	acap* n = T->captures;
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

void aloF_deletep(astate T, aproto_t* proto) {
	aloM_dela(T, proto->captures, proto->ncap);
	aloM_dela(T, proto->code, proto->ncode);
	aloM_dela(T, proto->consts, proto->nconst);
	aloM_dela(T, proto->children, proto->nchild);
	aloM_dela(T, proto->locvars, proto->nlocvar);
	aloM_dela(T, proto->lineinfo, proto->nlineinfo);
	aloM_delo(T, proto);
}
