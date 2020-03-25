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
ALO_API void aloL_newstringlist_(astate, size_t, ...);

#define aloL_newstringlist(T,args...) aloL_newstringlist_(T, sizeof((astr[]) { args }) / sizeof(astr), ##args)

ALO_API void aloL_ensure(astate, size_t);
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
ALO_API astr aloL_sreplace(astate, astr, astr, astr);

ALO_API void aloL_setfuns(astate, aindex_t, const acreg_t*);

ALO_API int aloL_getmetatable(astate, astr);
ALO_API int aloL_getsubtable(astate, aindex_t, astr);
ALO_API int aloL_getsubfield(astate, aindex_t, astr, astr);
ALO_API int aloL_callselfmeta(astate, aindex_t, astr);

#define aloL_getmodfield(T,mod,key) aloL_getsubfield(T, ALO_REGISTRY_INDEX, mod, key)

ALO_API void aloL_require(astate, astr, acfun, int);
ALO_API int aloL_compiles(astate, aindex_t, astr, astr);
ALO_API int aloL_compilef(astate, astr, astr);
ALO_API int aloL_loadf(astate, astr);
ALO_API int aloL_savef(astate, astr, int);

/**
 ** error handling
 */

ALO_API int aloL_getframe(astate, int, astr, aframeinfo_t*);
ALO_API void aloL_where(astate, int);
ALO_API anoret aloL_error(astate, int, astr, ...);

ALO_API anoret aloL_argerror(astate, aindex_t, astr, ...);
ALO_API anoret aloL_typeerror(astate, aindex_t, astr);
ALO_API anoret aloL_tagerror(astate, aindex_t, int);

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

#define aloL_usebuf(T,name) \
	for (ambuf_t name##$data, *name = (alo_pushbuf(T, &name##$data), &name##$data); \
		name##$data.buf; \
		alo_popbuf(T, name))
#define aloL_bputc(T,b,ch) (aloE_cast(void, (b)->len < (b)->cap || (aloL_bcheck(T, b, 1), true)), (b)->buf[(b)->len++] = aloE_byte(ch))
#define aloL_bputls(T,b,s,l) aloL_bputm(T, b, s, (l) * sizeof(char))
#define aloL_bputxs(T,b,s) aloL_bputls(T, b, ""s, sizeof(s) / sizeof(char) - 1)
#define aloL_bsetc(b,i,ch) ((b)->buf[i] = aloE_byte(ch))
#define aloL_blen(b) ((b)->len) /* readable and writable */
#define aloL_braw(b) aloE_cast(abyte*, (b)->buf) /* read-only */
#define aloL_bcheck(T,b,l) \
		({ \
			ambuf_t* $buf = (b); \
			size_t $req = $buf->len + (l); \
			if ($req > $buf->cap) alo_growbuf(T, $buf, $req); \
		})

ALO_API void aloL_bputm(astate, ambuf_t*, const void*, size_t);
ALO_API void aloL_bputs(astate, ambuf_t*, astr);
ALO_API void aloL_bputf(astate, ambuf_t*, astr, ...);
ALO_API void aloL_bputvf(astate, ambuf_t*, astr, va_list);
ALO_API void aloL_bwrite(astate, ambuf_t*, aindex_t);
ALO_API astr aloL_bpushstring(astate, ambuf_t*);

ALO_API int aloL_bwriter(astate, void*, const void*, size_t);

#endif /* AAUX_H_ */
