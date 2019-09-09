/*
 * atablib.c
 *
 *  Created on: 2019年9月9日
 *      Author: ueyudiud
 */

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

static void l_mkstr_checkarg(astate T) {
	aloL_checktype(T, 0, ALO_TTABLE);
	switch (alo_gettop(T)) {
	case 1:
		alo_pushstring(T, ", ");
		goto left;
	case 2:
		aloL_checkstring(T, 1);
		left:
		alo_pushstring(T, "[");
		goto right;
	case 3:
		aloL_checkstring(T, 2);
		right:
		alo_pushstring(T, "]");
		goto sep;
	case 4:
		aloL_checkstring(T, 3);
		sep:
		alo_pushstring(T, ": ");
		break;
	case 5:
		break;
	default:
		alo_settop(T, 5);
		break;
	}
	alo_getreg(T, "tostring");
}

static int table_mkstr(astate T) {
	ambuf_t buf;
	l_mkstr_checkarg(T);
	aloL_bempty(T, &buf);
	ptrdiff_t i = 0;
	int first = true;
	aloL_bwrite(&buf, 2);
	while (alo_inext(T, 0, &i) != ALO_TUNDEF) {
		if (!first) {
			aloL_bwrite(&buf, 1);
		}
		else {
			first = false;
		}
		alo_push(T, 5); /* push tostring function */
		alo_push(T, -3);
		alo_call(T, 1, 1);
		aloL_bwrite(&buf, -1);
		alo_drop(T);
		aloL_bwrite(&buf, 4);
		alo_push(T, 5); /* push tostring function */
		alo_push(T, -2);
		alo_call(T, 1, 1);
		aloL_bwrite(&buf, -1);
		alo_settop(T, -3);
	}
	aloL_bwrite(&buf, 3);
	aloL_bpushstring(&buf);
	return 1;
}

static const acreg_t mod_funcs[] = {
	{ "mkstr", table_mkstr },
	{ NULL, NULL }
};

int aloopen_tablib(astate T) {
	alo_getreg(T, "__basic_delegates");
	alo_rawgeti(T, -1, ALO_TTABLE);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
