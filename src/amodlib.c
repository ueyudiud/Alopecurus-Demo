/*
 * amodlib.c
 *
 *  Created on: 2019年10月5日
 *      Author: ueyudiud
 */

#define AMODLIB_C_
#define ALO_LIB

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MODINFO ".aloi"

/* package key, used to mark the current package of module */
#define PACKAGE_KEY "__package"

/**
 ** module library key definition.
 */

#define ALO_BUILTIN_KEY "builtin"
#define ALO_LPATH_KEY "__lpath"
#define ALO_CPATH_KEY "__cpath"
#define ALO_MODLOADER_KEY "__loaders"
#define ALO_LOADED_KEY "__loaded"

#if !defined(MAX_PATH_LENGTH)
#define MAX_PATH_LENGTH 1024
#endif

#define l_strchr(s,c,l) aloE_cast(const char*, memchr(s, c, (l) * sizeof(char)))

static void getpath(astate T, char* buf, const char* src, size_t len) {
	for (size_t i = 0; i < len; ++i) {
		if (src[i] == '.') {
			if (i != 0) {
				*(buf++) = ALOE_DIRSEP;
			}
		}
		else if (!isalnum(aloE_byte(src[i]))) {
			aloL_error(T, "illegal module name: %q", src, len);
		}
		else {
			*(buf++) = src[i];
		}
	}
}

#define aloE_modf(e) aloE_cast(acfun, __extension__ aloE_cast(void*, e))

#if defined(ALO_USE_DLOPEN)

#include <dlfcn.h>

static void* l_load(astate T, astr name, int seeglb) {
	void* lib = dlopen(name, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
	if (lib == NULL) {
		alo_pushstring(T, dlerror());
	}
	return lib;
}

static void l_unload(void* lib) {
	dlclose(lib);
}

static acfun l_get(astate T, void* lib, astr name) {
	acfun fun = aloE_modf(dlsym(lib, name));
	if (fun == NULL) {
		alo_pushstring(T, dlerror());
	}
	return fun;
}

#elif defined(ALO_USE_DLL)

#include <windows.h>

#define aloE_mod(e) aloE_cast(HMODULE, e)

#ifndef ALO_LLE_FLAGS
#define ALO_LLE_FLAGS 0 /* flag for LoadLibraryExA */
#endif

static void geterr(astate T) {
	DWORD error = GetLastError();
	char buffer[128];
	if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL)) {
	    alo_pushstring(T, buffer);
	}
	else {
		alo_pushfstring(T, "system error %d\n", error);
	}
}

static void* l_load(astate T, astr name, __attribute__((unused)) int seeglb) {
	HMODULE library = LoadLibraryExA(name, NULL, ALO_LLE_FLAGS);
	if (library == NULL) {
		geterr(T);
	}
	return library;
}

static void l_unload(void* lib) {
	FreeLibrary(aloE_mod(lib));
}

static acfun l_get(astate T, void* lib, astr name) {
	acfun fun = aloE_modf(GetProcAddress(aloE_mod(lib), name));
	if (fun == NULL) {
		geterr(T);
	}
	return fun;
}

#else

/* mark that dynamic library is not supported in the environment */
#define DL_NOT_SUPPORT
#define DL_NS_MSG	"dynamic libraries is not supported."

static void* l_load(astate T, __attribute__((unused)) astr name, __attribute__((unused)) int seeglb) {
	alo_pushcstring(T, DL_NS_MSG);
	return NULL;
}

static void l_unload(__attribute__((unused)) void* lib) {

}

static acfun l_get(astate T, __attribute__((unused)) void* lib, __attribute__((unused)) astr name) {
	alo_pushcstring(T, DL_NS_MSG);
	return NULL;
}

#endif

typedef struct {
	void* library;
} adlbox_t;

#define self(T) alo_toobject(T, 0, adlbox_t*)

static int dlbox_del(astate T) {
	adlbox_t* box = self(T);
	if (box->library) {
		l_unload(box->library);
		box->library = NULL;
	}
	return 0;
}

static int dlbox_index(astate T) {
	aloL_checkstring(T, 1);
	adlbox_t* box = self(T);
	astr name = alo_tostring(T, 1);
	acfun fun = l_get(T, box->library, name);
	if (fun == NULL) {
		aloL_error(T, "failed to get function '%s' from library", name);
	}
	alo_push(T, 0); /* mark reference to the box to avoid GC */
	alo_pushcclosure(T, fun, 1);
	return 1;
}

