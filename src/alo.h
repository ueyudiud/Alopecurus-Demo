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

#define ALO aloE_vernum

#define ALO_VERSION_MAJOR	"0"
#define ALO_VERSION_MINOR	"2"
#define ALO_VERSION_NUM		0, 2
#define ALO_VERSION_RELEASE	"1"

#define ALO_NAME "Alopecurus"
#define ALO_VERSION ALO_NAME" "ALO_VERSION_MAJOR"."ALO_VERSION_MINOR
#define ALO_VERSION_FULL ALO_VERSION"."ALO_VERSION_RELEASE
#define ALO_COPYRIGHT ALO_VERSION_FULL" Copyright (C) 2018-2020 ueyudiud"
#define ALO_AUTHOR "ueyudiud"

#include "adef.h"
#include "acfg.h"

/**
 ** version number declaration
 */

#define aloE_vermajor aloE_apply(aloE_1, ALO_VERSION_NUM)
#define aloE_verminor aloE_apply(aloE_2, ALO_VERSION_NUM)
#define aloE_vernum (aloE_vermajor * 100 + aloE_verminor)

/**
 ** the ALO_API marked for API functions.
 */
#if defined(ALO_BUILD_TO_DLL)

#if defined(ALO_CORE) || defined(ALO_LIB)
#define ALO_API __declspec(dllexport)
#else
#define ALO_API __declspec(dllimport)
#endif

#else

#define ALO_API extern

#endif

#define ALO_GLOBAL_IDNEX (-ALO_MAXSTACKSIZE - 50000)
#define ALO_REGISTRY_INDEX (-ALO_MAXSTACKSIZE - 100000)
#define ALO_CAPTURE_INDEX(x) (-ALO_MAXSTACKSIZE - 150000 + (x))

#define ALO_NOERRFUN (-ALO_MAXSTACKSIZE - 50000)
#define ALO_LASTERRFUN (-ALO_MAXSTACKSIZE - 100000)

#if defined(ALO_USER_H_)
#include ALO_USER_H_
#endif

/**
 * state manipulation
 */

ALO_API alo_State alo_newstate(alo_Alloc, void*);
ALO_API void alo_deletestate(alo_State);

ALO_API void alo_setpanic(alo_State, a_cfun);
ALO_API alo_Alloc alo_getalloc(alo_State, void**);
ALO_API void alo_setalloc(alo_State, alo_Alloc, void*);

ALO_API const aver_t* alo_version(alo_State);

/**
 ** basic stack manipulation
 */

ALO_API a_isize alo_absindex(alo_State, a_isize);
ALO_API a_bool alo_ensure(alo_State, a_usize);
ALO_API a_isize alo_gettop(alo_State);
ALO_API void alo_settop(alo_State, a_isize);
ALO_API void alo_copy(alo_State, a_isize, a_isize);
ALO_API void alo_push(alo_State, a_isize);
ALO_API void alo_pop(alo_State, a_isize);
ALO_API void alo_erase(alo_State, a_isize);
ALO_API void alo_insert(alo_State, a_isize);
ALO_API void alo_xmove(alo_State, alo_State, a_usize);

#define alo_drop(T) alo_settop(T, -1)

/**
 ** access functions (stack -> C)
 */

ALO_API a_bool alo_isinteger(alo_State, a_isize);
ALO_API a_bool alo_isnumber(alo_State, a_isize);
ALO_API a_bool alo_iscfunction(alo_State, a_isize);
ALO_API a_bool alo_israwdata(alo_State, a_isize);

#define alo_isboolean(T,index) (alo_typeid(T, index) == ALO_TBOOL)
#define alo_isstring(T,index) (alo_typeid(T, index) == ALO_TSTR)
#define alo_isreference(T,index) (alo_typeid(T, index) >= ALO_TTUPLE)
#define alo_isnothing(T,index) (alo_typeid(T, index) <= ALO_TNIL)
#define alo_isnone(T,index) (alo_typeid(T, index) == ALO_TUNDEF)
#define alo_isnonnil(T,index) (alo_typeid(T, index) > ALO_TNIL)

