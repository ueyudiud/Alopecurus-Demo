/*
 * alo.h
 *
 * basic API.
 *
 *  Created on: 2019年7月20日
 *      Author: ueyudiud
 */

#ifndef ALO_H_
#define ALO_H_

#define ALO_VERSION_MAJOR	"0"
#define ALO_VERSION_MINOR	"1"
#define ALO_VERSION_NUM		0, 1
#define ALO_VERSION_RELEASE	"4"

#define ALO_NAME "Alopecurus"
#define ALO_VERSION ALO_NAME" "ALO_VERSION_MAJOR"."ALO_VERSION_MINOR
#define ALO_VERSION_FULL ALO_VERSION"."ALO_VERSION_RELEASE
#define ALO_COPYRIGHT ALO_VERSION_FULL" Copyright (C) 2018-2020 ueyudiud"
#define ALO_AUTHOR "ueyudiud"

#include "adef.h"
#include "acfg.h"

#define ALO_API extern

typedef ptrdiff_t aindex_t;

#define ALO_GLOBAL_IDNEX (-ALO_MAXSTACKSIZE - 50000)
#define ALO_REGISTRY_INDEX (-ALO_MAXSTACKSIZE - 100000)
#define ALO_CAPTURE_INDEX(x) (-ALO_MAXSTACKSIZE - 150000 + (x))

/**
 * state manipulation
 */

ALO_API astate alo_newstate(aalloc, void*);
ALO_API void alo_deletestate(astate);

ALO_API void alo_setpanic(astate, acfun);
ALO_API aalloc alo_getalloc(astate, void**);
ALO_API void alo_setalloc(astate, aalloc, void*);
ALO_API void alo_bind(astate, astr, acfun);

/**
 ** basic stack manipulation
 */

ALO_API aindex_t alo_absindex(astate, aindex_t);
ALO_API int alo_ensure(astate, size_t);
ALO_API aindex_t alo_gettop(astate);
ALO_API void alo_settop(astate, aindex_t);
ALO_API void alo_copy(astate, aindex_t, aindex_t);
ALO_API void alo_push(astate, aindex_t);
ALO_API void alo_pop(astate, aindex_t);
ALO_API void alo_erase(astate, aindex_t);
ALO_API void alo_insert(astate, aindex_t);
ALO_API void alo_xmove(astate, astate, size_t);

#define alo_drop(T) alo_settop(T, -1)

/**
 ** access functions (stack -> C)
 */

ALO_API int alo_isinteger(astate, aindex_t);
ALO_API int alo_isnumber(astate, aindex_t);
ALO_API int alo_iscfunction(astate, aindex_t);
ALO_API int alo_israwdata(astate, aindex_t);

#define alo_isboolean(T,index) (alo_typeid(T, index) == ALO_TBOOL)
#define alo_isstring(T,index) (alo_typeid(T, index) == ALO_TSTRING)
#define alo_isnothing(T,index) (alo_typeid(T, index) <= ALO_TNIL)
#define alo_isnone(T,index) (alo_typeid(T, index) == ALO_TUNDEF)
#define alo_isnonnil(T,index) (alo_typeid(T, index) > ALO_TNIL)

ALO_API int alo_typeid(astate, aindex_t);
ALO_API astr alo_typename(astate, aindex_t);
ALO_API astr alo_tpidname(astate, int);
ALO_API int alo_toboolean(astate, aindex_t);
ALO_API aint alo_tointegerx(astate, aindex_t, int*);
ALO_API afloat alo_tonumberx(astate, aindex_t, int*);
ALO_API astr alo_tolstring(astate, aindex_t, size_t*);
ALO_API acfun alo_tocfunction(astate, aindex_t);
ALO_API void* alo_torawdata(astate, aindex_t);
ALO_API astate alo_tothread(astate, aindex_t);

#define alo_tointeger(T,index) alo_tointegerx(T, index, NULL)
#define alo_tonumber(T,index) alo_tonumberx(T, index, NULL)
#define alo_tostring(T,index) alo_tolstring(T, index, NULL)

ALO_API void* alo_rawptr(astate, aindex_t);
ALO_API size_t alo_rawlen(astate, aindex_t);
ALO_API int alo_equal(astate, aindex_t, aindex_t);
ALO_API void alo_arith(astate, int);
ALO_API int alo_compare(astate, aindex_t, aindex_t, int);

/**
 ** push functions (C -> stack)
 */

ALO_API void alo_pushnil(astate);
ALO_API void alo_pushboolean(astate, int);
ALO_API void alo_pushinteger(astate, aint);
ALO_API void alo_pushnumber(astate, afloat);
ALO_API astr alo_pushlstring(astate, const char*, size_t);
ALO_API astr alo_pushstring(astate, astr);
ALO_API astr alo_pushfstring(astate, astr, ...);
ALO_API astr alo_pushvfstring(astate, astr, va_list);
ALO_API void alo_pushlightcfunction(astate, acfun);
ALO_API void alo_pushcclosure(astate, acfun, size_t);
ALO_API void alo_pushpointer(astate, void*);
ALO_API int alo_pushthread(astate);

