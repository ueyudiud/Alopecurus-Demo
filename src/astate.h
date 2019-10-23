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

#include <signal.h>

/**
 ** interned string table.
 */
typedef struct {
	size_t capacity;
	size_t length;
	astring_t** array;
} aitable_t;

/**
 ** table to store extra C function information.
 */
typedef struct {
	size_t capacity;
	size_t length;
	acreg_t* array;
} actable_t;

/**
 ** global state, shared by each thread.
 */
typedef struct alo_Global {
	aalloc alloc;
	void* context;
	ssize_t mbase; /* base memory */
	ssize_t mdebt; /* memory debt */
	size_t mestimate; /* estimated memory for live objects */
	size_t mtraced; /* traced memory (only used in GC) */
	aitable_t itable; /* intern string table */
	actable_t ctable; /* C function information table */
	atval_t registry; /* global environment */
	ahash_t seed; /* randomized seed for hash */
	agct gnormal; /* list for normal objects */
	agct gfixed; /* list for fixed objects */
	agct gfnzable; /* list for finalizable objects */
	agct gtobefnz; /* list for objects to be finalize */
	agct gray; /* list for gray objects (only used in GC) */
	agct grayagain; /* list for gray again objects (only used in GC) */
	agct weakk; /* list for weak key linked collection */
	agct weakv; /* list for weak value linked collection */
	agct weaka; /* list for all weak linked collection */
	agct* sweeping; /* sweeping object list */
	size_t nfnzobj; /* count of calling finalizer each step */
	unsigned gcpausemul; /* the multiplier for GC pause */
	unsigned gcstepmul; /* the multiplier for GC step */
	acfun panic; /* the handler for uncaught errors */
	athread_t* tmain; /* main thread */
	athread_t* trun; /* the current running thread */
	astring_t* smerrmsg; /* memory error message */
	astring_t* sempty; /* empty string */
	astring_t* stagnames[TM_N]; /* special tag method names */
	astring_t* scache[ALO_STRCACHE_N][ALO_STRCACHE_M]; /* string cache */
	atval_t stack[1]; /* reserved stack, used for provided a fake stack */
	aver_t version; /* version of VM */
	abyte whitebit; /* current white bit */
	abyte gcstep; /* current GC step */
	abyte gckind; /* kind of GC running */
	abyte gcrunning; /* true if GC running */
	union {
		struct {
			abyte fgc : 1;
		};
		abyte flags;
	};
} aglobal_t;

/*  get total memory used by VM. */
#define totalmem(G) ((G)->mbase + (G)->mdebt)

/**
 ** function calling frame.
 */
typedef struct alo_Frame aframe_t;

struct alo_Frame {
	askid_t fun; /* function index */
	askid_t top; /* top index */
	aframe_t *prev, *next; /* linked list for frames */
	astr name; /* calling function name (might be null if it is unknown sources) */
	union { /* specific data for different functions */
		struct { /* for Alopecurus functions */
			askid_t base;
			ainsn_t* pc;
		} a;
		struct { /* for C functions */
			akfun kfun;
			void* ctx;
		} c;
	};
	int nresult; /* expected results from this frame */
	union { /* frame flags */
		struct {
			abyte mode : 3; /* 3 bits for calling mode */
			abyte falo : 1; /* is frame called by Alopecurus function */
			abyte ffnz : 1; /* is calling finalizer */
			abyte fypc : 1; /* is function calling in yieldable protection */
			abyte fact : 1; /* is active calling by VM */
		};
		abyte flags;
	};
};

enum {
	FrameModeNormal,	/* a normal call */
	FrameModeTail,		/* a tail call */
	FrameModeFianlize,	/* a finalizing call by GC */
	FrameModeCompare	/* a call with '__lt' or '__le' */
};

struct alo_Thread {
	RefHeader(
		abyte status;
		union {
			struct {
				abyte fallowhook : 1;
			};
			abyte flags;
		}
	);
	aglobal_t* g; /* global state */
	athread_t* caller;
	aframe_t* frame;
	acap* captures; /* captures */
	askid_t stack; /* stack base */
	askid_t top; /* top of stack */
	size_t stacksize; /* current stack size */
	ajmp_t* label;
	aframe_t base_frame;
	int berrno; /* buffer error */
	uint16_t nframe; /* frame depth */
	uint16_t nxyield; /* nested non-yieldable depth */
	uint16_t nccall; /* C caller depth */
	sig_atomic_t signal;
};

/**
 ** the extra stack for tagged methods call and other utility methods.
 */
#define EXTRA_STACK 8

#define Gd(T) aglobal_t* G = (T)->g

void aloR_closeframe(astate, aframe_t*);
void aloR_deletethread(astate, athread_t*);
aframe_t* aloR_topframe(astate);
aframe_t* aloR_pushframe(astate);

#endif /* ASTATE_H_ */
