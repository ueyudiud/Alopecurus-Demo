/*
 * aiolib.c
 *
 * IO library.
 *
 *  Created on: 2019年8月31日
 *      Author: ueyudiud
 */

#define AIOLIB_C_
#define ALO_LIB

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>

/**
 ** OS dependent functions.
 **/

#if !defined(l_lockstream)
#if defined(ALO_USE_POSIX)

#include <sys/types.h>

#define l_lockstream flockfile
#define l_unlockstream funlockfile

#define l_seek fseeko
#define l_tell ftello
typedef off_t l_fpos_t;

#elif defined(ALOE_WINDOWS)

#define l_lockstream _lock_file
#define l_unlockstream _unlock_file

#define l_seek _fseeki64
#define l_tell _ftelli64
typedef __int64 l_fpos_t;

#else

#define l_lockstream aloE_void
#define l_unlockstream aloE_void

#define l_seek fseek
#define l_tell ftell
typedef int l_fpos_t;

#endif

#endif

#if !defined(l_getc)
#if defined(ALO_USE_POSIX)

#define l_getc getc_unlocked

#elif defined(ALOE_WINDOWS)

#define l_getc _fgetc_nolock

#else

#define l_getc getc

#endif
#endif

/**
 ** input stream type.
 */
typedef FILE *fstream;

typedef struct {
	fstream stream; /* file stream */
	int (*closer)(fstream); /* file closer */
} afile;

static const acreg_t cls_funcs[];

static afile* l_preopen(alo_State T) {
	afile* file = alo_newobject(T, afile);
	file->stream = NULL;
	file->closer = NULL;
	if (aloL_getsimpleclass(T, "__file")) {
		aloL_setfuns(T, -1, cls_funcs);
	}
	alo_setmetatable(T, -2);
	return file;
}

static void l_checkopen(alo_State T, afile* file) {
	if (file->closer == NULL) {
		aloL_error(T, "file already closed.");
	}
}

#define self(T) alo_toobject(T, 0, afile*)

static int f_getc(alo_State T) {
	afile* file = self(T);
	l_checkopen(T, file);
	l_lockstream(file->stream);
	int ch = l_getc(file->stream);
	l_unlockstream(file->stream);
	alo_pushinteger(T, ch != EOF ? ch : -1);
	return 1;
}

#if !defined(SHTBUFSIZE)
#define SHTBUFSIZE 256
#endif

static void l_getline(alo_State T, afile* file, ambuf_t* buf) {
	int ch;
	while (true) {
		while (buf->len < buf->cap) {
			ch = l_getc(file->stream);
			if (ch == EOF || ch == '\n') /* end of line or file */
				return;
			aloL_bputcx(buf, ch);
		}
		alo_bufgrow(T, buf, buf->len + SHTBUFSIZE); /* grow buffer capacity */
	}
}

static int f_line(alo_State T) {
	afile* file = self(T);
	l_checkopen(T, file);
	if (!feof(file->stream)) {
		aloL_usebuf(T, buf,
			l_lockstream(file->stream);
			l_getline(T, file, buf);
			l_unlockstream(file->stream);
			aloL_bpushstring(T, buf);
		)
	}
	else {
		alo_pushnil(T); /* return nil read to end of file */
	}
	return 1;
}

static int f_get(alo_State T) {
	afile* file = self(T);
	l_checkopen(T, file);
	size_t l = aloL_checkinteger(T, 1);
	aloL_usebuf(T, buf,
		aloL_bcheck(T, buf, l);
		if ((buf->len = fread(aloL_braw(buf), sizeof(char), l, file->stream)) > 0 || !ferror((file->stream))) {
			aloL_bpushstring(T, buf);
		}
		else {
			alo_bufpop(T, buf); /* pop buffer before return */
			return aloL_errresult_(T, NULL, errno);
		}
	)
	return 1;
}

static int f_put(alo_State T) {
	afile* file = self(T);
	l_checkopen(T, file);
	int type = alo_typeid(T, 1);
	if (type != ALO_TSTR && type != ALO_TUSER) {
		aloL_argerror(T, 1, "type is not writable, got: %s", alo_typename(T, 1));
	}
	size_t l = alo_rawlen(T, 1);
	const void* p = alo_rawptr(T, 1);
	size_t a = fwrite(p, 1, l, file->stream);
	return aloL_errresult(T, l == a, NULL);
}

static int f_puts(alo_State T) {
	afile* file = self(T);
	l_checkopen(T, file);
	size_t l;
	const char* s = aloL_tostring(T, 1, &l);
	fwrite(s, sizeof(char), l, file->stream);
	aloL_fputln(file->stream);
	return 0;
}

static int f_concat(alo_State T) {
	afile* file = self(T);
	l_checkopen(T, file);
	size_t n = alo_gettop(T);
	size_t l;
	const char* s;
	for (size_t i = 1; i < n; ++i) {
		s = aloL_tostring(T, i, &l);
		fwrite(s, sizeof(char), l, file->stream);
	}
	return 0;
}

static int f_eof(alo_State T) {
	afile* file = self(T);
	alo_pushboolean(T, feof(file->stream));
	return 1;
}

static int f_err(alo_State T) {
	afile* file = self(T);
	alo_pushboolean(T, ferror(file->stream));
	return 1;
}

static int f_close(alo_State T) {
	afile* file = self(T);
	if (file->closer) {
		file->closer(file->stream);
		file->closer = NULL;
	}
	return 0;
}

static int f_flush(alo_State T) {
	afile* file = self(T);
	l_checkopen(T, file);
	return aloL_errresult(T, fflush(file->stream) == 0, NULL);
}

/**
 ** return true if stream is closed.
 ** prototype: file.isclosed(file)
 */
