/*
 * aaux.h
 *
 * auxiliary API.
 *
 *  Created on: 2019年7月26日
 *      Author: ueyudiud
 */

#ifndef AAUX_H_
#define AAUX_H_

#include "alo.h"

ALO_API astate aloL_newstate(void);

ALO_API void aloL_pushscopedcfunction(astate, acfun);

ALO_API void aloL_checkany(astate, aindex_t);
ALO_API void aloL_checktype(astate, aindex_t, int);
ALO_API int aloL_checkbool(astate, aindex_t);
ALO_API int aloL_getoptbool(astate, aindex_t, int);
ALO_API aint aloL_checkinteger(astate, aindex_t);
ALO_API aint aloL_getoptinteger(astate, aindex_t, aint);
ALO_API afloat aloL_checknumber(astate, aindex_t);
ALO_API afloat aloL_getoptnumber(astate, aindex_t, afloat);
ALO_API const char* aloL_checklstring(astate, aindex_t, size_t*);
ALO_API const char* aloL_getoptlstring(astate, aindex_t, const char*, size_t, size_t*);
ALO_API int aloL_checkenum(astate, aindex_t, astr, const astr[]);

#define aloL_checkstring(T,index) aloL_checklstring(T, index, NULL)
#define aloL_getoptstring(T,index,def) aloL_getoptlstring(T, index, def, 0, NULL)

ALO_API void aloL_checkcall(astate, aindex_t);
ALO_API const char* aloL_tostring(astate, aindex_t, size_t*);

ALO_API void aloL_setfuns(astate, aindex_t, const acreg_t*);

ALO_API int aloL_getmetatable(astate, astr);
ALO_API int aloL_getsubtable(astate, aindex_t, astr);
ALO_API int aloL_getsubfield(astate, aindex_t, astr, astr);
ALO_API int aloL_callselfmeta(astate, aindex_t, astr);

#define aloL_getmodfield(T,mod,key) aloL_getsubfield(T, ALO_REGISTRY_INDEX, mod, key)

ALO_API void aloL_require(astate, astr, acfun, int);
ALO_API int aloL_compiles(astate, aindex_t, astr, astr);

/**
 ** error handling
 */

ALO_API void aloL_appendwhere_(astate, int, astr, int);
ALO_API void aloL_where(astate, int);
ALO_API void aloL_error_(astate, int, astr, int, astr, ...);

#define aloL_appendwhere(T,level) aloL_appendwhere_(T, level, __FILE__, __LINE__)
#define aloL_error(T,level,fmt,args...) aloL_error_(T, level, __FILE__, __LINE__, fmt, ##args)

ALO_API void aloL_argerror(astate, aindex_t, astr, ...);
ALO_API void aloL_typeerror(astate, aindex_t, astr);
ALO_API void aloL_tagerror(astate, aindex_t, int);

/**
 ** base layer of messaging and logging.
 */

#define aloL_fflush(file) fflush(file)
#define aloL_fputs(file,src,len) fwrite(src, sizeof(char), len, file)
#define aloL_fputc(file,ch) fputc(ch, file)
#define aloL_fputln(file) (aloL_fputs(file, "\n", 1), aloL_fflush(file))

/**============================================================================
 ** the contents below this line only takes effect with standard class library
 **============================================================================*/

ALO_API void aloL_checkclassname(astate, aindex_t);
ALO_API int aloL_getsimpleclass(astate, astr);
ALO_API void aloL_newclass_(astate, astr, ...);

#define aloL_newclass(T,name,parents...) aloL_newclass_(T, name, ##parents, NULL)

/**
 ** functions for memory buffer.
 */

/** the minimum heaped memory buffer capacity */
#define ALOL_MBUFSIZE (256 * __SIZEOF_POINTER__)

typedef struct aloL_MemoryBuffer {
	size_t c; /* buffer capacity */
	size_t l; /* length of current data */
	abyte* p; /* start position of buffer */
	astate T;
	void* b; /* box in stack */
} ambuf_t;

#define aloL_bempty(T,buf) aloL_binit_(T, buf, NULL, 0)
#define aloL_binit(T,buf,mem) aloL_binit_(T, buf, mem, sizeof(mem))
#define aloL_bputc(buf,ch) (aloE_cast(void, (buf)->l < (buf)->c || (aloL_bcheck(buf, 1), true)), (buf)->p[(buf)->l++] = aloE_byte(ch))
#define aloL_bsetc(buf,i,ch) ((buf)->p[i] = aloE_byte(ch))
#define aloL_bsetlen(buf,len) ((buf)->l = (len))

ALO_API void aloL_binit_(astate, ambuf_t*, amem, size_t);
ALO_API void aloL_bcheck(ambuf_t*, size_t);
ALO_API void aloL_bputm(ambuf_t*, const void*, size_t);
ALO_API void aloL_bputls(ambuf_t*, const char*, size_t);
ALO_API void aloL_bputs(ambuf_t*, astr);
ALO_API void aloL_bputf(ambuf_t*, astr, ...);
ALO_API void aloL_bputvf(ambuf_t*, astr, va_list);
ALO_API void aloL_bwrite(ambuf_t*, aindex_t);
ALO_API void aloL_bpushstring(ambuf_t*);

ALO_API int aloL_bwriter(astate, void*, const void*, size_t);

#endif /* AAUX_H_ */
