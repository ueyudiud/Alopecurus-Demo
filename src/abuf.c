/*
 * abuf.c
 *
 *  Created on: 2019年7月25日
 *      Author: ueyudiud
 */

#include "abuf.h"
#include "astate.h"
#include "amem.h"

#include <string.h>

/**
 ** return true if buffer used inner memory.
 */
#define isinner(b) ((b)->shrbuf == (b)->array)

static int ifill(astate T, aibuf_t* buf) {
	aloE_assert(buf->len == 0, "not reached to end of buffer yet.");
	return (T->berrno = buf->reader(T, buf->context, &buf->pos, &buf->len)) || buf->len == 0;
}

int aloB_ifill_(astate T, aibuf_t* buf) {
	if (ifill(T, buf)) { /* catch an error or reach to end of buffer */
		return EOB;
	}
	buf->len--;
	return aloE_byte(*(buf->pos++));
}

size_t aloB_iread(astate T, aibuf_t* buf, amem mem, size_t n) {
	if (n) {
		if (buf->len == 0) {
			label:
			if (ifill(T, buf)) {
				return n;
			}
		}
		if (n > buf->len) {
			memcpy(mem, buf->pos, buf->len);
			mem += buf->len;
			n -= buf->len;
			buf->len = 0;
			goto label;
		}
		else {
			memcpy(mem, buf->pos, n);
			buf->pos += n;
			buf->len -= n;
		}
	}
	return 0;
}

int aloB_bwrite(astate T, void* context, const void* src, size_t len) {
	asbuf_t* buf = aloE_cast(asbuf_t*, context); /* context is buffer */
	aloB_lock(buf);
	size_t nlen = buf->length + len;
	if (nlen > buf->capacity) { /* should expand buffer size? */
		size_t newsize = aloM_adjsize(T, buf->capacity, nlen, ALO_SBUF_LIMIT);
		if (isinner(buf)) {
			char* newarray = aloM_newa(T, char, newsize);
			memcpy(newarray, buf->shrbuf, buf->length);
			buf->array = newarray;
		}
		else {
			buf->array = aloM_adja(T, buf->array, buf->capacity, newsize);
		}
		buf->capacity = newsize;
	}
	memcpy(buf->array + buf->length, src, len); /* writer data to buffer */
	buf->length = nlen; /* increase length */
	aloB_unlock(buf);
	return 0; /* write into string buffer never causes an error */
}

void aloB_bfreeaux(astate T, asbuf_t* buf) {
	aloB_lock(buf);
	aloE_assert(!isinner(buf), "invalid buffer.");
	aloM_delb(T, buf->array, buf->capacity); /* delete data and close buffer. */
}

void aloB_bgrow_(astate T, asbuf_t* buf) {
	aloE_assert(buf->length == buf->capacity, "buffer not full filled.");
	aloB_lock(buf);
	size_t newsize = aloM_growsize(T, buf->capacity, ALO_SBUF_LIMIT);
	if (isinner(buf)) {
		char* newarray = aloM_newa(T, char, newsize);
		memcpy(newarray, buf->shrbuf, buf->length);
		buf->array = newarray;
	}
	else {
		buf->array = aloM_adja(T, buf->array, buf->capacity, newsize);
	}
	buf->capacity = newsize;
	aloB_unlock(buf);
}

void aloB_puts(astate T, asbuf_t* buf, const char* src) {
	aloB_bwrite(T, buf, src, strlen(src) * sizeof(char));
}
