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

static int gc_fullgc(astate T) {
	alo_fullgc(T);
	return 0;
}

static int gc_memused(astate T) {
	alo_pushinteger(T, aloE_cast(aint, alo_memused(T)));
	return 1;
}

static const acreg_t mod_funcs[] = {
	{ "fullgc", gc_fullgc },
	{ "memused", gc_memused },
	{ NULL, NULL }
};

int aloopen_gclib(astate T) {
	alo_newtable(T, 0);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