#define alo_pushunit(T) alo_newtuple(T, 0)

/**
 ** arithmetic operation, comparison and other functions
 */

ALO_API void alo_rawcat(astate, size_t);

/**
 ** get functions (Alopecurus -> stack)
 */

ALO_API int alo_inext(astate, aindex_t, ptrdiff_t*);
ALO_API void alo_iremove(astate, aindex_t, ptrdiff_t);
ALO_API int alo_rawget(astate, aindex_t);
ALO_API int alo_rawgeti(astate, aindex_t, aint);
ALO_API int alo_rawgets(astate, aindex_t, astr);
ALO_API int alo_get(astate, aindex_t);
ALO_API int alo_gets(astate, aindex_t, astr);
ALO_API int alo_put(astate, aindex_t);
ALO_API int alo_getmetatable(astate, aindex_t);
ALO_API int alo_getmeta(astate, aindex_t, astr, int);
ALO_API int alo_getdelegate(astate, aindex_t);

#define alo_getreg(T,key) alo_gets(T, ALO_REGISTRY_INDEX, key)

ALO_API amem alo_newdata(astate, size_t);
ALO_API void alo_newtuple(astate, size_t);
ALO_API void alo_newlist(astate, size_t);
ALO_API void alo_newtable(astate, size_t);
ALO_API astate alo_newthread(astate);

#define alo_newobj(T,type) aloE_cast(typeof(type)*, alo_newdata(T, sizeof(type)))

/**
 ** set functions (stack -> Alopecurus)
 */

ALO_API void alo_trim(astate, aindex_t);
ALO_API void alo_triml(astate, aindex_t, size_t);
ALO_API void alo_rawsetx(astate, aindex_t, int);
ALO_API void alo_rawseti(astate, aindex_t, aint);
ALO_API void alo_rawsets(astate, aindex_t, astr);
ALO_API int alo_rawrem(astate, aindex_t);
ALO_API void alo_setx(astate, aindex_t, int);
ALO_API int alo_remove(astate, aindex_t);
ALO_API int alo_setmetatable(astate, aindex_t);
ALO_API int alo_setdelegate(astate, aindex_t);

#define alo_rawset(T,index) alo_rawset(T, index, false)
#define alo_set(T,index) alo_setx(T, index, false)

/**
 ** call and IO functions
 */

ALO_API void alo_callk(astate, int, int, akfun, void*);
ALO_API int alo_pcallk(astate, int, int, akfun, void*);
ALO_API int alo_compile(astate, astr, astr, areader, void*);
ALO_API int alo_load(astate, astr, areader, void*);
ALO_API int alo_save(astate, awriter, void*, int);

#define alo_call(T,narg,nres) alo_callk(T, narg, nres, NULL, NULL)
#define alo_pcall(T,narg,nres) alo_pcallk(T, narg, nres, NULL, NULL)

/**
 ** coroutine functions.
 */

ALO_API int alo_resume(astate, astate, int);
ALO_API void alo_yieldk(astate, int, akfun, void*);
ALO_API int alo_status(astate);
ALO_API int alo_isyieldable(astate);

#define alo_yield(T,nres) alo_yieldk(T, nres, NULL, NULL)

/**
 ** error handling
 */

ALO_API anoret alo_throw(astate);

/**
 ** miscellaneous functions.
 */

ALO_API size_t alo_memused(astate);
ALO_API void alo_fullgc(astate);
ALO_API void alo_checkgc(astate);
ALO_API int alo_growbuf(astate, ambuf_t*, size_t);
ALO_API void alo_delbuf(astate, ambuf_t*);
ALO_API int alo_format(astate, awriter, void*, astr, ...);
ALO_API int alo_vformat(astate, awriter, void*, astr, va_list);

#define alo_newbuf(name) ambuf_t name = ((ambuf_t) { ALO_MBUF_SHTLEN, 0, name.instk, NULL, {} })

/**
 ** debugger
 */

typedef struct alo_FrameDebug aframeinfo_t;

ALO_API void alo_getframe(astate, astr, aframeinfo_t*);
ALO_API int alo_prevframe(astate, astr, aframeinfo_t*);

struct alo_FrameDebug {
	astr name; /* apply by 'n' */
	astr kind; /* kind of frame, apply by 'n' */
	astr src; /* apply by 's' */
	int linefdef, lineldef; /* apply by 's' */
	int line; /* apply by 'l' */
	unsigned nargument; /* apply by 'a' */
	unsigned ncapture; /* apply by 'a' */
	abyte vararg; /* apply by 'a' */
	/* for private use */
	void* _frame;
};

#endif /* ALO_H_ */
