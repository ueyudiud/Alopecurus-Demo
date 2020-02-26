/*
 * atest.c
 *
 *  Created on: 2019年8月19日
 *      Author: ueyudiud
 */

#include "acfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "aall.h"

static int fwriter(astate T, void* buf, const void* src, size_t len) {
	fwrite(src, 1, len, aloE_cast(FILE*, buf));
	return 0;
}

static void prtkst(astate T, aproto_t* p, int index) {
	if (index > p->nconst) {
		printf("<error>");
		return;
	}
	const atval_t* o = p->consts + index;
	switch (ttpnv(o)) {
	case ALO_TNIL  : printf("nil"); break;
	case ALO_TBOOL : printf("%s", tgetbool(o) ? "true" : "false"); break;
	case ALO_TINT  : printf(ALO_INT_FORMAT, tgetint(o)); break;
	case ALO_TFLOAT: printf(ALO_FLT_FORMAT, tgetflt(o)); break;
	case ALO_TSTRING: {
		astring_t* s = tgetstr(o);
		alo_format(T, fwriter, stdout, "\"%\"\"", s->array, aloS_len(s));
		break;
	}
	}
}

static void prtreg(astate T, aproto_t* p, int index) {
	if (aloK_iscapture(index)) {
		index = aloK_getcapture(index);
		printf("#%d", index);
	}
	else {
		index = aloK_getstack(index);
		printf("%%%d", index);
	}
}

static void prtrk(astate T, aproto_t* p, int index, abyte mode) {
	(mode ? prtkst : prtreg)(T, p, index);
}

static int detail = false;

#define nameof(name) (*(name)->array ? (name)->array : "<anonymous>")

static void dump(astate T, aproto_t* p) {
	printf("%s (%s:%d,%d) %"PRIu64" instructions at %p\n",
			nameof(p->name), p->src->array, p->linefdef, p->lineldef, p->ncode, p);
	printf("%d%s params, %d stack, %d captures, %d local, %"PRIu64" constants, %"PRId64" functions\n",
			p->nargs, p->fvararg ? "+" : "", p->nstack, p->ncap, p->nlocvar, p->nconst, p->nchild);
	if (detail) {
		printf("constants (%"PRId64") for %p:\n", p->nconst, p);
		for (int i = 0; i < p->nconst; ++i) {
			printf("\t[%d] ", i);
			prtkst(T, p, i);
			printf("\n");
		}
		printf("captures (%d) for %p:\n", p->ncap, p);
		for (int i = 0; i < p->ncap; ++i) {
			acapinfo_t* info = p->captures + i;
			printf("\t[%d]\t%s\t%s%d\n", i + 1, nameof(info->name), info->finstack ? "%" : "#", info->index);
		}
		printf("local (%d) for %p:\n", p->nlocvar, p);
		for (int i = 0; i < p->nlocvar; ++i) {
			alocvar_t* info = p->locvars + i;
			printf("\t[%d]\t%s\n", info->index, nameof(info->name));
		}
	}
	alineinfo_t* info = p->lineinfo;
	int m = -1;
	for (int i = 0; i < p->ncode; ++i) {
		if (m + 1 < p->nlineinfo && i >= info->begin) {
			printf("line %d:\n", (info++)->line);
			m++;
		}
		ainsn_t code = p->code[i];
		int insn = GET_i(code);
		printf("\t%-5d %5s ", i + 1, aloK_opname[insn]);
		switch (insn) {
		case OP_MOV:
		case OP_PNM ... OP_BNOT: case OP_ITR:
		case OP_AADD ... OP_AXOR: case OP_NEW:
			prtreg(T, p, GET_A(code));
			putchar(' ');
			prtrk(T, p, GET_B(code), GET_xB(code));
			break;
		case OP_JMP:
			prtreg(T, p, GET_A(code));
			printf(" %d", GET_sBx(code) + i + 2);
			break;
		case OP_JCZ:
			prtreg(T, p, GET_A(code));
			printf(" %s %d", GET_xC(code) ? "true" : "false", GET_sBx(code) + i + 2);
			break;
		case OP_LDN:
			prtreg(T, p, GET_A(code));
			printf(" %d", GET_B(code));
			break;
		case OP_LDC:
			prtreg(T, p, GET_A(code));
			putchar(' ');
			prtkst(T, p, GET_Bx(code));
			break;
		case OP_LDP:
			prtreg(T, p, GET_A(code));
			aproto_t* proto = p->children[GET_Bx(code)];
			if (*proto->name->array) {
				printf(" %s", proto->name->array);
			}
			else {
				printf(" <function:%p>", proto);
			}
			break;
		case OP_SELV:
			switch (GET_xB(code)) {
			case 0: case 1:
				prtreg(T, p, GET_A(code));
				putchar(' ');
				prtrk(T, p, GET_B(code), GET_xB(code));
				break;
			case 2:
				prtreg(T, p, GET_A(code));
				printf(" %d", GET_B(code));
				break;
			case 3:
				prtreg(T, p, GET_A(code));
				printf(" #");
				break;
			}
			break;
		case OP_NEWA: case OP_CAT:
			prtreg(T, p, GET_A(code));
			putchar(' ');
			prtrk(T, p, GET_B(code), GET_xB(code));
			printf(" %d", GET_C(code));
			break;
		case OP_NEWL: case OP_NEWM:
			prtreg(T, p, GET_A(code));
			printf(" %d", GET_Bx(code));
			break;
		case OP_UNBOX:
			prtreg(T, p, GET_A(code));
			putchar(' ');
			prtrk(T, p, GET_B(code), GET_xB(code));
			printf(" %d", GET_C(code) - 1);
			break;
		case OP_EQ: case OP_LT: case OP_LE:
			prtrk(T, p, GET_A(code), GET_xA(code));
			putchar(' ');
			prtrk(T, p, GET_B(code), GET_xB(code));
			printf(" %s", GET_xC(code) ? "true" : "false");
			break;
		case OP_GET: case OP_SET: case OP_REM: case OP_SELF:
			prtrk(T, p, GET_A(code), GET_xA(code));
			putchar(' ');
			prtrk(T, p, GET_B(code), GET_xB(code));
			putchar(' ');
			prtrk(T, p, GET_C(code), GET_xC(code));
			break;
		case OP_ADD ... OP_BXOR:
			prtreg(T, p, GET_A(code));
			putchar(' ');
			prtrk(T, p, GET_B(code), GET_xB(code));
			putchar(' ');
			prtrk(T, p, GET_C(code), GET_xC(code));
			break;
		case OP_CALL:
			prtreg(T, p, GET_A(code));
			printf(" %d %d", GET_B(code) - 1, GET_C(code) - 1);
			break;
		case OP_TCALL:
			prtreg(T, p, GET_A(code));
			printf(" %d", GET_B(code) - 1);
			break;
		case OP_ICALL:
			prtreg(T, p, GET_A(code));
			printf(" %d", GET_C(code) - 1);
			break;
		case OP_RET:
			prtreg(T, p, GET_A(code));
			printf(" %d", GET_B(code) - 1);
			break;
		}
		printf("\n");
	}
	printf("\n");
	for (int i = 0; i < p->nchild; ++i) {
		dump(T, p->children[i]);
	}
}

