/*
 * aaux.c
 *
 *  Created on: 2019年7月26日
 *      Author: ueyudiud
 */

#define AAUX_C_
#define ALO_LIB

#include "aaux.h"
#include "alibs.h"
#include "actype.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static a_mem aux_alloc(__attribute__((unused)) void* context, a_mem oldblock, __attribute__((unused)) size_t oldsize, size_t newsize) {
	if (newsize > 0) {
		return realloc(oldblock, newsize);
	}
	else {
		free(oldblock);
		return NULL;
	}
}

static int aux_panic(alo_State T) {
	a_cstr msg = alo_tostring(T, -1);
	aloL_fputf(stderr, "a unprotected error caught: %s", msg ?: "<no error message>");
	return 0;
}

alo_State aloL_newstate(void) {
	alo_State T = alo_newstate(aux_alloc, NULL);
	if (T) {
		alo_setpanic(T, aux_panic);
	}
	return T;
}

void aloL_checkversion_(alo_State T, aver_t req, size_t signature) {
	const aver_t* ver = alo_version(T);
	if (signature != ALOL_SIGNATURE) {
		aloL_error(T, "invalid signature, core and library have different numeric format.");
	}
	else if (ver->major != req.major) {
		aloL_error(T, "incompatible AVM version: expected %d.%d+, got: %d.%d", req.major, req.minor, ver->major, ver->minor);
	}
	else if (ver != alo_version(NULL)) {
		aloL_error(T, "The VM %p not belongs to this API.", T);
	}
}

void aloL_pushscopedcfunction(alo_State T, a_cfun value) {
	alo_newtable(T, 0);
	alo_newtable(T, 1); /* meta table of scope */
	alo_push(T, ALO_REGISTRY_INDEX); /* push current environment */
	alo_rawsets(T, -2, "__get"); /* set lookup table */
	alo_setmetatable(T, -2);
	alo_pushcfunction(T, value, 1, true);
}

/**
 ** push a string list into the top of stack.
 ** the string list size is given by parameter size and
 ** the elements is given by variable parameters which is
 ** stored as C-style string.
 */
void aloL_newstringlist_(alo_State T, size_t size, ...) {
	va_list varg;
	alo_newlist(T, size);
	va_start(varg, size);
	for (size_t i = 0; i < size; ++i) {
		alo_pushstring(T, va_arg(varg, a_cstr));
		alo_rawseti(T, -2, i);
	}
	va_end(varg);
}

/**
 ** create a table which mode is provided by parameter prop.
 */
void aloL_newweaktable(alo_State T, a_cstr prop, size_t size) {
	alo_newtable(T, size);
	alo_newtable(T, 1);
	alo_pushstring(T, prop);
	alo_rawsets(T, -2, "__mode");
	alo_setmetatable(T, -2);
}

a_none aloL_argerror(alo_State T, ssize_t i, a_cstr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	a_cstr s = alo_pushvfstring(T, fmt, varg);
	va_end(varg);
	aloL_error(T, "illegal argument #%d: %s", (int) i, s);
}

a_none aloL_typeerror(alo_State T, ssize_t i, a_cstr t) {
	aloL_argerror(T, i, "type mismatched, expected: %s, got: %s", t, alo_typename(T, i));
}

a_none aloL_tagerror(alo_State T, ssize_t i, int t) {
	aloL_typeerror(T, i, alo_tpidname(T, t));
}

void aloL_ensure(alo_State T, size_t size) {
	if (!alo_ensure(T, size)) {
		aloL_error(T, "no enough stack.");
	}
}

void aloL_checkany(alo_State T, ssize_t index) {
	if (alo_isnone(T, index)) {
		aloL_typeerror(T, index, "any");
	}
}

void aloL_checktype(alo_State T, ssize_t index, int type) {
	if (alo_typeid(T, index) != type) {
		aloL_tagerror(T, index, type);
	}
}

