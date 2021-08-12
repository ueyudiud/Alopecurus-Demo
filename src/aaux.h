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

ALO_API alo_State aloL_newstate(void);

#define ALOL_SIGNATURE (sizeof(a_int) << 4 | sizeof(a_float))

ALO_API void aloL_checkversion_(alo_State, aver_t, size_t);

#define aloL_checkversion(T) aloL_checkversion_(T, (aver_t) { ALO_VERSION_NUM }, ALOL_SIGNATURE)

ALO_API void aloL_pushscopedcfunction(alo_State, a_cfun);
ALO_API void aloL_newstringlist_(alo_State, size_t, ...);
ALO_API void aloL_newweaktable(alo_State, a_cstr, size_t);

#define aloL_newstringlist(T,args...) aloL_newstringlist_(T, sizeof((astr[]) { args }) / sizeof(astr), ##args)

ALO_API void aloL_ensure(alo_State, size_t);
ALO_API void aloL_checkany(alo_State, ssize_t);
ALO_API void aloL_checktype(alo_State, ssize_t, int);
ALO_API int aloL_checkbool(alo_State, ssize_t);
ALO_API int aloL_getoptbool(alo_State, ssize_t, int);
ALO_API a_int aloL_checkinteger(alo_State, ssize_t);
ALO_API a_int aloL_getoptinteger(alo_State, ssize_t, a_int);
ALO_API a_float aloL_checknumber(alo_State, ssize_t);
ALO_API a_float aloL_getoptnumber(alo_State, ssize_t, a_float);
ALO_API const char* aloL_checklstring(alo_State, ssize_t, size_t*);
ALO_API const char* aloL_getoptlstring(alo_State, ssize_t, const char*, size_t, size_t*);
ALO_API int aloL_checkenum(alo_State, ssize_t, a_cstr, const a_cstr[]);

#define aloL_checkstring(T,index) aloL_checklstring(T, index, NULL)
#define aloL_getoptstring(T,index,def) aloL_getoptlstring(T, index, def, 0, NULL)

ALO_API void aloL_checkcall(alo_State, ssize_t);
ALO_API const char* aloL_tostring(alo_State, ssize_t, size_t*);
ALO_API a_cstr aloL_sreplace(alo_State, a_cstr, a_cstr, a_cstr);

typedef struct {
	const char* name;
	a_cfun handle;
} acreg_t;

ALO_API void aloL_setfuns(alo_State, ssize_t, const acreg_t*);

ALO_API int aloL_getmetatable(alo_State, a_cstr);
ALO_API int aloL_getsubtable(alo_State, ssize_t, a_cstr);
ALO_API int aloL_getsubfield(alo_State, ssize_t, a_cstr, a_cstr);
ALO_API int aloL_callselfmeta(alo_State, ssize_t, a_cstr);

#define aloL_getmodfield(T,mod,key) aloL_getsubfield(T, ALO_REGISTRY_INDEX, mod, key)

ALO_API void aloL_require(alo_State, a_cstr, a_cfun, int);
ALO_API int aloL_compileb(alo_State, const char*, size_t, a_cstr, a_cstr);
ALO_API int aloL_compiles(alo_State, ssize_t, a_cstr, a_cstr);
ALO_API int aloL_compilef(alo_State, a_cstr, a_cstr);
ALO_API int aloL_loadf(alo_State, a_cstr);
ALO_API int aloL_savef(alo_State, a_cstr, int);

/**
 ** error handling
 */

ALO_API int aloL_getframe(alo_State, int, a_cstr, alo_DbgInfo*);
ALO_API void aloL_where(alo_State, int);
ALO_API int aloL_errresult_(alo_State, a_cstr, int);
ALO_API a_none aloL_error(alo_State, a_cstr, ...);

ALO_API a_none aloL_argerror(alo_State, ssize_t, a_cstr, ...);
ALO_API a_none aloL_typeerror(alo_State, ssize_t, a_cstr);
ALO_API a_none aloL_tagerror(alo_State, ssize_t, int);

#define aloL_errresult(T,c,m) ((c) ? (alo_pushboolean(T, true), 1) : aloL_errresult_(T, m, errno))


/**
 ** base layer of messaging and logging.
 */

