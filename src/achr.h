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

#define aisprint isprint
#define aisalpha isalpha
#define aisdigit isdigit
#define aisxdigit isxdigit
#define aisprint isprint
#define aisident(ch) ({ int _ch = (ch); isalnum(_ch) || (_ch) == '_'; })
#define axdigit(ch) (ALO_CHAR_TYPES[ch].digit)

const struct alo_CharType {
	abyte fprint : 1;
	abyte fdigit : 1;
	abyte fxdigit : 1;
	abyte falpha : 1;
	abyte digit : 4;
} ALO_CHAR_TYPES[CHAR_COUNT];

#endif /* ACHR_H_ */
