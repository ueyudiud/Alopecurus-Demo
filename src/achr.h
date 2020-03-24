/*
 * achr.h
 *
 * character types.
 *
 *  Created on: 2019年8月5日
 *      Author: ueyudiud
 */

#ifndef ACHR_H_
#define ACHR_H_

#include "art.h"

#include <ctype.h>

#define CHAR_COUNT 256

#define aisprint(ch) (aloC_chtypes[aloE_byte(ch)].fprint)
#define aisalpha(ch) (aloC_chtypes[aloE_byte(ch)].falpha)
#define aisdigit(ch) (aloC_chtypes[aloE_byte(ch)].fdigit)
#define aisxdigit(ch) (aloC_chtypes[aloE_byte(ch)].fxdigit)
#define aisident(ch) ({ abyte _ch = aloE_byte(ch); isalnum(_ch) || (_ch) == '_'; })
#define axdigit(ch) (aloC_chtypes[aloE_byte(ch)].digit)

ALO_VDEC const struct alo_CharType {
	abyte fprint : 1;
	abyte fdigit : 1;
	abyte fxdigit : 1;
	abyte falpha : 1;
	abyte digit : 4;
} aloC_chtypes[CHAR_COUNT];

#endif /* ACHR_H_ */