int aloL_checkbool(alo_State T, ssize_t index) {
	if (!alo_isboolean(T, index)) {
		aloL_tagerror(T, index, ALO_TBOOL);
	}
	return alo_toboolean(T, index);
}

int aloL_getoptbool(alo_State T, ssize_t index, int def) {
	return alo_isnothing(T, index) ? def : aloL_checkbool(T, index);
}

a_int aloL_checkinteger(alo_State T, ssize_t index) {
	if (!alo_isinteger(T, index)) {
		aloL_tagerror(T, index, ALO_TINT);
	}
	return alo_tointeger(T, index);
}

a_int aloL_getoptinteger(alo_State T, ssize_t index, a_int def) {
	return alo_isnothing(T, index) ? def : aloL_checkinteger(T, index);
}

a_float aloL_checknumber(alo_State T, ssize_t index) {
	if (!alo_isnumber(T, index)) {
		aloL_typeerror(T, index, "number");
	}
	return alo_tonumber(T, index);
}

a_float aloL_getoptnumber(alo_State T, ssize_t index, a_float def) {
	return alo_isnothing(T, index) ? def : aloL_checknumber(T, index);
}

const char* aloL_checklstring(alo_State T, ssize_t index, size_t* psize) {
	if (!alo_isstring(T, index)) {
		aloL_tagerror(T, index, ALO_TSTR);
	}
	return alo_tolstring(T, index, psize);
}

const char* aloL_getoptlstring(alo_State T, ssize_t index, const char* def, size_t defsz, size_t* psize) {
	if (alo_isnothing(T, index)) {
		if (psize) {
			*psize = defsz;
		}
		return def;
	}
	return aloL_checklstring(T, index, psize);
}

int aloL_checkenum(alo_State T, ssize_t index, a_cstr def, const a_cstr list[]) {
	const char* src = def ? aloL_getoptstring(T, index, def) : aloL_checkstring(T, index);
	for (int i = 0; list[i]; i++) {
		if (strcmp(src, list[i]) == 0) {
			return i;
		}
	}
	aloL_argerror(T, index, "invalid enumeration '%s'", src);
	return -1;
}

void aloL_checkcall(alo_State T, ssize_t index) {
	int id = alo_typeid(T, index); /* get object id */
	if (id != ALO_TFUNC) { /* not a function */
		if (id != ALO_TUNDEF) {
			if (alo_getmeta(T, index, "__call", true) == ALO_TFUNC) { /* has meta method for apply */
				return;
			}
		}
		aloL_argerror(T, index, "not a functor");
	}
}

const char* aloL_tostring(alo_State T, ssize_t index, size_t* psize) {
	int type = alo_getmeta(T, index, "__tostr", true);
	if (type != ALO_TNIL) { /* find meta field? */
		switch (type) {
		case ALO_TFUNC: /* applied by function? */
			/* call function */
			alo_push(T, index);
			alo_call(T, 1, 1);
			if (alo_typeid(T, -1) != ALO_TSTR) {
				goto error;
			}
			break;
		case ALO_TSTR:
			break;
		default:
			error:
			aloL_error(T, "'__tostr' must apply a string value");
			break;
		}
	}
	else switch (alo_typeid(T, index)) {
	case ALO_TNIL:
		alo_pushstring(T, "nil");
		break;
	case ALO_TBOOL:
		alo_pushstring(T, alo_toboolean(T, index) ? "true" : "false");
		break;
	case ALO_TINT:
		alo_pushfstring(T, "%i", alo_tointeger(T, index));
		break;
	case ALO_TFLOAT:
		alo_pushfstring(T, "%f", alo_tonumber(T, index));
		break;
	case ALO_TSTR:
		alo_push(T, index);
		break;
	default:
		alo_pushfstring(T, "%s:%p", alo_typename(T, index), alo_rawptr(T, index));
		break;
	}
	return alo_tolstring(T, -1, psize);
}

/**
 ** push a string that replaced all t as sub sequence in s into m.
 */
