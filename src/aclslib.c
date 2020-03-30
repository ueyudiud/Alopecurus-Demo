/*
 * aclslib.c
 *
 * library for class, this is inner library for Alopecurus
 *
 *  Created on: 2019年7月31日
 *      Author: ueyudiud
 */

#define ACLSLIB_C_
#define ALO_LIB

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#include <string.h>

/**
 ** create new class.
 ** prototype: class.meta.__new(classes, name, parents...)
 */
static int class_meta_new(astate T) {
	aloL_checktype(T, 0, ALO_TTABLE);
	aloL_checkclassname(T, 1);
	const char* name = alo_tostring(T, 1);
	if (alo_rawgets(T, 0, name) != ALO_TUNDEF) {
		aloL_error(T, "class with name '%s' already registered.", name);
	}
	alo_drop(T);

	ssize_t n = alo_gettop(T) - 2;

	for (ssize_t i = 0; i < n; ++i) {
		aloL_checktype(T, 2 + i, ALO_TTABLE);
	}

	/* create new class table */
	alo_newtable(T, 4);

	/* place class into '@' table */
	alo_push(T, -1);
	alo_rawsets(T, 0, name);

	/* set 'id' entry */
	alo_push(T, 1);
	alo_rawsets(T, -2, "__id");

	/* set 'parents' entry */
	alo_newlist(T, n);
	alo_push(T, -1);
	alo_rawsets(T, -3, "__parents");
	for (ssize_t i = 0; i < n; ++i) {
		alo_push(T, 2 + i);
		alo_rawseti(T, -2, i);
	}
	alo_drop(T);

	/* set 'lookup' entry */
	alo_newlist(T, n);
	alo_push(T, -1);
	alo_rawsets(T, -3, "__lkup");
	for (ssize_t i = n - 1; i >= 0; --i) {
		switch (alo_rawgets(T, 2 + i, "__lkup")) {
		case ALO_TLIST: { /* MRO lookup */
			size_t m = alo_rawlen(T, -1);
			for (size_t j = 0; j < m; ++j) {
				alo_rawgeti(T, -1, j);
				alo_put(T, -3);
			}
			break;
		}
		case ALO_TTABLE: case ALO_TFUNCTION: { /* enclosed lookup */
			alo_put(T, -2);
			break;
		}
		case ALO_TUNDEF: case ALO_TNIL: { /* no look up */
			break;
		}
		default: {
			aloL_error(T, "illegal look up target.");
			break;
		}
		}
		alo_drop(T);
		alo_push(T, 2 + i); /* put parent class into list. */
		alo_put(T, -2);
	}
	alo_drop(T);
	return 1;
}

static int class_meta_newindex(astate T) {
	aloL_error(T, "can not put object into '@' directly, use 'new' function instead.");
	return 0;
}

static const acreg_t mod_funcs[] = {
	{ "__call", class_meta_new },
	{ "__nidx", class_meta_newindex },
	{ NULL, NULL }
};

int aloopen_cls(astate T) {
	alo_bind(T, "class.meta.__new", class_meta_new);
	alo_bind(T, "class.meta.__nidx", class_meta_newindex);
	alo_newtable(T, 16);
	alo_newtable(T, 16);
	aloL_setfuns(T, -1, mod_funcs);
	alo_setmetatable(T, -2);
	return 1;
}


