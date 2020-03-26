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

static astr prog;
static astr srcpath;
static astr rtpath;

static void prtusage() {
	l_msg(
			"usage: %s [options] [script]\n"
			"option list:\n"
			"  -h       show usage information\n"
			"  -l name  import library 'name' into global scope\n"
			"  -r dir   change working directory to 'dir'\n"
			"  -v       show version information\n", prog);
}

#define has_err	0x0001 /* error */
#define has_h	0x0002 /* -h */
#define has_v	0x0004 /* -v */
#define has_l	0x0008 /* -l exists */

static int check_args(int argc, const astr argv[]) {
	prog = argv[0];
	srcpath = NULL;
	rtpath = NULL;
	int mask = 0;
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '\0') {
			l_err("empty string is invalid filename.\n");
			return has_err;
		}
		if (argv[i][0] != '-') {
			srcpath = argv[i];
			return mask;
		}
		switch (argv[i][1]) {
		case '\0':
			l_err("missing option name");
			return has_err;
		case 'h':
			if (argv[i][2] != '\0')
				goto unknown;
			mask |= has_h;
			break;
		case 'v':
			if (argv[i][2] != '\0')
				goto unknown;
			mask |= has_v;
			break;
		case 'l':
			if (argv[i][2] == '\0') {
				if (++i == argc || argv[i][0] == '\0') {
					l_err("library name expected.");
					return has_err;
				}
			}
			mask |= has_l;
			break;
		case 'r':
			if (argv[i][2] != '\0')
				goto unknown;
			if (++i == argc || argv[i][0] == '\0') {
				l_err("runtime path expected.");
				return has_err;
			}
			rtpath = argv[i];
			break;
		default:
			unknown:
			l_err("unknown option '%s', insert '%s -h' to get more information.", argv[i], prog);
			return has_err;
		}
	}
	return mask;
}

static int openlib(astate T, astr name) {
	alo_getreg(T, "import");
	alo_pushstring(T, name);
	if (alo_pcall(T, 1, 1) != ThreadStateRun)
		return false;
	alo_rawsets(T, ALO_REGISTRY_INDEX, name);
	alo_settop(T, 0);
	return true;
}

static int openlibs(astate T, int argc, const astr argv[]) {
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-' && argv[i][1] == 'l') {
			astr name = argv[i][2] ? argv[i] + 2 : argv[++i];
			if (!openlib(T, name)) {
				l_err("%s\n", alo_tostring(T, -1));
				return false;
			}
		}
	}
	return true;
}

/* the action after initialize */

#define m_invoke 0
#define m_skip   1
#define m_error  2

static int initialize(astate T, int argc, const astr argv[]) {
	alo_setpanic(T, panic); /* set error callback */
	alo_bind(T, "<run>", run); /* put main function name */
	alo_bind(T, "<runc>", runc); /* put main function name */

	int flags = check_args(argc, argv); /* check and collect arguments */
	if (flags == has_err) /* error? */
		return m_error;

	if (flags & has_h)
		prtusage();

	if (flags & has_v)
		l_msg(ALO_COPYRIGHT"\n");

	/* move runtime directory to configurated path */
	if (rtpath ? chdir(rtpath) : srcpath ? movedir(srcpath) : 0) {
		l_err("fail to move directory to target place.\n");
		return m_error;
	}

	if (srcpath || !(flags & (has_h | has_v))) {
		aloL_openlibs(T); /* open libraries */

		if (flags & has_l) { /* has custom libraries */
			if (!openlibs(T, argc, argv)) { /* open custom libraries */
				return m_error;
			}
		}

		aloL_pushscopedcfunction(T, run);
		alo_pushstring(T, srcpath);
		return m_invoke;
	}

	return m_skip;
}

int main(int argc, astr argv[]) {
	astate T = aloL_newstate();
	if (T == NULL) {
		l_err("fail to initialize VM.\n");
	}
	switch (initialize(T, argc, argv)) {
	case m_error: /* initialized failed */
		alo_deletestate(T);
		return EXIT_FAILURE;
	case m_invoke: /* invoke script */
		if (alo_pcall(T, 1, 0)) {
			fputs(alo_tostring(T, -1), stderr);
		}
		break;
	}
	alo_deletestate(T);
	return EXIT_SUCCESS;
}
