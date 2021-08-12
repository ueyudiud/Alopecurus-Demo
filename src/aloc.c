/*
 * aloc.c
 *
 *  Created on: 2019年8月19日
 *      Author: ueyudiud
 */

#define ALOC_C_
#define ALO_CORE

#include "acfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "aall.h"

static int fwriter(__attribute__((unused)) alo_State T, void* buf, const void* src, size_t len) {
	fwrite(src, 1, len, aloE_cast(FILE*, buf));
	return 0;
}

static alo_Proto* proto = NULL;

static void prtkst(int index, __attribute__((unused)) int off) {
	if (index > (int) proto->nconst) {
		printf("<error>");
		return;
	}
	const alo_TVal* o = proto->consts + index;
	switch (ttpnv(o)) {
	case ALO_TNIL  : printf("nil"); break;
	case ALO_TBOOL : printf("%s", tasbool(o) ? "true" : "false"); break;
	case ALO_TINT  : printf(ALO_INT_FORMAT, tasint(o)); break;
	case ALO_TFLOAT: printf(ALO_FLT_FORMAT, tasflt(o)); break;
	case ALO_TSTR: {
		alo_Str* s = tasstr(o);
		alo_format(NULL, fwriter, stdout, "\"%\"\"", s->array, aloS_len(s));
		break;
	}
	}
}

static void prtreg(int index, int off) {
	if (aloK_iscapture(index)) {
		index = aloK_getcapture(index);
		if (index < proto->ncap && proto->captures[index].name) {
			printf("#%s", proto->captures[index].name->array);
		}
		else {
			printf("#%d", index);
		}
	}
	else {
		index = aloK_getstack(index);
		/* find register name */
		off++;
		for (int i = 0; i < proto->nlocvar; ++i) {
			alo_LocVarInfo* var = &proto->locvars[i];
			if (off >= var->start && off <= var->end && var->index == index) {
				printf("%%%s", var->name->array);
				return;
			}
		}
		/* or use register index instead */
		printf("%%%d", index);
	}
}

#define prtrk(index,off,mode) ((mode) ? prtkst : prtreg)(index, off)

static int detail = false;

#define nameof(name) (*(name)->array ? (name)->array : "<anonymous>")

