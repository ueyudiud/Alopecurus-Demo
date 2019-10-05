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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define l_msg(fmt,args...) (printf(fmt, ##args), fflush(stdout))

static int readf(astate T, void* context, const char** ps, size_t* pl) {
	static char ch;
	FILE* file = aloE_cast(FILE*, context);
	int c = fgetc(file);
	if (c != EOF) {
		ch = c;
		*ps = &ch;
		*pl = 1;
	}
	else {
		*ps = NULL;
		*pl = 0;
	}
	return ferror(file);
}

static void compilef(astate T, astr name) {
	FILE* file = fopen(name, "r");
	if (file) {
		if (alo_compile(T, "run", name, readf, file) != ThreadStateRun) { /* compile prototype */
			aloL_error(T, 2, alo_tostring(T, -1));
		}
		alo_call(T, 0, 0); /* call function */
	}
	else {
		aloL_error(T, 1, "fail to open file '%s'", name);
	}
}

/* mark in error messages for incomplete statements */
#define EOFMARK "<eof>"
#define marklen (sizeof(EOFMARK) / sizeof(char) - 1)

static int incomplete(astate T, int status) {
	if (status == ThreadStateErrCompile) {
		size_t lmsg;
		const char* smsg = alo_tolstring(T, -1, &lmsg);
		if (lmsg > marklen && strcmp(smsg + lmsg - marklen, EOFMARK) == 0) {
			return true;
		}
	}
	return false;
}

static int compilec(astate T, astr name, int strict) {
	int status = aloL_compiles(T, -1, "main", "<stdin>");
	if (status == ThreadStateRun) {
		return true;
	}
	else if (strict && !incomplete(T, status)) {
		alo_throw(T);
	}
	alo_settop(T, -2);
	return false;
}

#define MAXINPUT 1024

static char* readline(astate T) {
	static char buf[MAXINPUT + 1];
	char* p = fgets(buf, MAXINPUT, stdin);
	if (p == NULL) {
		if (feof(stdin)) {
			exit(EXIT_SUCCESS);
		}
		alo_pushstring(T, strerror(errno));
		alo_throw(T);
	}
	size_t l = strlen(p);
	if (l == MAXINPUT + 1 && buf[MAXINPUT] != '\n') {
		alo_pushstring(T, "script too long.");
		alo_throw(T);
	}
	buf[l - 1] = '\0';
	return p;
}

/**
 ** run script in console
 */
static int runc(astate T) {
	while (true) {
		l_msg("> ");
		astr s = readline(T);
		if (*s == ':') { /* command */
			if (s[2] == '\0') {
				switch (s[1]) {
				case 'q': case 'Q': /* quit */
					exit(EXIT_SUCCESS);
				case 'v': case 'V': /* display local values */
					alo_getreg(T, "println");
					aloL_getsubfield(T, ALO_GLOBAL_IDNEX, "table", "mkstr");
					alo_push(T, ALO_REGISTRY_INDEX);
					alo_call(T, 1, 1);
					alo_call(T, 1, 0);
					continue;
				}
			}
			l_msg("unknown command '%s'", s + 1);
			continue;
		}
		alo_pushstring(T, s);
		alo_pushfstring(T, "return %s;", s);
		if (!compilec(T, "main", false)) {
			alo_push(T, 0);
			while (!compilec(T, "main", true)) {
				alo_pushstring(T, "\n");
				l_msg(">> ");
				alo_pushstring(T, readline(T));
				alo_rawcat(T, 3);
				alo_push(T, 0);
			}
		}
		alo_pop(T, 0);
		alo_settop(T, 1);
		alo_call(T, 0, ALO_MULTIRET);
		int n = alo_gettop(T);
		if (n > 0) {
			alo_getreg(T, "println");
			alo_insert(T, 0);
			if (alo_pcall(T, n, 0) != ThreadStateRun) {
				l_msg("error calling 'println', %s\n", alo_tostring(T, -1));
			}
		}
		alo_settop(T, 0);
	}
	return 0;
}

static int run(astate T) {
	astr src = alo_tostring(T, 0);
	if (*src) {
		compilef(T, src);
	}
	else {
		l_msg(ALO_COPYRIGHT"\n");
		alo_settop(T, 0);
		while (true) {
			alo_pushlightcfunction(T, runc);
			alo_push(T, 0);
			if (alo_pcall(T, 0, 0) != ThreadStateRun) {
				fprintf(stderr, "%s\n", alo_tostring(T, -1));
				alo_settop(T, 1);
			}
		}
	}
	return 0;
}

static __attribute__((noreturn)) int panic(__attribute__((unused)) astate T) {
	exit(EXIT_FAILURE);
}

static int initialize(astate T, int argc, const astr argv[]) {
	alo_setpanic(T, panic); /* set error callback */
	aloL_openlibs(T); /* open libraries */
	alo_bind(T, "run", run); /* put main function name */

	astr filename = NULL; /* source name, use stdin if absent */
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') { /* setting */
			switch (argv[i][1]) {
			case '-':
				fprintf(stderr, "illegal command '%s'", &argv[i][2]);
				break;
			default:
				fprintf(stderr, "illegal abbreviation of command '%s'", &argv[i][1]);
				break;
			}
		}
		else if (filename != NULL) {
			fprintf(stderr, "file name already settled.");
			return false;
		}
		else {
			filename = argv[i];
		}
	}

	aloL_pushscopedcfunction(T, run);
	alo_pushstring(T, filename);

	return true;
}

int main(int argc, astr argv[]) {
	astate T = aloL_newstate();
	if (T == NULL) {
		fputs("fail to initialize VM.", stderr);
	}
	if (!initialize(T, argc, argv)) {
		alo_deletestate(T);
		return EXIT_FAILURE;
	}
	if (alo_pcall(T, 1, 0)) {
		fputs(alo_tostring(T, -1), stderr);
	}
	alo_deletestate(T);
	return EXIT_SUCCESS;
}
