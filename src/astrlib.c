/*
 * astrlib.c
 *
 * library for string.
 *
 *  Created on: 2019年7月31日
 *      Author: ueyudiud
 */

#define ASTRLIB_C_
#define ALO_LIB

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/**
 ** max buffer length in stack.
 */
#define MAX_STACKBUF_LEN 256

#define l_strcpy(d,s,l) aloE_void(memcpy(d, s, (l) * sizeof(char)))
#define l_strchr(s,c,l) aloE_cast(const char*, memchr(s, c, (l) * sizeof(char)))

/**
 ** reverse string.
 ** prototype: string.reverse(self)
 */
static int str_reverse(astate T) {
	size_t len;
	const char* src = aloL_checklstring(T, 0, &len);
	if (len < MAX_STACKBUF_LEN) { /* for short string, use heap memory */
		char buf[len];
		for (size_t i = 0; i < len; ++i) {
			buf[i] = src[len - 1 - i];
		}
		alo_pushlstring(T, buf, len);
	}
	else { /* or use memory buffer instead */
		aloL_usebuf(T, buf) {
			aloL_bcheck(T, buf, len * sizeof(char));
			for (size_t i = 0; i < len; ++i) {
				aloL_bsetc(buf, i, src[len - 1 - i]);
			}
			aloL_blen(buf) = len * sizeof(char);
			aloL_bpushstring(T, buf);
		}
	}
	return 1;
}

/**
 ** repeat string with number of times.
 ** prototype: string.repeat(self, times)
 */
static int str_repeat(astate T) {
	size_t len;
	const char* src = aloL_checklstring(T, 0, &len);
	size_t time = aloL_checkinteger(T, 1);
	if (time >= ALO_INT_PROP(MAX) / len) {
		aloL_error(T, "repeat time overflow.");
	}
	const size_t nlen = len * time;
	if (nlen == 0) {
		alo_pushstring(T, "");
	}
	else if (nlen <= MAX_STACKBUF_LEN) { /* for short string, use buffer on stack */
		char buf[nlen];
		char* p = buf;
		for (size_t i = 0; i < time; ++i) {
			l_strcpy(p, src, len);
			p += len;
		}
		alo_pushlstring(T, buf, nlen);
	}
	else { /* for long string, use buffer on heap */
		aloL_usebuf(T, buf) {
			aloL_bcheck(T, buf, nlen * sizeof(char));
			abyte* p = aloL_braw(buf);
			for (size_t i = 0; i < time; ++i) {
				memcpy(p, src, len);
				p += len * sizeof(char);
			}
			aloL_blen(buf) = nlen * sizeof(char);
			aloL_bpushstring(T, buf);
		}
	}
	return 1;
}

/**
 ** trim white space in the front and tail of string.
 ** prototype: string.trim(self)
 */
static int str_trim(astate T) {
	size_t len;
	const char* src = aloL_checklstring(T, 0, &len);
	size_t i, j;
	for (i = 0; i < len && isspace(aloE_byte(src[i])); ++i);
	if (i == len) {
		alo_pushstring(T, "");
		return 1;
	}
	for (j = len; j > 0 && isspace(aloE_byte(src[j - 1])); --j);
	if (i == 0 && j == len) {
		alo_settop(T, 1);
	}
	else {
		len = j - i;
		if (len <= MAX_STACKBUF_LEN) {
			char buf[len];
			l_strcpy(buf, src + i, len);
			alo_pushlstring(T, buf, len);
		}
		else aloL_usebuf(T, buf) {
			aloL_bcheck(T, buf, len * sizeof(char));
			aloL_bputls(T, buf, src + i, len);
			aloL_bpushstring(T, buf);
		}
	}
	return 1;
}

/**
 ** transform character to byte value.
 ** 0 is the default value for index.
 ** prototype: string.byte(self, index)
 */
static int str_byte(astate T) {
	size_t l;
	const char* s = aloL_checklstring(T, 0, &l);
	aint i = aloL_getoptinteger(T, 1, 0);
	if (i >= 0) {
		if (aloE_cast(aint, l) < i) {
			goto error;
		}
	}
	else {
		i += l;
		if (i < 0) {
			goto error;
		}
	}
	alo_pushinteger(T, aloE_byte(s[i]));
	return 1;

	error:
	aloL_error(T, "index out of bound");
	return 0;
}

/**
 ** transform byte index into string.
 ** prototype: string.char(chars...)
 */
static int str_char(astate T) {
	ssize_t n = alo_gettop(T);
	aloL_usebuf(T, buf) {
		aloL_bcheck(T, buf, n);
		for (ssize_t i = 0; i < n; ++i) {
			aint ch = aloL_checkinteger(T, i);
			if (ch != aloE_byte(ch)) {
				aloL_argerror(T, i, "integer can not cast to character");
			}
			aloL_bsetc(buf, i, ch);
		}
		aloL_blen(buf) = n;
		aloL_bpushstring(T, buf);
	}
	return 1;
}

/**================================================================================
 **                    regular expression support start
 **================================================================================*/

/* max match group number */
#define MAX_GROUP_SIZE 64

#define SIZE_i 4
#define SIZE_A 14
#define SIZE_B 14
#define SIZE_Ax (SIZE_A + SIZE_B)

