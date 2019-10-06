/*
 * amodlib.c
 *
 *  Created on: 2019年10月5日
 *      Author: ueyudiud
 */

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#include "acfg.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MODINFO ".aloi"

#define PACKAGE_KEY "__package"

#define ALO_PATH_KEY "__PATH"

#define ALO_SUFFIX ".alo"

static void getpath(astate T, char* buf, const char* src, size_t len) {
	for (size_t i = 0; i < len; ++i) {
		if (src[i] == '\\' || src[i] == '/') {
			if (i != 0) {
				*(buf++) = '.';
			}
		}
		else if (!isalnum(src[i])) {
			aloL_error(T, 2, "illegal module path name");
		}
		else {
			*(buf++) = src[i];
		}
	}
	*(buf++) = '\0';
}

static int loadmod(astate T, astr name, astr path, astr src) {
	astr location = alo_pushfstring(T, "%s/%s"ALO_SUFFIX, path, src);
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

static int loadenvmod(astate T, astr name, astr src) {
	astr path = getenv(ALO_PATH);
	if (path == NULL) { /* no path */
		return false;
	}
	astr next;
	while ((next = strstr(path, ALO_PATH_SEP))) {
		if (loadmod(T, name, alo_pushfstring(T, "%q"ALO_LIB_DIR, path, next - path), src)) {
			return true;
		}
		alo_drop(T);
		path = next + (sizeof(ALO_PATH_SEP) / sizeof(char) - 1);
	}
	if (*path) { /* has last value */
		if (loadmod(T, name, alo_pushfstring(T, "%q"ALO_LIB_DIR, path, next - path), src)) {
			return true;
		}
		alo_drop(T);
	}
	return false;
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
	char buf[len + 1];
	getpath(T, buf, src, len);
	if (alo_rawgets(T, ALO_CAPTURE_INDEX(0), buf) != ALO_TUNDEF || !load) { /* module already imported */
		return 1;
	}
	alo_drop(T);
	alo_gets(T, 1, ALO_PATH_KEY); /* stack[2] = __PATH */
	size_t n = alo_rawlen(T, 2);
	for (int i = n - 1; i >= 0; --i) {
		alo_rawgeti(T, 2, i);
		astr path = alo_tostring(T, 3);
		if (strcmp(ALO_PATH_ENV, path) == 0) { /* in environment */
			if (loadenvmod(T, buf, src)) {
				goto success;
			}
		}
		else {
			if (loadmod(T, buf, path, src)) {
				goto success;
			}
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
	{ ALO_PATH_KEY, NULL },
	{ PACKAGE_KEY, NULL },
	{ NULL, NULL }
};

int aloopen_modlib(astate T) {
	alo_bind(T, "mod.import", mod_import);
	alo_push(T, ALO_GLOBAL_IDNEX);
	aloL_setfuns(T, -1, mod_funcs);
	/* put import function */
	alo_newtable(T, 0);
	alo_pushcclosure(T, mod_import, 1);
	alo_rawsets(T, -2, "import");
	/* put current package */
	alo_pushstring(T, "");
	alo_rawsets(T, -2, PACKAGE_KEY);
	/* put search location */
	aloL_newstringlist(T, ALO_PATH_ENV, ".");
	alo_rawsets(T, -2, ALO_PATH_KEY);
	return 1;
}
