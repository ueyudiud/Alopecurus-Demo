/*
 * acorolib.c
 *
 *  Created on: 2020年2月5日
 *      Author: ueyudiud
 */

#define ACOROLIB_C_
#define ALO_LIB

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

/**
 ** create a new coroutine with runnable as start function.
 ** prototype: thread.create(runnable)
 */
static int coro_create(alo_State T) {
	aloL_checkcall(T, 0);
	alo_settop(T, 1);
	alo_State T1 = alo_newthread(T);
	alo_push(T, 0);
	alo_xmove(T, T1, 1);
	return 1;
}

static int aux_resume(alo_State T, alo_State from, int narg) {
	alo_xmove(from, T, narg);
	int status = alo_resume(T, from, narg);
	if (status == -1) {
		return -1;
	}
	else if (status != ALO_STOK && status != ALO_STYIELD) {
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

static int aux_resume_closure(alo_State T) {
	int narg = alo_gettop(T);
	alo_State T1 = alo_tothread(T, ALO_CAPTURE_INDEX(0));
	int nres = aux_resume(T1, T, narg);
	if (nres == -1) {
		alo_error(T);
	}
	return nres;
}

/**
 ** create a new coroutine with runnable as start function,
 ** and wrap it into a function.
 ** prototype: thread.wrap(runnable)
 */
static int coro_wrap(alo_State T) {
	aloL_checkcall(T, 0);
	alo_settop(T, 1);
	alo_State T1 = alo_newthread(T);
	alo_push(T, 0);
	alo_xmove(T, T1, 1);
	alo_pushcclosure(T, aux_resume_closure, 1);
	return 1;
}

/**
 ** wrap a coroutine into a function.
 ** prototype: thread.tofun(coroutine)
 */
static int coro_tofun(alo_State T) {
	aloL_checktype(T, 0, ALO_TTHREAD);
	alo_push(T, 0);
	alo_pushcclosure(T, aux_resume_closure, 1);
	return 1;
}

/**
 ** move arguments into resumed coroutine and resume a function.
 ** return the parameters given in next time calling yield function
 ** or start function running to the end.
 ** if error occurs in coroutine, throw it directly.
 ** if current coroutine is main coroutine or it is not suspended,
 ** an runtime error will be thrown.
 ** prototype: thread.resume(coroutine, {args})
 */
static int coro_resume(alo_State T) {
	aloL_checktype(T, 0, ALO_TTHREAD);
	alo_State T1 = alo_tothread(T, 0);
	int narg = alo_gettop(T) - 1;
	int nres = aux_resume(T1, T, narg);
	if (nres == -1) { /* error occurred? */
		alo_error(T);
	}
	return nres;
}

/**
 ** move arguments into resumed coroutine and resume a function.
 ** return the parameters given in next time calling yield function
 ** or start function running to the end, a 'true' will insert into
 ** the base of returned values.
 ** if error occurs in coroutine, it will return false and error message.
 ** if current coroutine is main coroutine or it is not suspended,
 ** false and an runtime error will be returned.
 ** prototype: thread.presume(coroutine, {args})
 */
static int coro_presume(alo_State T) {
	aloL_checktype(T, 0, ALO_TTHREAD);
	alo_State T1 = alo_tothread(T, 0);
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

/**
 ** suspend current coroutine, the environment of current coroutine will
 ** be saved and wait till next time to resume.
 ** the arguments will called and returns to the caller.
 ** if coroutine is not yieldable, an error will be thrown.
 ** prototype: thread.yield({args})
 */
static int coro_yield(alo_State T) {
	alo_yield(T, alo_gettop(T));
	return 0;
}

/**
 ** check current coroutine is yieldable.
 ** it is yieldable if and only if it has no non-yieldable C call stack frame
 ** and it is not main coroutine.
 ** return true if it is yieldable and false.
 ** prototype: thread.isyieldable()
 */
static int coro_isyieldable(alo_State T) {
	alo_pushboolean(T, alo_isyieldable(T));
	return 1;
}

/**
 ** get status of current state.
 ** prototype: thread.status(self)
 */
static int coro_status(alo_State T) {
	if (alo_isnothing(T, 0))
		goto is_self;
	aloL_checktype(T, 0, ALO_TTHREAD);
	alo_State self = alo_tothread(T, 0);
	if (T == self) {
		is_self:
		alo_pushcstring(T, "running");
	}
	else switch (alo_status(self)) {
	case ALO_STYIELD: {
		alo_DbgInfo info;
		if (!alo_getinfo(self, ALO_INFCURR, "", &info) && alo_gettop(self) != 1) {
			goto dead;
		}
		suspended:
		alo_pushcstring(T, "suspended");
		break;
	}
	case ALO_STOK: {
		if (alo_gettop(self) == 0) {
			alo_pushcstring(T, "normal");
			break;
		}
		else {
			goto suspended;
		}
	}
	default: {
		dead:
		alo_pushcstring(T, "dead");
		break;
	}
	}
	return 1;
}

/**
 ** return current coroutine, and second result shows that current coroutine
 ** is main coroutine.
 ** prototype: thread.current()
 */
static int coro_current(alo_State T) {
	int ismain = alo_pushthread(T);
	alo_pushboolean(T, ismain);
	return 2;
}

static const acreg_t mod_funcs[] = {
	{ "create", coro_create },
	{ "current", coro_current },
	{ "isyieldable", coro_isyieldable },
	{ "presume", coro_presume },
	{ "resume", coro_resume },
	{ "status", coro_status },
	{ "tofun", coro_tofun },
	{ "wrap", coro_wrap },
	{ "yield", coro_yield },
	{ NULL, NULL }
};

int aloopen_coro(alo_State T) {
	alo_getreg(T, "__basic_delegates");
	alo_rawgeti(T, -1, ALO_TTHREAD);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