#define POS_i 0
#define POS_A (POS_i + SIZE_i)
#define POS_B (POS_A + SIZE_A)
#define POS_Ax POS_A

#define GetVal(t,x) (((x) >> POS##t) & ((1ULL << SIZE##t) - 1))
#define MaskVal(t,x) (((x) & ((1ULL << SIZE##t) - 1)) << POS##t)
#define SetVal(s,t,x) ((s) = ((s) & ~(((1ULL << SIZE##t) - 1) << POS##t)) | MaskVal(t, x))

#define u2s(x) ({ uint32_t $x = (x); ($x & (1ULL << (SIZE_A - 1))) ? aloE_cast(int32_t, $x) - (1LL << SIZE_A) : aloE_cast(int32_t, $x); })
#define s2u(x) ({ typeof(x) $x = (x); $x < 0 ? (1ULL << SIZE_A) + aloE_cast(uint32_t, $x) : aloE_cast(uint32_t, $x); })

#define geti(x)  GetVal(_i, x)
#define getA(x)  GetVal(_A, x)
#define getB(x)  GetVal(_B, x)
#define getAx(x) GetVal(_Ax, x)

#define seti(s,x)  SetVal(s, _i, x)
#define setA(s,x)  SetVal(s, _A, x)
#define setB(s,x)  SetVal(s, _B, x)
#define setAx(s,x) SetVal(s, _Ax, x)

#define maski(x)  MaskVal(_i, x)
#define maskA(x)  MaskVal(_A, x)
#define maskB(x)  MaskVal(_B, x)
#define maskAx(x) MaskVal(_Ax, x)

#define createi(i) maski(i)
#define createiAx(i,Ax) (maski(i) | maskAx(Ax))
#define createisAsB(i,A,B) (maski(i) | maskA(s2u(A)) | maskB(s2u(B)))

/* matching group */
typedef struct alo_MatchGroup {
	astr begin, end; /* begin and end of matched string */
} amgroup_t;

enum {
	MOP_NONE, /*     | do nothing */
	MOP_RET,  /*     | return matching result */
	MOP_PUSH, /* Ax  | push matching stack */
	MOP_POP,  /* Ax  | pop matching stack */
	MOP_TSTB, /*     | test begin of line */
	MOP_TSTE, /*     | test end of line */
	MOP_TSTC, /* Ax  | test a character */
	MOP_TSTS, /* Ax  | test by character set */
	MOP_TSTX, /* Ax  | test by negative character set */
	MOP_TSTQ, /* Ax  | test character sequence */
	MOP_SPT,  /* A B | split into two pattern, push B into jump stack then jump to A */
	MOP_JMP,  /* A _ | jump to A */
};

/* match state */
typedef struct alo_MatchState {
	astate T;
	astr sbegin, send; /* begin and end of source */
	amgroup_t* groups; /* group buffer */
	ambuf_t js; /* jump stack */
} amstat_t;

typedef struct alo_MatchLabel {
	uint32_t* to;
	astr s;
} amlabel_t;

typedef struct alo_MatchFun {
	uint32_t* codes;
	char* seq;
	size_t len;
	int ngroup;
	int ncode;
} amfun_t;

/* end of string */
#define EOS (-1)

#define sgetc(M,s) ((M)->send == (s) ? EOS : *((s)++))

#define pushlabel(M) ({ \
		aloL_bcheck((M)->T, &(M)->js, sizeof(amlabel_t) * 4); \
		amlabel_t* $label = aloE_cast(amlabel_t*, aloL_braw(&(M)->js) + aloL_blen(&(M)->js)); \
		aloL_blen(&(M)->js) += sizeof(amlabel_t); \
		$label; })

#define poplabel(M) \
	(aloL_blen(&(M)->js) -= sizeof(amlabel_t), aloE_cast(amlabel_t*, aloL_braw(&(M)->js) + aloL_blen(&(M)->js)))

static int match_class(int ch, int cl) {
	int res;
	switch (cl | ('a' ^ 'A')) {
	case 'a': res = isalpha(ch); break;
	case 'c': res = iscntrl(ch); break;
	case 'd': res = isdigit(ch); break;
	case 'g': res = isgraph(ch); break;
	case 'l': res = islower(ch); break;
	case 'p': res = ispunct(ch); break;
	case 's': res = isspace(ch); break;
	case 'u': res = isupper(ch); break;
	case 'w': res = isalnum(ch); break;
	case 'x': res = isxdigit(ch); break;
	default : return ch == cl;
	}
	return ((cl) & ~('a' ^ 'A') ? res : !res);
}

