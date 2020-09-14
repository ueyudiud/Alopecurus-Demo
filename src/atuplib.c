/*
 * atuplib.c
 *
 * tuple library.
 *
 *  Created on: 2020年7月5日
 *      Author: ueyudiud
 */

#define ATUPLELIB_C_
#define ALO_LIB

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#define DEFAULT_SEP ", "
#define DEFAULT_LEFT "("
#define DEFAULT_RIGHT ")"

#define defaults(s,l,x) aloE_void((s) = ""x, (l) = (sizeof(x) / sizeof(char) - 1))

/**
 * wrap arguments into tuple.
 * prototype: tuple.create(_,[args...])
 */
static int tup_create(astate T) {
	size_t n = alo_gettop(T) - 1;
	alo_newtuple(T, n);
	return 1;
}

/**
 ** make string from tuple.
 ** prototype: tuple.mkstr(self,[sep]|[left,seq,right])
 */
static int tup_mkstr(astate T) {
	aloL_checktype(T, 0, ALO_TTUPLE);
	size_t l1, l2, l3, l4;
	const char *s1, *s2, *s3, *s4;
	switch (alo_gettop(T)) {
	case 1:
		defaults(s1, l1, DEFAULT_SEP);
		goto lr;
	case 2:
		s1 = aloL_checklstring(T, 1, &l1);
		lr:
		defaults(s2, l2, DEFAULT_LEFT);
		defaults(s3, l3, DEFAULT_RIGHT);
		break;
	case 3:
		aloL_error(T, "illegal arguments: wrong argument count.");
		break;
	default:
		s1 = aloL_checklstring(T, 2, &l1);
		s2 = aloL_checklstring(T, 1, &l2);
		s3 = aloL_checklstring(T, 3, &l3);
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
			/* append first element */
			alo_rawgeti(T, 0, 0);
			s4 = aloL_tostring(T, -1, &l4);
			aloL_bputls(T, buf, s4, l4);
			alo_settop(T, -2);
			/* append forward elements */
			for (size_t i = 1; i < n; ++i) {
				aloL_bputls(T, buf, s1, l1);
				alo_rawgeti(T, 0, i);
				s4 = aloL_tostring(T, -1, &l4);
				aloL_bputls(T, buf, s4, l4);
				alo_settop(T, -2);
			}
			aloL_bputls(T, buf, s3, l3);
		}
		aloL_bpushstring(T, buf);
	)
	return 1;
}

static acreg_t mod_funcs[] = {
	{ "create", tup_create },
	{ "mkstr", tup_mkstr },
	{ NULL, NULL }
};

int aloopen_tup(astate T) {
	alo_getreg(T, "__basic_delegates");
	alo_rawgeti(T, -1, ALO_TTUPLE);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
