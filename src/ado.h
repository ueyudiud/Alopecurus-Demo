/*
 * ado.h
 *
 * implements of VM.
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#ifndef ADO_H_
#define ADO_H_

#include "astate.h"

#define getstkoff(T,s) ((s) - (T)->stack)
#define putstkoff(T,o) ((T)->stack + (o))

#ifdef ALO_HARDMEMTEST
#define adjuststack(T,n) aloD_reallocstack(T, (T)->stacksize)
#else
#define adjuststack(T,n) aloE_void(0)
#endif

#define aloD_adjustresult(T,nres) ({ if (nres == ALO_MULTIRET && T->frame->top < T->top) { T->frame->top = T->top; } })

/**
 ** check stack size, if stack will be resized, call 'pre' and 'post'.
 */
#define aloD_checkstack(T,n) aloD_checkstackaux(T, n, aloE_void(0), aloE_void(0))
#define aloD_checkstackaux(T,n,pre,post) \
	({ if (aloE_cast(size_t, ((T)->top + (n)) - (T)->stack) >= (T)->stacksize) { pre; aloD_growstack(T, n); post; } else { adjuststack(T, n); } })

#define aloB_pushobj(T,o) aloB_pushobj_(T, r2g(o))

/**
 ** function with protection.
 */
typedef void (*apfun)(alo_State, void*);

ALO_IFUN void aloD_setdebt(aglobal_t*, ssize_t);
ALO_IFUN void aloD_growstack(alo_State, size_t);
ALO_IFUN void aloD_reallocstack(alo_State, size_t);
ALO_IFUN void aloD_shrinkstack(alo_State);
ALO_IFUN int aloD_prun(alo_State, apfun, void*);
ALO_IFUN a_none aloD_throw(alo_State, int);
ALO_IFUN void aloD_hook(alo_State, int, int);
ALO_IFUN int aloD_rawcall(alo_State, alo_StkId, int, int*);
ALO_IFUN int aloD_precall(alo_State, alo_StkId, int);
ALO_IFUN void aloD_postcall(alo_State, alo_StkId, int);
ALO_IFUN void aloD_call(alo_State, alo_StkId, int);
ALO_IFUN void aloD_callnoyield(alo_State, alo_StkId, int);
ALO_IFUN int aloD_pcall(alo_State, alo_StkId, int, ptrdiff_t);

#endif /* ADO_H_ */