static int match(amstat_t* M, amfun_t* f, const char* start, int full) {
	alo_pushbuf(M->T, &M->js);
	uint32_t* pc = f->codes;
	const char* s = start;
	mcheck:
	switch (geti(*pc)) {
	case MOP_NONE:
		pc++;
		goto mcheck;
	case MOP_RET:
		if (full && s != M->send)
			goto mthrow;
		alo_popbuf(M->T, &M->js);
		return true;
	case MOP_PUSH: {
		M->groups[getAx(*pc)].begin = s;
		pc++;
		goto mcheck;
	}
	case MOP_POP: {
		M->groups[getAx(*pc)].end = s;
		pc++;
		goto mcheck;
	}
	case MOP_TSTB:
		if (s == M->sbegin || s[-1] == '\r' || s[-1] == '\n') {
			pc++;
			goto mcheck;
		}
		else
			goto mthrow;
	case MOP_TSTE: {
		if (s == M->send || s[0] == '\r' || s[0] == '\n') {
			pc++;
			goto mcheck;
		}
		else
			goto mthrow;
	}
	case MOP_TSTC: {
		if (*(s++) != aloE_byte(getAx(*pc)))
			goto mthrow;
		pc++;
		goto mcheck;
	}
	case MOP_TSTS: case MOP_TSTX: {
		int mode = geti(*pc) == MOP_TSTS;
		astr p = f->seq + getAx(*pc);
		int ch = sgetc(M, s);
		if (ch != EOS) {
			while (*p) {
				if (*p == '\\') {
					if (match_class(*(p + 1), ch) == mode) {
						pc++;
						goto mcheck;
					}
					p += 1;
				}
				if (*(p + 1) == '-') {
					if ((*p <= ch && ch <= *(p + 2)) == mode) {
						pc++;
						goto mcheck;
					}
					p += 3;
				}
				else {
					if ((*p == ch) != mode) {
						pc++;
						goto mcheck;
					}
					p += 1;
				}
			}
		}
		goto mthrow;
	}
	case MOP_TSTQ: {
		astr p = f->seq + getAx(*pc);
		while (true) {
			switch (*p) {
			case '\0': {
				pc++;
				goto mcheck; /* end of string */
			}
			case '\\': {
				if (!match_class(sgetc(M, s), *(p + 1))) {
					goto mthrow;
				}
				p += 2;
				break;
			}
			default: {
				if (sgetc(M, s) != *p) {
					goto mthrow;
				}
				p += 1;
				break;
			}
			}
		}
		/* unreachable code */
		return false;
	}
	case MOP_SPT: {
		amlabel_t* label = pushlabel(M);
		label->s = s;
		label->to = pc + u2s(getB(*pc));
		pc += u2s(getA(*pc));
		goto mcheck;
	}
	case MOP_JMP: {
		pc += u2s(getA(*pc));
		goto mcheck;
	}
	default:
		aloE_assert(false, "");
	}

	mthrow:
	if (M->js.len > 0) {
		/* recover through match stack */
		amlabel_t* label = poplabel(M);
		pc = label->to;
		s = label->s;
		goto mcheck;
	}
	/* not recoverable, match failed */
	alo_popbuf(M->T, &M->js);
	return false;
}

enum {
	T_END, /* end of pattern */
	T_BOL, /* begin of line */
	T_EOL, /* end of line */
	T_CHR, /* single character, data = char in UTF-32 */
	T_STR, /* string, data = pattern length, str = pattern, extra = group index */
	T_SET, /* set, extra = negative set, data = pattern length, str = set  */
	T_SUB, /* union of pattern, extra = group index, data = union node length, arr = union nodes */
	T_SEQ, /* pattern sequence, data = structure node length, arr = children nodes */
};

enum {
	M_NORMAL= 0x0, /* S -> a */
	M_MORE	= 0x1, /* S -> aS */
	M_MOREG	= 0x9, /* S -> aS (greedy mode) */
	M_ANY	= 0x2, /* S -> aS | epsilon */
	M_ANYG	= 0xA, /* S -> aS | epsilon (greedy mode) */
	M_OPT	= 0x3, /* S -> a | epsilon */
	M_OPTG	= 0xB  /* S -> a | epsilon (greedy mode) */
};

/* compile state */
typedef struct alo_CompileState {
	astate T;
	amfun_t* f;
	aalloc alloc;
	void* context;
	size_t ncs; /* number of char sequence */
	astr pos; /* current position */
	astr end; /* end of source */
	int index; /* group index */
	int ncode; /* number of code */
	struct {
		size_t length;
		char array[256];
	}; /* string buffer */
} acstat_t;

static void minit(amstat_t* M, astate T, astr src, size_t len, amgroup_t* groups) {
	M->T = T;
	M->sbegin = src;
	M->send = src + len;
	M->groups = groups;
}

#define cerror(C,msg,args...) aloL_error((C)->T, msg, ##args)

static int c_insn(acstat_t* C) {
	if (C->ncode == C->f->ncode) {
		size_t newlen = C->f->ncode * 2;
		if (newlen < 16) {
			newlen = 16;
		}
		void* newbuf = C->alloc(C->context, C->f->codes, C->f->ncode * sizeof(uint32_t), newlen * sizeof(uint32_t));
		if (newbuf == NULL)
			cerror(C, "no enough memory.");
		C->f->codes = newbuf;
		C->f->ncode = newlen;
	}
	return C->ncode++;
}

static void c_erase(acstat_t* C, int id) {
	uint32_t* p = C->f->codes + id + 1;
	uint32_t* const e = C->f->codes + C->ncode;
	while (p < e) {
		*(p - 1) = *p;
		p++;
	}
	C->ncode--;
}

