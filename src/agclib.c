/*
 * agclib.c
 *
 * defines GC related function
 *
 *  Created on: 2019年11月1日
 *      Author: ueyudiud
 */

#define AGCLIB_C_
#define ALO_LIB

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#define MIN_GCSTEPMUL 40

static int gc_fullgc(astate T) {
	alo_fullgc(T);
	return 0;
}

static int gc_memused(astate T) {
	alo_pushinteger(T, alo_memused(T));
	return 1;
}

static int gc_isrunning(astate T) {
	alo_pushboolean(T, alo_isgcrunning(T));
	return 0;
}

static int gc_setstate(astate T) {
	alo_gcconf(T, aloL_checkbool(T, 0) ? ALO_GCRERUN : ALO_GCSTOP, 0);
	return 0;
}

static int gc_setpausemul(astate T) {
	aint value = aloL_checkinteger(T, 0);
	alo_gcconf(T, ALO_GCPAUSEMUL, value);
	return 0;
}

static int gc_setstepmul(astate T) {
	aint value = aloL_checkinteger(T, 0);
	if (value < MIN_GCSTEPMUL) {
		aloL_error(T, "GC step multiplier too small. (should more than %d)", MIN_GCSTEPMUL);
	}
	alo_gcconf(T, ALO_GCSTEPMUL, value);
	return 0;
}

static const acreg_t mod_funcs[] = {
	{ "fullgc", gc_fullgc },
	{ "isrunning", gc_isrunning },
	{ "memused", gc_memused },
	{ "setpausemul", gc_setpausemul },
	{ "setstate", gc_setstate },
	{ "setstepmul", gc_setstepmul },
	{ NULL, NULL }
};

int aloopen_gc(astate T) {
	alo_newtable(T, 0);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
