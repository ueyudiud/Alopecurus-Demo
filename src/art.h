/*
 * art.h
 *
 * runtime environment.
 *
 *  Created on: 2019年7月20日
 *      Author: ueyudiud
 */

#ifndef ART_H_
#define ART_H_

#include "acfg.h"
#include "adef.h"

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

/**
 ** basic header include.
 */
#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <sys/types.h>

/**
 ** primitive scalar type definition.
 */
typedef uint64_t ainsn_t;
typedef uint64_t ahash_t;

typedef struct alo_JumpBuf ajmp_t;

struct alo_JumpBuf {
	ajmp_t* prev;
	int status;
	jmp_buf buf;
};

/**
 ** string buffer type.
 */
typedef struct alo_SBuf asbuf_t;

/**
 ** limits and constants.
 */

#define BYTE_MAX UCHAR_MAX

#define AINT_MAX ALO_INT_PROP(MAX)
#define AINT_MIN ALO_INT_PROP(MIN)

/* the length of buffer used in 'sprintf' function */
#define ALO_SPRIBUFLEN 32
#define ALO_SPRIINT(b,v) sprintf(b, ALO_INT_FORMAT, v)
#define ALO_SPRIFLT(b,v) sprintf(b, ALO_FLT_FORMAT, v)

/* the environment version, defined in astate.c */
extern const aver_t aloR_version;

/**
 ** interned limits and size macros.
 */

/* seed mask */
#if !defined(ALO_MASKSEED)
#define ALO_MASKSEED 0x435309EBF71BA90DULL
#endif

#if !defined(ALO_STRCACHE_N)
#define ALO_STRCACHE_N 53
#define ALO_STRCACHE_M 2
#endif

#if !defined(ALO_MIN_BUFSIZE)
#define ALO_MIN_BUFSIZE 4
#endif

#if !defined(ALO_MAX_BUFSIZE)
#define ALO_MAX_BUFSIZE 100000000
#endif

#if !defined(ALO_GCSTEPMUL)
#define ALO_GCSTEPMUL 200
#endif

#if !defined(ALO_GCPAUSEMUL)
#define ALO_GCPAUSEMUL 200
#endif

/* minimum working cost each GC step */
#if !defined(ALO_MINWORK)
#define ALO_MINWORK (sizeof(astring_t) * 64)
#endif

/* min stack size for C function */
#if !defined(ALO_MINSTACKSIZE)
#define ALO_MINSTACKSIZE 16
#endif

#if !defined(ALO_BASESTACKSIZE)
#define ALO_BASESTACKSIZE (ALO_MINSTACKSIZE * 2)
#endif

/* max size of C function calling */
#if !defined(ALO_MAXCCALL)
#define ALO_MAXCCALL 200
#endif

/* the max length of string stored in intern table. */
#if !defined(ALO_MAXISTRLEN)
#define ALO_MAXISTRLEN 40
#endif

/* the hash limit */
#if !defined(ALO_STRHASHLIMIT)
#define ALO_STRHASHLIMIT 5
#endif

/* properties for intern table */
#if !defined(ALO_ITABLE)
#define ALO_ITABLE_INITIAL_CAPACITY 128
#define ALO_ITABLE_LOAD_FACTOR 0.75
#define ALO_ITABLE
#endif

/* properties for C function table */
#if !defined(ALO_CTABLE)
#define ALO_CTABLE_INITIAL_CAPACITY 23
#define ALO_CTABLE_LOAD_FACTOR 0.75
#define ALO_CTABLE
#endif

#if !defined(ALO_MEMERRMSG)
#define ALO_MEMERRMSG "no enough memory"
#endif

#if !defined(ALO_GLOBAL_INITIALIZESIZE)
#define ALO_GLOBAL_INITIALIZESIZE 32
#endif

/**
 ** useful macros.
 */

#define aloE_xassert(exp) aloE_assert(exp, #exp)
#define aloE_sassert(exp,what) _Static_assert(exp, what)
#define aloE_check(exp,what,ret...) (aloE_assert(exp, what), ret)
#define aloE_xcheck(exp,ret...) aloE_check(exp, #exp, ret)

#define aloE_int(exp) aloE_cast(aint, exp)
#define aloE_flt(exp) aloE_cast(afloat, exp)
#define aloE_addr(exp) aloE_cast(uintptr_t, exp)

#define api_check(T,e,m) aloE_assert(e, m)
#define api_checkelems(T,n) api_check(T, (n) <= (T)->top - ((T)->frame->fun + 1), "arguments not enough")
#define api_checkslots(T,n) api_check(T, (n) <= (T)->frame->top - (T)->top, "stack overflow")
#define api_incrtop(T) (api_check(T, (T)->top < (T)->frame->top, "stack overflow"), (T)->top++)
#define api_decrtop(T) (api_check(T, (T)->top > (T)->frame->fun, "arguments not enough"), --(T)->top)

/* users' thread opening function declaration */
#ifdef ALO_OPEN_THREAD
#define aloE_openthread(T,from) ALO_OPEN_THREAD(T, from)
#else
#define aloE_openthread(T,from) aloE_void(aloE_void(T), from)
#endif

/* users' thread closing function declaration */
#ifdef ALO_CLOSE_THREAD
#define aloE_closethread(T) ALO_CLOSE_THREAD(T)
#else
#define aloE_closethread(T) aloE_void(T)
#endif

#endif /* ART_H_ */
