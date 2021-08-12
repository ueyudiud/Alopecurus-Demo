/*
 * astate.h
 *
 * thread and global state.
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#ifndef ASTATE_H_
#define ASTATE_H_

#include "aobj.h"
#include "ameta.h"
#include "alo.h"

#include <signal.h>

/**
 ** interned string table.
 */
typedef struct {
	size_t capacity;
	size_t length;
	alo_Str** array;
} aitable_t;

/**
 ** memory stack to allocate and free memory buffer.
 */
typedef struct {
	ambuf_t* top; /* top of buffer stack */
	a_byte* ptr; /* the memory pointer of buffer */
	size_t cap; /* the capacity of buffer */
	size_t len; /* always be zero */
} amstack_t;

/* get base memory buffer */
#define basembuf(T) aloE_cast(ambuf_t*, &(T)->memstk.ptr)

/**
 ** global state, shared by each thread.
 */
typedef struct alo_Global {
	alo_Alloc alloc;
	void* context;
	ssize_t mbase; /* base memory */
	ssize_t mdebt; /* memory debt */
	size_t mestimate; /* estimated memory for live objects */
	size_t mtraced; /* traced memory (only used in GC) */
	aitable_t itable; /* intern string table */
	alo_TVal registry; /* global environment */
	a_hash seed; /* randomized seed for hash */
	alo_Obj* gnormal; /* list for normal objects */
	alo_Obj* gfixed; /* list for fixed objects */
	alo_Obj* gfnzable; /* list for finalizable objects */
	alo_Obj* gtobefnz; /* list for objects to be finalize */
	alo_Obj* gray; /* list for gray objects (only used in GC) */
	alo_Obj* grayagain; /* list for gray again objects (only used in GC) */
	alo_Obj* weakk; /* list for weak key linked collection */
	alo_Obj* weakv; /* list for weak value linked collection */
	alo_Obj* weaka; /* list for all weak linked collection */
	alo_Obj** sweeping; /* sweeping object list */
	size_t nfnzobj; /* count of calling finalizer each step */
	unsigned gcpausemul; /* the multiplier for GC pause */
	unsigned gcstepmul; /* the multiplier for GC step */
	a_cfun panic; /* the handler for uncaught errors */
	alo_Thread* tmain; /* main thread */
	alo_Thread* trun; /* the current running thread */
	alo_Str* smerrmsg; /* memory error message */
	alo_Str* sempty; /* empty string */
	alo_Str* stagnames[TM_N]; /* special tag method names */
	alo_Str* scache[ALO_STRCACHE_N][ALO_STRCACHE_M]; /* string cache */
	alo_TVal stack[1]; /* reserved stack, used for provided a fake stack */
	const aver_t* version; /* version of VM */
	a_byte whitebit; /* current white bit */
	a_byte gcstep; /* current GC step */
	a_byte gckind; /* kind of GC running */
	a_byte gcrunning; /* true if GC running */
	union {
		struct {
			a_byte fgc : 1;
		};
		a_byte flags;
	};
} aglobal_t;

/*  get total memory used by VM. */
#define totalmem(G) aloE_cast(size_t, (G)->mbase + (G)->mdebt)

/**
 ** function calling frame.
 */
typedef struct alo_Frame aframe_t;

struct alo_Frame {
	alo_StkId fun; /* function index */
	alo_StkId top; /* top index */
	aframe_t *prev, *next; /* linked list for frames */
	a_cstr name; /* calling function name (might be null if it is unknown sources) */
	alo_TVal* env; /* the environment of current frame */
	union { /* specific data for different functions */
		struct { /* for Alopecurus functions */
			alo_StkId base;
			const a_insn* pc;
		} a;
		struct { /* for C functions */
			a_kfun kfun;
			a_kctx kctx;
			ptrdiff_t oef; /* the old error function for previous frame */
		} c;
	};
	int nresult; /* expected results from this frame */
	union { /* frame flags */
		struct {
			a_byte mode : 3; /* 3 bits for calling mode */
			a_byte falo : 1; /* is frame called by Alopecurus function */
			a_byte ffnz : 1; /* is calling finalizer */
			a_byte fypc : 1; /* is function calling in yieldable protection */
			a_byte fact : 1; /* is active calling by VM */
			a_byte fitr : 1; /* is iterator 'hasNext' checking */
			a_byte fhook: 1; /* is frame calling a hook */
		};
		a_byte flags;
	};
};

enum {
	FrameModeNormal,	/* a normal call */
	FrameModeTail,		/* a tail call */
	FrameModeFianlize,	/* a finalizing call by GC */
	FrameModeCompare	/* a call with '__lt' or '__le' */
};

struct alo_Thread {
	ALO_AGGR_HEADER(
		a_byte status;
		union {
			struct {
				a_byte fallowhook : 1;
			};
			a_byte flags;
		};
		a_byte hookmask; /* hook mask */
	);
	aglobal_t* g; /* global state */
	alo_Thread* caller;
	aframe_t* frame;
	alo_Capture* captures; /* captures */
	alo_StkId stack; /* stack base */
	alo_StkId top; /* top of stack */
	size_t stacksize; /* current stack size */
	ajmp_t* label;
	ptrdiff_t errfun; /* index of error handling function */
	amstack_t memstk; /* memory stack */
	ahfun hook;
	aframe_t base_frame;
	uint16_t nframe; /* frame depth */
	uint16_t nxyield; /* nested non-yieldable depth */
	uint16_t nccall; /* C caller depth */
};

/**
 ** the extra stack for tagged methods call and other utility methods.
 */
#define EXTRA_STACK 8

#define Gd(T) aglobal_t* G = (T)->g

ALO_IFUN void aloR_closeframe(alo_State, aframe_t*);
ALO_IFUN void aloR_deletethread(alo_State, alo_Thread*);
ALO_IFUN aframe_t* aloR_topframe(alo_State);
ALO_IFUN aframe_t* aloR_pushframe(alo_State);

#endif /* ASTATE_H_ */
