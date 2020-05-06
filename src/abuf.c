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
#include "adebug.h"

#include <string.h>

#define SIZE_ALIGNMENT (1 << 4)

#define alignsize(value) (((value) + (SIZE_ALIGNMENT - 1)) & ~(SIZE_ALIGNMENT - 1))

/* get top of buffer */
#define gettop(b) ((b)->ptr + alignsize((b)->len))

#define ALO_INIT_MEMORY_STACK_SIZE 256
#define ALO_MAX_MEMORY_STACK_SIZE 10000000

static int ifill(astate T, aibuf_t* buf) {
	aloE_assert(buf->len == 0, "not reached to end of buffer yet.");
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

static size_t adjsize(astate T, size_t oldsize, size_t required) {
	if (required > ALO_MAX_MEMORY_STACK_SIZE) {
		aloU_szoutoflim(T);
	}
	size_t newsize = oldsize > 0 ? oldsize * 2 : ALO_INIT_MEMORY_STACK_SIZE;
	if (newsize > ALO_MAX_MEMORY_STACK_SIZE) {
		newsize = ALO_MAX_MEMORY_STACK_SIZE;
	}
	else if (newsize < required) {
		newsize = alignsize(required); /* align size */
	}
	aloE_assert((newsize & (SIZE_ALIGNMENT - 1)) == 0, "size not aligned");
	return newsize;
}

static void growstack(astate T, size_t required) {
	ambuf_t* base = basembuf(T);
	size_t oldsize = base->cap;
	void* oldmemory = base->ptr;
	size_t newsize = adjsize(T, oldsize, required);
	void* newmemory = aloM_realloc(T, oldmemory, oldsize, newsize);
	base->cap = newsize;
	if (oldmemory != newmemory) { /* address changed? */
		/* adjust memory buffer addresses */
		base->ptr = newmemory;
		ptrdiff_t off = newmemory - oldmemory;
		ambuf_t* buf = T->memstk.top;
		while (buf != base) {
			buf->ptr += off;
			buf = buf->prev;
		}
	}
}

static void growbuf(astate T, ambuf_t* buf, size_t size) {
	aloE_assert(size > buf->cap, "growing is not required.");
	aloE_assert(T->memstk.top == buf, "not on the top of stack.");
	size_t req = size + (buf->ptr - T->memstk.ptr);
	if (req > T->memstk.cap) {
		growstack(T, req);
	}
	buf->cap = size;
}

void aloB_open(astate T, ambuf_t* buf) {
	buf->ptr = gettop(T->memstk.top);
	buf->cap = T->memstk.cap - (buf->ptr - T->memstk.ptr);
	buf->len = 0;
	buf->prev = T->memstk.top;
	T->memstk.top = buf;
}

int aloB_bwrite(astate T, void* context, const void* src, size_t len) {
	ambuf_t* buf = aloE_cast(ambuf_t*, context); /* context is buffer */
	aloB_lock(stack);
	size_t nlen = buf->len + len;
	if (nlen > buf->cap) { /* should expand buffer size? */
		ptrdiff_t off = src - aloE_cast(void*, T->memstk.ptr);
		if (off >= 0 && off < aloE_cast(ptrdiff_t, T->memstk.cap)) { /* if source is also in the memory buffer */
			growbuf(T, buf, aloM_adjsize(T, buf->cap, nlen, ALO_MBUF_LIMIT));
			src = T->memstk.ptr + off; /* adjust source pointer */
		}
		else {
			growbuf(T, buf, aloM_adjsize(T, buf->cap, nlen, ALO_MBUF_LIMIT));
		}
	}
	memcpy(buf->ptr + buf->len, src, len); /* writer data to buffer */
	buf->len = nlen; /* increase length */
	aloB_unlock(stack);
	return 0; /* write into string buffer never causes an error */
}

void aloB_bgrow(astate T, ambuf_t* buf) {
	aloB_check(T, buf);
	aloE_assert(buf->len == buf->cap, "buffer not full filled.");
	aloB_lock(stack);
	growbuf(T, buf, buf->cap + 1);
	aloB_unlock(stack);
}

void aloB_puts(astate T, ambuf_t* buf, const char* src) {
	aloB_check(T, buf);
	aloB_bwrite(T, buf, src, strlen(src) * sizeof(char));
}

void aloB_shrink(astate T) {
	amstack_t* stack = &T->memstk;
	if (stack->cap > ALO_INIT_MEMORY_STACK_SIZE * 2 &&
			aloE_cast(size_t, gettop(stack->top) - stack->ptr) * 4 < stack->cap) {
		aloM_adjb(T, stack->ptr, stack->cap, alignsize(stack->cap / 2));
	}
}

void alo_bufpush(astate T, ambuf_t* buf) {
	aloB_lock(buf);
	aloB_open(T, buf);
	aloB_unlock(buf);
}

void alo_bufgrow(astate T, ambuf_t* buf, size_t req) {
	aloB_lock(buf);
	api_check(T, buf == T->memstk.top, "not the top of memory stack.");
	growbuf(T, buf, req);
	aloB_unlock(buf);
}

void alo_bufpop(astate T, ambuf_t* buf) {
	aloB_lock(stack);
	api_check(T, buf == T->memstk.top, "not the top of memory stack.");
	aloB_close(T, buf);
	buf->ptr = NULL; /* avoid released memory be visited */
	buf->prev = NULL; /* mark buffer released */
	aloB_unlock(buf);
}
