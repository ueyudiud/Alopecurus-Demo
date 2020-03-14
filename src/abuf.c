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

/* return true if buffer used inner memory. */
#define isinner(b) ((b)->instk == (b)->buf)
/* get top of buffer */
#define gettop(b) ((b)->buf + (b)->len)

#define ALO_MAX_MEMORY_STACK_SIZE 100000

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

static void growmstack(astate T, size_t required) {
	size_t oldsize = T->memstk.cap;
	void* oldmemory = T->memstk.buf;
	size_t newsize = aloM_adjsize(T, oldsize, required, ALO_MAX_MEMORY_STACK_SIZE);
	void* newmemory = aloM_realloc(T, oldmemory, oldsize, newsize);
	T->memstk.cap = newsize;
	if (oldmemory != newmemory) { /* address changed? */
		/* adjust memory buffer addresses */
		T->memstk.buf = newmemory;
		ptrdiff_t off = newmemory - oldmemory;
		ambuf_t* buf = T->memstk.top;
		while (buf != basembuf(T)) {
			buf->buf += off;
			buf = buf->prev;
		}
	}
}

#define checkmstack(T,r) { size_t _required = (r); if (_required > (T)->memstk.cap) growmstack(T, _required);  }

static void growmbuf(astate T, ambuf_t* buf, size_t size) {
	aloE_assert(size > buf->data.cap, "growing is not required.");
	if (isinner(buf)) {
		checkmstack(T, size + (gettop(T->memstk.top) - T->memstk.buf));
		buf->buf = gettop(T->memstk.top);
		memcpy(buf->buf, buf->instk, buf->len);
		buf->prev = T->memstk.top;
		T->memstk.top = buf;
	}
	else {
		aloE_assert(T->memstk.top == buf, "not on the top of stack.");
		checkmstack(T, size + (buf->buf - T->memstk.buf));
	}
	buf->cap = size;
}

int aloB_bwrite(astate T, void* context, const void* src, size_t len) {
	ambuf_t* buf = aloE_cast(ambuf_t*, context); /* context is buffer */
	aloB_lock(buf);
	size_t nlen = buf->len + len;
	if (nlen > buf->cap) { /* should expand buffer size? */
		growmbuf(T, buf, aloM_adjsize(T, buf->cap, nlen, ALO_MBUF_LIMIT));
	}
	memcpy(buf->buf + buf->len, src, len); /* writer data to buffer */
	buf->len = nlen; /* increase length */
	aloB_unlock(buf);
	return 0; /* write into string buffer never causes an error */
}

void aloB_bcloseaux(astate T, ambuf_t* buf) {
	aloB_lock(buf);
	aloE_assert(!isinner(buf), "invalid buffer.");
	aloE_assert(T->memstk.top == buf, "illegal nested buffer.");
	T->memstk.top = buf->prev;
}

void aloB_bgrow_(astate T, ambuf_t* buf) {
	aloB_check(T, b);
	aloE_assert(buf->data.len == buf->data.cap, "buffer not full filled.");
	aloB_lock(buf);
	growmbuf(T, buf, buf->cap + 1);
	aloB_unlock(buf);
}

int alo_growbuf(astate T, ambuf_t* buf, size_t req) {
	aloB_check(T, buf);
	if (req > ALO_MBUF_LIMIT) {
		return false;
	}
	size_t ncap = buf->cap * 2;
	if (ncap > ALO_MBUF_LIMIT) {
		if (buf->cap >= ALO_MBUF_LIMIT) {
			return false;
		}
		ncap = ALO_MBUF_LIMIT;
	}
	else {
		if (ncap < ALO_MIN_BUFSIZE) {
			ncap = ALO_MIN_BUFSIZE;
		}
		if (ncap < req) {
			ncap = req;
		}
	}
	growmbuf(T, buf, ncap);
	return true;
}

void alo_delbuf(astate T, ambuf_t* buf) {
	aloB_close(T, *buf);
	buf->buf = NULL; /* force to close buffer */
}

void aloB_puts(astate T, ambuf_t* buf, const char* src) {
	aloB_check(T, buf);
	aloB_bwrite(T, buf, src, strlen(src) * sizeof(char));
}
