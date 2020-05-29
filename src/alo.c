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

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define l_msg(fmt,args...) (printf(fmt, ##args), fflush(stdout))
#define l_err(fmt,args...) fprintf(stderr, fmt, ##args)

static astate glbT;

static void sigstop(astate T, __attribute__((unused)) aframeinfo_t* info) {
	alo_sethook(T, NULL, 0);
	aloL_error(T, "interrupted!");
}

static void sigaction(int id) {
	signal(id, SIG_DFL);
	alo_sethook(glbT, sigstop, ALO_HMASKCALL | ALO_HMASKRET);
}

static void compilef(astate T, astr name) {
	int status = aloL_compilef(T, name, name);
	switch (status) {
	case ThreadStateRun:
		break;
	case -1:
		aloL_error(T, "fail to open file '%s'", name);
		break;
	default:
		alo_error(T);
		break;
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
		alo_error(T);
	}
	alo_settop(T, -2);
	return false;
}

#define MAXINPUT 1023

/**
 ** read line from standard input stream.
 */
static int readline(astate T, ambuf_t* buf) {
	size_t l;
	do {
		aloL_bcheck(T, buf, MAXINPUT + 1);
		char* p = fgets(aloL_btop(buf), MAXINPUT + 1, stdin);
		if (p == NULL) {
			/* read failed */
			if (feof(stdin)) {
				alo_deletestate(T);
				exit(EXIT_SUCCESS);
			}
			l_err("fail to read line: %s", strerror(errno));
			return false;
		}
		l = strlen(p);
		aloL_blen(buf) += l * sizeof(char);
	}
	while (l == MAXINPUT && /* reach to end of buffer? */
			aloL_bgetc(buf, (aloL_blen(buf) - 1) / sizeof(char)) != '\n'); /* not read to end? */

	return true;
}

static int loadscript(astate T) {
	int succ = false;
	l_msg("> ");
	aloL_usebuf(T, buf,
		if (readline(T, buf)) {
			alo_pushfstring(T, "return %s;", aloL_b2str(T, buf));
			if (!compilec(T, "main", false)) { /* try add return statement */
				aloL_blen(buf) -= 1; /* remove '\0' character */
				succ = true;
				aloL_bpushstring(T, buf);
				while (!compilec(T, "main", true)) { /* unclosed statement, attempt to read more */
					l_msg(">> ");
					if (!readline(T, buf)) {
						succ = false;
						break;
					}
					aloL_bpushstring(T, buf);
				}
			}
			else {
				succ = true;
			}
			alo_pop(T, 0);
			alo_settop(T, 1);
		}
	)
	return succ;
}

static void runscript(astate T) {
	glbT = T;
	signal(SIGINT, sigaction);
	alo_call(T, 0, ALO_MULTIRET);
	signal(SIGINT, SIG_DFL);
	int n = alo_gettop(T);
	if (n > 0) {
		alo_getreg(T, "println");
		alo_insert(T, 0);
		if (alo_pcall(T, n, 0, ALO_NOERRFUN) != ThreadStateRun) {
			l_msg("error calling 'println', %s\n", alo_tostring(T, -1));
		}
	}
}

/**
 ** do REPL in console
 */
static int runc(astate T) {
	while (true) {
		/* load and run run script */
		if (loadscript(T)) {
			runscript(T);
		}
		alo_settop(T, 0);
	}
	return 0;
}

/**
 ** run script from file.
 */
static int runf(astate T) {
	compilef(T, alo_tostring(T, 0));
	alo_call(T, 0, 0); /* call function */
	return 0;
}

static anoret int panic(__attribute__((unused)) astate T) {
	l_err("unmanaged error caught.\n");
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

#define DEFAULT_LOG_DEPTH 5

static astr prog;
static astr srcpath;
static astr rtpath;
static int logdepth = DEFAULT_LOG_DEPTH;

static void prtusage() {
	l_msg(
			"usage: %s [options] [script]\n"
			"option list:\n"
			"  -h       show usage information\n"
			"  -l name  import library 'name' into global scope\n"
			"  -r dir   change working directory to 'dir'\n"
			"  -v       show version information\n"
			"  -d depth set the max log stack trace depth (default is %d)\n", prog, DEFAULT_LOG_DEPTH);
}

#define has_err	0x0001 /* error */
#define has_h	0x0002 /* -h */
#define has_v	0x0004 /* -v */
#define has_l	0x0008 /* -l exists */
#define has_d	0x0010 /* -d level */

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
		case 'd': {
			if (argv[i][2] != '\0')
				goto unknown;
			if (mask & has_d) {
				l_err("max stack trace depth already settled.");
				return has_err;
			}
			mask |= has_d;
			char* s;
			logdepth = strtol(argv[++i], &s, 10);
			if (*s || errno || logdepth < 0) {
				l_err("illegal max stack trace depth: '%s'", argv[i]);
				return has_err;
			}
			break;
		}
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
	if (alo_pcall(T, 1, 1, ALO_NOERRFUN) != ThreadStateRun)
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

static int ferrmsg(astate T) {
	if (!alo_isstring(T, 0)) {
		int type = alo_getmeta(T, 0, "__tostr", true);
		switch (type) {
		case ALO_TFUNCTION: /* applied by function? */
			/* call function */
			alo_push(T, 0);
			alo_call(T, 1, 1);
			if (alo_typeid(T, -1) != ALO_TSTRING) {
				goto error;
			}
			break;
		case ALO_TSTRING:
			break;
		default:
			error:
			alo_pushfstring(T, "can not transform %s value to string");
			break;
		}
	}
	if (logdepth > 0) {
		alo_pushfstring(T, "%s\nstack trace:", alo_tostring(T, -1));
		aloL_where(T, logdepth);
	}
	return 1;
}

int main(int argc, astr argv[]) {
	astate T = aloL_newstate();
	aloE_log("alo in debug mode");
	if (T == NULL) {
		l_err("fail to initialize VM.\n");
	}
	alo_setpanic(T, panic); /* set error callback */

	int flags = check_args(argc, argv); /* check and collect arguments */
	if (flags == has_err) /* error? */
		goto error;

	if (flags & has_h)
		prtusage();

	if (flags & has_v)
		l_msg(ALO_COPYRIGHT"\n");

	/* move runtime directory to configurated path */
	if (rtpath ? chdir(rtpath) : srcpath ? movedir(srcpath) : 0) {
		l_err("fail to move directory to target place.\n");
		goto error;
	}

	if (srcpath || !(flags & (has_h | has_v))) {
		aloL_openlibs(T); /* open libraries */

		if (flags & has_l) { /* has custom libraries */
			if (!openlibs(T, argc, argv)) { /* open custom libraries */
				goto error;
			}
		}

		alo_pushlightcfunction(T, ferrmsg);
		if (srcpath) {
			alo_bind(T, "<run>", runf); /* put main function name */
			aloL_pushscopedcfunction(T, runf);
			alo_pushstring(T, srcpath);
			if (alo_pcall(T, 1, 0, 0) != ThreadStateRun) {
				l_err("%s\n", alo_tostring(T, -1));
			}
		}
		else {
			alo_bind(T, "<run>", runc); /* put main function name */
			l_msg(ALO_COPYRIGHT"\n");
			alo_settop(T, 0);
			alo_pushlightcfunction(T, ferrmsg);
			while (true) {
				alo_pushlightcfunction(T, runc);
				if (alo_pcall(T, 0, 0, 0) != ThreadStateRun) {
					l_err("%s\n", alo_tostring(T, -1));
					alo_settop(T, 1);
				}
			}
		}
	}

	alo_deletestate(T);
	return EXIT_SUCCESS;

	error:
	alo_deletestate(T);
	return EXIT_FAILURE;
}
