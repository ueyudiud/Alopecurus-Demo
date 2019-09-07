/*
 * amem.h
 *
 * memory allocation.
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#ifndef AMEM_H_
#define AMEM_H_

#include "aobj.h"

/**
 ** basic memory allocation.
 */

amem aloM_realloc(astate, amem, size_t, size_t);

#define aloM_malloc(T,newlen) aloM_realloc(T, NULL, 0, newlen)

void aloM_free(astate, amem, size_t);

/**
 ** typed memory allocation.
 */

#define aloM_auxt(t,b) aloE_cast(typeof(t)*, b)

#define aloM_newo(T,t) aloM_auxt(t, aloM_malloc(T, sizeof(t)))

#define aloM_delo(T,b) aloM_free(T, b, sizeof(*(b)))

#define aloM_newa(T,t,l) aloM_auxt(t, aloM_malloc(T, (l) * sizeof(t)))

#define aloM_adja(T,b,o,n) aloM_auxt((b)[0], aloM_realloc(T, b, (o) * sizeof((b)[0]), (n) * sizeof((b)[0])))

#define aloM_dela(T,b,l) aloM_free(T, b, (l) * sizeof((b)[0]))

size_t aloM_adjsize(astate, size_t capacity, size_t require, size_t limit);

size_t aloM_growsize(astate, size_t capacity, size_t limit);

/**
 ** buffer auxiliary macros.
 */

#define aloM_updateaux(T,b,c,n,d...) \
	({ size_t _old = c; size_t _new = n; b = aloM_adja(T, b, _old, _new); d; c = _new; })

#define aloM_update(T,b,c,r,i) aloM_updateaux(T, b, c, r, for (size_t _index = _old; _index < _new; ++_index) i((b) + _index))

#define aloM_ensbx(T,b,c,l,e,x,i) \
	({ \
		size_t _require = (l) + (e); \
		_require > (c) ? (aloM_update(T, b, c, aloM_adjsize(T, c, _require, x), i), true) : false; \
	})

#define aloM_chkb(T,b,c,l,x) ((l) >= (c) ? (aloM_adjb(T, b, c, aloM_growsize(T, c, x)), true) : false)
#define aloM_chkbx(T,b,c,l,x,i) ((l) >= (c) ? (aloM_update(T, b, c, aloM_growsize(T, c, x), i), true) : false)

#define aloM_newb(T,b,c,i) aloE_void(b = aloM_newa(T, (b)[0], i), c = (i))

#define aloM_adjb(T,b,c,n) aloM_updateaux(T, b, c, n, aloE_void(0))

#define aloM_clsb(b,c) aloE_void(b = NULL, c = 0)

#define aloM_delb(T,b,c) (aloM_dela(T, b, c), aloM_clsb(b, c))

#endif /* AMEM_H_ */