#define aloL_fflush(file) fflush(file)
#define aloL_fputf(file,fmt,args...) (fprintf(file, fmt, ##args), aloL_fflush(file))
#define aloL_fputs(file,src,len) fwrite(src, sizeof(char), len, file)
#define aloL_fputc(file,ch) fputc(ch, file)
#define aloL_fputln(file) (aloL_fputs(file, "\n", 1), aloL_fflush(file))

/**===============================================================================
 ** the following section this line only takes effect with standard class library
 **===============================================================================*/

ALO_API void aloL_checkclassname(alo_State, ssize_t);
ALO_API int aloL_getsimpleclass(alo_State, a_cstr);
ALO_API void aloL_newclass_(alo_State, a_cstr, ...);

#define aloL_newclass(T,name,parents...) aloL_newclass_(T, name, ##parents, NULL)

/**===============================================================================*/

/**
 ** functions for memory buffer.
 */

/** the minimum heaped memory buffer capacity */
#define ALOL_MBUFSIZE (256 * __SIZEOF_POINTER__)

#define aloL_newbuf(T,n) ambuf_t n[1]; alo_bufpush(T, n)
#define aloL_usebuf(T,n,s...) { aloL_newbuf(T, n); s; alo_bufpop(T, n); }

/* buffer field getter */
#define aloL_blen(b) ((b)->len) /* readable and writable */
#define aloL_braw(b) aloE_cast(char* const, (b)->ptr) /* read-only */

/* basic buffer operation */
#define aloL_bclean(b) aloE_void((b)->len = 0)
#define aloL_bempty(b) ((b)->len == 0)
#define aloL_bcheck(T,b,l) ({ size_t $req = (b)->len + (l); if ($req > (b)->cap) alo_bufgrow(T, b, $req); })
#define aloL_btop(b) ((b)->ptr + (b)->len)

/* buffer as string builder */
#define aloL_bgetc(b,i) ((b)->ptr[i])
#define aloL_bputc(T,b,ch) (aloE_cast(void, (b)->len < (b)->cap || (aloL_bcheck(T, b, 1), true)), aloL_bputcx(b, ch))
#define aloL_bputls(T,b,s,l) aloL_bputm(T, b, s, (l) * sizeof(char))
#define aloL_bputxs(T,b,s) aloL_bputls(T, b, ""s, sizeof(s) / sizeof(char) - 1)
#define aloL_bsetc(b,i,ch) ((b)->ptr[i] = aloE_cast(char, ch))
#define aloL_bputcx(b,ch) aloL_bsetc(b, (b)->len++, ch)
#define aloL_b2str(T,b) aloE_cast(a_cstr, (aloL_bputc(T, b, '\0'), aloL_braw(b))) /* read-only, not alive till memory buffer pop */
#define aloL_bpushstring(T,b) alo_pushlstring(T, aloL_braw(b), aloL_blen(buf) / sizeof(char))

/* buffer as object stack */
#define aloL_btpush(T,b,o) \
	aloE_void(aloL_bcheck(T, b, sizeof(o)), *aloL_bttop(b, o) = (o), aloL_blen(b) += sizeof(o))
#define aloL_btpop(b,t) \
	aloE_check(aloL_blen(b) >= sizeof(t), "no "#t" in buffer.", aloE_cast(typeof(t)*, aloL_braw(b) + (aloL_blen(b) -= sizeof(t))))
#define aloL_bttop(b,t) aloE_cast(typeof(t)*, aloL_btop(b))

ALO_API void aloL_bputm(alo_State, ambuf_t*, const void*, size_t);
ALO_API void aloL_bputs(alo_State, ambuf_t*, a_cstr);
ALO_API void aloL_breptc(alo_State, ambuf_t*, a_cstr, int, int);
ALO_API void aloL_brepts(alo_State, ambuf_t*, a_cstr, a_cstr, a_cstr);
ALO_API void aloL_bputf(alo_State, ambuf_t*, a_cstr, ...);
ALO_API void aloL_bputvf(alo_State, ambuf_t*, a_cstr, va_list);
ALO_API void aloL_bwrite(alo_State, ambuf_t*, ssize_t);

ALO_API int aloL_bwriter(alo_State, void*, const void*, size_t);

#endif /* AAUX_H_ */