static adlbox_t* l_preload(astate T) {
	adlbox_t* box = alo_newobject(T, adlbox_t);
	box->library = NULL;
	if (aloL_getsimpleclass(T, "__file")) {
		static const acreg_t funs[] = {
				{ "__idx", dlbox_index },
				{ "__del", dlbox_del },
				{ NULL, NULL }
		};
		aloL_setfuns(T, -1, funs);
	}
	alo_setmetatable(T, -2);
	return box;

}

static int loaddl(astate T, astr path, int seeglb) {
	adlbox_t* box = l_preload(T);
	box->library = l_load(T, path, seeglb);
	if (box->library == NULL) {
		return false;
	}
	return true;
}

#if defined(ALO_USE_POSIX)

#include <unistd.h>
#define l_readable(path) (access(path, R_OK) == 0)

#elif defined(ALOE_WINDOWS)

#include <io.h>
#define l_readable(path) (_access(path, R_OK) == 0)

#else

static int l_readable(astr path) {
	FILE* file = fopen(path, "r"); /* try to open file */
	if (file == NULL) { /* cannot open */
		return false;
	}
	fclose(file);
	return true;
}

#endif

/**
 ** load dynamic library from files.
 ** prototype: [imported] mod.loaddl(path, [load])
 */
static int mod_loaddl(astate T) {
	astr path = aloL_checkstring(T, 0);
	astr fname = aloL_getoptstring(T, 1, "*");
	alo_settop(T, 2);
	/* find library in loaded list */
	if (alo_rawgets(T, ALO_CAPTURE_INDEX(0), path) != ALO_TRAWDATA) {
		if (!loaddl(T, path, *fname == '*'))
			goto error;
		/* put library into loaded list */
		alo_push(T, 2);
		alo_rawsets(T, ALO_CAPTURE_INDEX(0), path);
	}

	adlbox_t* box = alo_toobject(T, 2, adlbox_t*);
	if (*fname == '*') {
		alo_pushboolean(T, true);
		return 1;
	}
	acfun fun = l_get(T, box->library, fname);
	if (fun == NULL)
		goto error;
	alo_pushcclosure(T, fun, 1);
	return 1;

	error:
	alo_pushnil(T);
	alo_push(T, -2); /* push error message */
	return 2;
}

static int loader_b(astate T) {
	astr name = aloL_checkstring(T, 0);
	aloL_checkstring(T, 1);
	aloL_checktype(T, 2, ALO_TLIST);
	alo_settop(T, 3);
	alo_rawgets(T, ALO_CAPTURE_INDEX(0), ALO_BUILTIN_KEY);
	if (alo_rawgets(T, 3, name) == ALO_TUNDEF) { /* find module in built-in table */
		alo_pushfstring(T, "\n\tno field mod.builtin['%s']", name);
		alo_add(T, 2);
		return 0;
	}
	return 1;
}

static int loader_l(astate T) {
	aloL_checkstring(T, 0);
	astr path = aloL_checkstring(T, 1);
	aloL_checktype(T, 2, ALO_TLIST);
	alo_settop(T, 3);
	alo_gets(T, ALO_CAPTURE_INDEX(0), ALO_LPATH_KEY); /* stack[3] = mod.__lpath */
	int n = alo_rawlen(T, 3);
	for (int i = n - 1; i >= 0; --i) {
		alo_rawgeti(T, 3, i);
		astr actual = aloL_sreplace(T, alo_tostring(T, 4), "?", path);
		switch (aloL_compilef(T, path, actual)) {
		case (-1): /* file not found */
			alo_drop(T);
			break;
		case ThreadStateRun: /* module script loaded */
			alo_pop(T, 1);
			alo_settop(T, 2);
			goto initialize;
		default: /* compiled failed */
			alo_error(T); /* throw error message */
		}
		alo_pushfstring(T, "\n\tno file '%s'", actual);
		alo_add(T, 2);
		alo_settop(T, 4);
	}
	return 0;

	initialize:
	alo_newtable(T, 0); /* delegate = [:]  */
	alo_newtable(T, 1); /* meta = [:] */
	alo_push(T, ALO_REGISTRY_INDEX); /* push environment */
	alo_rawsets(T, 3, "__idx"); /* meta.__idx = _G */
	alo_push(T, 0);
	alo_rawsets(T, 3, PACKAGE_KEY); /* put package path */
	alo_setmetatable(T, 2); /* setmeta(delegate, meta) */

	/* put module table into loaded table */
	alo_rawgets(T, ALO_CAPTURE_INDEX(0), ALO_LOADED_KEY);
	alo_push(T, 0);
	alo_push(T, 1);
	alo_rawset(T, 2);
	alo_drop(T);

	alo_push(T, 2);
	alo_setdelegate(T, 1); /* set module as delegate for initialize function */
	alo_push(T, 1);
	alo_call(T, 0, 0); /* call initialize function,
	                      the error thrown will be caught by import function directly */

	alo_push(T, 1);
	return 1;
}

