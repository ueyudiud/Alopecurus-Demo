/*
 * alo.c
 *
 *  Created on: 2019年7月20日
 *      Author: ueyudiud
 */

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#include <stdio.h>

static char buf[256];

static int readf(astate T, void* context, const char** ps, size_t* pl) {
	if (feof((FILE*) context)) {
		*pl = 0;
		return 0;
	}
	*pl = fread(buf, 1, sizeof(buf), (FILE*) context);
	*ps = buf;
	return ferror((FILE*) context);
}

static int run(astate T) {
	aloL_openlibs(T);
	alo_bind(T, "run", run); /* put main function name */
	astr src = alo_tostring(T, 0);
	FILE* file = stdin;
	if (file) {
		if (alo_compile(T, "run", src, readf, file) != ThreadStateRun) {
			aloL_error(T, 2, alo_tostring(T, -1));
		}
		else if (alo_pcall(T, 0, 0) != ThreadStateRun) {
			aloL_error(T, 2, alo_tostring(T, -1));
		}
		alo_settop(T, 1);
	}
	else {
		aloL_error(T, 1, "fail to open file '%s'", src);
	}
	return 0;
}

int main(int argc, const char* argv[]) {
	setvbuf(stdout, NULL, _IONBF, 0);
	astate T = aloL_newstate();
	alo_pushlightcfunction(T, run);
	alo_pushstring(T, argv[1]);
	if (alo_pcall(T, 1, 0)) {
		fputs(alo_tostring(T, -1), stderr);
	}
	alo_deletestate(T);
	return 0;
}
