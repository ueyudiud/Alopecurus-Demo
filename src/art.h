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

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901
#error "C99 or newer standard required"
#endif

/**
 ** the ALO_IFUN marked for inner function which will not exported to outside module.
 */
#if ((__GNUC__ * 100 + __GNUC_MINOR__) >= 302) && defined(__ELF__)
#define ALO_IFUN __attribute__((visibility("hidden"))) extern
#else
#define ALO_IFUN extern
#endif

#define ALO_VDEC ALO_IFUN
#define ALO_VDEF

/**
 ** basic header include.
 */
#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <float.h>
#include <sys/types.h>

/**
 ** primitive scalar type definition.
 */
typedef uint64_t ainsn_t;
typedef uint64_t ahash_t;

typedef struct alo_JumpBuf ajmp_t;

struct alo_JumpBuf {
	ajmp_t* prev;
	jmp_buf buf;
};

#if defined(ALO_CORE)

/* the implement for memory buffer */
struct alo_MemBuf {
	abyte* ptr;
	size_t cap;
	size_t len;
	ambuf_t* prev; /* the previous memory buffer in linked list */
};

#endif

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

#if !defined(ALO_DEFAULT_GCSTEPMUL)
#define ALO_DEFAULT_GCSTEPMUL 200
#endif

#if !defined(ALO_DEFAULT_GCPAUSEMUL)
#define ALO_DEFAULT_GCPAUSEMUL 200
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
#define ALO_CTABLE_MAX_CAPACITY (ALO_CTABLE_INITIAL_CAPACITY * 256)
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

#if !defined(api_check)
#define api_check(T,e,m) aloE_assert(e, m)
#endif

#define api_checkelems(T,n) api_check(T, aloE_cast(ptrdiff_t, n) <= (T)->top - ((T)->frame->fun + 1), "arguments not enough")
#define api_checkslots(T,n) api_check(T, aloE_cast(ptrdiff_t, n) <= (T)->frame->top - (T)->top, "stack overflow")
#define api_incrtop(T) (api_check(T, (T)->top < (T)->frame->top, "stack overflow"), (T)->top++)
#define api_decrtop(T) (api_check(T, (T)->top > (T)->frame->fun, "arguments not enough"), --(T)->top)

/* user defined thread opening function */
#if !defined(aloi_openthread)
#define aloi_openthread(T,from) aloE_void(T)
#endif

/* user defined thread closing function */
#if !defined(aloi_closethread)
#define aloi_closethread(T) aloE_void(T)
#endif

/* user defined thread yielding function */
#if !defined(aloi_yieldthread)
#define aloi_yieldthread(T,n) aloE_void(T)
#endif

/* user defined thread resuming function */
#if !defined(aloi_resumethread)
#define aloi_resumethread(T,n) aloE_void(T)
#endif

/* user defined try function */
#if !defined(aloi_try)
#define aloi_try(T,b,p) ({ aloE_void(T); volatile int $status = setjmp((b).buf); if ($status == ThreadStateRun) { p; } $status; })
#endif

/* user defined throw function */
#if !defined(aloi_throw)
#define aloi_throw(T,b,s) ({ aloE_void(T); longjmp((b).buf, s); })
#endif

/* user defined yield function */
#if !defined(aloi_yield)
#define aloi_yield(T,b) ({ aloE_void(T); longjmp((b).buf, ThreadStateYield); })
#endif

#endif /* ART_H_ */