static uint32_t c_str(acstat_t* C) {
	int length = C->length;
	C->length = 0;
	if (length + C->ncs > C->f->len) {
		size_t newlen = C->f->len * 2;
		if (newlen < sizeof(C->array) / sizeof(char)) {
			newlen = sizeof(C->array) / sizeof(char); /* at lease full fill the array */
		}
		void* newseq = C->alloc(C->context, C->f->seq, C->f->len * sizeof(char), newlen * sizeof(char));
		if (newseq == NULL)
			cerror(C, "no enough memory.");
		C->f->seq = newseq;
		C->f->len = newlen;
	}
	uint32_t off = C->ncs;
	char* s = C->f->seq + off;
	l_strcpy(s, C->array, length);
	s[length] = '\0';
	C->ncs += length + 1;
	return off;
}

static void cput(acstat_t* C, int ch) {
	if (C->length >= 256) {
		cerror(C, "pattern too long.");
	}
	C->array[C->length++] = ch;
}

/* character used in class */
#define CLASS_CHAR "acdglpsuwx"

/* character used for modifier */
#define MODIFIER_CHAR "+*?"

#define c_setinsn(C,id,insn) aloE_void((C)->f->codes[id] = (insn))
#define c_putinsn(C,insn) ({ int $id = c_insn(C); c_setinsn(C, $id, insn); })
#define c_putsinsn(C,op) c_putinsn(C, createiAx(op, c_str(C)))

static void p_str(acstat_t* C) {
	int flag = (*C->pos == '\\') && strchr(CLASS_CHAR, *(C->pos + 1));
	next:
	switch (*C->pos) {
	case '+': case '*': case '?':
		aloE_assert(false, "unexpected character.");
		break;
	case ']':
		cerror(C, "illegal character.");
		break;
	case '(': case ')': case '[':
	case '^': case '$': case '|':
		break;
	case '\0': {
		if (C->pos == C->end) {
			break;
		}
		goto single;
	}
	case '\\': {
		if (C->pos + 1 == C->end) {
			cerror(C, "illegal escape character.");
		}
		if (C->length > 0) {
			switch (C->pos[2]) {
			case '+': case '*': case '?':
				goto end; /* modifier found, skip */
			}
		}
		cput(C, '\\');
		cput(C, C->pos[1]);
		C->pos += 2;
		goto next;
	}
	single: default: {
		if (C->length > 0) {
			switch (C->pos[1]) {
			case '+': case '*': case '?':
				goto end; /* modifier found, skip */
			}
		}
		cput(C, *C->pos);
		C->pos += 1;
		goto next;
	}
	}
	end:
	if (flag || C->length > 1) {
		c_putsinsn(C, MOP_TSTQ);
	}
	else {
		c_putinsn(C, createiAx(MOP_TSTC, aloE_byte(C->array[0])));
		C->length = 0;
	}
}

static void p_chset(acstat_t* C) {
	int type;
	if (*C->pos == '^') {
		type = MOP_TSTX;
		C->pos += 1;
	}
	else {
		type = MOP_TSTS;
	}
	while (*C->pos != ']') {
		if (*C->pos == '\\' && strchr(CLASS_CHAR, *(C->pos + 1))) { /* is class marker */
			cput(C, '\\');
			C->pos += 1;
		}
		if (C->pos == C->end) {
			cerror(C, "illegal pattern.");
		}
		cput(C, *C->pos);
		C->pos += 1;
		if (*C->pos == '-') {
			if (C->pos + 1 == C->end) {
				cerror(C, "illegal pattern.");
			}
			cput(C, '-');
			cput(C, *(C->pos + 1));
			C->pos += 2;
		}
	}
	c_putsinsn(C, type);
}

static void p_expr(acstat_t*);

static void p_atom(acstat_t* C) {
	/* atom -> '(' expr ')' | '[' chset ']' */
	switch (*C->pos) {
	case '(': {
		int index = C->index++;
		C->pos += 1;
		c_putinsn(C, createiAx(MOP_PUSH, index));
		p_expr(C);
		if (*(C->pos++) != ')') {
			cerror(C, "illegal pattern.");
		}
		c_putinsn(C, createiAx(MOP_POP, index));
		break;
	}
	case '[': {
		C->pos += 1;
		p_chset(C);
		C->pos += 1;
		break;
	}
	case '^': {
		c_putinsn(C, createi(MOP_TSTB));
		C->pos += 1;
		break;
	}
	case '$': {
		c_putinsn(C, createi(MOP_TSTE));
		C->pos += 1;
		break;
	}
	default: {
		p_str(C);
		break;
	}
	}
}

