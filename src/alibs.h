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

ALO_API int aloopen_base(alo_State);

#define ALO_GCLIB_NAME "gc"
ALO_API int aloopen_gc(alo_State);

#define ALO_MODLIB_NAME "mod"
ALO_API int aloopen_mod(alo_State);

#define ALO_CLSLIB_NAME "@"
ALO_API int aloopen_cls(alo_State);

#define ALO_COROLIB_NAME "thread"
ALO_API int aloopen_coro(alo_State);

#define ALO_STRLIB_NAME "string"
ALO_API int aloopen_str(alo_State);

#define ALO_TUPLELIB_NAME "tuple"
ALO_API int aloopen_tup(alo_State);

#define ALO_LISTLIB_NAME "list"
ALO_API int aloopen_lis(alo_State);

#define ALO_TABLELIB_NAME "table"
ALO_API int aloopen_tab(alo_State);

#define ALO_MATHLIB_NAME "math"
ALO_API int aloopen_math(alo_State);

#define ALO_IOLIB_NAME "io"
ALO_API int aloopen_io(alo_State);

#define ALO_SYSLIB_NAME "sys"
ALO_API int aloopen_sys(alo_State);

ALO_API void aloL_openlibs(alo_State);

#endif /* ALIBS_H_ */
