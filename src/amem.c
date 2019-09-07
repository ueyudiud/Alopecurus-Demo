/*
 * amem.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#include "astate.h"
#include "adebug.h"
#include "amem.h"
#include "agc.h"
#include "ado.h"

amem aloM_realloc(astate T, amem oldblock, size_t oldsize, size_t newsize) {
	aloE_assert(!oldblock == !oldsize, "block size mismatch");
	Gd(T);
#if defined(ALO_HARDMEMTEST)
	if (newsize > oldsize && G->fgc)
		aloG_fullgc(T, GCKindEmergency);
#endif
	amem newblock = G->alloc(G->context, oldblock, oldsize, newsize);
	if (newblock == NULL && newsize > 0) { /* failed to allocate block? */
		aloG_fullgc(T, GCKindEmergency); /* collect some unused memory */
		newblock = G->alloc(G->context, oldblock, oldsize, newsize); /* try to alocate block again */
		if (newblock == NULL) { /* still failed? */
			/* throw a memory error */
			tsetstr(T, T->top++, G->smerrmsg);
			aloD_throw(T, ThreadStateErrMemory);
		}
	}
	G->mdebt = G->mdebt + newsize - oldsize;
	return newblock;
}

void aloM_free(astate T, amem oldblock, size_t oldsize) {
	aloE_assert(!oldblock == !oldsize, "block size mismatch");
	Gd(T);
	G->alloc(G->context, oldblock, oldsize, 0);
	G->mdebt -= oldsize;
}

static anoret throwsol(astate T) {
	aloU_rterror(T, "array size over limit.");
}

size_t aloM_adjsize(astate T, size_t capacity, size_t require, size_t limit) {
	if (require > limit) {
		throwsol(T);
	}
	size_t newcap = capacity * 2;
	if (newcap > limit) {
		if (capacity >= limit) {
			throwsol(T);
		}
		return limit;
	}
	else {
		if (newcap < ALO_MIN_BUFSIZE) {
			newcap = ALO_MIN_BUFSIZE;
		}
		if (newcap < require) {
			newcap = require;
		}
		return newcap;
	}
}

size_t aloM_growsize(astate T, size_t capacity, size_t limit) {
	size_t newcap = capacity * 2;
	if (newcap > limit) {
		if (capacity >= limit) {
			throwsol(T);
		}
		return limit;
	}
	else {
		if (newcap < ALO_MIN_BUFSIZE) {
			newcap = ALO_MIN_BUFSIZE;
		}
		return newcap;
	}
}