static void p_suffix(acstat_t* C) {
	/* suffix -> atom [ ('+'|'*'|'?'|'{' [int] ',' [int] '}') ['?'] ] */
	int id1 = c_insn(C), id2;
	p_atom(C);
	switch (*C->pos) {
	/* TODO: '{}' is not supported yet. */
//	case '{': {
//		int min = 0, max = INT32_MAX;
//		switch (*C->pos) {
//		case '0' ... '9': {
//			errno = 0;
//			min = strtol(C->pos, aloE_cast(char**, &C->pos), 10);
//			if (errno)
//				cerror(C, "illegal range size.");
//			if (*C->pos != ',') {
//				max = min;
//				goto merge;
//			}
//			break;
//		}
//		}
//		if (*(C->pos++) != ',')
//			cerror(C, "illegal pattern.");
//		switch (*C->pos) {
//		case '0' ... '9': {
//			errno = 0;
//			max = strtol(C->pos, aloE_cast(char**, &C->pos), 10);
//			if (errno || min > max)
//				cerror(C, "illegal range size.");
//			break;
//		}
//		}
//		merge:
//		if (*(C->pos++) != '}')
//			cerror(C, "illegal pattern.");
//		int greedy = *C->pos == '?'? (C->pos++, false) : true;
//		if (max == min) {
//			switch (min) {
//			case 0: /* 0 length of pattern? */
//				C->ncode = id1;
//				break;
//			case 1: /* itself? */
//				c_erase(C, id1);
//				break;
//			default:
//
//			}
//		}
//		break;
//	}
	case '?': {
		if (*(++C->pos) == '?') {
			C->pos++;
			c_setinsn(C, id1, createisAsB(MOP_SPT, C->ncode - id1, 1));
		}
		else {
			c_setinsn(C, id1, createisAsB(MOP_SPT, 1, C->ncode - id1));
		}
		return;
	}
	case '+': {
		c_erase(C, id1);
		id2 = c_insn(C);
		if (*(++C->pos) == '?') {
			C->pos++;
			c_setinsn(C, id2, createisAsB(MOP_SPT, 1, id1 - id2));
		}
		else {
			c_setinsn(C, id2, createisAsB(MOP_SPT, id1 - id2, 1));
		}
		break;
	}
	case '*': {
		if (*(++C->pos) == '?') {
			C->pos++;
			id2 = c_insn(C);
			c_setinsn(C, id1, createisAsB(MOP_SPT, id2 + 1 - id1, 1));
			c_setinsn(C, id2, createisAsB(MOP_SPT, 1, id1 + 1 - id2));
		}
		else {
			id2 = c_insn(C);
			c_setinsn(C, id1, createisAsB(MOP_SPT, 1, id2 + 1 - id1));
			c_setinsn(C, id2, createisAsB(MOP_SPT, id1 + 1 - id2, 1));
		}
		return;
	}
	default: {
		c_erase(C, id1);
		break;
	}
	}
}

static void p_mono(acstat_t* C) {
	/* mono -> { suffix } */
	p_suffix(C);
	next:
	switch (*C->pos) {
	case '\0':
		if (C->pos == C->end) { /* end of string */
			break;
		}
		goto single;
	single: default:
		p_suffix(C);
		goto next;
	case ']': case '}':
		cerror(C, "illegal pattern.");
		break;
	case ')': case '|':
		break;
	}
}

static void p_expr(acstat_t* C) {
	/* expr -> mono { '|' mono } */
	int id1 = c_insn(C);
	p_mono(C);
	if (*C->pos == '|') {
		int id3, id4 = -1;
		do {
			id3 = c_insn(C);
			c_setinsn(C, id3, createisAsB(MOP_JMP, id4 == -1 ? 0 : id4 - id3, 0));
			id4 = id3;
			c_setinsn(C, id1, createisAsB(MOP_SPT, 1, C->ncode - id1));
			id1 = c_insn(C);
			C->pos++;
			p_mono(C);
		}
		while (*C->pos == '|');
		c_erase(C, id1);
		uint32_t* p = C->f->codes + id3;
		while (getA(*p)) {
			uint32_t* q = p + u2s(getA(*p));
			setA(*p, s2u(C->ncode - (p - C->f->codes)));
			p = q;
		}
		setA(*p, s2u(C->ncode - (p - C->f->codes)));
	}
	else {
		c_erase(C, id1);
	}
}

static void c_trim(acstat_t* C) {
	C->f->codes = C->alloc(C->context, C->f->codes, C->f->ncode * sizeof(uint32_t), C->ncode * sizeof(uint32_t));
	C->f->ncode = C->ncode;
	C->f->seq = C->alloc(C->context, C->f->seq, C->f->len * sizeof(char), C->ncs * sizeof(char));
	C->f->len = C->ncs;
}

static void compile(amfun_t* f, astate T, const char* src, size_t len) {
	acstat_t C;
	C.T = T;
	C.f = f;
	C.alloc = alo_getalloc(T, &C.context);
	C.length = 0;
	C.index = 1;
	C.pos = src;
	C.end = src + len;
	C.ncode = 0;
	C.ncs = 0;
	c_putinsn(&C, createiAx(MOP_PUSH, 0));
	p_expr(&C);
	if (C.end != C.pos) {
		cerror(&C, "illegal pattern.");
	}
	c_putinsn(&C, createiAx(MOP_POP, 0));
	c_putinsn(&C, createi(MOP_RET));
	f->ngroup = C.index;
	c_trim(&C);
}

static void destory(astate T, amfun_t* f) {
	aalloc alloc;
	void* context;
	alloc = alo_getalloc(T, &context);
	alloc(context, f->codes, f->ncode * sizeof(uint32_t), 0);
	alloc(context, f->seq, f->len * sizeof(char), 0);
}

static void initialize(amfun_t* f) {
	f->codes = NULL;
	f->ncode = 0;
	f->seq = NULL;
	f->len = 0;
	f->ngroup = 0;
}

