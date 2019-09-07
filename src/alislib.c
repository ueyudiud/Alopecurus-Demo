/*
 * alislib.c
 *
 * library for list.
 *
 *  Created on: 2019年8月9日
 *      Author: ueyudiud
 */

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

/* masks for tagged methods */

static int list_contains(astate T) {
	aloL_checktype(T, 0, ALO_TLIST);
	size_t n = alo_rawlen(T, 0);
	int top = alo_gettop(T);
	for (size_t i = 0; i < n; ++i) {
		alo_rawgeti(T, 0, i);
		for (int j = 1; j < top; ++j) {
			if (alo_equal(T, -1, j)) {
				alo_pushboolean(T, true);
				return 1;
			}
		}
		alo_drop(T);
	}
	alo_pushboolean(T, false);
	return 1;
}

static int list_map(astate T) {
	aloL_checktype(T, 0, ALO_TLIST);
	aloL_checkany(T, 1);
	alo_settop(T, 2);
	size_t n = alo_rawlen(T, 0);
	alo_newlist(T, n);
	for (size_t i = 0; i < n; ++i) {
		alo_push(T, 1);
		alo_rawgeti(T, 0, i);
		alo_call(T, 1, 1);
		alo_rawseti(T, 2, i);
	}
	return 1;
}

static int list_filter(astate T) {
	aloL_checktype(T, 0, ALO_TLIST);
	aloL_checkany(T, 1);
	alo_settop(T, 2);
	size_t n = alo_rawlen(T, 0);
	alo_newlist(T, n);
	size_t j = 0;
	for (size_t i = 0; i < n; ++i) {
		alo_rawgeti(T, 0, i);
		alo_push(T, 1);
		alo_push(T, 3);
		alo_call(T, 1, 1);
		if (alo_toboolean(T, 4)) {
			alo_drop(T);
			alo_rawseti(T, 2, j++);
		}
		else {
			alo_settop(T, -2);
		}
	}
	alo_trim(T, 2);
	return 1;
}

static int list_mkstr(astate T) {
	ambuf_t buf;
	aloL_checktype(T, 0, ALO_TLIST);
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
		break;
	case 4:
		break;
	default:
		alo_settop(T, 4);
		break;
	}
	alo_getreg(T, "tostring");
	aloL_bempty(T, &buf);
	size_t n = alo_rawlen(T, 0);
	aloL_bwrite(&buf, 2);
	for (size_t i = 0; i < n; ++i) {
		if (i != 0) {
			aloL_bwrite(&buf, 1);
		}
		alo_push(T, 4); /* push tostring function */
		alo_rawgeti(T, 0, i);
		alo_call(T, 1, 1);
		aloL_bwrite(&buf, -1);
		alo_drop(T);
	}
	aloL_bwrite(&buf, 3);
	aloL_bpushstring(&buf);
	return 1;
}

static const acreg_t mod_funcs[] = {
	{ "contains", list_contains },
	{ "filter", list_filter },
	{ "map", list_map },
	{ "mkstr", list_mkstr },
	{ NULL, NULL }
};

int aloopen_lislib(astate T) {
	alo_bind(T, "list.contains", list_contains);
	alo_bind(T, "list.filter", list_filter);
	alo_bind(T, "list.map", list_map);
	alo_bind(T, "list.mkstr", list_mkstr);
	alo_getreg(T, "__basic_delegates");
	alo_rawgeti(T, -1, ALO_TLIST);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