static int f_isclosed(alo_State T) {
	afile* file = self(T);
	alo_pushboolean(T, file->closer == NULL);
	return 1;
}

#define IOBUF_SIZE 32

/**
 ** set stream buffering mode.
 ** prototype: file.setbut(file, [mode, [size]])
 */
static int f_setbuf(alo_State T) {
	static const a_cstr mode[] = { "no", "line", "all", NULL };
	static const int masks[] = { _IONBF, _IOLBF, _IOFBF };
	afile* file = self(T);
	int id = aloL_checkenum(T, 1, NULL, mode);
	size_t size = aloL_getoptinteger(T, 2, IOBUF_SIZE);
	return aloL_errresult(T, setvbuf(file->stream, NULL, masks[id], size) == 0, NULL);
}

/**
 ** change the stream position, and return the position if success.
 ** prototype: file.seek(file, whence, [offset])
 */
static int f_seek(alo_State T) {
	afile* file = self(T);
	static const a_cstr modes[] = { "set", "cur", "end", NULL };
	static const int modeids[] = { SEEK_SET, SEEK_CUR, SEEK_END };
	int id = aloL_checkenum(T, 1, NULL, modes);
	a_int off = aloL_getoptinteger(T, 2, 0);
	l_fpos_t pos = aloE_cast(l_fpos_t, off);
	if (pos != off) {
		aloL_error(T, "seek position out of range.");
	}
	if (l_seek(file->stream, pos, modeids[id])) {
		return aloL_errresult_(T, NULL, errno);
	}
	alo_pushinteger(T, l_tell(file->stream));
	return 1;
}

static int aux_lines(alo_State T, __attribute__((unused)) int status, a_kctx context) {
	afile* file = aloE_cast(afile*, context);
	aloL_usebuf(T, buf,
		while (!feof(file->stream)) {
			l_lockstream(file->stream);
			l_getline(T, file, buf);
			l_unlockstream(file->stream);
			aloL_bpushstring(T, buf);
			aloL_bclean(buf); /* rewind buffer */
			alo_push(T, 1); /* push function */
			alo_push(T, -2); /* push string */
			alo_callk(T, 1, 0, aux_lines, aloE_addr(file));
		}
	)
	return 0;
}

/**
 ** transform file to lines input, and apply by function if present.
 */
static int f_lines(alo_State T) {
	afile* file = self(T);
	l_checkopen(T, file);
	if (alo_gettop(T) >= 2) {
		aloL_checkcall(T, 1); /* check function */
		return aux_lines(T, ALO_STOK, aloE_addr(file)); /* invoke by auxiliary function */
	}
	else {
		alo_newlist(T, 0); /* create new line list */
		int index = 0;
		aloL_usebuf(T, buf,
			l_lockstream(file->stream);
			while (!feof(file->stream)) {
				l_getline(T, file, buf);
				aloL_bpushstring(T, buf);
				alo_rawseti(T, 1, index++); /* add string to list */
				aloL_bclean(buf); /* rewind buffer */
			}
			l_unlockstream(file->stream);
		)
		alo_push(T, 1);
		return 1;
	}
}

/**
 ** open file with path.
 ** prototype: io.open(name, [mode])
 */
static int io_open(alo_State T) {
	a_cstr path = aloL_checkstring(T, 0);
	a_cstr mode = aloL_getoptstring(T, 1, "r");
	afile* file = l_preopen(T);
	if ((file->stream = fopen(path, mode))) {
		file->closer = fclose;
		return 1;
	}
	return 0;
}

/**
 ** open a temporary file.
 ** prototype: io.opentmp()
 */
static int io_opentmp(alo_State T) {
	afile* file = l_preopen(T);
	if ((file->stream = tmpfile())) {
		file->closer = fclose;
		return 1;
	}
	return 1;
}

static int l_emptyclose(__attribute__((unused)) fstream stream) {
	return 0;
}

/**
 ** open standard stream
 */
static int l_openstd(alo_State T, a_cstr name, FILE* stream, size_t len, const char* src) {
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

static int io_index(alo_State T) {
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

static int io_remove(alo_State T) {
	a_cstr fname = aloL_checkstring(T, 0);
	return aloL_errresult(T, remove(fname) == 0, alo_pushfstring(T, "can not remove file '%s'", fname));
}

static int io_rename(alo_State T) {
	a_cstr oldfname = aloL_checkstring(T, 0);
	a_cstr newfname = aloL_checkstring(T, 1);
	return aloL_errresult(T, rename(oldfname, newfname) == 0,
			alo_pushfstring(T, "can not rename file '%s' to '%s'", oldfname, newfname));
}

static const acreg_t cls_funcs[] = {
	{ "__acat", f_concat },
	{ "__del", f_close },
	{ "close", f_close },
	{ "eof", f_eof },
	{ "err", f_err },
	{ "flush", f_flush },
	{ "get", f_get },
	{ "getc", f_getc },
	{ "line", f_line },
	{ "isclosed", f_isclosed },
	{ "lines", f_lines },
	{ "put", f_put },
	{ "puts", f_puts },
	{ "seek", f_seek },
	{ "setbuf", f_setbuf },
	{ NULL, NULL }
};

static const acreg_t mod_metafuncs[] = {
	{ "__get", io_index },
	{ NULL, NULL }
};

static const acreg_t mod_funcs[] = {
	{ "open", io_open },
	{ "opentmp", io_opentmp },
	{ "remove", io_remove },
	{ "rename", io_rename },
	{ NULL, NULL }
};

int aloopen_io(alo_State T) {
	alo_newtable(T, 16);
	aloL_setfuns(T, -1, mod_funcs);
	alo_newtable(T, 16);
	aloL_setfuns(T, -1, mod_metafuncs);
	alo_setmetatable(T, -2);
	return 1;
}