a_cstr aloL_sreplace(alo_State T, a_cstr s, a_cstr t, a_cstr m) {
	a_cstr result = NULL;
	aloL_usebuf(T, buf,
		aloL_brepts(T, buf, s, t, m);
		result = aloL_bpushstring(T, buf);
	)
	return result;
}

/**
 ** put name-function entries into table, if function is NULL, put nil into table.
 */
void aloL_setfuns(alo_State T, ssize_t index, const acreg_t* regs) {
	index = alo_absindex(T, index); /* get absolute index */
	while (regs->name) {
		if (regs->handle) {
			alo_pushlightcfunction(T, regs->handle);
		}
		else {
			alo_pushnil(T);
		}
		alo_rawsets(T, index, regs->name);
		regs++;
	}
}

int aloL_getmetatable(alo_State T, a_cstr name) {
	aloL_getsubtable(T, ALO_REGISTRY_INDEX, "@");
	if (alo_rawgets(T, -1, name) != ALO_TTABLE) {
		alo_settop(T, -1);
		return false;
	}
	return true;
}

/**
 ** force to get sub table of table, create a new table value if entry is nil.
 ** return true if create a new table.
 */
int aloL_getsubtable(alo_State T, ssize_t index, a_cstr key) {
	index = alo_absindex(T, index);
	if (alo_gets(T, index, key) == ALO_TTABLE) {
		return false;
	}
	alo_settop(T, -1); /* remove illegal result */
	alo_newtable(T, 0);
	alo_pushstring(T, key);
	alo_push(T, -2);
	alo_set(T, index);
	return true;
}

/**
 ** get field of sub table, return type id, if it is present or 'none' if module
 ** are not exist or entry not exist.
 */
int aloL_getsubfield(alo_State T, ssize_t index, a_cstr mod, a_cstr key) {
	if (alo_gets(T, index, mod) != ALO_TTABLE) {
		alo_erase(T, -1);
		return ALO_TUNDEF;
	}
	else {
		int type = alo_gets(T, -1, key);
		if (type == ALO_TUNDEF) {
			alo_settop(T, -2);
		}
		else {
			alo_erase(T, -2);
		}
		return type;
	}
}

/**
 ** call meta field with itself, return true if meta field exist and called it.
 */
int aloL_callselfmeta(alo_State T, ssize_t index, a_cstr name) {
	index = alo_absindex(T, index);
	if (alo_getmeta(T, index, name, true)) { /* find meta field */
		alo_push(T, index);
		alo_call(T, 1, 1);
		return true;
	}
	return false;
}

/**
 ** check a specific module required, if it not present, open module by function,
 ** and place the module at the top of stack.
 */
void aloL_require(alo_State T, a_cstr name, a_cfun openf, int putreg) {
	aloL_getsubtable(T, ALO_REGISTRY_INDEX, ALO_LOADEDMODS);
	if (alo_rawgets(T, -1, name) == ALO_TUNDEF) { /* if (LOADEDMODS[name] == undefined) */
		alo_settop(T, -1);
		/* local module = openf(name) */
		alo_pushlightcfunction(T, openf);
		alo_pushstring(T, name);
		alo_call(T, 1, 1);
		/* LOADEDMODS[name] = module */
		alo_push(T, -1);
		alo_rawsets(T, -3, name);
	}
	alo_erase(T, -2);
	if (putreg) {
		alo_pushstring(T, name);
		alo_push(T, -2);
		alo_set(T, ALO_REGISTRY_INDEX);
	}
}

/* string with length */
struct LS {
	const char* src;
	size_t len;
};

int LSReader(__attribute__((unused)) alo_State T, void* context, const char** pdest, size_t* psize) {
	struct LS* l = (struct LS*) context;
	*psize = l->len;
	*pdest = l->src;
	l->src = NULL;
	l->len = 0;
	return 0;
}

struct FS {
	FILE* stream;
	char buf[256];
};

