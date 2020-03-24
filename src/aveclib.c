/*
 * aveclib.c
 *
 * vector calculus library
 *
 *  Created on: 2019年9月1日
 *      Author: ueyudiud
 */

#define AVECLIB_C_
#define ALO_LIB

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

static void convx(astate T, int u, int v) {
	size_t p = alo_rawlen(T, u);
	size_t q = alo_rawlen(T, v);
	if (p > 0 && q > 0) {
		size_t r = p + q - 1;
		alo_ensure(T, r + 2);
		int i = 1;
		for (i = 0; i < p && i < q; ++i) {
			alo_rawgeti(T, u, i);
			alo_rawgeti(T, v, 0);
			alo_arith(T, ALO_OPMUL);
			for (int j = 1; j <= i; ++j) {
				alo_rawgeti(T, u, i - j);
				alo_rawgeti(T, v, j);
				alo_arith(T, ALO_OPMUL);
				alo_arith(T, ALO_OPADD);
			}
		}
		for (; i < r; ++i) {
			alo_rawgeti(T, u, p - 1);
			alo_rawgeti(T, v, i - p + 1);
			alo_arith(T, ALO_OPMUL);
			for (int j = 1; j < r - i; ++j) {
				alo_rawgeti(T, u, p - 1 - j);
				alo_rawgeti(T, v, i - p + 1 + j);
				alo_arith(T, ALO_OPMUL);
				alo_arith(T, ALO_OPADD);
			}
		}
		alo_newtuple(T, r);
	}
	else {
		alo_newtuple(T, 0);
	}
}

static int vec_conv(astate T) {
	int n = alo_gettop(T);
	switch (n) {
	case 0: /* no type, throw error from type checking. */
	case 1:
		aloL_checktype(T, 0, ALO_TTUPLE);
		return 1;
	case 2:
		aloL_checktype(T, 0, ALO_TTUPLE);
		aloL_checktype(T, 1, ALO_TTUPLE);
		convx(T, 0, 1);
		return 1;
	default:
		aloL_checktype(T, 0, ALO_TTUPLE);
		for (int i = 1; i < n; ++i) {
			aloL_checktype(T, i, ALO_TTUPLE);
			convx(T, 0, i);
			if (i != n - 1) {
				alo_pop(T, 0);
			}
		}
		return 1;
	}
}

static const acreg_t mod_funcs[] = {
	{ "conv", vec_conv },
	{ NULL, NULL }
};

int aloopen_veclib(astate T) {
	alo_newtable(T, 0);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}


