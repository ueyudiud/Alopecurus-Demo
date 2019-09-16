/*
 * ainit.c
 *
 *  Created on: 2019年7月29日
 *      Author: ueyudiud
 */

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
	{ "vec", aloopen_veclib },
	{ "sys", aloopen_syslib },
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
