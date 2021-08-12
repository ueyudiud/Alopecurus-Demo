/*
 * aload.h
 *
 * load and save compiled scripts.
 *
 *  Created on: 2019年9月2日
 *      Author: ueyudiud
 */

#ifndef ALOAD_H_
#define ALOAD_H_

#include "aobj.h"
#include "abuf.h"

/* the signature of file head, means "Alo" */
#define ALOZ_SIGNATURE "\x41\x6c\x6f\x00"

/* the extra data to check file correct */
#define ALOZ_DATA "\x20\x19\x09\x02"

#define ALOZ_INT aloE_cast(a_int, 0x9876)
#define ALOZ_FLT aloE_cast(a_float, 23.33)

#define ALOZ_FORMAT 0

enum {
	/* since 0.1 */
	CT_NIL,
	CT_TRUE,
	CT_FALSE,
	CT_INT,
	CT_FLT,
	CT_XSTR,
	CT_SSTR,
	CT_LSTR
};

ALO_IFUN void aloZ_delete(alo_State, alo_Proto*);

/* load chunk, from aload.c */
ALO_IFUN int aloZ_load(alo_State, alo_Proto**, a_cstr, alo_Reader, void*);

/* save chunk, from asave.c */
ALO_IFUN int aloZ_save(alo_State, const alo_Proto*, alo_Writer, void*, int);

#endif /* ALOAD_H_ */
