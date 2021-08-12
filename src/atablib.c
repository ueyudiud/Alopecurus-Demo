/*
 * atablib.c
 *
 *  Created on: 2019年9月9日
 *      Author: ueyudiud
 */

#define ATABLIB_C_
#define ALO_LIB

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

/**
 ** allocate new table.
 ** prototype: table.__alloc()
 */
static int table_alloc(alo_State T) {
	alo_newtable(T, 0);
	return 1;
}

/**
 ** create a new table with capacity (the table is already allocated).
 ** prototype: table.__new(initial_capacity)
 */
static int table_new(alo_State T) {
	aloL_checktype(T, 0, ALO_TTABLE);
	if (!alo_isnothing(T, 1)) {
		if (alo_typeid(T, 1) == ALO_TTABLE) {
			size_t n = alo_rawlen(T, 1);
			alo_sizehint(T, 0, n);
			a_itr itr = alo_ibegin(T, 1);
			while (alo_inext(T, 0, &itr)) {
				alo_rawset(T, 0);
			}
		}
		else {
			alo_sizehint(T, 0, aloL_checkinteger(T, 1));
		}
	}
	return 0;
}

static int table_clear(alo_State T) {
	aloL_checktype(T, 0, ALO_TTABLE);
	alo_rawclr(T, 0);
	return 0;
}

static int table_filter(alo_State T) {
	aloL_checktype(T, 0, ALO_TTABLE);
	aloL_checkcall(T, 1);
	int flag = aloL_getoptbool(T, 2, true);
	alo_settop(T, 2);
	size_t n = alo_rawlen(T, 0);
	if (flag) { /* copy to a new table */
		alo_newtable(T, n);
		if (n == 0) {
			return 1;
		}
		a_itr i = alo_ibegin(T, 0);
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
	else { /* put in it self */
		if (n > 0) {
			a_itr i = alo_ibegin(T, 0);
			while (alo_push(T, 1), alo_inext(T, 0, &i) != ALO_TUNDEF) {
				alo_call(T, 2, 1);
				if (!alo_toboolean(T, 2)) {
					alo_iremove(T, 0, &i);
				}
				alo_settop(T, 2);
			}
			alo_drop(T);
		}
	}
	alo_trim(T, 0);
	alo_push(T, 0);
	return 1;
}

static int table_map(alo_State T) {
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
	a_itr i = alo_ibegin(T, 0);
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

#define DEFAULT_SEP ", "
#define DEFAULT_LEFT "["
#define DEFAULT_RIGHT "]"
#define DEFAULT_CAT ": "

#define defaults(s,l,x) aloE_void((s) = ""x, (l) = (sizeof(x) / sizeof(char) - 1))

static int table_mkstr(alo_State T) {
	aloL_checktype(T, 0, ALO_TTABLE);
	size_t l1, l2, l3, l4, l5;
	const char *s1, *s2, *s3, *s4, *s5;
	switch (alo_gettop(T)) {
	case 1:
		defaults(s1, l1, DEFAULT_SEP);
		goto lr;
	case 2:
		s1 = aloL_checklstring(T, 1, &l1);
		lr:
		defaults(s2, l2, DEFAULT_LEFT);
		defaults(s3, l3, DEFAULT_RIGHT);
		goto ct;
	case 3:
		aloL_error(T, "illegal arguments: wrong argument count.");
		break;
	case 4:
		s1 = aloL_checklstring(T, 2, &l1);
		s2 = aloL_checklstring(T, 1, &l2);
		s3 = aloL_checklstring(T, 3, &l3);
		ct:
		defaults(s4, l4, DEFAULT_CAT);
		break;
	default:
		s1 = aloL_checklstring(T, 2, &l1);
		s2 = aloL_checklstring(T, 1, &l2);
		s3 = aloL_checklstring(T, 3, &l3);
		s4 = aloL_checklstring(T, 4, &l4);
		break;
	}
	size_t n = alo_rawlen(T, 0);
	aloL_usebuf(T, buf,
		if (n == 0) {
			aloL_bputls(T, buf, s2, l2);
			aloL_bputls(T, buf, s3, l3);
		}
		else {
			aloL_bputls(T, buf, s2, l2);
			a_itr i = alo_ibegin(T, 0);
			/* append first element */
			alo_inext(T, 0, &i);
			s5 = aloL_tostring(T, -2, &l5);
			aloL_bputls(T, buf, s5, l5);
			alo_drop(T);
			aloL_bputls(T, buf, s4, l4);
			s5 = aloL_tostring(T, -1, &l5);
			aloL_bputls(T, buf, s5, l5);
			alo_settop(T, -3);
			/* append forward elements */
			while (alo_inext(T, 0, &i) != ALO_TUNDEF) {
				aloL_bputls(T, buf, s1, l1);
				s5 = aloL_tostring(T, -2, &l5);
				aloL_bputls(T, buf, s5, l5);
				alo_drop(T);
				aloL_bputls(T, buf, s4, l4);
				s5 = aloL_tostring(T, -1, &l5);
				aloL_bputls(T, buf, s5, l5);
				alo_settop(T, -3);
			}
			aloL_bputls(T, buf, s3, l3);
		}
		aloL_bpushstring(T, buf);
	)
	return 1;
}

static const acreg_t mod_funcs[] = {
	{ "clear", table_clear },
	{ "filter", table_filter },
	{ "map", table_map },
	{ "mkstr", table_mkstr },
	{ "__alloc", table_alloc },
	{ "__new", table_new },
	{ NULL, NULL }
};

int aloopen_tab(alo_State T) {
	alo_getreg(T, "__basic_delegates");
	alo_rawgeti(T, -1, ALO_TTABLE);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
