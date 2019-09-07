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

#define ALO_SBUF_SHTLEN 32
#define ALO_SBUF_LIMIT 10000000

typedef struct alo_IBuf aibuf_t;

#define aloB_lock(buf) aloE_void(0)
#define aloB_unlock(buf) aloE_void(0)

#define aloB_bdecl(name) asbuf_t name; aloB_bopen(name)
#define aloB_bopen(buf) aloE_void((buf).capacity = ALO_SBUF_SHTLEN, (buf).length = 0, (buf).array = (buf).shrbuf)

#define aloB_bfree(T,buf) if ((buf).shrbuf != (buf).array) aloB_bfreeaux(T, &(buf))

#define aloB_iopen(buf,reader,context) (*(buf) = (aibuf_t) { NULL, 0, reader, context })
#define aloB_iget(T,buf) ((buf)->len > 0 ? ((buf)->len--, *((buf)->pos++)) : aloB_ifill_(T, buf))

int aloB_ifill_(astate, aibuf_t*);
size_t aloB_iread(astate, aibuf_t*, amem, size_t);

int aloB_bwrite(astate, void*, const void*, size_t);
void aloB_bfreeaux(astate, asbuf_t*);

#define aloB_putc(T,buf,ch) ((buf)->length < (buf)->capacity ? aloB_rputc(buf, ch) : (aloB_bgrow_(T, buf), aloB_rputc(buf, ch)))
#define aloB_rputc(buf,ch) aloE_void((buf)->array[(buf)->length++] = (ch))

void aloB_bgrow_(astate, asbuf_t*);
void aloB_puts(astate, asbuf_t*, const char*);

struct alo_SBuf {
	size_t capacity;
	size_t length;
	char* array;
	char shrbuf[ALO_SBUF_SHTLEN];
};

struct alo_IBuf {
	const char* pos;
	size_t len;
	areader reader;
	void* context;
};

#endif /* ABUF_H_ */
