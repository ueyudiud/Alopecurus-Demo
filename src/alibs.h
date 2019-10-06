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

ALO_API int aloopen_baselib(astate);

ALO_API int aloopen_modlib(astate);

ALO_API int aloopen_clslib(astate);

ALO_API int aloopen_strlib(astate);

ALO_API int aloopen_lislib(astate);

ALO_API int aloopen_tablib(astate);

ALO_API int aloopen_mathlib(astate);

ALO_API int aloopen_veclib(astate);

ALO_API int aloopen_iolib(astate);

ALO_API int aloopen_syslib(astate);

void aloL_openlibs(astate);

#endif /* ALIBS_H_ */
