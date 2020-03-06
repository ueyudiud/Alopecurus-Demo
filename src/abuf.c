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
#define isinner(b) ((b)->instk == (b)->buf)

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
	ambuf_t* buf = aloE_cast(ambuf_t*, context); /* context is buffer */
	aloB_lock(buf);
	size_t nlen = buf->len + len;
	if (nlen > buf->cap) { /* should expand buffer size? */
		size_t ncap = aloM_adjsize(T, buf->cap, nlen, ALO_MBUF_LIMIT);
		if (isinner(buf)) {
			abyte* nbuf = aloM_newa(T, abyte, ncap);
			memcpy(nbuf, buf->instk, buf->len);
			buf->buf = nbuf;
			buf->prev = T->memstk;
			T->memstk = buf;
		}
		else {
			buf->buf = aloM_adja(T, buf->buf, buf->cap, ncap);
		}
		buf->cap = ncap;
	}
	memcpy(buf->buf + buf->len, src, len); /* writer data to buffer */
	buf->len = nlen; /* increase length */
	aloB_unlock(buf);
	return 0; /* write into string buffer never causes an error */
}

void aloB_bcloseaux(astate T, ambuf_t* buf) {
	aloB_lock(buf);
	aloE_assert(!isinner(buf), "invalid buffer.");
	aloE_assert(T->memstk == buf, "illegal nested buffer.");
	T->memstk = buf->prev;
	aloM_delb(T, buf->buf, buf->cap); /* delete data and close buffer. */
}

void aloB_bgrow_(astate T, ambuf_t* buf) {
	aloE_assert(buf->len == buf->cap, "buffer not full filled.");
	aloB_lock(buf);
	size_t ncap = aloM_growsize(T, buf->cap, ALO_MBUF_LIMIT);
	if (isinner(buf)) {
		abyte* nbuf = aloM_newa(T, abyte, ncap);
		memcpy(nbuf, buf->instk, buf->len);
		buf->buf = nbuf;
		buf->prev = T->memstk;
		T->memstk = buf;
	}
	else {
		buf->buf = aloM_adja(T, buf->buf, buf->cap, ncap);
	}
	buf->cap = ncap;
	aloB_unlock(buf);
}

int alo_growbuf(astate T, ambuf_t* buf, size_t req) {
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
	if (isinner(buf)) {
		abyte* nbuf = aloM_newa(T, abyte, ncap);
		memcpy(nbuf, buf->instk, buf->len);
		buf->buf = nbuf;
		buf->prev = T->memstk;
		T->memstk = buf;
	}
	else {
		buf->buf = aloM_adja(T, buf->buf, buf->cap, ncap);
	}
	buf->cap = ncap;
	return true;
}

void alo_delbuf(astate T, ambuf_t* buf) {
	aloB_close(T, *buf);
	buf->buf = NULL; /* force to close buffer */
}

void aloB_puts(astate T, ambuf_t* buf, const char* src) {
	aloB_bwrite(T, buf, src, strlen(src) * sizeof(char));
}
