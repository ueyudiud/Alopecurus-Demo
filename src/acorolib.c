/*
 * acorolib.c
 *
 *  Created on: 2020年2月5日
 *      Author: ueyudiud
 */

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

static int coro_create(astate T) {
	aloL_checkcall(T, 0);
	alo_settop(T, 1);
	astate T1 = alo_newthread(T);
	alo_push(T, 0);
	alo_xmove(T, T1, 1);
	return 1;
}

static int aux_resume_closure(astate T) {
	int narg = alo_gettop(T);
	astate T1 = alo_tothread(T, ALO_CAPTURE_INDEX(0));
	alo_xmove(T, T1, narg);
	alo_resume(T1, T, narg);
	return alo_gettop(T1);
}

static int coro_wrap(astate T) {
	aloL_checkcall(T, 0);
	alo_settop(T, 1);
	astate T1 = alo_newthread(T);
	alo_push(T, 0);
	alo_xmove(T, T1, 1);
	alo_pushcclosure(T, aux_resume_closure, 1);
	return 1;
}

static int coro_tofun(astate T) {
	aloL_checktype(T, 0, ALO_TTHREAD);
	alo_push(T, 0);
	alo_pushcclosure(T, aux_resume_closure, 1);
	return 1;
}

static int aux_resume(astate T, astate from, int narg) {
	alo_xmove(from, T, narg);
	int status = alo_resume(T, from, narg);
	if (status != ThreadStateRun && status != ThreadStateYield) {
		/* resumed failed or throw an unrecoverable error */
		alo_xmove(T, from, 1);
		return -1;
	}
	else {
		int nres = alo_gettop(T);
		if (!alo_ensure(T, nres)) {
			alo_pushstring(T, "too many results to resume");
			return -1;
		}
		alo_xmove(T, from, nres);
		return nres;
	}

}

static int coro_resume(astate T) {
	aloL_checktype(T, 0, ALO_TTHREAD);
	astate T1 = alo_tothread(T, 0);
	int narg = alo_gettop(T) - 1;
	int nres = aux_resume(T1, T, narg);
	if (nres == -1) { /* error occurred? */
		alo_throw(T);
	}
	return nres;
}

static int coro_presume(astate T) {
	aloL_checktype(T, 0, ALO_TTHREAD);
	astate T1 = alo_tothread(T, 0);
	int narg = alo_gettop(T) - 1;
	int nres = aux_resume(T1, T, narg);
	if (nres == -1) { /* error occurred? */
		alo_pushboolean(T, false);
		alo_push(T, 2);
		return 2;
	}
	else  {
		alo_pushboolean(T, true);
		alo_insert(T, - (nres + 1));
		return nres + 1;
	}
}

static int coro_yield(astate T) {
	alo_yield(T, alo_gettop(T) - 1);
	return 0;
}

static const acreg_t mod_funcs[] = {
	{ "__new", coro_create },
	{ "create", coro_create },
	{ "presume", coro_presume },
	{ "resume", coro_resume },
	{ "tofun", coro_tofun },
	{ "wrap", coro_wrap },
	{ "yield", coro_yield },
	{ NULL, NULL }
};

int aloopen_corolib(astate T) {
	alo_bind(T, "thread.create", coro_create);
	alo_bind(T, "thread.presume", coro_presume);
	alo_bind(T, "thread.resume", coro_resume);
	alo_bind(T, "thread.tofun.resume", aux_resume_closure);
	alo_bind(T, "thread.tofun", coro_tofun);
	alo_bind(T, "thread.wrap", coro_wrap);
	alo_bind(T, "thread.yield", coro_yield);
	alo_getreg(T, "__basic_delegates");
	alo_rawgeti(T, -1, ALO_TTHREAD);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
