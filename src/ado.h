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
	({ if (((T)->top + (n)) - (T)->stack >= (T)->stacksize) { pre; aloD_growstack(T, n); post; } else { adjuststack(T, n); } })

#define aloB_pushobj(T,o) aloB_pushobj_(T, r2g(o))

/**
 ** function with protection.
 */
typedef void (*apfun)(astate, void*);

ALO_IFUN void aloD_setdebt(aglobal_t*, ssize_t);
ALO_IFUN void aloD_growstack(astate, size_t);
ALO_IFUN void aloD_reallocstack(astate, size_t);
ALO_IFUN void aloD_shrinkstack(astate);
ALO_IFUN int aloD_prun(astate, apfun, void*);
ALO_IFUN anoret aloD_throw(astate, int);
ALO_IFUN void aloD_hook(astate, int, int);
ALO_IFUN int aloD_rawcall(astate, askid_t, int, int*);
ALO_IFUN int aloD_precall(astate, askid_t, int);
ALO_IFUN void aloD_postcall(astate, askid_t, int);
ALO_IFUN void aloD_call(astate, askid_t, int);
ALO_IFUN void aloD_callnoyield(astate, askid_t, int);

#endif /* ADO_H_ */
