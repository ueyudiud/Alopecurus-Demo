/*
 * astrlib.c
 *
 * library for string.
 *
 *  Created on: 2019年7月31日
 *      Author: ueyudiud
 */

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#include <string.h>

#define MAX_HEAPBUF_LEN 256

/**
 ** reverse string.
 ** prototype: stdstr.reverse(string)
 */
int str_reverse(astate T) {
	size_t len;
	const char* src = aloL_checklstring(T, 0, &len);
	if (len < MAX_HEAPBUF_LEN) { /* for short string, use heap memory */
		char buf[len];
		for (size_t i = 0; i < len; ++i) {
			buf[i] = src[len - 1 - i];
		}
		alo_pushlstring(T, buf, len);
	}
	else { /* or use memory buffer instead */
		ambuf_t buf;
		aloL_bempty(T, &buf);
		aloL_bcheck(&buf, len);
		for (size_t i = 0; i < len; ++i) {
			aloL_bsetc(&buf, i, src[len - 1 - i]);
		}
		aloL_bsetlen(&buf, len);
		aloL_bpushstring(&buf);
	}
	return 1;
}

static const acreg_t mod_funcs[] = {
	{ "reverse", str_reverse },
	{ NULL, NULL }
};

int aloopen_strlib(astate T) {
	alo_bind(T, "string.reverse", str_reverse);
	alo_getreg(T, "__basic_delegates");
	alo_rawgeti(T, -1, ALO_TSTRING);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