ALO_API int alo_typeid(alo_State, a_isize);
ALO_API a_cstr alo_typename(alo_State, a_isize);
ALO_API a_cstr alo_tpidname(alo_State, int);
ALO_API int alo_toboolean(alo_State, a_isize);
ALO_API a_int alo_tointegerx(alo_State, a_isize, a_bool*);
ALO_API a_float alo_tonumberx(alo_State, a_isize, a_bool*);
ALO_API a_cstr alo_tolstring(alo_State, a_isize, a_usize*);
ALO_API a_cfun alo_tocfunction(alo_State, a_isize);
ALO_API void* alo_torawdata(alo_State, a_isize);
ALO_API alo_State alo_tothread(alo_State, a_isize);

#define alo_tointeger(T,index) alo_tointegerx(T, index, NULL)
#define alo_tonumber(T,index) alo_tonumberx(T, index, NULL)
#define alo_tostring(T,index) alo_tolstring(T, index, NULL)
#define alo_toobject(T,index,type) aloE_cast(type, alo_torawdata(T, index))
#define alo_toiterator(T,index) ((a_itr) { alo_tointeger(T, index) })

ALO_API void* alo_rawptr(alo_State, a_isize);
ALO_API size_t alo_rawlen(alo_State, a_isize);
ALO_API size_t alo_len(alo_State, a_isize);
ALO_API a_bool alo_equal(alo_State, a_isize, a_isize);
ALO_API void alo_arith(alo_State, int);
ALO_API a_bool alo_compare(alo_State, a_isize, a_isize, int);

/**
 ** push functions (C -> stack)
 */

ALO_API void alo_pushnil(alo_State);
ALO_API void alo_pushboolean(alo_State, a_bool);
ALO_API void alo_pushinteger(alo_State, a_int);
ALO_API void alo_pushnumber(alo_State, a_float);
ALO_API a_cstr alo_pushlstring(alo_State, const char*, size_t);
ALO_API a_cstr alo_pushstring(alo_State, a_cstr);
ALO_API a_cstr alo_pushfstring(alo_State, a_cstr, ...);
ALO_API a_cstr alo_pushvfstring(alo_State, a_cstr, va_list);
ALO_API void alo_pushcfunction(alo_State, a_cfun, a_usize, int);
ALO_API void alo_pushpointer(alo_State, void*);
ALO_API int alo_pushthread(alo_State);

#define alo_pushunit(T) alo_newtuple(T, 0)
#define alo_pushiterator(T,i) alo_pushinteger(T, (i).offset)
#define alo_pushcstring(T,s) alo_pushlstring(T, ""s, sizeof(s) / sizeof(char) - 1)
#define alo_pushlightcfunction(T,f) alo_pushcfunction(T, f, 0, false)
#define alo_pushcclosure(T,f,n) alo_pushcfunction(T, f, n, false)

/**
 ** arithmetic operation, comparison and other functions
 */

ALO_API void alo_rawcat(alo_State, size_t);

/**
 ** get functions (Alopecurus -> stack)
 */

ALO_API int alo_inext(alo_State, a_isize, a_itr*);
ALO_API void alo_iremove(alo_State, a_isize, a_itr*);
ALO_API int alo_rawget(alo_State, a_isize);
ALO_API int alo_rawgeti(alo_State, a_isize, a_int);
ALO_API int alo_rawgets(alo_State, a_isize, a_cstr);
ALO_API int alo_get(alo_State, a_isize);
ALO_API int alo_gets(alo_State, a_isize, a_cstr);
ALO_API int alo_put(alo_State, a_isize);
ALO_API int alo_getmetatable(alo_State, a_isize);
ALO_API int alo_getmeta(alo_State, ssize_t, a_cstr, int);
ALO_API int alo_getdelegate(alo_State, a_isize);

#define alo_getreg(T,key) alo_gets(T, ALO_REGISTRY_INDEX, key)

ALO_API a_mem alo_newdata(alo_State, a_usize);
ALO_API void alo_newtuple(alo_State, a_usize);
ALO_API void alo_newlist(alo_State, a_usize);
ALO_API void alo_newtable(alo_State, a_usize);
ALO_API alo_State alo_newthread(alo_State);

#define alo_newobject(T,type) aloE_cast(typeof(type)*, alo_newdata(T, sizeof(type)))
#define alo_ibegin(T,index) (aloE_void(T), aloE_void(index), (a_itr) { ALO_ITERATE_BEGIN })

/**
 ** set functions (stack -> Alopecurus)
 */

