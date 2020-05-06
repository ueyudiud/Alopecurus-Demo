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
#include "achr.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static amem aux_alloc(__attribute__((unused)) void* context, amem oldblock, __attribute__((unused)) size_t oldsize, size_t newsize) {
	if (newsize > 0) {
		return realloc(oldblock, newsize);
	}
	else {
		free(oldblock);
		return NULL;
	}
}

astate aloL_newstate(void) {
	return alo_newstate(aux_alloc, NULL);
}

void aloL_pushscopedcfunction(astate T, acfun value) {
	alo_pushcclosure(T, value, 0);
	alo_newtable(T, 0);
	alo_newtable(T, 1); /* meta table of scope */
	alo_push(T, ALO_REGISTRY_INDEX); /* push current environment */
	alo_rawsets(T, -2, "__idx"); /* set lookup table */
	alo_setmetatable(T, -2);
	alo_setdelegate(T, -2);
}

void aloL_newstringlist_(astate T, size_t size, ...) {
	alo_newlist(T, size);
	va_list varg;
	va_start(varg, size);
	for (size_t i = 0; i < size; ++i) {
		alo_pushstring(T, va_arg(varg, astr));
		alo_rawseti(T, -2, i);
	}
	va_end(varg);
}

anoret aloL_argerror(astate T, ssize_t i, astr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	astr s = alo_pushvfstring(T, fmt, varg);
	va_end(varg);
	aloL_error(T, "illegal argument #%d: %s", (int) i, s);
}

anoret aloL_typeerror(astate T, ssize_t i, astr t) {
	aloL_argerror(T, i, "type mismatched, expected: %s, got: %s", t, alo_typename(T, i));
}

anoret aloL_tagerror(astate T, ssize_t i, int t) {
	aloL_typeerror(T, i, alo_tpidname(T, t));
}

void aloL_ensure(astate T, size_t size) {
	if (!alo_ensure(T, size)) {
		aloL_error(T, "no enough stack.");
	}
}

void aloL_checkany(astate T, ssize_t index) {
	if (alo_isnone(T, index)) {
		aloL_typeerror(T, index, "any");
	}
}

void aloL_checktype(astate T, ssize_t index, int type) {
	if (alo_typeid(T, index) != type) {
		aloL_tagerror(T, index, type);
	}
}

int aloL_checkbool(astate T, ssize_t index) {
	if (!alo_isboolean(T, index)) {
		aloL_tagerror(T, index, ALO_TBOOL);
	}
	return alo_toboolean(T, index);
}

int aloL_getoptbool(astate T, ssize_t index, int def) {
	return alo_isnothing(T, index) ? def : aloL_checkbool(T, index);
}

aint aloL_checkinteger(astate T, ssize_t index) {
	if (!alo_isinteger(T, index)) {
		aloL_tagerror(T, index, ALO_TINT);
	}
	return alo_tointeger(T, index);
}

aint aloL_getoptinteger(astate T, ssize_t index, aint def) {
	return alo_isnothing(T, index) ? def : aloL_checkinteger(T, index);
}

afloat aloL_checknumber(astate T, ssize_t index) {
	if (!alo_isnumber(T, index)) {
		aloL_typeerror(T, index, "number");
	}
	return alo_tonumber(T, index);
}

afloat aloL_getoptnumber(astate T, ssize_t index, afloat def) {
	return alo_isnothing(T, index) ? def : aloL_checknumber(T, index);
}

const char* aloL_checklstring(astate T, ssize_t index, size_t* psize) {
	if (!alo_isstring(T, index)) {
		aloL_tagerror(T, index, ALO_TSTRING);
	}
	return alo_tolstring(T, index, psize);
}

const char* aloL_getoptlstring(astate T, ssize_t index, const char* def, size_t defsz, size_t* psize) {
	if (alo_isnothing(T, index)) {
		if (psize) {
			*psize = defsz;
		}
		return def;
	}
	return aloL_checklstring(T, index, psize);
}