static int str_match(astate T) {
	aloL_checkstring(T, 0);
	aloL_checkstring(T, 1);
	amfun_t f;
	initialize(&f);
	size_t len;
	const char* src = alo_tolstring(T, 0, &len);
	compile(&f, T, src, len);
	amstat_t M;
	amgroup_t groups[f.ngroup];
	src = alo_tolstring(T, 1, &len);
	minit(&M, T, src, len, groups);
	alo_pushboolean(T, match(&M, &f, src, true));
	destory(T, &f);
	return 1;
}

static int matcher_gc(astate T) {
	amfun_t* f = aloE_cast(amfun_t*, alo_torawdata(T, 0));
	if (f->codes) {
		destory(T, f);
		f->codes = NULL;
	}
	return 0;
}

static amfun_t* self(astate T) {
	aloL_checktype(T, 0, ALO_TRAWDATA);
	amfun_t* f = aloE_cast(amfun_t*, alo_torawdata(T, 0));
	if (f->codes == NULL) {
		aloL_error(T, "matcher already destroyed.");
	}
	return f;
}

static int matcher_test(astate T) {
	amfun_t* f = self(T);
	aloL_checkstring(T, 1);
	amstat_t M;
	amgroup_t groups[f->ngroup];
	const char* src;
	size_t len;
	src = alo_tolstring(T, 1, &len);
	minit(&M, T, src, len, groups);
	alo_pushboolean(T, match(&M, f, src, true));
	return 1;
}

static int matcher_match(astate T) {
	amfun_t* f = self(T);
	amstat_t M;
	amgroup_t groups[f->ngroup];
	const char* src;
	size_t len;
	src = alo_tolstring(T, 1, &len);
	minit(&M, T, src, len, groups);
	if (!match(&M, f, src, true)) {
		return 0;
	}
	alo_ensure(T, f->ngroup);
	for (int i = 0; i < f->ngroup; ++i) {
		amgroup_t* group = &groups[i];
		if (group->begin != NULL) {
			alo_pushlstring(T, group->begin, group->end - group->begin);
		}
		else {
			alo_pushnil(T);
		}
	}
	return f->ngroup;
}

static void aux_replaces(astate T, amfun_t* f, amstat_t* M, ambuf_t* buf) {
	size_t len;
	const char* s1 = alo_tolstring(T, 2, &len);
	const char* s0 = s1;
	const char* const se = s0 + len;
	while (s1 < se) {
		if (*s1 == '$') { /* escape character? */
			if (s1 != s0) {
				aloL_bputls(T, buf, s0, s1 - s0); /* put normal sub sequence before */
			}
			int index;
			s1 ++;
			int ch = *(s1++);
			switch (ch) {
			case '$':
				aloL_bputc(T, buf, '$');
				break;
			case '0' ... '9':
				index = ch - '0';
				goto format;
			case '{':
				index = 0;
				do {
					if ('0' <= *s1 && *s1 <= '9') {
						index = index * 10 + (*s1 - '0');
					}
					else {
						aloL_error(T, "illegal replacement string.");
					}
				}
				while (*(++s1) != '}');
				s1++;
			format:
				if (index >= f->ngroup) {
					aloL_error(T, "replacement index out of bound.");
				}
				amgroup_t* group = &M->groups[index];
				aloL_bputls(T, buf, group->begin, group->end - group->begin);
				s0 = s1;
				break;
			}
		}
		else {
			s1 += 1;
		}
	}
	if (s0 != s1) {
		aloL_bputls(T, buf, s0, s1 - s0);
	}
}

static void aux_replacef(astate T, __attribute__((unused)) amfun_t* f, amstat_t* M, ambuf_t* buf) {
	int index = alo_gettop(T);
	alo_push(T, 2); /* push transformer */
	alo_pushlstring(T, M->groups[0].begin, M->groups[0].end - M->groups[0].begin);
	alo_pushinteger(T, M->groups[0].begin - M->sbegin);
	alo_call(T, 2, 1);
	aloL_bwrite(T, buf, index);
	alo_drop(T);
}

static int matcher_replace(astate T) {
	amfun_t* f = self(T);
	void (*transformer)(astate, amfun_t*, amstat_t*, ambuf_t*);
	if (alo_typeid(T, 2) == ALO_TSTRING) {
		transformer = aux_replaces;
	}
	else {
		aloL_checkcall(T, 2);
		transformer = aux_replacef;
	}
	amstat_t M;
	amgroup_t groups[f->ngroup];
	const char* src;
	size_t len;
	src = aloL_checklstring(T, 1, &len);
	minit(&M, T, src, len, groups);
	alo_settop(T, 3);
	aloL_usebuf(T, buf) {
		const char* s1 = M.sbegin;
		const char* s0 = s1;
		while (s1 <= M.send) {
			if (match(&M, f, s1, false)) {
				if (s1 != s0) {
					aloL_bputls(T, buf, s0, s1 - s0);
				}
				(*transformer)(T, f, &M, buf);
				s1 = s0 = M.groups[0].end;
			}
			else {
				s1 += 1;
			}
		}
		if (s1 > M.send) {
			s1 = M.send;
		}
		if (s1 != s0) {
			aloL_bputls(T, buf, s0, s1 - s0);
		}
		aloL_bpushstring(T, buf);
	}
	return 1;
}

