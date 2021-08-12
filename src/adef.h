/*
 * adef.h
 *
 * predefined types and macros.
 *
 *  Created on: 2019年7月20日
 *      Author: ueyudiud
 */

#ifndef ADEF_H_
#define ADEF_H_

#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>

/**
 ** primitive types
 */

#if defined(ALO_USE_STD_BOOL) && __STDC__ >= 201112L
typedef _Bool            a_bool;
#else
typedef unsigned char    a_bool;
#endif

typedef unsigned char    a_byte;
typedef intmax_t         a_int;
typedef double           a_float;
typedef char const*      a_cstr;
typedef void*            a_mem;

typedef uint8_t          a_u8;
typedef uint16_t         a_u16;
typedef uint32_t         a_u32;
typedef uint64_t         a_u64;
typedef uintptr_t        a_uptr;
typedef size_t           a_usize;
typedef int8_t           a_i8;
typedef int16_t          a_i16;
typedef int32_t          a_i32;
typedef int64_t          a_i64;
typedef intptr_t         a_iptr;
typedef ptrdiff_t        a_isize;

typedef struct alo_Iterator {
	ptrdiff_t offset; /* inner use */
} a_itr;

typedef struct alo_Version {
	a_byte major;
	a_byte minor;
} aver_t;

#define a_none __attribute__((__noreturn__)) void

typedef struct alo_MemBuf ambuf_t;

#if !defined(ALO_CORE)
struct alo_MemBuf {
	char* const ptr; /* the start pointer of buffer */
	size_t const cap; /* the capacity of buffer */
	size_t len; /* the length filled in buffer */
	const void* const prev;
};
#endif

/**
 ** Alopecurus state and handle types
 */

typedef struct alo_Thread *alo_State;

/* the C function type in Alopecurus */
typedef int (*a_cfun)(alo_State);

/* the context for function */
typedef intptr_t a_kctx;

/* context based function type, the function is used to recover yielded function */
typedef int (*a_kfun)(alo_State, int, a_kctx);

/* allocation function handle type, used for memory allocation */
typedef a_mem (*alo_Alloc)(void*, a_mem, size_t, size_t);

/**
 ** reader function, use to read data.
 ** return 0 if read success and non 0 otherwise.
 ** an error message should push to top of stack if error occurred.
 */
typedef int (*alo_Reader)(alo_State, void*, const char**, size_t*);

/**
 ** writer function, use to write data.
 ** return 0 if write success and non 0 otherwise.
 ** an error message should push to top of stack if error occurred.
 */
typedef int (*alo_Writer)(alo_State, void*, const void*, size_t);

/**
 ** type and operation tags.
 */

enum { /* type tags */
	ALO_TUNDEF = -1,
	/* value type */
	ALO_TNIL = 0,
	ALO_TBOOL,
	ALO_TINT,
	ALO_TFLOAT,
	ALO_TPTR,
	ALO_TSTR,
	ALO_TTUPLE,
	ALO_TLIST,
	ALO_TTABLE,
	ALO_TFUNC,
	ALO_TTHREAD,
	ALO_TUSER,

	/* non-value type */
	ALO_TPROTO
};

enum { /* operation tags */
	/* binary operation */
	ALO_OPADD,
	ALO_OPSUB,
	ALO_OPMUL,
	ALO_OPDIV,
	ALO_OPIDIV,
	ALO_OPREM,
	ALO_OPPOW,
	ALO_OPSHL,
	ALO_OPSHR,
	ALO_OPBAND,
	ALO_OPBOR,
	ALO_OPBXOR,
	/* unary operation */
	ALO_OPPOS,
	ALO_OPNEG,
	ALO_OPLEN,
	ALO_OPBNOT,
	/* compare operation */
	ALO_OPEQ,
	ALO_OPLT,
	ALO_OPLE
};

/**
 ** thread states.
 */
enum {
	ALO_STOK,
	ALO_STYIELD,
	ALO_STERRRT,
	ALO_STERRCOM,
	ALO_STERRFIN,
	ALO_STERROOM,
	ALO_STERRERR
};

/* option for multiple returns in 'alo_call' and 'alo_pcall' */
#define ALO_MULTIRET (-1)

/* initialized value for iterator in 'alo_ibegin' */
#define ALO_ITERATE_BEGIN (-1)

/**
 ** numeric type property getter.
 ** common available property keys: MAX, MIN
 */
#define ALO_INT_PROP(field) INT64_##field
#define ALO_FLT_PROP(field) DBL_##field

/**
 ** macro derived macros
 */

#define aloE_apply(f,x...) f(x)
#define aloE_1(e1,...) e1
#define aloE_2(e1,e2,...) e2

/**
 ** useful macros
 */

#if defined(ALO_DEBUG) || defined(ALOI_DEBUG)

/* ALO_ASSERT definition */
#if !defined(ALO_ASSERT)
#define ALO_ASSERT(exp,what) aloE_void(!!(exp) || (alo_assert(what, __FILE__, __LINE__), 0))
extern void alo_assert(a_cstr, a_cstr, int);
#endif

/* ALO_LOG definition */
#if !defined(ALO_LOG)
#define ALO_LOG(what,args...) alo_log(what, __FILE__, __LINE__, ##args)
extern void alo_log(a_cstr, a_cstr, int, ...);
#endif

#endif

#if defined(ALO_DEBUG)
#define aloE_assert(exp,what) ALO_ASSERT(exp,what)
#define aloE_log(what,fmt...) ALO_LOG(what, ##fmt)
#else
#define aloE_assert(exp,what) ((void) 0)
#define aloE_log(what,fmt...) ((void) 0)
#endif

#define aloE_xassert(exp) aloE_assert(exp, #exp)
#define aloE_sassert(exp,what) _Static_assert(exp, what)
#define aloE_check(exp,what,ret...) (aloE_assert(exp, what), ret)
#define aloE_xcheck(exp,ret...) aloE_check(exp, #exp, ret)


/**
 ** type castion macros.
 */

#define aloE_void(exp...) ((void) (exp))
#define aloE_cast(type,exp) ((type) (exp))
#define aloE_byte(exp) aloE_cast(a_byte, exp)
#define aloE_int(exp) aloE_cast(a_int, exp)
#define aloE_flt(exp) aloE_cast(a_float, exp)
#define aloE_addr(exp) aloE_cast(uintptr_t, exp)

/**
 ** non-standard features using controls,
 ** detecting specific environment.
 */

#if defined(_WIN32)
#define ALOE_DIRSEP '\\'
#else
#define ALOE_DIRSEP '/'
#endif

#if defined(_WIN32) && !defined(_WIN32_WCE)
#define ALOE_WINDOWS /* enable Windows features */
#endif

#if defined(ALOE_WINDOWS)
#define ALO_USE_DLL /* enable support for dll */
#endif

#if defined(ALOE_LINUX)
#define ALO_USE_POSIX
#define ALO_USE_DLOPEN		/* needs an extra library: -ldl */
#endif

#if defined(ALOE_MACOSX)
#define ALO_USE_POSIX
#define ALO_USE_DLOPEN		/* MacOS does not need -ldl */
#endif

#endif /* ADEF_H_ */
