/*
 * atest.c
 *
 *  Created on: 2019年8月19日
 *      Author: ueyudiud
 */

#include "acfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

void aassert(const char* msg, const char* src, int ln) {
	fprintf(stderr, "%s %s:%d\n", msg, src, ln);
	exit(1);
}

#include "aall.h"

struct lstr {
	const char* src;
	size_t len;
};

#define lstr_c(s) ((struct lstr) { ""s, sizeof(s) - 1 })

int read(astate T, void* context, const char** pdest, size_t* psize) {
	struct lstr* l = (struct lstr*) context;
	*psize = l->len;
	*pdest = l->src;
	l->src = NULL;
	l->len = 0;
	return 0;
}

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
		putchar('\"');
		aloO_escape(T, fwriter, stdout, s->array, aloS_len(s));
		putchar('\"');
		break;
	}
	}
}

static void prtreg(astate T, aproto_t* p, int index) {
	if (aloK_iscapture(index)) {
		printf("c%d", aloK_getcapture(index));
	}
	else {
		printf("r%d", aloK_getstack(index));
	}
}

static void prtrk(astate T, aproto_t* p, int index, abyte mode) {
	(mode ? prtkst : prtreg)(T, p, index);
}

static void dump(astate T, aproto_t* p) {
	printf("%s (%s:%d,%d) %"PRId64" instructions at %p\n",
			p->name->array, p->src->array, p->linefdef, p->lineldef, p->ncode, p);
	printf("%d%s params, %d stack, %d captures, %d local, %"PRId64" constants, %"PRId64" functions\n",
			p->nargs, p->fvararg ? "+" : "", p->nstack, p->ncap, p->nlocvar, p->nconst, p->nchild);
	alineinfo_t* info = p->lineinfo;
	int m = -1;
	for (int i = 0; i < p->ncode; ++i) {
		if (m + 1 < p->nlineinfo && i >= info->begin) {
			printf("l%d:\n", (info++)->line);
			m++;
		}
		ainsn_t code = p->code[i];
		int insn = GET_i(code);
		printf("\t%-5d %5s ", i + 1, aloK_opname[insn]);
		switch (insn) {
		case OP_MOV:
		case OP_PNM ... OP_BNOT:
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
			if (proto->name == T->g->sempty) {
				printf(" <function:%p>", proto);
			}
			else {
				printf(" %s", proto->name->array);
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
		case OP_NEWA:
			prtreg(T, p, GET_A(code));
			putchar(' ');
			prtrk(T, p, GET_B(code), GET_xB(code));
			printf(" %d", GET_C(code));
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

static int tmain(astate T) {
	struct lstr a = lstr_c("println(a[2] + b)");
	if (alo_compile(T, "run", "<tmain>", read, &a) == ThreadStateRun) {
		dump(T, tgetclo(T->top - 1)->a.proto);
//		alo_call(T, 0, 0);
	}
	else {
		fprintf(stderr, "%s\n", alo_tostring(T, -1));
	}
	return 0;
}

int main(int argc, astr argv[]) {
	astate T = aloL_newstate();
	aloL_openlibs(T);
	alo_bind(T, "main", tmain);
	alo_pushlightcfunction(T, tmain);
	if (alo_pcall(T, 0, 0)) {
		fprintf(stderr, "%s\n", alo_tostring(T, 0));
	}
	alo_deletestate(T);
	return 0;
}
