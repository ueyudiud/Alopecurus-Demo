/*
 * abuf.c
 *
 *  Created on: 2019年7月25日
 *      Author: ueyudiud
 */

#define ABUF_C_
#define ALO_CORE

#include "abuf.h"
#include "astate.h"
#include "amem.h"

#include <string.h>

/* get top of buffer */
#define gettop(b) ((b)->buf + (((b)->len + 15) & ~0xF))

#define ALO_MAX_MEMORY_STACK_SIZE 10000000

static int ifill(astate T, aibuf_t* buf) {
	aloE_assert(stack->len == 0, "not reached to end of buffer yet.");
	return buf->reader(T, buf->context, &buf->pos, &buf->len) || buf->len == 0;
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

static void growstack(astate T, size_t required) {
	ambuf_t* base = basembuf(T);
	size_t oldsize = base->cap;
	void* oldmemory = base->buf;
	size_t newsize = aloM_adjsize(T, oldsize, required, ALO_MAX_MEMORY_STACK_SIZE);
	newsize = (newsize + 15) & ~0xF; /* align size */
	void* newmemory = aloM_realloc(T, oldmemory, oldsize, newsize);
	base->cap = newsize;
	if (oldmemory != newmemory) { /* address changed? */
		/* adjust memory buffer addresses */
		base->buf = newmemory;
		ptrdiff_t off = newmemory - oldmemory;
		ambuf_t* buf = T->memstk.top;
		while (buf != base) {
			buf->buf += off;
			buf = buf->prev;
		}
	}
}

static void growbuf(astate T, ambuf_t* buf, size_t size) {
	aloE_assert(size > stack->data.cap, "growing is not required.");
	aloE_assert(T->memstk.top == stack, "not on the top of stack.");
	size_t req = size + (buf->buf - T->memstk.base.buf);
	if (req > T->memstk.base.cap) {
		growstack(T, req);
	}
	buf->cap = size;
}

void aloB_openaux(astate T, ambuf_t* buf) {
	buf->buf = gettop(T->memstk.top);
	buf->cap = T->memstk.base.cap - (buf->buf - T->memstk.base.buf);
	buf->len = 0;
	buf->prev = T->memstk.top;
	T->memstk.top = buf;
}

int aloB_bwrite(astate T, void* context, const void* src, size_t len) {
	ambuf_t* buf = aloE_cast(ambuf_t*, context); /* context is buffer */
	aloB_lock(stack);
	size_t nlen = buf->len + len;
	if (nlen > buf->cap) { /* should expand buffer size? */
		ptrdiff_t off = src - aloE_cast(void*, basembuf(T)->buf);
		if (off >= 0 && off < aloE_cast(ptrdiff_t, basembuf(T)->cap)) { /* if source is also in the memory buffer */
			growbuf(T, buf, aloM_adjsize(T, buf->cap, nlen, ALO_MBUF_LIMIT));
			src = basembuf(T)->buf + off; /* adjust source pointer */
		}
		else {
			growbuf(T, buf, aloM_adjsize(T, buf->cap, nlen, ALO_MBUF_LIMIT));
		}
	}
	memcpy(buf->buf + buf->len, src, len); /* writer data to buffer */
	buf->len = nlen; /* increase length */
	aloB_unlock(stack);
	return 0; /* write into string buffer never causes an error */
}

void aloB_bgrow_(astate T, ambuf_t* buf) {
	aloB_check(T, b);
	aloE_assert(stack->data.len == stack->data.cap, "buffer not full filled.");
	aloB_lock(stack);
	growbuf(T, buf, buf->cap + 1);
	aloB_unlock(stack);
}

void aloB_puts(astate T, ambuf_t* buf, const char* src) {
	aloB_check(T, stack);
	aloB_bwrite(T, buf, src, strlen(src) * sizeof(char));
}

void alo_pushbuf(astate T, ambuf_t* buf) {
	aloB_lock(buf);
	aloB_openaux(T, buf);
	aloB_unlock(buf);
}

void alo_growbuf(astate T, ambuf_t* buf, size_t req) {
	aloB_lock(buf);
	api_check(T, buf == T->memstk.top, "not the top of memory stack.");
	growbuf(T, buf, req);
	aloB_unlock(buf);
}

void alo_popbuf(astate T, ambuf_t* buf) {
	aloB_lock(stack);
	api_check(T, buf == T->memstk.top, "not the top of memory stack.");
	aloB_close(T, *buf);
	buf->buf = NULL; /* force to close buffer */
	aloB_unlock(buf);
}
