/*
 * atablib.c
 *
 *  Created on: 2019年9月9日
 *      Author: ueyudiud
 */

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

static int table_filter(astate T) {
	aloL_checktype(T, 0, ALO_TTABLE);
	aloL_checkcall(T, 1);
	int flag = aloL_getoptbool(T, 2, true);
	alo_settop(T, 2);
	size_t n = alo_rawlen(T, 0);
	if (flag) { /* copy to a new table */
		alo_newtable(T, n);
	}
	else { /* put in it self */
		alo_push(T, 0);
	}
	if (n == 0) { /* no elements contained */
		return 1; /* return empty table directly */
	}
	ptrdiff_t i = ALO_ITERATE_BEGIN;
	if (flag) {
		while (alo_inext(T, 0, &i) != ALO_TUNDEF) {
			alo_push(T, 1);
			alo_push(T, 3);
			alo_push(T, 4);
			alo_call(T, 2, 1);
			int f = alo_toboolean(T, 5);
			alo_drop(T);
			if (f) {
				alo_set(T, 2);
			}
			else {
				alo_settop(T, -2);
			}
		}
	}
	else {
		while (alo_push(T, 1), alo_inext(T, 0, &i) != ALO_TUNDEF) {
			alo_call(T, 2, 1);
			if (alo_toboolean(T, 3)) {
				alo_iremove(T, 1, i);
			}
			alo_settop(T, 2);
		}
		alo_drop(T);
	}
	alo_trim(T, 2);
	return 1;
}

static int table_map(astate T) {
	aloL_checktype(T, 0, ALO_TTABLE);
	aloL_checkcall(T, 1);
	int flag = aloL_getoptbool(T, 2, true);
	alo_settop(T, 2);
	size_t n = alo_rawlen(T, 0);
	if (flag) { /* copy to a new table */
		alo_newtable(T, n);
	}
	else { /* put in it self */
		alo_push(T, 0);
	}
	if (n == 0) { /* no elements contained */
		return 1; /* return empty table directly */
	}
	ptrdiff_t i = ALO_ITERATE_BEGIN;
	while (alo_inext(T, 0, &i) != ALO_TUNDEF) {
		alo_push(T, 1);
		alo_push(T, 3);
		alo_push(T, 4);
		alo_call(T, 2, 1);
		alo_pop(T, 4);
		alo_set(T, 2);
	}
	alo_trim(T, 2);
	return 1;
}

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
	ptrdiff_t i = ALO_ITERATE_BEGIN;
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
	{ "filter", table_filter },
	{ "map", table_map },
	{ "mkstr", table_mkstr },
	{ NULL, NULL }
};

int aloopen_tablib(astate T) {
	alo_getreg(T, "__basic_delegates");
	alo_rawgeti(T, -1, ALO_TTABLE);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