static int FSReader(__attribute__((unused)) alo_State T, void* context, const char** ps, size_t* pl) {
	struct FS* fs = aloE_cast(struct FS*, context);
	size_t len = fread(fs->buf, sizeof(char), sizeof(fs->buf) / sizeof(char), fs->stream);
	if (len > 0) {
		*ps = fs->buf;
		*pl = len;
	}
	else {
		*ps = NULL;
		*pl = 0;
	}
	return ferror(fs->stream);
}

static int FSWriter(__attribute__((unused)) alo_State T, void* context, const void* s, size_t l) {
	struct FS* fs = aloE_cast(struct FS*, context);
	fwrite(s, 1, l, fs->stream);
	return ferror(fs->stream);
}

/**
 ** compile script in buffer, the string will not be removed.
 */
int aloL_compileb(alo_State T, const char* buf, size_t len, a_cstr name, a_cstr src) {
	struct LS context = { buf, len };
	return alo_compile(T, name, src, LSReader, &context);
}

/**
 ** compile string as script in the stack, the string will not be removed.
 */
int aloL_compiles(alo_State T, ssize_t index, a_cstr name, a_cstr src) {
	struct LS context;
	context.src = alo_tolstring(T, index, &context.len);
	return alo_compile(T, name, src, LSReader, &context);
}

/**
 ** compile script in file, if file not found, return -1.
 */
int aloL_compilef(alo_State T, a_cstr name, a_cstr src) {
	struct FS context;
	context.stream = fopen(src, "r");
	if (context.stream == NULL) {
		return -1;
	}
	int result = alo_compile(T, name, src, FSReader, &context);
	fclose(context.stream);
	return result;
}

int aloL_loadf(alo_State T, a_cstr src) {
	struct FS context;
	context.stream = fopen(src, "rb");
	if (context.stream == NULL) {
		return -1;
	}
	int result = alo_load(T, src, FSReader, &context);
	fclose(context.stream);
	return result;
}

int aloL_savef(alo_State T, a_cstr dest, int debug) {
	struct FS context;
	context.stream = fopen(dest, "wb");
	if (context.stream == NULL) {
		return -1;
	}
	setvbuf(context.stream, context.buf, _IOFBF, sizeof(context.buf));
	int result = alo_save(T, FSWriter, &context, debug);
	fclose(context.stream);
	return result;
}

/**
 ** get frame with level
 */
int aloL_getframe(alo_State T, int level, a_cstr what, alo_DbgInfo* info) {
	if (level == 1) {
		alo_getinfo(T, ALO_INFCURR, what, info);
		return true;
	}
	else {
		alo_getinfo(T, ALO_INFCURR, "", info);
		for (int i = 1; i < level; ++i) {
			if (!alo_getinfo(T, ALO_INFPREV, "", info)) {
				return false;
			}
		}
		return alo_getinfo(T, ALO_INFPREV, what, info);
	}
}

/**
 ** append stack trace in the string in the top of stack.
 */
void aloL_where(alo_State T, int level) {
	aloL_checkstring(T, -1);
	alo_DbgInfo info;
	alo_getinfo(T, ALO_INFCURR, "nslc", &info); /* skip first frame */
	aloL_usebuf(T, buf,
		aloL_bwrite(T, buf, -1);
		do {
			if (level-- == 0) {
				aloL_bputxs(T, buf, "\n\t...");
				break;
			}
			aloL_bputxs(T, buf, "\n\tat ");
			aloL_bputs(T, buf, info.name);
			aloL_bputxs(T, buf, " (");
			aloL_bputs(T, buf, info.src);
			if (info.line > 0) {
				aloL_bputf(T, buf, ":%d", info.line);
			}
			aloL_bputc(T, buf, ')');
			if (info.istailc) {
				aloL_bputxs(T, buf, "\n\t(tail calls ...)");
			}
			else if (info.isfinc) /* finalize call is not relevant to previous call */
				break;
		}
		while (alo_getinfo(T, ALO_INFPREV, "nslc", &info));
		aloL_bpushstring(T, buf);
	)
}

/**
 ** provide error message and give the return count.
 */