ALO_API void alo_sizehint(alo_State, a_isize, a_usize);
ALO_API void alo_trim(alo_State, a_isize);
ALO_API void alo_triml(alo_State, a_isize, size_t);
ALO_API void alo_rawsetx(alo_State, a_isize, int);
ALO_API void alo_rawseti(alo_State, a_isize, a_int);
ALO_API void alo_rawsets(alo_State, a_isize, a_cstr);
ALO_API int alo_rawrem(alo_State, a_isize);
ALO_API void alo_rawclr(alo_State, a_isize);
ALO_API void alo_add(alo_State, a_isize);
ALO_API void alo_setx(alo_State, a_isize, int);
ALO_API int alo_remove(alo_State, a_isize);
ALO_API int alo_setmetatable(alo_State, a_isize);
ALO_API int alo_setdelegate(alo_State, a_isize);

#define alo_rawset(T,index) alo_rawsetx(T, index, false)
#define alo_set(T,index) alo_setx(T, index, false)

/**
 ** call and IO functions
 */

ALO_API void alo_callk(alo_State, int, int, a_kfun, a_kctx);
ALO_API int alo_pcallk(alo_State, int, int, ssize_t, a_kfun, a_kctx);
ALO_API int alo_compile(alo_State, a_cstr, a_cstr, alo_Reader, void*);
ALO_API int alo_load(alo_State, a_cstr, alo_Reader, void*);
ALO_API int alo_save(alo_State, alo_Writer, void*, int);

#define alo_call(T,a,r) alo_callk(T, a, r, NULL, 0)
#define alo_pcall(T,a,r,e) alo_pcallk(T, a, r, e, NULL, 0)

/**
 ** coroutine functions.
 */

ALO_API int alo_resume(alo_State, alo_State, int);
ALO_API void alo_yieldk(alo_State, int, a_kfun, a_kctx);
ALO_API int alo_status(alo_State);
ALO_API int alo_isyieldable(alo_State);

#define alo_yield(T,nres) alo_yieldk(T, nres, NULL, 0)

/**
 ** error handling
 */

ALO_API a_none alo_error(alo_State);
ALO_API a_none alo_throw(alo_State);

/**
 ** miscellaneous functions.
 */

enum {
	ALO_GCRUNNING,
	ALO_GCSTOP,
	ALO_GCRERUN,
	ALO_GCUSED,
	ALO_GCSTEPMUL,
	ALO_GCPAUSEMUL,
};

ALO_API a_usize alo_gcconf(alo_State, int, a_usize);
ALO_API void alo_fullgc(alo_State);
ALO_API void alo_checkgc(alo_State);
ALO_API int alo_format(alo_State, alo_Writer, void*, a_cstr, ...);
ALO_API int alo_vformat(alo_State, alo_Writer, void*, a_cstr, va_list);

ALO_API void alo_bufpush(alo_State, ambuf_t*);
ALO_API void alo_bufgrow(alo_State, ambuf_t*, size_t);
ALO_API void alo_bufpop(alo_State, ambuf_t*);

#define alo_isgcrunning(T) aloE_cast(int, alo_gcconf(T, ALO_GCRUNNING, 0))
#define alo_memused(T) alo_gcconf(T, ALO_GCUSED, 0)

/**
 ** debugger
 */

typedef struct alo_DbgInfo alo_DbgInfo;

/* hook function type, the function will be called in specific events */
typedef void (*ahfun)(alo_State, alo_DbgInfo*);

enum {
	ALO_INFCURR,
	ALO_INFPREV,
	ALO_INFSTACK
};

ALO_API int alo_getinfo(alo_State, int, a_cstr, alo_DbgInfo*);

#define ALO_HMASKCALL  (1 << 0)
#define ALO_HMASKRET   (1 << 1)
#define ALO_HMASKLINE  (1 << 2)

ALO_API void alo_sethook(alo_State, ahfun, int);
ALO_API ahfun alo_gethook(alo_State);
ALO_API int alo_gethookmask(alo_State);

struct alo_DbgInfo {
	int event;
	a_cstr name; /* apply by 'n' */
	a_cstr kind; /* kind of frame, apply by 'n' */
	a_cstr src; /* apply by 's' */
	int linefdef, lineldef; /* apply by 's' */
	int line; /* apply by 'l' */
	unsigned nargument; /* apply by 'a' */
	unsigned ncapture; /* apply by 'a' */
	a_byte vararg : 1; /* apply by 'a' */
	a_byte istailc : 1; /* apply by 'c' */
	a_byte isfinc : 1; /* apply by 'c' */
	/* for private use */
	void* _frame;
};

#endif /* ALO_H_ */
