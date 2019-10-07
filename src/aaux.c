/*
 * aaux.c
 *
 *  Created on: 2019年7月26日
 *      Author: ueyudiud
 */

#include "aaux.h"
#include "alibs.h"
#include "achr.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

static amem aux_alloc(__attribute__((unused)) void* context, amem oldblock, size_t oldsize, size_t newsize) {
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
	for (int i = 0; i < size; ++i) {
		alo_pushstring(T, va_arg(varg, astr));
		alo_rawseti(T, -2, i);
	}
	va_end(varg);
}

void aloL_argerror(astate T, aindex_t i, astr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	astr s = alo_pushvfstring(T, fmt, varg);
	va_end(varg);
	aloL_error(T, 2, "illegal argument #%d: %s", (int) i, s);
}

void aloL_typeerror(astate T, aindex_t i, astr t) {
	aloL_argerror(T, i, "type mismatched, expected: %s, got: %s", t, alo_typename(T, i));
}

void aloL_tagerror(astate T, aindex_t i, int t) {
	aloL_typeerror(T, i, alo_tpidname(T, t));
}

void aloL_checkany(astate T, aindex_t index) {
	if (alo_isnone(T, index)) {
		aloL_typeerror(T, index, "any");
	}
}

void aloL_checktype(astate T, aindex_t index, int type) {
	if (alo_typeid(T, index) != type) {
		aloL_tagerror(T, index, type);
	}
}

int aloL_checkbool(astate T, aindex_t index) {
	if (!alo_isboolean(T, index)) {
		aloL_tagerror(T, index, ALO_TBOOL);
	}
	return alo_toboolean(T, index);
}

int aloL_getoptbool(astate T, aindex_t index, int def) {
	return alo_isnothing(T, index) ? def : aloL_checkbool(T, index);
}

aint aloL_checkinteger(astate T, aindex_t index) {
	if (!alo_isinteger(T, index)) {
		aloL_tagerror(T, index, ALO_TINT);
	}
	return alo_tointeger(T, index);
}

aint aloL_getoptinteger(astate T, aindex_t index, aint def) {
	return alo_isnothing(T, index) ? def : aloL_checkinteger(T, index);
}

afloat aloL_checknumber(astate T, aindex_t index) {
	if (!alo_isnumber(T, index)) {
		aloL_typeerror(T, index, "number");
	}
	return alo_tonumber(T, index);
}

afloat aloL_getoptnumber(astate T, aindex_t index, afloat def) {
	return alo_isnothing(T, index) ? def : aloL_checknumber(T, index);
}

const char* aloL_checklstring(astate T, aindex_t index, size_t* psize) {
	if (!alo_isstring(T, index)) {
		aloL_tagerror(T, index, ALO_TSTRING);
	}
	return alo_tolstring(T, index, psize);
}

const char* aloL_getoptlstring(astate T, aindex_t index, const char* def, size_t defsz, size_t* psize) {
	if (alo_isnothing(T, index)) {
		if (psize) {
			*psize = defsz;
		}
		return def;
	}
	return aloL_checklstring(T, index, psize);
}

int aloL_checkenum(astate T, aindex_t index, astr def, const astr list[]) {
	const char* src = def ? aloL_getoptstring(T, index, def) : aloL_checkstring(T, index);
	for (int i = 0; list[i]; i++) {
		if (strcmp(src, list[i]) == 0) {
			return i;
		}
	}
	aloL_argerror(T, index, "invalid enumeration '%s'", src);
	return -1;
}

