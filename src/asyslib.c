/*
 * asyslib.c
 *
 * system library.
 *
 *  Created on: 2019年9月16日
 *      Author: ueyudiud
 */

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#include <stdlib.h>
#include <time.h>

static int sys_exit(astate T) {
	int status = aloL_getoptinteger(T, 0, 0);
	if (aloL_getoptbool(T, 1, false)) {
		alo_deletestate(T);
	}
	aloE_void(exit(status));
	return 0;
}

static int sys_getenv(astate T) {
	alo_pushstring(T, getenv(aloL_checkstring(T, 0)));
	return 1;
}

static int sys_clock(astate T) {
	alo_pushinteger(T, (afloat) clock() / CLOCKS_PER_SEC);
	return 1;
}

static const acreg_t mod_funcs[] = {
	{ "exit", sys_exit },
	{ "getenv", sys_getenv },
	{ "clock", sys_clock },
	{ NULL, NULL }
};

int aloopen_syslib(astate T) {
	alo_newtable(T, 0);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
