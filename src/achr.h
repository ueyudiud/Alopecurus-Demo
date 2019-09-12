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

#include "adef.h"

#include <ctype.h>

#define CHAR_COUNT 256

#define aisprint(ch) (ALO_CHAR_TYPES[ch].fprint)
#define aisalpha(ch) (ALO_CHAR_TYPES[ch].falpha)
#define aisdigit(ch) (ALO_CHAR_TYPES[ch].fdigit)
#define aisxdigit(ch) (ALO_CHAR_TYPES[ch].fxdigit)
#define aisident(ch) ({ int _ch = (ch); isalnum(_ch) || (_ch) == '_'; })
#define axdigit(ch) (ALO_CHAR_TYPES[ch].digit)

extern const struct alo_CharType {
	abyte fprint : 1;
	abyte fdigit : 1;
	abyte fxdigit : 1;
	abyte falpha : 1;
	abyte digit : 4;
} ALO_CHAR_TYPES[CHAR_COUNT];

#endif /* ACHR_H_ */