int aloL_checkenum(astate T, ssize_t index, astr def, const astr list[]) {
	const char* src = def ? aloL_getoptstring(T, index, def) : aloL_checkstring(T, index);
	for (int i = 0; list[i]; i++) {
		if (strcmp(src, list[i]) == 0) {
			return i;
		}
	}
	aloL_argerror(T, index, "invalid enumeration '%s'", src);
	return -1;
}

void aloL_checkcall(astate T, ssize_t index) {
	int id = alo_typeid(T, index); /* get object id */
	if (id != ALO_TFUNCTION) { /* not a function */
		if (id != ALO_TUNDEF) {
			if (alo_getmeta(T, index, "__call", true) == ALO_TFUNCTION) { /* has meta method for apply */
				return;
			}
		}
		aloL_argerror(T, index, "not a functor");
	}
}

const char* aloL_tostring(astate T, ssize_t index, size_t* psize) {
	int type = alo_getmeta(T, index, "__tostr", true);
	if (type != ALO_TNIL) { /* find meta field? */
		switch (type) {
		case ALO_TFUNCTION: /* applied by function? */
			/* call function */
			alo_push(T, index);
			alo_call(T, 1, 1);
			if (alo_typeid(T, -1) != ALO_TSTRING) {
				goto error;
			}
			break;
		case ALO_TSTRING:
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
	case ALO_TSTRING:
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
astr aloL_sreplace(astate T, astr s, astr t, astr m) {
	astr result = NULL;
	aloL_usebuf(T, buf) {
		aloL_brepts(T, buf, s, t, m);
		result = aloL_bpushstring(T, buf);
	}
	return result;
}

/**
 ** put name-function entries into table, if function is NULL, put nil into table.
 */
void aloL_setfuns(astate T, ssize_t index, const acreg_t* regs) {
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

int aloL_getmetatable(astate T, astr name) {
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
int aloL_getsubtable(astate T, ssize_t index, astr key) {
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
int aloL_getsubfield(astate T, ssize_t index, astr mod, astr key) {
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
int aloL_callselfmeta(astate T, ssize_t index, astr name) {
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
void aloL_require(astate T, astr name, acfun openf, int putreg) {
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

int LSReader(__attribute__((unused)) astate T, void* context, const char** pdest, size_t* psize) {
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

static int FSReader(__attribute__((unused)) astate T, void* context, const char** ps, size_t* pl) {
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

static int FSWriter(__attribute__((unused)) astate T, void* context, const void* s, size_t l) {
	struct FS* fs = aloE_cast(struct FS*, context);
	fwrite(s, 1, l, fs->stream);
	return ferror(fs->stream);
}

/**
 ** compile string as script in the stack, the string will not be removed.
 */
int aloL_compiles(astate T, ssize_t index, astr name, astr src) {
	struct LS context;
	context.src = alo_tolstring(T, index, &context.len);
	return alo_compile(T, name, src, LSReader, &context);
}

/**
 ** compile script in file, if file not found, return -1.
 */
int aloL_compilef(astate T, astr name, astr src) {
	struct FS context;
	context.stream = fopen(src, "r");
	if (context.stream == NULL) {
		return -1;
	}
	int result = alo_compile(T, name, src, FSReader, &context);
	fclose(context.stream);
	return result;
}

int aloL_loadf(astate T, astr src) {
	struct FS context;
	context.stream = fopen(src, "rb");
	if (context.stream == NULL) {
		return -1;
	}
	int result = alo_load(T, src, FSReader, &context);
	fclose(context.stream);
	return result;
}

int aloL_savef(astate T, astr dest, int debug) {
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
int aloL_getframe(astate T, int level, astr what, aframeinfo_t* info) {
	if (level == 1) {
		alo_getframe(T, what, info);
		return true;
	}
	else {
		alo_getframe(T, "", info);
		for (int i = 1; i < level; ++i) {
			if (!alo_prevframe(T, "", info)) {
				return false;
			}
		}
		return alo_prevframe(T, what, info);
	}
}

/**
 ** append stack trace in the string in the top of stack.
 */
void aloL_where(astate T, int level) {
	aloL_checkstring(T, -1);
	aframeinfo_t info;
	int n = level;
	alo_getframe(T, "", &info); /* skip first frame */
	aloL_usebuf(T, buf) {
		aloL_bwrite(T, buf, -1);
		while (alo_prevframe(T, "nsl", &info)) {
			if (n-- == 0) {
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
		}
		aloL_bpushstring(T, buf);
	}
}

/**
 ** provide error message and give the return count.
 */
int aloL_errresult_(astate T, astr msg) {
	int no = errno;
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
anoret aloL_error(astate T, astr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	alo_pushvfstring(T, fmt, varg);
	alo_error(T);
	/* unreachable code */
	va_end(varg);
}

void aloL_checkclassname(astate T, ssize_t index) {
	size_t len;
	const char* src = aloL_checklstring(T, index, &len);
	if (strlen(src) != len)
		goto error;
	for (size_t i = 0; i < len; ++i) {
		if (!isalnum(src[i]) && src[i] != '_') {
			goto error;
		}
	}
	return;

	error:
	aloL_error(T, "illegal class name.");
}

int aloL_getsimpleclass(astate T, astr name) {
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

void aloL_newclass_(astate T, astr name, ...) {
	ssize_t base = alo_gettop(T);
	alo_getreg(T, "@");
	alo_pushstring(T, name);
	va_list varg;
	va_start(varg, name);
	astr parent;
	int c = 1;
	while ((parent = va_arg(varg, astr))) {
		if (alo_gets(T, base, parent) != ALO_TTABLE) {
			aloL_error(T, "fail to create new class: class @%s not found", parent);
		}
		c++;
	}
	alo_call(T, c, 1);
}

void aloL_bputm(astate T, ambuf_t* buf, const void* src, size_t len) {
	aloL_bcheck(T, buf, len);
	memcpy(aloL_btop(buf), src, len);
	aloL_blen(buf) += len;
}

void aloL_bputs(astate T, ambuf_t* buf, astr src) {
	aloL_bputls(T, buf, src, strlen(src));
}

void aloL_breptc(astate T, ambuf_t* buf, astr src, int tar, int rep) {
	size_t len = strlen(src);
	aloL_bcheck(T, buf, len);
	char* d = aloL_braw(buf);
	astr p = src;
	astr q;
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

void aloL_brepts(astate T, ambuf_t* buf, astr src, astr tar, astr rep) {
	size_t l1 = strlen(tar);
	size_t l2 = strlen(rep);
	astr p = src;
	astr q;
	while ((q = strstr(p, tar))) {
		aloL_bputls(T, buf, p, q - p);
		aloL_bputls(T, buf, rep, l2);
		p = q + l1;
	}
	aloL_bputs(T, buf, p);
}

void aloL_bputf(astate T, ambuf_t* buf, astr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	aloL_bputvf(T, buf, fmt, varg);
	va_end(varg);
}

void aloL_bputvf(astate T, ambuf_t* buf, astr fmt, va_list varg) {
	alo_vformat(T, aloL_bwriter, buf, fmt, varg);
}

void aloL_bwrite(astate T, ambuf_t* buf, ssize_t index) {
	size_t len;
	const char* src = alo_tolstring(T, index, &len);
	aloL_bputls(T, buf, src, len);
}

astr aloL_bpushstring(astate T, ambuf_t* buf) {
	return alo_pushlstring(T, aloE_cast(char*, aloL_braw(buf)), aloL_blen(buf) / sizeof(char));
}

int aloL_bwriter(astate T, void* buf, const void* src, size_t len) {
	aloL_bputm(T, aloE_cast(ambuf_t*, buf), src, len);
	return 0;
}
