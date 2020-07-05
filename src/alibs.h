/*
 * alibs.h
 *
 *  Created on: 2019年7月29日
 *      Author: ueyudiud
 */

#ifndef ALIBS_H_
#define ALIBS_H_

#include "alo.h"

#define ALO_LOADEDMODS "_MODS"

ALO_API int aloopen_base(astate);

#define ALO_GCLIB_NAME "gc"
ALO_API int aloopen_gc(astate);

#define ALO_MODLIB_NAME "mod"
ALO_API int aloopen_mod(astate);

#define ALO_CLSLIB_NAME "@"
ALO_API int aloopen_cls(astate);

#define ALO_COROLIB_NAME "thread"
ALO_API int aloopen_coro(astate);

#define ALO_STRLIB_NAME "string"
ALO_API int aloopen_str(astate);

#define ALO_TUPLELIB_NAME "tuple"
ALO_API int aloopen_tup(astate);

#define ALO_LISTLIB_NAME "list"
ALO_API int aloopen_lis(astate);

#define ALO_TABLELIB_NAME "table"
ALO_API int aloopen_tab(astate);

#define ALO_MATHLIB_NAME "math"
ALO_API int aloopen_math(astate);

#define ALO_IOLIB_NAME "io"
ALO_API int aloopen_io(astate);

#define ALO_SYSLIB_NAME "sys"
ALO_API int aloopen_sys(astate);

ALO_API void aloL_openlibs(astate);

#endif /* ALIBS_H_ */
