/*
 * actype.h
 *
 * character types.
 *
 *  Created on: 2019年8月5日
 *      Author: ueyudiud
 */

#ifndef ACTYPE_H_
#define ACTYPE_H_

#include "art.h"

#include <ctype.h>

#define CHAR_COUNT 256

#define aisprint(ch) (aloC_chtypes[aloE_byte(ch)].fprint)
#define aisalpha(ch) (aloC_chtypes[aloE_byte(ch)].falpha)
#define aisdigit(ch) (aloC_chtypes[aloE_byte(ch)].fdigit)
#define aisxdigit(ch) (aloC_chtypes[aloE_byte(ch)].fxdigit)
#define aisident(ch) ({ a_byte _ch = aloE_byte(ch); isalnum(_ch) || (_ch) == '_'; })
#define axdigit(ch) (aloC_chtypes[aloE_byte(ch)].digit)

ALO_VDEC const struct alo_CharType {
	a_byte fprint : 1;
	a_byte fdigit : 1;
	a_byte fxdigit : 1;
	a_byte falpha : 1;
	a_byte digit : 4;
} aloC_chtypes[CHAR_COUNT];

#endif /* ACTYPE_H_ */
