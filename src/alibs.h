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

ALO_API int aloopen_gc(astate);

ALO_API int aloopen_mod(astate);

ALO_API int aloopen_cls(astate);

ALO_API int aloopen_coro(astate);

ALO_API int aloopen_str(astate);

ALO_API int aloopen_lis(astate);

ALO_API int aloopen_tab(astate);

ALO_API int aloopen_math(astate);

ALO_API int aloopen_io(astate);

ALO_API int aloopen_sys(astate);

ALO_API void aloL_openlibs(astate);

#endif /* ALIBS_H_ */