void aloL_checkcall(astate T, aindex_t index) {
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

const char* aloL_tostring(astate T, aindex_t index, size_t* psize) {
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
			aloL_error(T, 2, "'__tostr' must apply a string value");
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
 ** put name-function entries into table, if function is NULL, put nil into table.
 */
void aloL_setfuns(astate T, aindex_t index, const acreg_t* regs) {
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
int aloL_getsubtable(astate T, aindex_t index, astr key) {
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
int aloL_getsubfield(astate T, aindex_t index, astr mod, astr key) {
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
int aloL_callselfmeta(astate T, aindex_t index, astr name) {
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

int LSReader(astate T, void* context, const char** pdest, size_t* psize) {
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

static int FSReader(astate T, void* context, const char** ps, size_t* pl) {
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

/**
 ** compile string as script in the stack, the string will not be removed.
 */
int aloL_compiles(astate T, aindex_t index, astr name, astr src) {
	struct LS context;
	context.src = alo_tolstring(T, index, &context.len);
	return alo_compile(T, name, src, LSReader, &context);
}

/**
 ** compile script in file, if file not found, return -1.
 */
int aloL_compilef(astate T, astr name, astr src) {
	struct FS context;
	context.stream = fopen(src, "rb");
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

static void printstacktrace(astate T, astr name, astr src, int line) {
	if (line > 0) {
		alo_pushfstring(T, "\nat %s (%s:%d)", name, src, line);
	}
	else {
		alo_pushfstring(T, "\nat %s (%s)", name, src);
	}
}

/**
 ** append stack trace in the string in the top of stack.
 */
void aloL_where(astate T, int level) {
	aframeinfo_t info;
	size_t n = 2;
	alo_getframe(T, "nsl", &info);
	printstacktrace(T, info.name, info.src, info.line);
	while (n <= level && alo_prevframe(T, "nsl", &info)) {
		printstacktrace(T, info.name, info.src, info.line);
		n += 1;
	}
	alo_rawcat(T, n);
}

/**
 ** throw a runtime error.
 */
void aloL_error(astate T, int level, astr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	alo_pushvfstring(T, fmt, varg);
	aloL_where(T, level);
	va_end(varg);
	alo_throw(T);
}

void aloL_checkclassname(astate T, aindex_t index) {
	size_t len;
	const char* src = aloL_checklstring(T, index, &len);
	if (strlen(src) != len)
		goto error;
	for (size_t i = 0; i < len; ++i) {
		if (!aisident(src[i])) {
			goto error;
		}
	}
	return;

	error:
	aloL_error(T, 2, "illegal class name.");
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
	aindex_t base = alo_gettop(T);
	alo_getreg(T, "@");
	alo_pushstring(T, name);
	va_list varg;
	va_start(varg, name);
	astr parent;
	int c = 1;
	while ((parent = va_arg(varg, astr))) {
		if (alo_gets(T, base, parent) != ALO_TTABLE) {
			aloL_error(T, 2, "fail to create new class: class @%s not found", parent);
		}
		c++;
	}
	alo_call(T, c, 1);
}

/**
 ** memory box
 */
typedef struct {
	amem m;
	size_t s;
} MB;

#define boxof(b) aloE_cast(MB*, b)

static void adjbox(astate T, MB* box, size_t sz) {
	void* context;
	aalloc alloc = alo_getalloc(T, &context);
	void* newblock = alloc(context, box->m, box->s, sz);
	if (newblock == NULL && sz > box->s) {
		aloL_error(T, 2, "no enough memory for buffer.");
	}
	box->m = newblock;
	box->s = sz;
}

static void delbox(astate T, MB* box) {
	void* context;
	aalloc alloc = alo_getalloc(T, &context);
	box->m = alloc(context, box->m, box->s, 0);
	box->s = 0;
}

static int boxgc(astate T) {
	MB* b = boxof(alo_torawdata(T, 0));
	delbox(T, b);
	return 0;
}

static MB* newbox(astate T, size_t sz) {
	MB* box = alo_newobj(T, MB);
	if (aloL_getsimpleclass(T, "__mbox")) {
		static const acreg_t funcs[] = {
				{ "__del", boxgc },
				{ NULL, NULL }
		};
		aloL_setfuns(T, -1, funcs);
	}
	alo_setmetatable(T, -2);
	void* context;
	aalloc alloc = alo_getalloc(T, &context);
	box->m = alloc(context, NULL, 0, sz);
	if (box->m == NULL) {
		aloL_error(T, 2, "no enough memory for buffer.");
	}
	box->s = sz;
	return box;
}

void aloL_binit_(astate T, ambuf_t* buf, amem initblock, size_t initcap) {
	buf->T = T;
	buf->l = 0;
	buf->c = initcap;
	buf->p = initblock;
	buf->b = NULL;
}

void aloL_bcheck(ambuf_t* buf, size_t sz) {
	size_t req = buf->l + sz;
	if (req > buf->c) {
		size_t nc = buf->c * 2;
		if (nc < ALOL_MBUFSIZE) {
			nc = ALOL_MBUFSIZE;
		}
		if (nc < req) {
			nc = req;
		}
		MB* b;
		if (buf->b) { /* box in heap? */
			adjbox(buf->T, b = boxof(buf->b), nc);
		}
		else {
			amem old = buf->p;
			b = newbox(buf->T, nc);
			buf->b = b;
			memcpy(b->m, old, buf->l);
		}
		buf->p = b->m;
		buf->c = nc;
	}
}

void aloL_bputm(ambuf_t* buf, const void* src, size_t len) {
	aloL_bcheck(buf, len);
	memcpy(buf->p + buf->l, src, len);
	buf->l += len;
}

void aloL_bputls(ambuf_t* buf, const char* src, size_t len) {
	aloL_bputm(buf, src, len * sizeof(char));
}

void aloL_bputs(ambuf_t* buf, astr src) {
	aloL_bputls(buf, src, strlen(src));
}

void aloL_bputf(ambuf_t* buf, astr fmt, ...) {
	va_list varg;
	va_start(varg, fmt);
	aloL_bputvf(buf, fmt, varg);
	va_end(varg);
}

void aloL_bputvf(ambuf_t* buf, astr fmt, va_list varg) {
	alo_vformat(buf->T, aloL_bwriter, buf, fmt, varg);
}

void aloL_bwrite(ambuf_t* buf, aindex_t index) {
	size_t len;
	const char* src = alo_tolstring(buf->T, index, &len);
	aloL_bputls(buf, src, len);
}

void aloL_bpushstring(ambuf_t* buf) {
	alo_pushlstring(buf->T, aloE_cast(char*, buf->p), buf->l / sizeof(char));
}

int aloL_bwriter(astate T, void* rbuf, const void* src, size_t len) {
	ambuf_t* buf = aloE_cast(ambuf_t*, rbuf);
	aloL_bputm(buf, src, len);
	return 0;
}
