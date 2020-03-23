/*
 * aiolib.c
 *
 * IO library.
 *
 *  Created on: 2019年8月31日
 *      Author: ueyudiud
 */

#include "alo.h"
#include "aaux.h"
#include "alibs.h"
#include "abuf.h"

#include <stdio.h>
#include <string.h>

#define l_lockstream(f) aloE_void(0)
#define l_unlockstream(f) aloE_void(0)

/**
 ** input stream type.
 */
typedef FILE *fstream;

typedef struct {
	fstream stream; /* file stream */
	int (*closer)(fstream); /* file closer */
} afile;

static const acreg_t cls_funcs[];

static afile* l_preopen(astate T) {
	afile* file = alo_newobject(T, afile);
	file->stream = NULL;
	file->closer = NULL;
	if (aloL_getsimpleclass(T, "__file")) {
		aloL_setfuns(T, -1, cls_funcs);
	}
	alo_setmetatable(T, -2);
	return file;
}

static void l_checkopen(astate T, afile* file) {
	if (file->closer == NULL) {
		aloL_error(T, 2, "file already closed.");
	}
}

#define self(T) alo_toobject(T, 0, afile*)

static int f_getc(astate T) {
	afile* file = self(T);
	l_checkopen(T, file);
	l_lockstream(file);
	int ch = fgetc(file->stream);
	l_unlockstream(file);
	alo_pushinteger(T, ch != EOF ? ch : -1);
	return 1;
}

#define SHTBUFSIZE 256

static void l_getline(astate T, afile* file, ambuf_t* buf) {
	int ch;
	do {
		l_lockstream(file);
		while (buf->len < buf->cap) {
			if ((ch = fgetc(file->stream)) != EOF && ch != '\n') {
				aloL_bsetc(buf, buf->len++, ch);
			}
			else {
				l_unlockstream(file);
				aloL_bpushstring(T, buf);
				return;
			}
		}
		l_unlockstream(file);
		aloL_bcheck(T, buf, SHTBUFSIZE);
	}
	while (true);
}

static int f_line(astate T) {
	afile* file = self(T);
	l_checkopen(T, file);
	aloL_usebuf(T, buf) {
		l_getline(T, file, buf);
	}
	return 1;
}

static int f_put(astate T) {
	afile* file = self(T);
	l_checkopen(T, file);
	size_t l;
	astr s = aloL_tostring(T, 1, &l);
	l_lockstream(file);
	fwrite(s, sizeof(char), l, file->stream);
	l_unlockstream(file);
	return 0;
}

static int f_puts(astate T) {
	afile* file = self(T);
	l_checkopen(T, file);
	size_t l;
	astr s = aloL_checklstring(T, 1, &l);
	l_lockstream(file);
	fwrite(s, sizeof(char), l, file->stream);
	l_unlockstream(file);
	return 0;
}

static int f_eof(astate T) {
	afile* file = self(T);
	alo_pushboolean(T, feof(file->stream));
	return 1;
}

static int f_err(astate T) {
	afile* file = self(T);
	alo_pushboolean(T, ferror(file->stream));
	return 1;
}

static int f_close(astate T) {
	afile* file = self(T);
	if (file->closer) {
		l_lockstream(file);
		file->closer(file->stream);
		file->closer = NULL;
		l_unlockstream(file);
	}
	return 0;
}

/**
 ** return true if stream is closed.
 ** prototype: file.isclosed(file)
 */
static int f_isclosed(astate T) {
	afile* file = self(T);
	alo_pushboolean(T, file->closer == NULL);
	return 1;
}

#define IOBUF_SIZE 32

/**
 ** set stream buffering mode.
 ** prototype: file.setbut(file, [mode, [size]])
 */
static int f_setbuf(astate T) {
	static const astr mode[] = { "no", "line", "all", NULL };
	static const int masks[] = { _IONBF, _IOLBF, _IOFBF };
	afile* file = self(T);
	int id = aloL_checkenum(T, 1, NULL, mode);
	size_t size = aloL_getoptinteger(T, 2, IOBUF_SIZE);
	l_lockstream(file);
	setvbuf(file->stream, NULL, masks[id], size);
	l_unlockstream(file);
	alo_push(T, 0);
	return 1;
}

/**
 ** transform file to lines input, and apply by function if present.
 */
static int f_lines(astate T) {
	afile* file = self(T);
	l_checkopen(T, file);
	int n = alo_gettop(T) < 2;
	aloL_usebuf(T, buf) {
		if (!n) {
			aloL_checkcall(T, 1); /* check function */
			while (!feof(file->stream)) {
				l_getline(T, file, buf);
				alo_push(T, 1); /* push function */
				alo_push(T, -2); /* push string */
				alo_call(T, 1, 0);
				aloL_blen(buf) = 0; /* rewind buffer */
			}
		}
		else {
			alo_newlist(T, 0); /* create new line list */
			int index = 0;
			while (!feof(file->stream)) {
				l_getline(T, file, buf);
				alo_rawseti(T, 1, index++); /* add string to list */
				aloL_blen(buf) = 0; /* rewind buffer */
			}
			alo_push(T, 1);
		}
	}
	return n;
}

/**
 ** open file with path.
 ** prototype: io.open(name, [mode])
 */
static int io_open(astate T) {
	astr path = aloL_checkstring(T, 0);
	astr mode = aloL_getoptstring(T, 1, "r");
	afile* file = l_preopen(T);
	if ((file->stream = fopen(path, mode))) {
		file->closer = fclose;
		return 1;
	}
	return 0;
}

static int l_emptyclose(fstream stream) {
	return 0;
}

/**
 ** open standard stream
 */
static int l_openstd(astate T, astr name, FILE* stream, size_t len, const char* src) {
	if (len == strlen(name) && strcmp(src, name) == 0) {
		afile* file = l_preopen(T);
		file->stream = stream;
		file->closer = l_emptyclose;
		alo_push(T, -1);
		alo_rawsets(T, 0, name);
		return true;
	}
	return false;
}

static int io_index(astate T) {
	alo_push(T, 1);
	if (alo_rawget(T, 0) != ALO_TUNDEF) {
		return 1;
	}
	alo_drop(T);
	if (alo_isstring(T, 1)) {
		size_t len;
		const char* src = alo_tolstring(T, 1, &len);
		if (l_openstd(T, "stdout", stdout, len, src) ||
			l_openstd(T, "stdin", stdin, len, src) ||
			l_openstd(T, "stderr", stderr, len, src)) {
			return 1;
		}
	}
	return 0;
}

static const acreg_t cls_funcs[] = {
	{ "__del", f_close },
	{ "close", f_close },
	{ "eof", f_eof },
	{ "err", f_err },
	{ "getc", f_getc },
	{ "line", f_line },
	{ "isclosed", f_isclosed },
	{ "lines", f_lines },
	{ "put", f_put },
	{ "puts", f_puts },
	{ "setbuf", f_setbuf },
	{ NULL, NULL }
};

static const acreg_t mod_metafuncs[] = {
	{ "__idx", io_index },
	{ NULL, NULL }
};

static const acreg_t mod_funcs[] = {
	{ "open", io_open },
	{ NULL, NULL }
};

int aloopen_iolib(astate T) {
	alo_bind(T, "io.open", io_open);
	alo_bind(T, "io.meta.__idx", io_index);
	alo_bind(T, "file.puts", f_puts);
	alo_newtable(T, 16);
	aloL_setfuns(T, -1, mod_funcs);
	alo_newtable(T, 16);
	aloL_setfuns(T, -1, mod_metafuncs);
	alo_setmetatable(T, -2);
	return 1;
}