static int mmatch(amstat_t* M, amfun_t* nodes, const char* begin, const char* end) {
	for (const char* s = begin; s <= end; ++s) {
		if (match(M, nodes, s, false)) {
			return true;
		}
	}
	return false;
}

static int matcher_split(astate T) {
	amfun_t* f = self(T);
	size_t len;
	const char* src = aloL_checklstring(T, 1, &len);
	amstat_t M;
	amgroup_t groups[f->ngroup];
	minit(&M, T, src, len, groups);
	size_t n = 0;
	alo_settop(T, 2);
	alo_newlist(T, 0);
	if (mmatch(&M, f, M.sbegin, M.send)) {
		do
		{
			alo_pushlstring(T, M.sbegin, groups->begin - M.sbegin);
			alo_rawseti(T, 2, n++);
			M.sbegin = groups->end;
		}
		while (mmatch(&M, f, M.sbegin, M.send));
		alo_pushlstring(T, M.sbegin, M.send - M.sbegin);
	}
	else {
		alo_push(T, 1);
	}
	alo_rawseti(T, 2, n);
	return 1;
}

static const astr opnames[] = {
		[MOP_NONE] = "none",
		[MOP_RET] = "ret",
		[MOP_PUSH] = "push",
		[MOP_POP] = "pop",
		[MOP_TSTB] = "begin",
		[MOP_TSTE] = "end",
		[MOP_TSTC] = "char",
		[MOP_TSTS] = "set",
		[MOP_TSTX] = "xset",
		[MOP_TSTQ] = "str",
		[MOP_SPT] = "split",
		[MOP_JMP] = "jmp"
};

static void addcodes(astate T, ambuf_t* buf, amfun_t* f) {
	for (int i = 0; i < f->ncode; ++i) {
		uint32_t insn = f->codes[i];
		char s[64];
		int n = sprintf(s, "\n\t%3d %-5s ", i, opnames[geti(insn)]);
		aloL_bputls(T, buf, s, n);
		switch (geti(insn)) {
		case MOP_PUSH: case MOP_POP: {
			aloL_bputf(T, buf, "%d", aloE_cast(int, getAx(insn)));
			break;
		}
		case MOP_TSTC: {
			char ch = getAx(insn);
			aloL_bputf(T, buf, "'%\"'", &ch, 1);
			break;
		}
		case MOP_TSTS: case MOP_TSTX: case MOP_TSTQ: {
			aloL_bputf(T, buf, "\"%s\"", f->seq + getAx(insn));
			break;
		}
		case MOP_SPT: {
			aloL_bputf(T, buf, "%d %d",
					i + aloE_cast(int, u2s(getA(insn))),
					i + aloE_cast(int, u2s(getB(insn))));
			break;
		}
		case MOP_JMP: {
			aloL_bputf(T, buf, "%d",
					i + aloE_cast(int, u2s(getA(insn))));
			break;
		}
		}
	}
}

static int matcher_tostr(astate T) {
	amfun_t* f = self(T);
	aloL_usebuf(T, buf) {
		aloL_bputf(T, buf, "__matcher: { ngroup: %d, addr: %p, opcodes:", (int) f->ngroup, f);
		addcodes(T, buf, f);
		aloL_bputxs(T, buf, "\n}");
		aloL_bpushstring(T, buf);
	}
	return 1;
}

static int matcher_find(astate T) {
	const char* src;
	size_t len;
	src = aloL_checklstring(T, 1, &len);
	amfun_t* f = self(T);
	amstat_t M;
	amgroup_t groups[f->ngroup];
	aint off = aloL_getoptinteger(T, 2, 0);
	if (off < 0) {
		off += len;
	}
	if (off > aloE_cast(aint, len) || off < 0) {
		aloL_argerror(T, 2, "index out of bound.");
	}
	minit(&M, T, src, len, groups);
	if (mmatch(&M, f, src + off, M.send)) {
		alo_pushinteger(T, groups[0].begin - M.sbegin);
		alo_pushinteger(T, groups[0].end - M.sbegin);
		return 2;
	}
	return 0;
}

static amfun_t* preload(astate T) {
	amfun_t* f = alo_newobject(T, amfun_t);
	initialize(f);
	if (aloL_getsimpleclass(T, "__matcher")) {
		static const acreg_t funcs[] = {
				{ "__del", matcher_gc },
				{ "__call", matcher_test },
				{ "__tostr", matcher_tostr },
				{ "match", matcher_match },
				{ "find", matcher_find },
				{ "replace", matcher_replace },
				{ "split", matcher_split },
				{ NULL, NULL }
		};
		aloL_setfuns(T, -1, funcs);
	}
	alo_setmetatable(T, -2);
	return f;
}

static int str_matcher(astate T) {
	size_t len;
	const char* src = aloL_checklstring(T, 0, &len);
	amfun_t* f = preload(T);
	compile(f, T, src, len);
	return 1;
}

/**================================================================================*/