static int loader_c(astate T) {
	/* the loader will only take effect when loading dynamic library is available */
	aloL_checkstring(T, 0);
	astr path = aloL_checkstring(T, 1);
	aloL_checktype(T, 2, ALO_TLIST);
	alo_settop(T, 3);
	alo_gets(T, ALO_CAPTURE_INDEX(0), ALO_CPATH_KEY); /* stack[2] = mod.__cpath */
	int n = alo_rawlen(T, 3);
	for (int i = n - 1; i >= 0; --i) {
		alo_rawgeti(T, 3, i);
		astr actual = aloL_sreplace(T, alo_tostring(T, 4), "?", path);
#ifndef DL_NOT_SUPPORT
		if (l_readable(actual)) {
			if (!loaddl(T, actual, false)) {
				alo_error(T); /* throw error message */
			}
			return 1;
		}
#endif
		alo_pushfstring(T, "\n\tno file '%s'", actual);
		alo_add(T, 2);
		alo_settop(T, 4);
	}
	return 0;
}

/**
 ** import module from files.
 ** prototype: [imported] mod.import(path, [load])
 */
static int mod_import(astate T) {
	size_t len;
	const char* name = aloL_checklstring(T, 0, &len);
	if (len > MAX_PATH_LENGTH) {
		aloL_argerror(T, 0, "module name too long.");
	}
	int load = aloL_getoptbool(T, 1, true); /* get 'force to load' flag */

	/* try find cached module */
	if (alo_rawgets(T, ALO_CAPTURE_INDEX(1), name) != ALO_TUNDEF || !load) { /* module already imported */
		return 1;
	}

	alo_settop(T, 1); /* fix index */
	/* translate module source name to path */
	{
		char buf[len];
		getpath(T, buf, name, len);
		alo_pushlstring(T, buf, len);
	}

	if (alo_rawgets(T, ALO_CAPTURE_INDEX(0), ALO_MODLOADER_KEY) != ALO_TLIST) /* stack[2] = mod.LOADERS */
		aloL_error(T, "invalid module loader list.");
	alo_newlist(T, 4); /* initialize not found list */
	size_t n = alo_rawlen(T, 2);
	for (size_t i = 0; i < n; ++i) {
		alo_rawgeti(T, 2, i); /* get loader */
		alo_push(T, 0); /* push name */
		alo_push(T, 1); /* push path */
		alo_push(T, 3); /* push fail back message list */
		if (alo_pcall(T, 3, 1, ALO_NOERRFUN) != ThreadStateRun) {
			/* caught error */
			astr msg = alo_tostring(T, -1);
			alo_pushfstring(T, "module '%s' failed to load: %s", name, msg);
			alo_error(T);
		}
		if (alo_isnonnil(T, 4)) /* load success? */
			goto success;
		alo_drop(T);
	}
	/* module not found, build error to throw */
	aloL_usebuf(T, buf) {
		aloL_bputf(T, buf, "module '%s' not found:", name);
		n = alo_rawlen(T, 3);
		for (size_t i = 0; i < n; ++i) {
			alo_rawgeti(T, 3, i);
			aloL_bwrite(T, buf, 4);
			alo_drop(T);
		}
		aloL_bpushstring(T, buf);
	}
	/* throw error */
	alo_error(T);

	success:
	/* push module to cache table */
	alo_push(T, 3);
	alo_rawsets(T, ALO_CAPTURE_INDEX(1), name);

	return 1;
}

/**
 ** create search path list and push into top of state.
 */