static void dump(alo_State T, alo_Proto* p) {
	proto = p;
	printf("%s (%s:%d,%d) %d instructions at %p\n",
			nameof(p->name), p->src->array, p->linefdef, p->lineldef, p->ncode, p);
	printf("%d%s params, %d stack, %d captures, %d local, %d constants, %d functions\n",
			p->nargs, p->fvararg ? "+" : "", p->nstack, p->ncap, p->nlocvar, p->nconst, p->nchild);
	if (detail) {
		printf("constants (%d) for %p:\n", p->nconst, p);
		for (int i = 0; i < (int) p->nconst; ++i) {
			printf("\t[%d] ", i);
			prtkst(i, 0);
			printf("\n");
		}
		printf("captures (%d) for %p:\n", p->ncap, p);
		for (int i = 0; i < p->ncap; ++i) {
			alo_CapInfo* info = p->captures + i;
			printf("\t[%d]\t%s\t%s%d\n", i + 1, nameof(info->name), info->finstack ? "%" : "#", info->index);
		}
		printf("local (%d) for %p:\n", p->nlocvar, p);
		for (int i = 0; i < p->nlocvar; ++i) {
			alo_LocVarInfo* info = p->locvars + i;
			printf("\t[%d]\t%s\n", info->index, nameof(info->name));
		}
	}
	alo_LineInfo* info = p->lineinfo;
	int m = -1;
	for (int i = 0; i < p->ncode; ++i) {
		if (m + 1 < p->nlineinfo && i >= info->begin) {
			printf("line %d:\n", (info++)->line);
			m++;
		}
		a_insn code = p->code[i];
		int insn = GET_i(code);
		printf("\t%-5d %5s ", i + 1, aloP_opname[insn]);
		switch (insn) {
		case OP_MOV:
		case OP_PNM ... OP_BNOT: case OP_ITR:
		case OP_AADD ... OP_AXOR: case OP_NEW:
			prtreg(GET_A(code), i);
			putchar(' ');
			prtrk(GET_B(code), i, GET_xB(code));
			break;
		case OP_JMP:
			if (GET_xA(code)) {
				prtreg(GET_A(code), i);
				printf(" %d", GET_sBx(code) + i + 2);
			}
			else {
				printf("%d", GET_sBx(code) + i + 2);
			}
			break;
		case OP_JCZ:
			prtreg(GET_A(code), i);
			printf(" %s %d", GET_xC(code) ? "true" : "false", GET_sBx(code) + i + 2);
			break;
		case OP_LDN:
			prtreg(GET_A(code), i);
			printf(" %d", GET_B(code));
			break;
		case OP_LDC:
			prtreg(GET_A(code), i);
			putchar(' ');
			prtkst(GET_Bx(code), i);
			break;
		case OP_LDP:
			prtreg(GET_A(code), i);
			alo_Proto* proto = p->children[GET_Bx(code)];
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
				prtreg(GET_A(code), i);
				putchar(' ');
				prtrk(GET_B(code), i, GET_xB(code));
				break;
			case 2:
				prtreg(GET_A(code), i);
				printf(" %d", GET_B(code));
				break;
			case 3:
				prtreg(GET_A(code), i);
				printf(" #");
				break;
			}
			break;
		case OP_NEWA: case OP_CAT:
			prtreg(GET_A(code), i);
			putchar(' ');
			prtrk(GET_B(code), i, GET_xB(code));
			printf(" %d", GET_C(code));
			break;
		case OP_NEWL: case OP_NEWM:
			prtreg(GET_A(code), i);
			printf(" %d", GET_Bx(code));
			break;
		case OP_UNBOX:
			prtreg(GET_A(code), i);
			putchar(' ');
			prtrk(GET_B(code), i, GET_xB(code));
			printf(" %d", GET_C(code) - 1);
			break;
		case OP_EQ: case OP_LT: case OP_LE:
			prtrk(GET_A(code), i, GET_xA(code));
			putchar(' ');
			prtrk(GET_B(code), i, GET_xB(code));
			printf(" %s", GET_xC(code) ? "true" : "false");
			break;
		case OP_GET: case OP_SET: case OP_REM: case OP_SELF:
			prtrk(GET_A(code), i, GET_xA(code));
			putchar(' ');
			prtrk(GET_B(code), i, GET_xB(code));
			putchar(' ');
			prtrk(GET_C(code), i, GET_xC(code));
			break;
		case OP_ADD ... OP_BXOR:
			prtreg(GET_A(code), i);
			putchar(' ');
			prtrk(GET_B(code), i, GET_xB(code));
			putchar(' ');
			prtrk(GET_C(code), i, GET_xC(code));
			break;
		case OP_CALL:
			prtreg(GET_A(code), i);
			printf(" %d %d", GET_B(code) - 1, GET_C(code) - 1);
			break;
		case OP_TCALL:
			prtreg(GET_A(code), i);
			printf(" %d", GET_B(code) - 1);
			break;
		case OP_ICALL:
			prtreg(GET_A(code), i);
			printf(" %d", GET_C(code) - 1);
			break;
		case OP_RET:
			prtreg(GET_A(code), i);
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

static a_cstr name = "<main>";
static a_cstr src = NULL;
static a_cstr dest = "out.aloc";

static unsigned mask = 0;

static int tmain(alo_State T) {
	int flag = aloL_compilef(T, name, src);
	if (flag == ALO_STOK) {
		if (mask & MASK_INFO) {
			dump(T, tasclo(T->top - 1)->a.proto);
		}
		else {
			if (aloL_savef(T, dest, true) != ALO_STOK) {
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

static int parsearg(int argc, const a_cstr argv[]) {
	for (int i = 1; i < argc; ++i) {
		a_cstr s = argv[i];
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

int main(int argc, a_cstr argv[]) {
	alo_State T = aloL_newstate();
	if (T == NULL) {
		fputs("fail to initialize VM.\n", stderr);
		return EXIT_FAILURE;
	}
	if (!parsearg(argc, argv)) {
		return EXIT_FAILURE;
	}
	alo_pushlightcfunction(T, tmain);
	if (alo_pcall(T, 0, 0, ALO_NOERRFUN)) {
		fprintf(stderr, "%s\n", alo_tostring(T, -1));
	}
	alo_deletestate(T);
	return EXIT_SUCCESS;
}