int aloL_errresult_(alo_State T, a_cstr msg, int no) {
	alo_pushnil(T);
	if (msg)
		alo_pushfstring(T, "%s: %s", msg, strerror(no));
	else
		alo_pushstring(T, strerror(no));
	alo_pushinteger(T, no);
	return 3;
}

/**
 ** throw a runtime error.
 */
a_none aloL_error(alo_State T, a_cstr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	alo_pushvfstring(T, fmt, varg);
	alo_error(T);
	/* unreachable code */
	va_end(varg);
}

void aloL_checkclassname(alo_State T, ssize_t index) {
	size_t len;
	const char* src = aloL_checklstring(T, index, &len);
	for (size_t i = 0; i < len; ++i) {
		if (!aisident(src[i])) {
			aloL_error(T, "illegal class name.");
		}
	}
}

int aloL_getsimpleclass(alo_State T, a_cstr name) {
	alo_getreg(T, "@");
	if (alo_rawgets(T, -1, name) == ALO_TUNDEF) { /* no class find? */
		alo_settop(T, -1);
		alo_pushstring(T, name);
		alo_call(T, 1, 1); /* construct class */
		return true;
	}
	else {
		alo_erase(T, -2);
		return false;
	}
}

void aloL_newclass_(alo_State T, a_cstr name, ...) {
	ssize_t base = alo_gettop(T);
	alo_getreg(T, "@");
	alo_pushstring(T, name);
	va_list varg;
	va_start(varg, name);
	a_cstr parent;
	int c = 1;
	while ((parent = va_arg(varg, a_cstr))) {
		if (alo_gets(T, base, parent) != ALO_TTABLE) {
			aloL_error(T, "fail to create new class: class @%s not found", parent);
		}
		c++;
	}
	alo_call(T, c, 1);
}

void aloL_bputm(alo_State T, ambuf_t* buf, const void* src, size_t len) {
	aloL_bcheck(T, buf, len);
	memcpy(aloL_btop(buf), src, len);
	aloL_blen(buf) += len;
}

void aloL_bputs(alo_State T, ambuf_t* buf, a_cstr src) {
	aloL_bputls(T, buf, src, strlen(src));
}

void aloL_breptc(alo_State T, ambuf_t* buf, a_cstr src, int tar, int rep) {
	size_t len = strlen(src);
	aloL_bcheck(T, buf, len);
	char* d = aloL_braw(buf);
	a_cstr p = src;
	a_cstr q;
	while ((q = strchr(p, tar))) {
		size_t n = q - p;
		memcpy(d, p, n);
		d += n;
		*(d++) = aloE_byte(rep);
		p = q + 1;
	}
	memcpy(d, p, src + len - p);
	aloL_blen(buf) += len;
}

void aloL_brepts(alo_State T, ambuf_t* buf, a_cstr src, a_cstr tar, a_cstr rep) {
	size_t l1 = strlen(tar);
	size_t l2 = strlen(rep);
	a_cstr p = src;
	a_cstr q;
	while ((q = strstr(p, tar))) {
		aloL_bputls(T, buf, p, q - p);
		aloL_bputls(T, buf, rep, l2);
		p = q + l1;
	}
	aloL_bputs(T, buf, p);
}

void aloL_bputf(alo_State T, ambuf_t* buf, a_cstr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	aloL_bputvf(T, buf, fmt, varg);
	va_end(varg);
}

void aloL_bputvf(alo_State T, ambuf_t* buf, a_cstr fmt, va_list varg) {
	alo_vformat(T, aloL_bwriter, buf, fmt, varg);
}

void aloL_bwrite(alo_State T, ambuf_t* buf, ssize_t index) {
	size_t len;
	const char* src = alo_tolstring(T, index, &len);
	aloL_bputls(T, buf, src, len);
}

int aloL_bwriter(alo_State T, void* buf, const void* src, size_t len) {
	aloL_bputm(T, aloE_cast(ambuf_t*, buf), src, len);
	return 0;
}
