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

/**
 ** primitive types
 */

typedef uint8_t abyte;
typedef int64_t aint;
typedef double afloat;
typedef const char* astr;
typedef void* amem;

typedef struct alo_Version {
	abyte major;
	abyte minor;
} aver_t;

#define anoret __attribute__((noreturn)) void

typedef struct alo_MemBuf ambuf_t;

/* minimum capacity of memory buffer, the memory will be allocated on stack */
#define ALO_MBUF_SHTLEN 32

struct alo_MemBuf {
	size_t cap; /* the capacity of buffer */
	size_t len; /* the length filled in buffer */
	abyte* buf; /* the start pointer of buffer */
	ambuf_t* prev; /* the previous memory buffer in linked list */
	abyte instk[ALO_MBUF_SHTLEN]; /* fast buffer in stack */
};

/**
 ** Alopecurus state and handle types
 */

typedef struct alo_Thread *astate;
typedef int (*acfun)(astate);
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
	ALO_TPROTO,
	ALO_TCAPTURE
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
	ThreadStateRun,
	ThreadStateYield,
	ThreadStateErrRuntime,
	ThreadStateErrSerialize,
	ThreadStateErrCompile,
	ThreadStateErrFinalize,
	ThreadStateErrMemory,
	ThreadStateErrError
};

#define ALO_MULTIRET (-1)

#define ALO_ITERATE_BEGIN (-1)

#define ALO_INT_PROP(field) INT64_##field
#define ALO_FLT_PROP(field) DOUBLE##field

/**
 ** useful macros
 */

#define aloE_assert(exp,what) ALO_ASSERT(exp,what)
#define aloE_log(what,fmt...) ALO_LOG(what, ##fmt)
#define aloE_void(exp...) ((void) (exp))
#define aloE_cast(type,exp) ((type) (exp))
#define aloE_byte(exp) aloE_cast(abyte, exp)

#endif /* ADEF_H_ */
