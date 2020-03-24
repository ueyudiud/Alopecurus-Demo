/*
 * achr.c
 *
 *  Created on: 2019年8月18日
 *      Author: ueyudiud
 */

#define ACHR_C_
#define ALO_CORE

#include "achr.h"

/**
 ** character data.
 */
ALO_VDEF const struct alo_CharType aloC_chtypes[] = {
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , true , true , false, 0 },
		{ true , true , true , false, 1 },
		{ true , true , true , false, 2 },
		{ true , true , true , false, 3 },
		{ true , true , true , false, 4 },
		{ true , true , true , false, 5 },
		{ true , true , true , false, 6 },
		{ true , true , true , false, 7 },
		{ true , true , true , false, 8 },
		{ true , true , true , false, 9 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, true , true , 10},
		{ true , false, true , true , 11},
		{ true , false, true , true , 12},
		{ true , false, true , true , 13},
		{ true , false, true , true , 14},
		{ true , false, true , true , 15},
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, true , true , 10},
		{ true , false, true , true , 11},
		{ true , false, true , true , 12},
		{ true , false, true , true , 13},
		{ true , false, true , true , 14},
		{ true , false, true , true , 15},
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, true , 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ true , false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
		{ false, false, false, false, 0 },
};