static void l_createsp(astate T, astr search) {
	astr path = getenv(ALO_PATH);
	astr s1 = search, s2, s3, s4, s5;
	while ((s2 = strchr(s1, ';')) != NULL || (*s1 != '\0' && ((s2 = s1 + strlen(s1)), true))) {
		if ((s3 = l_strchr(s1, '~', s2 - s1))) { /* environmental path? */
			if (path) { /* has path? */
				s4 = path;
				while ((s5 = strchr(s4, ';'))) {
					aloL_usebuf(T, buf) {
						aloL_bputls(T, buf, s1, s3 - s1);
						aloL_bputls(T, buf, s4, s5 - s4);
						s3 += 1;
						aloL_bputls(T, buf, s3, s2 - s3);
						s5 = s4 + 1;
						aloL_bpushstring(T, buf);
					}
					alo_add(T, -2);
				}
				aloL_usebuf(T, buf) {
					aloL_bputls(T, buf, s1, s3 - s1);
					aloL_bputs(T, buf, s4);
					s3 += 1;
					aloL_bputls(T, buf, s3, s2 - s3);
					aloL_bpushstring(T, buf);
				}
				alo_add(T, -2);
			}
		}
		else {
			alo_pushlstring(T, s1, s2 - s1);
			alo_add(T, -2);
		}
		if (*s2 == '\0') /* end? */
			break;
		s1 = s2 + 1;
	}
}

static int mod_setpath(astate T) {
	static const astr names[] = { "cpath", "lpath", NULL };
	static const astr keys[] = { ALO_CPATH_KEY, ALO_LPATH_KEY };
	int type = aloL_checkenum(T, 0, NULL, names);
	astr paths = aloL_checkstring(T, 1);
	alo_settop(T, 3);
	if (alo_rawgets(T, ALO_CAPTURE_INDEX(0), keys[type]) != ALO_TLIST) {
		aloL_error(T, "invalid module search path list.");
	}
	if (aloL_getoptbool(T, 2, false)) {
		alo_rawclr(T, 3);
	}
	l_createsp(T, paths);
	return 0;
}

static const acreg_t mod_funcs[] = {
	{ ALO_BUILTIN_KEY, NULL },
	{ "import", NULL },
	{ "loaddl", NULL },
	{ "setpath", NULL },
	{ ALO_CPATH_KEY, NULL },
	{ ALO_LPATH_KEY, NULL },
	{ ALO_LOADED_KEY, NULL },
	{ ALO_MODLOADER_KEY, NULL },
	{ NULL, NULL }
};

static void create_loaderlist(astate T) {
	static const acfun loaders[] = { loader_b, loader_l, loader_c, NULL };

	alo_newlist(T, sizeof(loaders) / sizeof(acfun) - 1);

	for (const acfun* ploader = loaders; *ploader; ++ploader) {
		alo_push(T, -2); /* push module table as capture value for each function */
		alo_pushcclosure(T, *ploader, 1);
		alo_add(T, -2);
	}
}

int aloopen_modlib(astate T) {
	alo_bind(T, "mod.loaddl", mod_loaddl);
	alo_bind(T, "mod.import", mod_import);
	alo_newtable(T, 8);
	aloL_setfuns(T, -1, mod_funcs);
	/* put current package */
	alo_pushstring(T, "");
	alo_rawsets(T, ALO_GLOBAL_IDNEX, PACKAGE_KEY);
	/* put import function */
	alo_push(T, -1);
	alo_newtable(T, 0);
	alo_push(T, -1);
	alo_rawsets(T, -3, ALO_LOADED_KEY);
	alo_pushcclosure(T, mod_import, 2);
	alo_push(T, -1);
	alo_rawsets(T, ALO_GLOBAL_IDNEX, "import");
	alo_rawsets(T, -2, "import");
	/* push loaders */
	create_loaderlist(T);
	alo_rawsets(T, -2, ALO_MODLOADER_KEY);
	/* push builtin table */
	alo_newtable(T, 0);
	alo_rawsets(T, -2, ALO_BUILTIN_KEY);
	/* put loaddl function */
	alo_newtable(T, 0);
	alo_pushcclosure(T, mod_loaddl, 1);
	alo_rawsets(T, -2, "loaddl");
	/* put setpath function */
	alo_push(T, -1);
	alo_pushcclosure(T, mod_setpath, 1);
	alo_rawsets(T, -2, "setpath");
	/* put search location */
	alo_newlist(T, 0);
	l_createsp(T, ALO_SEARCH_LPATH);
	alo_rawsets(T, -2, ALO_LPATH_KEY);
	alo_newlist(T, 0);
	l_createsp(T, ALO_SEARCH_CPATH);
	alo_rawsets(T, -2, ALO_CPATH_KEY);
	return 1;
}
