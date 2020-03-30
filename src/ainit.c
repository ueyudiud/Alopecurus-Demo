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
	{ "_G", aloopen_base },
	{ "@", aloopen_cls },
	{ "math", aloopen_math },
	{ "io", aloopen_io },
	{ "string", aloopen_str },
	{ "list", aloopen_lis },
	{ "table", aloopen_tab },
	{ "sys", aloopen_sys },
	{ "mod", aloopen_mod },
	{ "gc", aloopen_gc },
	{ "thread", aloopen_coro },
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
