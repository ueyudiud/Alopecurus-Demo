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
	*pl = fread(buf, sizeof(char), 256, (FILE*) context);
	*ps = buf;
	return ferror((FILE*) context);
}

static int run(astate T) {
	puts("load libraries");
	aloL_openlibs(T);
	alo_bind(T, "run", run); /* put main function name */
	puts("open file");
	astr src = alo_tostring(T, 0);
	FILE* file = fopen(src, "r");
	if (file) {
		puts("compile script");
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
	astate T = aloL_newstate();
	alo_pushlightcfunction(T, run);
	alo_pushstring(T, argv[1]);
	if (alo_pcall(T, 1, 0)) {
		fputs(alo_tostring(T, -1), stderr);
	}
	alo_deletestate(T);
	return 0;
}
