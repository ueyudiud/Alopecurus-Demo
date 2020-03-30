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
	astr name;
	acfun openf;
} amodinfo_t;

const amodinfo_t stdmods[] = {
	{ "_G", aloopen_baselib },
	{ "@", aloopen_clslib },
	{ "math", aloopen_mathlib },
	{ "io", aloopen_iolib },
	{ "string", aloopen_strlib },
	{ "list", aloopen_lislib },
	{ "table", aloopen_tablib },
	{ "sys", aloopen_syslib },
	{ "mod", aloopen_modlib },
	{ "gc", aloopen_gclib },
	{ "thread", aloopen_corolib },
	{ NULL, NULL }
};

void aloL_openlibs(astate T) {
	const amodinfo_t* m = stdmods;
	while (m->name) {
		aloL_require(T, m->name, m->openf, true);
		alo_settop(T, -1);
		m++;
	}
}
