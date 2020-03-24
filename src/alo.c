/*
 * alo.c
 *
 *  Created on: 2019年7月20日
 *      Author: ueyudiud
 */

#define ALO_C_

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define l_msg(fmt,args...) (printf(fmt, ##args), fflush(stdout))
#define l_err(fmt,args...) fprintf(stderr, fmt, ##args)

static int readf(__attribute__((unused)) astate T, void* context, const char** ps, size_t* pl) {
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

static int compilef(astate T, astr name) {
	FILE* file = fopen(name, "r");
	if (file) {
		if (alo_compile(T, "run", name, readf, file) != ThreadStateRun) { /* compile prototype */
			aloL_error(T, 2, alo_tostring(T, -1));
			return false;
		}
		return true;
	}
	else {
		aloL_error(T, 1, "fail to open file '%s'", name);
		return false;
	}
}

/* mark in error messages for incomplete statements */
#define EOFMARK "<eof>"
#define marklen (sizeof(EOFMARK) / sizeof(char) - 1)

/**
 ** return true if script is incomplete.
 */
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

/**
 ** compile script in top of stack, return true if compile success,
 ** return false if statement is incomplete and throw an error otherwise.
 */
static int compilec(astate T, astr name, int strict) {
	int status = aloL_compiles(T, -1, name, "<stdin>");
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

/**
 ** read line from standard input stream.
 */
static char* readline(astate T) {
	static char buf[MAXINPUT + 1];
	char* p = fgets(buf, MAXINPUT + 1, stdin);
	if (p == NULL) {
		if (feof(stdin)) {
			alo_deletestate(T);
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

static void loadscript(astate T, astr s) {
	alo_pushstring(T, s);
	alo_pushfstring(T, "return %s;", s);
	if (!compilec(T, "main", false)) { /* try add return statement */
		alo_push(T, 0);
		while (!compilec(T, "main", true)) { /* unclosed statement, try multi-line statement */
			alo_pushstring(T, "\n");
			l_msg(">> ");
			alo_pushstring(T, readline(T));
			alo_rawcat(T, 3);
			alo_push(T, 0);
		}
	}
	alo_pop(T, 0);
	alo_settop(T, 1);
}

static void runscript(astate T) {
	alo_call(T, 0, ALO_MULTIRET);
	int n = alo_gettop(T);
	if (n > 0) {
		alo_getreg(T, "println");
		alo_insert(T, 0);
		if (alo_pcall(T, n, 0) != ThreadStateRun) {
			l_msg("error calling 'println', %s\n", alo_tostring(T, -1));
		}
	}
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
					alo_deletestate(T);
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
			l_msg("unknown command '%s'\n", s + 1);
			continue;
		}
		/* load and run run script */
		loadscript(T, s);
		runscript(T);
		alo_settop(T, 0);
	}
	return 0;
}

static int run(astate T) {
	astr src = alo_tostring(T, 0);
	if (*src) {
		if (compilef(T, src)) {
			alo_call(T, 0, 0); /* call function */
		}
	}
	else {
		l_msg(ALO_COPYRIGHT"\n");
		alo_settop(T, 0);
		while (true) {
			alo_pushlightcfunction(T, runc);
			alo_push(T, 0);
			if (alo_pcall(T, 0, 0) != ThreadStateRun) {
				l_err("%s\n", alo_tostring(T, -1));
				alo_settop(T, 1);
			}
		}
	}
	return 0;
}

static __attribute__((noreturn)) int panic(__attribute__((unused)) astate T) {
	exit(EXIT_FAILURE);
}

static int movedir(astr f) {
	astr p = f;
	astr l = NULL;
	while (*p) {
		if (*p == '\\' || *p == '/') {
			l = p;
		}
		p++;
	}
	if (l == NULL || l == f) {
		return false;
	}
	size_t len = l - f;
	char path[len + 1];
	memcpy(path, f, len);
	path[len] = '\0';
	return chdir(path);
}

static int initialize(astate T, int argc, const astr argv[]) {
	alo_setpanic(T, panic); /* set error callback */
	aloL_openlibs(T); /* open libraries */
	alo_bind(T, "<run>", run); /* put main function name */
	alo_bind(T, "<runc>", runc); /* put main function name */

	astr filename = NULL; /* source name, use stdin if absent */
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') { /* setting */
			switch (argv[i][1]) {
			case '-': {
				l_err("illegal command '%s'.\n", &argv[i][2]);
				return false;
			}
			default: {
				l_err("illegal abbreviation of command '%s'.\n", &argv[i][1]);
				return false;
			}
			}
		}
		else if (filename != NULL) {
			l_err("file name already settled.\n");
			return false;
		}
		else {
			filename = argv[i];
			if (movedir(filename)) {
				l_err("fail to move directory to target place.\n");
				return false;
			}
		}
	}

	aloL_pushscopedcfunction(T, run);
	alo_pushstring(T, filename);

	return true;
}

int main(int argc, astr argv[]) {
	astate T = aloL_newstate();
	if (T == NULL) {
		l_err("fail to initialize VM.");
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
