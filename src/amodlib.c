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

#include "acfg.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MODINFO ".aloi"

#define PACKAGE_KEY "__package"

#define ALO_LPATH_KEY "__LPATH"
#define ALO_CPATH_KEY "__CPATH"

#define ALO_SUFFIX ".alo"

#define MAX_PATH_LENGTH 1024

static void getpath(astate T, char* buf, const char* src, size_t len) {
	for (size_t i = 0; i < len; ++i) {
		if (src[i] == '\\' || src[i] == '/') {
			if (i != 0) {
				*(buf++) = '.';
			}
		}
		else if (!isalnum(aloE_byte(src[i]))) {
			aloL_error(T, 2, "illegal module path name");
		}
		else {
			*(buf++) = src[i];
		}
	}
	*(buf++) = '\0';
}

#if defined(ALO_USE_DLOPEN)

#include <dlfcn.h>

#define aloE_fun(f) (__extension__ aloE_cast(acfun, f))

static void* l_load(astate T, astr name, int seeglb) {
	void* lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
	if (lib == NULL) {
		alo_pushstring(T, dlerror());
	}
	return lib;
}

static void l_unload(astate T, void* lib) {
	dlclose(lib);
}

static acfun l_get(astate T, void* lib, astr name) {
	acfun fun = aloE_fun(dlsym(lib, name));
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

static void l_unload(astate T, void* lib) {
	FreeLibrary(aloE_mod(lib));
}

static acfun l_get(astate T, void* lib, astr name) {
	acfun fun = aloE_cast(acfun, GetProcAddress(aloE_mod(lib), name));
	if (fun == NULL) {
		geterr(T);
	}
	return fun;
}

#else

#define DL_NS_MSG	"dynamic libraries is not supported."

static void* l_load(astate T, astr name, int seeglb) {
	alo_pushcstring(T, DL_NS_MSG);
	return NULL;
}

static void l_unload(astate T, __attribute__((unused)) void* lib) {

}

static acfun l_get(astate T, void* lib, astr name) {
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
		l_unload(T, box->library);
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
		aloL_error(T, 2, "failed to get function '%s' from library", name);
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

static int loaddl(astate T, astr name, astr location) {
	adlbox_t* box = l_preload(T);
	box->library = l_load(T, location, false);
	if (box->library == NULL) {
		return false;
	}
	return true;
}

static int is_readable(astr path) {
	FILE* file = fopen(path, "r"); /* try to open file */
	if (file == NULL) { /* cannot open */
		return false;
	}
	fclose(file);
	return true;
}

/**
 ** load dynamic library from files.
 ** prototype: [imported] mod.loaddl(path, [load])
 */
static int mod_loaddl(astate T) {
	aloL_checkstring(T, 0);
	int load = aloL_getoptbool(T, 1, true);
	alo_settop(T, 1);
	alo_getreg(T, "mod"); /* stack[1] = mod */
	size_t len;
	const char* src = alo_tolstring(T, 0, &len);
	if (len > MAX_PATH_LENGTH) {
		aloL_error(T, 2, "module name too long.");
	}
	char buf[len + 1];
	getpath(T, buf, src, len);
	if (alo_rawgets(T, ALO_CAPTURE_INDEX(0), buf) != ALO_TUNDEF || !load) { /* module already loaded */
		return 1;
	}
	alo_drop(T);
	alo_gets(T, 1, ALO_CPATH_KEY); /* stack[2] = __CPATH */
	size_t n = alo_rawlen(T, 2);
	astr location;
	for (int i = n - 1; i >= 0; --i) {
		alo_rawgeti(T, 2, i);
		aloL_checkstring(T, 3);
		location = aloL_sreplace(T, alo_tostring(T, 3), "?", src);
		if (is_readable(location)) {
			goto found;
		}
		alo_settop(T, -2);
	}
	aloL_error(T, 2, "can not found library: %s", buf);

	found:
	if (!loaddl(T, buf, location)) {
		/* failed to load library */
		aloL_error(T, 2, "fail to open library (%s)", alo_tostring(T, -1));
	}

	alo_push(T, -1);
	alo_rawsets(T, ALO_CAPTURE_INDEX(0), buf);

	return 1;

}

static int loadmod(astate T, astr name, astr path, astr src) {
	astr location = aloL_sreplace(T, path, "?", src);
	int result = aloL_compilef(T, name, location);
	switch (result) {
	case (-1): /* file not found */
		alo_drop(T); /* drop string value */
		return false;
	case ThreadStateRun:
		return true;
	default:
		aloL_error(T, 2, "fail to import module, %s", alo_tostring(T, -1));
		alo_throw(T); /* caught error, throw */
		return false;
	}
}

/**
 ** import module from files.
 ** prototype: [imported] mod.import(path, [load])
 */
static int mod_import(astate T) {
	aloL_checkstring(T, 0);
	int load = aloL_getoptbool(T, 1, true);
	alo_settop(T, 1);
	alo_getreg(T, "mod"); /* stack[1] = mod */
	size_t len;
	const char* src = alo_tolstring(T, 0, &len);
	if (len > MAX_PATH_LENGTH) {
		aloL_error(T, 2, "module name too long.");
	}
	char buf[len + 1];
	getpath(T, buf, src, len);
	if (alo_rawgets(T, ALO_CAPTURE_INDEX(0), buf) != ALO_TUNDEF || !load) { /* module already imported */
		return 1;
	}
	alo_drop(T);
	alo_gets(T, 1, ALO_LPATH_KEY); /* stack[2] = __PATH */
	size_t n = alo_rawlen(T, 2);
	for (int i = n - 1; i >= 0; --i) {
		alo_rawgeti(T, 2, i);
		aloL_checkstring(T, 3);
		astr path = alo_tostring(T, 3);
		if (loadmod(T, buf, path, src)) {
			goto success;
		}
		alo_drop(T);
	}
	aloL_error(T, 2, "can not found module: %s", buf);

	success:
	alo_newtable(T, 0);
	alo_newtable(T, 1);
	alo_push(T, ALO_GLOBAL_IDNEX); /* push global environment */
	alo_rawsets(T, -2, "__idx"); /* set lookup table */
	alo_setmetatable(T, -2);
	alo_push(T, -1);
	alo_rawsets(T, ALO_CAPTURE_INDEX(0), buf);
	alo_pop(T, 0);

	alo_pop(T, 1); /* stack[0] = openf */
	alo_settop(T, 2);
	alo_push(T, 0);
	alo_setdelegate(T, 1);
	alo_call(T, 0, 0);
	return 1;
}

static const acreg_t mod_funcs[] = {
	{ "import", NULL },
	{ "loaddl", NULL },
	{ ALO_LPATH_KEY, NULL },
	{ ALO_CPATH_KEY, NULL },
	{ PACKAGE_KEY, NULL },
	{ NULL, NULL }
};

#define l_strchr(s,c,l) aloE_cast(const char*, memchr(s, c, (l) * sizeof(char)))

/**
 ** create search path list and push into top of state.
 */
static void l_createsp(astate T, astr search) {
	astr path = getenv(ALO_PATH);
	alo_newlist(T, 0);
	int l = 0;
	astr s1 = search, s2, s3, s4, s5;
	while ((s2 = strchr(s1, ';')) != NULL) {
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
					alo_rawseti(T, -2, l++);
				}
				aloL_usebuf(T, buf) {
					aloL_bputls(T, buf, s1, s3 - s1);
					aloL_bputs(T, buf, s4);
					s3 += 1;
					aloL_bputls(T, buf, s3, s2 - s3);
					aloL_bpushstring(T, buf);
				}
				alo_rawseti(T, -2, l++);
			}
		}
		else {
			alo_pushlstring(T, s1, s2 - s1);
			alo_rawseti(T, -2, l++);
		}
		s1 = s2 + 1;
	}
}

int aloopen_modlib(astate T) {
	alo_bind(T, "mod.import", mod_import);
	alo_push(T, ALO_GLOBAL_IDNEX);
	aloL_setfuns(T, -1, mod_funcs);
	/* put import function */
	alo_newtable(T, 0);
	alo_pushcclosure(T, mod_import, 1);
	alo_rawsets(T, -2, "import");
	/* put loaddl function */
	alo_newtable(T, 0);
	alo_pushcclosure(T, mod_loaddl, 1);
	alo_rawsets(T, -2, "loaddl");
	/* put current package */
	alo_pushstring(T, "");
	alo_rawsets(T, -2, PACKAGE_KEY);
	/* put search location */
	l_createsp(T, ALO_SEARCH_LPATH);
	alo_rawsets(T, -2, ALO_LPATH_KEY);
	l_createsp(T, ALO_SEARCH_CPATH);
	alo_rawsets(T, -2, ALO_CPATH_KEY);
	return 1;
}
