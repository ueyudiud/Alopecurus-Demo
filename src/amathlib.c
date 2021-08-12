/*
 * amathlib.c
 *
 * math library
 *
 *  Created on: 2019年9月1日
 *      Author: ueyudiud
 */

#define AMATHLIB_C_
#define ALO_LIB

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#define _USE_MATH_DEFINES

#include <math.h>
#include <stdlib.h>

#if defined(M_PI) && defined(M_E)

#define PI aloE_flt(M_PI)
#define E aloE_flt(M_E)

#else

#define PI aloE_flt(3.1415926535897932384626433832795)
#define E aloE_flt(2.7182818284590452353602874713527)

#endif

static int math_abs(alo_State T) {
	a_bool flag;
	a_int i = alo_tointegerx(T, 0, &flag);
	if (flag) {
		alo_pushinteger(T, llabs(i));
	}
	else {
		alo_pushnumber(T, fabs(aloL_checknumber(T, 0)));
	}
	return 1;
}

static int math_min(alo_State T) {
	int n = alo_gettop(T);
	if (n == 0) {
		aloL_argerror(T, 0, "value expected");
	}
	int min = 0;
	for (int i = 1; i < n; ++i) {
		if (alo_compare(T, i, min, ALO_OPLT)) {
			min = i;
		}
	}
	alo_push(T, min);
	return 1;
}

static int math_max(alo_State T) {
	int n = alo_gettop(T);
	if (n == 0) {
		aloL_argerror(T, 0, "value expected");
	}
	int max = 0;
	for (int i = 1; i < n; ++i) {
		if (alo_compare(T, max, i, ALO_OPLT)) {
			max = i;
		}
	}
	alo_push(T, max);
	return 1;
}

static int math_deg(alo_State T) {
	alo_pushnumber(T, aloL_checknumber(T, 0) * (180.0 / PI));
	return 1;
}

static int math_rad(alo_State T) {
	alo_pushnumber(T, aloL_checknumber(T, 0) * (PI / 180.0));
	return 1;
}

static int math_sqrt(alo_State T) {
	alo_pushnumber(T, sqrt(aloL_checknumber(T, 0)));
	return 1;
}

static int math_cbrt(alo_State T) {
	alo_pushnumber(T, cbrt(aloL_checknumber(T, 0)));
	return 1;
}

static int math_exp(alo_State T) {
	alo_pushnumber(T, exp(aloL_checknumber(T, 0)));
	return 1;
}

static int math_sin(alo_State T) {
	alo_pushnumber(T, sin(aloL_checknumber(T, 0)));
	return 1;
}

static int math_cos(alo_State T) {
	alo_pushnumber(T, cos(aloL_checknumber(T, 0)));
	return 1;
}

static int math_tan(alo_State T) {
	alo_pushnumber(T, tan(aloL_checknumber(T, 0)));
	return 1;
}

static int math_ln(alo_State T) {
	alo_pushnumber(T, log(aloL_checknumber(T, 0)));
	return 1;
}

static int math_log(alo_State T) {
	alo_pushnumber(T, log(aloL_checknumber(T, 0)) / log(aloL_getoptnumber(T, 1, E)));
	return 1;
}

static int math_asin(alo_State T) {
	alo_pushnumber(T, asin(aloL_checknumber(T, 0)));
	return 1;
}

static int math_acos(alo_State T) {
	alo_pushnumber(T, acos(aloL_checknumber(T, 0)));
	return 1;
}

static int math_atan(alo_State T) {
	alo_pushnumber(T, atan2(aloL_checknumber(T, 0), aloL_getoptnumber(T, 1, 1)));
	return 1;
}

static int math_floor(alo_State T) {
	a_bool flag;
	a_int i = alo_tointegerx(T, 0, &flag);
	if (flag) {
		alo_pushinteger(T, i);
	}
	else {
		alo_pushnumber(T, floor(aloL_checknumber(T, 0)));
	}
	return 1;
}

static int math_ceil(alo_State T) {
	a_bool flag;
	a_int i = alo_tointegerx(T, 0, &flag);
	if (flag) {
		alo_pushinteger(T, i);
	}
	else {
		alo_pushnumber(T, ceil(aloL_checknumber(T, 0)));
	}
	return 1;
}

static int math_erf(alo_State T) {
	alo_pushnumber(T, erf(aloL_checknumber(T, 0)));
	return 1;
}

static int math_erfc(alo_State T) {
	alo_pushnumber(T, erfc(aloL_checknumber(T, 0)));
	return 1;
}

static int math_tgamma(alo_State T) {
	alo_pushnumber(T, tgamma(aloL_checknumber(T, 0)));
	return 1;
}

static int math_lgamma(alo_State T) {
	alo_pushnumber(T, lgamma(aloL_checknumber(T, 0)));
	return 1;
}

#if defined(ALO_USE_POSIX)

#define l_srand48 srand48
#define l_drand48 drand48

#else

static uint64_t seed = 1;

static double l_drand48(void) {
	seed = (0x5DEECE66D * seed + 0xB) & ((UINT64_C(1) << 48) - 1);
	return aloE_flt(seed >> 16) / aloE_flt(0x1.0p32);
}

static void l_srand48(uint32_t value) {
	seed = value << 16 | 0x330E;
}

#endif

static int math_setseed(alo_State T) {
	l_srand48(aloL_checkinteger(T, 0));
	return 0;
}

static int math_random(alo_State T) {
	alo_pushnumber(T, l_drand48());
	return 1;
}

static const acreg_t mod_funcs[] = {
	{ "E", NULL },
	{ "INTMAX", NULL },
	{ "INTMIN", NULL },
	{ "NAN", NULL },
	{ "PI", NULL },
	{ "abs", math_abs },
	{ "acos", math_acos },
	{ "asin", math_asin },
	{ "atan", math_atan },
	{ "cbrt", math_cbrt },
	{ "ceil", math_ceil },
	{ "cos", math_cos },
	{ "deg", math_deg },
	{ "erf", math_erf },
	{ "erfc", math_erfc },
	{ "exp", math_exp },
	{ "floor", math_floor },
	{ "lgamma", math_lgamma },
	{ "log", math_log },
	{ "ln", math_ln },
	{ "max", math_max },
	{ "min", math_min },
	{ "rad", math_rad },
	{ "random", math_random },
	{ "setseed", math_setseed },
	{ "sin", math_sin },
	{ "sqrt", math_sqrt },
	{ "tan", math_tan },
	{ "tgamma", math_tgamma },
	{ NULL, NULL }
};

int aloopen_math(alo_State T) {
	alo_newtable(T, 0);
	aloL_setfuns(T, -1, mod_funcs);
	alo_pushnumber(T, NAN);
	alo_rawsets(T, -2, "NAN");
	alo_pushnumber(T, PI);
	alo_rawsets(T, -2, "PI");
	alo_pushnumber(T, E);
	alo_rawsets(T, -2, "E");
	alo_pushinteger(T, ALO_INT_PROP(MAX));
	alo_rawsets(T, -2, "INTMAX");
	alo_pushinteger(T, ALO_INT_PROP(MIN));
	alo_rawsets(T, -2, "INTMIN");
	return 1;
}
