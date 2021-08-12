/*
 * ainit.c
 *
 *  Created on: 2019年7月29日
 *      Author: ueyudiud
 */

#define AINIT_C_
#define ALO_LIB

#include "aaux.h"
#include "alibs.h"

typedef struct alo_ModInfo {
	a_cstr name;
	a_cfun openf;
} amodinfo_t;

const amodinfo_t stdmods[] = {
	{ "_G", aloopen_base },
	{ ALO_CLSLIB_NAME, aloopen_cls },
	{ ALO_MATHLIB_NAME, aloopen_math },
	{ ALO_IOLIB_NAME, aloopen_io },
	{ ALO_STRLIB_NAME, aloopen_str },
	{ ALO_TUPLELIB_NAME, aloopen_tup },
	{ ALO_LISTLIB_NAME, aloopen_lis },
	{ ALO_TABLELIB_NAME, aloopen_tab },
	{ ALO_SYSLIB_NAME, aloopen_sys },
	{ ALO_MODLIB_NAME, aloopen_mod },
	{ ALO_GCLIB_NAME, aloopen_gc },
	{ ALO_COROLIB_NAME, aloopen_coro },
	{ NULL, NULL }
};

void aloL_openlibs(alo_State T) {
	const amodinfo_t* m = stdmods;
	while (m->name) {
		aloL_require(T, m->name, m->openf, true);
		alo_settop(T, -1);
		m++;
	}
}
