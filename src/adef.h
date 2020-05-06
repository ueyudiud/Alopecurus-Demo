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
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>

/**
 ** primitive types
 */

typedef uint8_t abyte;
typedef int64_t aint;
typedef double afloat;
typedef const char* astr;
typedef void* amem;

typedef struct alo_Iterator {
	ptrdiff_t offset; /* inner use */
} aitr;

typedef struct alo_Version {
	abyte major;
	abyte minor;
} aver_t;

#define anoret __attribute__((noreturn)) void

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

typedef struct alo_Thread *astate;

/* the C function type in Alopecurus */
typedef int (*acfun)(astate);

/* context based function type, the function is used to recover yielded function */
typedef int (*akfun)(astate, int, void*);

typedef amem (*aalloc)(void*, amem, size_t, size_t);
typedef int (*areader)(astate, void*, const char**, size_t*);
typedef int (*awriter)(astate, void*, const void*, size_t);

typedef struct alo_CRegistry {
	astr name;
	acfun handle;
} acreg_t;

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
	ALO_TPOINTER,
	ALO_TSTRING,
	ALO_TTUPLE,
	ALO_TLIST,
	ALO_TTABLE,
	ALO_TFUNCTION,
	ALO_TTHREAD,
	ALO_TRAWDATA,

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
	ThreadStateRun = 0,
	ThreadStateYield,
	ThreadStateErrRuntime,
	ThreadStateErrSerialize,
	ThreadStateErrCompile,
	ThreadStateErrFinalize,
	ThreadStateErrMemory,
	ThreadStateErrError
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

#ifdef ALO_DEBUG

/* ALO_ASSERT definition */
#if !defined(ALO_ASSERT)
#define ALO_ASSERT(exp,what) aloE_void(!!(exp) || (alo_assert(what, __FILE__, __LINE__), 0))
extern void alo_assert(astr, astr, int);
#endif

/* ALO_LOG definition */
#if !defined(ALO_LOG)
#define ALO_LOG(what,args...) alo_log(what, __FILE__, __LINE__, ##args)
extern void alo_log(astr, astr, int, ...);
#endif

#else
#undef ALO_ASSERT
#undef ALO_LOG
#define ALO_ASSERT(exp,what) ((void) 0)
#define ALO_LOG(what,args...) ((void) 0)
#endif

#define aloE_assert(exp,what) ALO_ASSERT(exp,what)
#define aloE_xassert(exp) aloE_assert(exp, #exp)
#define aloE_sassert(exp,what) _Static_assert(exp, what)
#define aloE_check(exp,what,ret...) (aloE_assert(exp, what), ret)
#define aloE_xcheck(exp,ret...) aloE_check(exp, #exp, ret)

#define aloE_log(what,fmt...) ALO_LOG(what, ##fmt)

#define aloE_void(exp...) ((void) (exp))
#define aloE_cast(type,exp) ((type) (exp))
#define aloE_byte(exp) aloE_cast(abyte, exp)
#define aloE_int(exp) aloE_cast(aint, exp)
#define aloE_flt(exp) aloE_cast(afloat, exp)
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
