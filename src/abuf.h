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

#define aloB_iopen(b,reader,context) (*(b) = (aibuf_t) { NULL, 0, reader, context })
#define aloB_iget(T,b) ((b)->len > 0 ? ((b)->len--, aloE_byte(*((b)->pos++))) : aloB_ifill_(T, b))

#define aloB_tostr(T,b) aloS_new(T, aloE_cast(char*, (b)->ptr), (b)->len)

ALO_IFUN int aloB_ifill_(astate, aibuf_t*);
ALO_IFUN size_t aloB_iread(astate, aibuf_t*, amem, size_t);


#define aloB_decl(T,n) ambuf_t n[1]; aloB_open(T, n)

#define aloB_check(T,b,e...) aloE_check((b) == (T)->memstk.top, "buffer not on the top of stack", (aloE_void(0), ##e))

#define aloB_close(T,b) aloB_check(T, b, (T)->memstk.top = (b)->prev)

#define aloB_putc(T,b,ch) ((b)->len < (b)->cap ? aloB_rputc(T, b, ch) : (aloB_bgrow(T, b), aloB_rputc(T, b, ch)))
#define aloB_rputc(T,b,ch) aloE_void(aloB_check(T, b, (b)->ptr[(b)->len++] = aloE_byte(ch)))

ALO_IFUN void aloB_open(astate, ambuf_t*);
ALO_IFUN void aloB_bgrow(astate, ambuf_t*);
ALO_IFUN void aloB_puts(astate, ambuf_t*, const char*);
ALO_IFUN int aloB_bwrite(astate, void*, const void*, size_t);

ALO_IFUN void aloB_shrink(astate);

struct alo_IBuf {
	const char* pos;
	size_t len;
	areader reader;
	void* context;
};

#endif /* ABUF_H_ */