static const char* findaux(const char* sh, size_t lh, const char* st, size_t lt) {
	const char* se1 = sh + lh - lt;
	const char* se2 = st + lt;
	const char* sx;
	int ch = st[0];
	failed:
	while (sh <= se1) {
		if (*sh == ch) {
			const char* su = st + 1;
			const char* sv = sh + 1;
			while (su < se2) {
				if (*sv == ch) {
					sx = sv;
					goto marked;
				}
				if (*su != *sv) {
					sh = sv;
					goto failed;
				}
				su++;
				sv++;
			}
			return sh;

			marked:
			while (su < se2) {
				if (*su != *sv) {
					sh = sx;
					goto failed;
				}
				su++;
				sv++;
			}
			return sh;
		}
		else {
			sh++;
		}
	}
	return NULL;
}

/**
 ** replace all target to replacement.
 ** use matcher to replace string if regex is true, which is false in default.
 ** prototype: string.replace(self, target, replacement, [regex])
 */
static int str_replace(astate T) {
	aloL_checkstring(T, 0);
	if (aloL_getoptbool(T, 3, false)) { /* replace in regex mode. */
		alo_settop(T, 3);
		if (!aloL_callselfmeta(T, 1, "matcher")) { /* stack[3] = target->matcher */
			aloL_argerror(T, 1, "failed to call function 'string.matcher'");
		}
		if (alo_getmeta(T, 3, "replace", false) != ALO_TFUNCTION) {
			aloL_argerror(T, 3, "failed to call function 'matcher.replace'");
		}
		alo_push(T, 3);
		alo_push(T, 0);
		alo_push(T, 2);
		alo_call(T, 3, 1); /* stack[4] = stack[3]->replace(self,to) */
		return 1;
	}
	else {
		aloL_checkstring(T, 2);
		const char *s1, *s2, *s3, *st;
		size_t l1, l2, l3;
		s1 = alo_tolstring(T, 0, &l1);
		if (l1 == 0) {
			aloL_argerror(T, 0, "replace target cannot be empty.");
		}
		s2 = aloL_checklstring(T, 1, &l2);
		if (l1 < l2) {
			goto norep;
		}
		st = findaux(s1, l1, s2, l2);
		if (st == NULL) {
			goto norep;
		}

		s3 = alo_tolstring(T, 2, &l3);
		aloL_usebuf(T, buf) {
			do
			{
				aloL_bputls(T, buf, s1, st - s1);
				aloL_bputls(T, buf, s3, l3);
				l1 -= (st - s1) + l2;
				s1 = st + l2;
			}
			while ((st = findaux(s1, l1, s2, l2)));
			if (l1 > 0) {
				aloL_bputls(T, buf, s1, l1);
			}
			aloL_bpushstring(T, buf);
		}
		return 1;

		norep:
		alo_settop(T, 1);
		return 1;
	}
}

/**
 ** split string into list.
 ** use matcher to split string if regex is true, which is false in default.
 ** prototype: string.split(self,separator, [regex])
 */
static int str_split(astate T) {
	size_t l1, l2;
	const char* s1 = aloL_checklstring(T, 0, &l1);
	const char* s2 = aloL_checklstring(T, 1, &l2);
	if (aloL_getoptbool(T, 2, false)) { /* replace in regex mode. */
		alo_settop(T, 2);
		if (!aloL_callselfmeta(T, 1, "matcher")) { /* stack[3] = target->matcher */
			aloL_argerror(T, 1, "failed to call function 'string.matcher'");
		}
		if (alo_getmeta(T, 2, "split", false) != ALO_TFUNCTION) {
			aloL_argerror(T, 2, "failed to call function 'matcher.split'");
		}
		alo_push(T, 2);
		alo_push(T, 0);
		alo_call(T, 2, 1); /* stack[4] = stack[3]->replace(self,to) */
	}
	else {
		alo_settop(T, 2);
		alo_newlist(T, 0);
		size_t n = 0;
		const char* st;
		while ((st = findaux(s1, l1, s2, l2))) {
			alo_pushlstring(T, s1, st - s1);
			alo_rawseti(T, 2, n++);
			size_t off = st - s1 + l2;
			s1 += off;
			l1 -= off;
		}
		alo_pushlstring(T, s1, l1);
		alo_rawseti(T, 2, n);
	}
	return 1;
}

static const acreg_t mod_funcs[] = {
	{ "byte", str_byte },
	{ "char", str_char },
	{ "match", str_match },
	{ "matcher", str_matcher },
	{ "repeat", str_repeat },
	{ "replace", str_replace },
	{ "reverse", str_reverse },
	{ "split", str_split },
	{ "trim", str_trim },
	{ NULL, NULL }
};

int aloopen_str(astate T) {
	alo_bind(T, "string.byte", str_byte);
	alo_bind(T, "string.char", str_char);
	alo_bind(T, "string.repeat", str_repeat);
	alo_bind(T, "string.replace", str_replace);
	alo_bind(T, "string.reverse", str_reverse);
	alo_bind(T, "string.split", str_split);
	alo_bind(T, "string.trim", str_trim);
	alo_bind(T, "string.match", str_match);
	alo_bind(T, "string.matcher", str_matcher);
	alo_getreg(T, "__basic_delegates");
	alo_rawgeti(T, -1, ALO_TSTRING);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
