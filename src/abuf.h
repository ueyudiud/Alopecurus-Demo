/*
 * abuf.h
 *
 * buffer.
 *
 *  Created on: 2019年7月25日
 *      Author: ueyudiud
 */

#ifndef ABUF_H_
#define ABUF_H_

#include "art.h"

/**
 ** properties of string buffer.
 */

/* end of buffer */
#define EOB (-1)

typedef struct alo_IBuf aibuf_t;

#define aloB_lock(b) aloE_void(0)
#define aloB_unlock(b) aloE_void(0)

#define aloB_decl(n) ambuf_t n; aloB_open(n)
#define aloB_open(b) aloE_void((b).cap = sizeof((b).instk), (b).len = 0, (b).buf = (b).instk)

#define aloB_close(T,b) ({ if ((b).instk != (b).buf) aloB_bcloseaux(T, &(b)); })

#define aloB_iopen(b,reader,context) (*(b) = (aibuf_t) { NULL, 0, reader, context })
#define aloB_iget(T,b) ((b)->len > 0 ? ((b)->len--, aloE_byte(*((b)->pos++))) : aloB_ifill_(T, b))

#define aloB_tostr(T,b) aloS_new(T, aloE_cast(char*, (b).buf), (b).len)

ALO_IFUN int aloB_ifill_(astate, aibuf_t*);
ALO_IFUN size_t aloB_iread(astate, aibuf_t*, amem, size_t);

ALO_IFUN int aloB_bwrite(astate, void*, const void*, size_t);
ALO_IFUN void aloB_bcloseaux(astate, ambuf_t*);

#define aloB_putc(T,b,ch) ((b)->len < (b)->cap ? aloB_rputc(b, ch) : (aloB_bgrow_(T, b), aloB_rputc(b, ch)))
#define aloB_rputc(b,ch) aloE_void((b)->buf[(b)->len++] = (ch))

ALO_IFUN void aloB_bgrow_(astate, ambuf_t*);
ALO_IFUN void aloB_puts(astate, ambuf_t*, const char*);

struct alo_IBuf {
	const char* pos;
	size_t len;
	areader reader;
	void* context;
};

#endif /* ABUF_H_ */