#define MASK_SRC	0x0001
#define MASK_DEST	0x0002
#define MASK_NAME	0x0004

#define MASK_INFO	0x0100

static astr name = "<main>";
static astr src = NULL;
static astr dest = "out.aloc";

static unsigned mask = 0;

static int tmain(astate T) {
	int flag = aloL_compilef(T, name, src);
	if (flag == ThreadStateRun) {
		if (mask & MASK_INFO) {
			dump(T, tgetclo(T->top - 1)->a.proto);
		}
		else {
			if (aloL_savef(T, -1, dest, true) != ThreadStateRun) {
				fprintf(stderr, "fail to save to file %s", dest);
				return 0;
			}
		}
	}
	else {
		fprintf(stderr, "%s\n", alo_tostring(T, -1));
	}
	return 0;
}

#define checkmask(m,msg) if ((m) & mask) { fputs(msg"\n", stderr); return false; } mask |= (m)

static int parsearg(astate T, int argc, const astr argv[]) {
	for (int i = 1; i < argc; ++i) {
		astr s = argv[i];
		if (s[0] == '-') {
			if (strcmp(s + 1, "o") == 0) {
				checkmask(MASK_DEST, "Destination already settled.");
				if (++i < argc) {
					fputs("Missing file path after '-o'.\n", stderr);
					return false;
				}
				dest = argv[i];
			}
			else if (strcmp(s + 1, "n") == 0) {
				checkmask(MASK_NAME, "Function name already settled.");
			}
			else if (strcmp(s + 1, "i") == 0) {
				checkmask(MASK_INFO, "Information mode already settled.");
			}
			else if (strcmp(s + 1, "I") == 0) {
				checkmask(MASK_INFO, "Information mode already settled.");
				detail = true;
			}
			else {
				fprintf(stderr, "Unknown option got: '%s'\n", s);
				return false;
			}
		}
		else {
			checkmask(MASK_SRC, "Source already settled.");
			src = s;
		}
	}
	if (src == NULL) {
		fputs("No file input.\n", stderr);
		return false;
	}
	return true;
}

int main(int argc, astr argv[]) {
	astate T = aloL_newstate();
	if (T == NULL) {
		fputs("fail to initialize VM.\n", stderr);
		return EXIT_FAILURE;
	}
	if (!parsearg(T, argc, argv)) {
		return EXIT_FAILURE;
	}
	alo_pushlightcfunction(T, tmain);
	if (alo_pcall(T, 0, 0)) {
		fprintf(stderr, "%s\n", alo_tostring(T, -1));
	}
	alo_deletestate(T);
	return EXIT_SUCCESS;
}
