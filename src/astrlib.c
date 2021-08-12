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
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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
static int str_reverse(alo_State T) {
	size_t len;
	const char* src = aloL_checklstring(T, 0, &len);
	if (len < MAX_STACKBUF_LEN) { /* for short string, use heap memory */
		char buf[MAX_STACKBUF_LEN];
		for (size_t i = 0; i < len; ++i) {
			buf[i] = src[len - 1 - i];
		}
		alo_pushlstring(T, buf, len);
	}
	else { /* or use memory buffer instead */
		aloL_usebuf(T, buf,
			aloL_bcheck(T, buf, len * sizeof(char));
			for (size_t i = 0; i < len; ++i) {
				aloL_bsetc(buf, i, src[len - 1 - i]);
			}
			aloL_blen(buf) = len * sizeof(char);
			aloL_bpushstring(T, buf);
		)
	}
	return 1;
}

/**
 ** repeat string with number of times.
 ** prototype: string.repeat(self, times)
 */
static int str_repeat(alo_State T) {
	size_t len;
	const char* src = aloL_checklstring(T, 0, &len);
	size_t time = aloL_checkinteger(T, 1);
	if (time >= ALO_INT_PROP(MAX) / len) {
		aloL_error(T, "repeat time overflow.");
	}
	size_t nlen = len * time;
	if (nlen == 0) {
		alo_pushstring(T, "");
	}
	else if (nlen <= MAX_STACKBUF_LEN) { /* for short string, use buffer on stack */
		char buf[MAX_STACKBUF_LEN];
		char* p = buf;
		for (size_t i = 0; i < time; ++i) {
			l_strcpy(p, src, len);
			p += len;
		}
		alo_pushlstring(T, buf, nlen);
	}
	else { /* for long string, use buffer on heap */
		aloL_usebuf(T, buf,
			aloL_bcheck(T, buf, nlen * sizeof(char));
			aloL_blen(buf) = nlen * sizeof(char);
			memcpy(aloL_braw(buf), src, len);
			char* p = aloL_braw(buf) + len;
			nlen -= len;
			while (len <= nlen) {
				memcpy(p, aloL_braw(buf), len);
				p += len;
				nlen -= len;
				len <<= 1;
			}
			memcpy(p, src, nlen);
			aloL_bpushstring(T, buf);
		)
	}
	return 1;
}

/**
 ** trim white space in the front and tail of string.
 ** prototype: string.trim(self)
 */
static int str_trim(alo_State T) {
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
		alo_pushlstring(T, src + i, j - i);
	}
	return 1;
}

/**
 ** transform character to byte value.
 ** 0 is the default value for index.
 ** prototype: string.byte(self, index)
 */
static int str_byte(alo_State T) {
	size_t l;
	const char* s = aloL_checklstring(T, 0, &l);
	a_int i = aloL_getoptinteger(T, 1, 0);
	if (i >= 0) {
		if (aloE_cast(a_int, l) < i) {
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
static int str_char(alo_State T) {
	ssize_t n = alo_gettop(T);
	aloL_usebuf(T, buf,
		aloL_bcheck(T, buf, n);
		for (ssize_t i = 0; i < n; ++i) {
			a_int ch = aloL_checkinteger(T, i);
			if (ch != aloE_byte(ch)) {
				aloL_argerror(T, i, "integer can not cast to character");
			}
			aloL_bsetc(buf, i, ch);
		}
		aloL_blen(buf) = n;
		aloL_bpushstring(T, buf);
	)
	return 1;
}

/**================================================================================
 **                    regular expression support start
 **================================================================================*/

/* max match group number */
#define MAX_GROUP_SIZE 64

/* matching group */
typedef struct alo_MatchGroup {
	a_cstr begin, end; /* begin and end of matched string */
} amgroup_t;

enum {
	RE_RET,
	RE_CHKC,
	RE_CHKS,
	RE_CHKXS,
	RE_CHKQ,
	RE_CHKF,
	RE_CHKL,
	RE_REP,
	RE_REPX,
	RE_REPY,
	RE_SPT,
	RE_BEG,
	RE_END,
	RE_JMP,
	RE_CNT,

	RE_C_JMP
};

typedef struct {
	int kind;
	int arga;
	int argb;
} aminsn_t;

/* the compiled matching function for regular expression */
typedef struct {
	aminsn_t* codes;
	char* seq;
	size_t len;
	int ngroup;
	int ncounter;
	int ncode;
} amfun_t;

/* match state */
typedef struct alo_MatchState {
	alo_State T;
	amfun_t* f; /* matching function */
	a_cstr sbegin, send; /* begin and end of source */
	union {
		amgroup_t* groups; /* group buffer */
		ambuf_t gp;
	};
} amstat_t;

typedef struct alo_MatchLabel {
	aminsn_t* to;
	int pos; /* position of string */
	int cnt; /* counter */
} amlabel_t;

/* end of string */
#define EOS (-1)

#define sgetc(M,s) ((M)->send == (s) ? EOS : *((s)++))

static int match_class(int cl, int ch) {
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

#define MASK_FULL 0x0001

static int match_set(a_cstr p, int ch) {
	if (ch != EOS) {
		while (*p == '-') {
			if (p[1] <= ch && ch <= p[2])
				return true;
			p += 3;
		}
		while (*p) {
			if (*p == '\\') {
				p += 1;
				while (*p) {
					if (match_class(*p, ch)) {
						return true;
					}
					p += 1;
				}
				return false;
			}
			if (*p == ch) {
				return true;
			}
			p += 1;
		}
	}
	return false;
}

static int match_seq(amstat_t* M, a_cstr p, a_cstr* ps) {
	while (true) {
		switch (*p) {
		case '\0': {
			return true; /* end of string */
		}
		case '\\': {
			if (!match_class(*(p + 1), sgetc(M, *ps))) {
				return false;
			}
			p += 2;
			break;
		}
		default: {
			if (sgetc(M, *ps) != *p) {
				return false;
			}
			p += 1;
			break;
		}
		}
	}
}

static int match_lim(amstat_t* M, int p, a_cstr s) {
	switch (p) {
	case '^':
		return M->sbegin == s || s[-1] == '\n' || s[-1] == '\r';
	case '$':
		return M->send == s || s[0] == '\n' || s[0] == '\r';
	default:
		return false;
	}
}

#define pushlabel(M,js,s,n,c) aloL_btpush((M)->T, js, ((amlabel_t) { n, (s) - (M)->sbegin, c }))
#define toplabel(M,js) aloL_bttop(js, amlabel_t)

static int match(amstat_t* M, amfun_t* f, const char* start, int mask) {
	aminsn_t* pc = f->codes;
	const char* s = start;
	int failed = false;
	int counter[f->ncounter];
	aloL_newbuf(M->T, js);
	for (int i = 0; i < f->ncounter; counter[i++] = 0);
	mcheck:
	while (true) {
		switch (pc->kind) {
		case RE_RET: {
			if ((mask & MASK_FULL) && s != M->send)
				goto mthrow;
			alo_bufpop(M->T, js);
			return true;
		}
		case RE_BEG: {
			M->groups[pc->arga].begin = s;
			pc++;
			break;
		}
		case RE_END: {
			M->groups[pc->arga].end = s;
			pc++;
			break;
		}
		case RE_CHKC: {
			if (aloE_byte(*(s++)) != aloE_byte(pc->arga))
				goto mthrow;
			pc++;
			break;
		}
		case RE_CHKS: case RE_CHKXS: {
			if (match_set(f->seq + pc->arga, sgetc(M, s)) != (pc->kind == RE_CHKS))
				goto mthrow;
			pc++;
			break;
		}
		case RE_CHKQ: {
			if (!match_seq(M, f->seq + pc->arga, &s))
				goto mthrow;
			pc++;
			break;
		}
		case RE_CHKF: {
			if (!match_class(pc->arga, sgetc(M, s)))
				goto mthrow;
			pc++;
			break;
		}
		case RE_CHKL: {
			if (!match_lim(M, pc->arga, s))
				goto mthrow;
			pc++;
			break;
		}
		case RE_REP: {
			if (failed) {
				counter[(pc - 1)->arga] = toplabel(M, js)->cnt;
				goto mthrow;
			}
			if (counter[(pc - 1)->arga] == pc->arga) {
				pc += pc->argb;
			}
			else {
				pushlabel(M, js, s, pc, counter[(pc - 1)->arga]++);
				pc++;
			}
			break;
		}
		case RE_REPX: {
			aloE_xassert(!failed);
			if (counter[(pc - 1)->arga] == pc->arga) {
				pc += pc->argb;
			}
			else {
				pushlabel(M, js, s, pc + pc->argb, 0);
				pc++;
			}
			break;
		}
		case RE_REPY: {
			if (failed) {
				counter[(pc - 1)->arga] = toplabel(M, js)->cnt;
				if (counter[(pc - 1)->arga] == pc->arga)
					goto mthrow;
				counter[(pc - 1)->arga]++;
				pc++;
			}
			else
			{
				pushlabel(M, js, s, pc, counter[(pc - 1)->arga]);
				pc += pc->argb;
			}
			break;
		}
		case RE_SPT: {
			pushlabel(M, js, s, pc + pc->arga, 0);
			pc += pc->argb;
			break;
		}
		case RE_JMP: {
			pc += pc->argb;
			break;
		}
		case RE_CNT: {
			counter[pc->arga] = 0;
			pc++;
			break;
		}
		default:
			aloE_assert(false, "");
		}
		failed = false;
	}

	mthrow:
	if (js->len > 0) {
		/* recover through match stack */
		amlabel_t* label = aloL_btpop(js, amlabel_t);
		pc = label->to;
		s = M->sbegin + label->pos;
		failed = true;
		goto mcheck;
	}
	/* not recoverable, match failed */
	alo_bufpop(M->T, js);
	return false;
}

/* compile state */
typedef struct alo_CompileState {
	alo_State T;
	amfun_t* f;
	alo_Alloc alloc;
	void* context;
	size_t ncs; /* number of char sequence */
	a_cstr pos; /* current position */
	a_cstr end; /* end of source */
	int ngroup; /* number of group */
	int ncount; /* number of counter */
	int ncode; /* number of code */
	struct {
		size_t length;
		char array[256];
	}; /* string buffer */
} acstat_t;

static void mbegin(amstat_t* M, alo_State T, amfun_t* f, a_cstr src, size_t len) {
	M->T = T;
	M->f = f;
	M->sbegin = src;
	M->send = src + len;
	alo_bufpush(T, &M->gp);
	size_t used = f->ngroup * sizeof(amgroup_t);
	aloL_bcheck(T, &M->gp, used);
	M->gp.len = used;
}

static void mend(amstat_t* M) {
	alo_bufpop(M->T, &M->gp);
}

#define cerror(C,msg,args...) aloL_error((C)->T, msg, ##args)

static int c_node(acstat_t* C) {
	if (C->ncode == C->f->ncode) {
		size_t newlen = C->f->ncode * 2;
		if (newlen < 16) {
			newlen = 16;
		}
		void* newbuf = C->alloc(C->context, C->f->codes, C->f->ncode * sizeof(aminsn_t), newlen * sizeof(aminsn_t));
		if (newbuf == NULL)
			cerror(C, "no enough memory.");
		C->f->codes = newbuf;
		C->f->ncode = newlen;
	}
	return C->ncode++;
}

static void c_erase(acstat_t* C, int id) {
	aminsn_t* p = C->f->codes + id + 1;
	aminsn_t* const e = C->f->codes + C->ncode;
	while (p < e) {
		*(p - 1) = *p;
		p++;
	}
	C->ncode--;
}

static int c_str(acstat_t* C) {
	int length = C->length;
	C->length = 0;
	if (length + C->ncs >= C->f->len) {
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
	char* s = C->f->seq + C->ncs;
	l_strcpy(s, C->array, length);
	s[length] = '\0';
	int off = C->ncs;
	C->ncs += length + 1;
	C->length = 0;
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

#define c_set(C,id,k,a,b) ({ int $id = id; (C)->f->codes[$id] = (aminsn_t) { k, a, b }; })
#define c_put(C,k,a,b) c_set(C, c_node(C), k, a, b)

static void p_chr(acstat_t* C) {
	int kind = RE_CHKC;
	int ch;
	if (*C->pos == '\\') {
		switch (C->pos[1]) {
		case '0': ch = '\0'; break;
		case 'b': ch = '\b'; break;
		case 'f': ch = '\f'; break;
		case 'n': ch = '\n'; break;
		case 'r': ch = '\r'; break;
		case 't': ch = '\t'; break;
		case '\'': ch = '\''; break;
		case '\"': ch = '\"'; break;
		case '\\': ch = '\\'; break;
		default:
			kind = RE_CHKF;
			ch = C->pos[1];
			break;
		}
		C->pos += 2;
	}
	else {
		ch = *(C->pos++);
	}
	c_put(C, kind, ch, 0);
}

static void p_str(acstat_t* C) {
	next:
	switch (*C->pos) {
	case '+': case '*': case '?':
	case '(': case ')': case '|':
	case '[': case '^': case '$':
	case '\0':
		break;
	case ']':
		cerror(C, "illegal character.");
		break;
	case '\\': {
		if (C->pos + 1 == C->end) {
			cerror(C, "illegal escape character.");
		}
		switch (C->pos[1]) {
		case '0': cput(C, '\0'); break;
		case 'b': cput(C, '\b'); break;
		case 'f': cput(C, '\f'); break;
		case 'n': cput(C, '\n'); break;
		case 'r': cput(C, '\r'); break;
		case 't': cput(C, '\t'); break;
		case '\\': cput(C, '\\'); break;
		case '\'': cput(C, '\''); break;
		case '\"': cput(C, '\"'); break;
		default:
			cput(C, '\\');
			cput(C, C->pos[1]);
			break;
		}
		C->pos += 2;
		goto next;
	}
	default: {
		cput(C, *C->pos);
		C->pos += 1;
		goto next;
	}
	}
	if (C->length == 1) {
		c_put(C, RE_CHKC, aloE_byte(C->array[0]), 0);
		C->length = 0;
	}
	else if (C->array[0] == '\\' && C->length == 2) {
		c_put(C, RE_CHKF, aloE_byte(C->array[1]), 0);
		C->length = 0;
	}
	else {
		c_put(C, RE_CHKQ, c_str(C), 0);
	}
}

static void p_chset(acstat_t* C) {
	int type = *C->pos == '^' ? (C->pos++, RE_CHKXS) : RE_CHKS;
	a_cstr s = C->pos;
	while (*C->pos != ']') {
		if (*C->pos == '\0')
			cerror(C, "unclosed character set.");
		else if (*C->pos == '\\')
			C->pos += 2;
		else if (*(C->pos + 1) == '-') {
			if (C->pos + 2 < C->end || C->pos[0] > C->pos[2])
				cerror(C, "illegal character bound.");
			cput(C, '-');
			cput(C, C->pos[0]);
			cput(C, C->pos[2]);
			C->pos += 3;
		}
		else
			C->pos += 1;
	}
	C->pos = s;
	int hascls = false;
	while (*C->pos != ']') {
		if (*(C->pos + 1) == '-') {
			C->pos += 3;
			continue;
		}
		if (*C->pos == '\\') {
			if (strchr(CLASS_CHAR, *(C->pos + 1))) { /* is class marker */
				C->pos += 2;
				hascls = true;
				continue;
			}
			C->pos += 1;
		}
		cput(C, *(C->pos++));
	}
	if (hascls) {
		cput(C, '\\');
		C->pos = s;
		while (*C->pos != ']') {
			if (*(C->pos + 1) == '-')
				C->pos += 3;
			else if (*C->pos == '\\') {
				if (strchr(CLASS_CHAR, *(C->pos + 1))) { /* is class marker */
					cput(C, *(C->pos + 1));
				}
				C->pos += 2;
			}
			else
				C->pos += 1;
		}
	}
	c_put(C, type, c_str(C), 0);
}

static void p_expr(acstat_t*);

static void p_prefix(acstat_t*);

static void p_atom(acstat_t* C, int multi) {
	/* atom -> '(' expr ')' | '[' chset ']' | char */
	switch (*C->pos) {
	case '(': {
		int index = C->ngroup++;
		C->pos++;
		c_put(C, RE_BEG, index, 0);
		p_expr(C);
		if (*(C->pos++) != ')') {
			cerror(C, "illegal pattern.");
		}
		c_put(C, RE_END, index, 0);
		break;
	}
	case '[': {
		C->pos++;
		p_chset(C);
		aloE_xassert(*C->pos == ']');
		C->pos++;
		break;
	}
	case '^': {
		c_put(C, RE_CHKL, '^', 0);
		C->pos++;
		break;
	}
	case '$': {
		c_put(C, RE_CHKL, '$', 0);
		C->pos++;
		break;
	}
	case '+': case '*': case '?': case '{': {
		p_prefix(C);
		break;
	}
	case ']': case '}': {
		cerror(C, "unexpected '%c' in pattern.", *C->pos);
		break;
	}
	case ')': {
		break;
	}
	default: {
		(multi ? p_str : p_chr)(C);
		break;
	}
	}
}

static int p_limit(acstat_t* C) {
	int n = *(C->pos++) - '0';
	while ('0' <= *C->pos && *C->pos <= '9') {
		int d = *C->pos - '0';
		if (n > INT_MAX / 10 || (n == INT_MAX / 10 && d > INT_MAX % 10)) {
			cerror(C, "range out of bound.");
		}
		n = n * 10 + d;
		C->pos++;
	}
	return n;
}

static void p_prefix(acstat_t* C) {
	/* prefix -> [ ('+'|'*'|'?'|'{' [int] ',' [int] '}') ['?'] ] atom */
	switch (*C->pos) {
	case '{': {
		int min = 0, max = INT32_MAX;
		switch (*(++C->pos)) {
		case '0' ... '9': {
			min = p_limit(C);
			if (*C->pos != ',') {
				max = min;
				goto merge;
			}
			break;
		}
		}
		if (*(C->pos++) != ',')
			cerror(C, "illegal pattern.");
		switch (*C->pos) {
		case '0' ... '9': {
			max = p_limit(C);
			if (min > max)
				cerror(C, "illegal range size.");
			break;
		}
		}
		merge:
		if (*(C->pos++) != '}')
			cerror(C, "illegal pattern.");
		int greedy = *C->pos == '?' ? (C->pos++, false) : true;
		int cnt = C->ncount++;
		c_put(C, RE_CNT, cnt, 0);
		int id1 = c_node(C);
		a_cstr s = C->pos; /* store source position */
		p_atom(C, false);
		int id2 = c_node(C);
		c_set(C, id1, RE_REP, min, C->ncode - id1);
		c_set(C, id2, RE_C_JMP, 0, id1 - id2);
		if (max != min) {
			c_put(C, RE_CNT, cnt, 0);
			C->pos = s; /* revert position */
			id1 = c_node(C);
			p_atom(C, false);
			id2 = c_node(C);
			c_set(C, id1, greedy ? RE_REPX : RE_REPY, max - min, C->ncode - id1);
			c_set(C, id2, RE_C_JMP, 0, id1 - id2);
		}
		break;
	}
	case '?': {
		int id1 = c_node(C);
		if (*(++C->pos) == '?') {
			C->pos++;
			p_atom(C, false);
			c_set(C, id1, RE_SPT, 1, C->ncode - id1);
		}
		else {
			p_atom(C, false);
			c_set(C, id1, RE_SPT, C->ncode - id1, 1);
		}
		break;
	}
	case '+': {
		int id1 = C->ncode;
		if (*(++C->pos) == '?') {
			C->pos++;
			p_atom(C, false);
			int id2 = c_node(C);
			c_set(C, id2, RE_SPT, id1 - id2, 1);
		}
		else {
			p_atom(C, false);
			int id2 = c_node(C);
			c_set(C, id2, RE_SPT, 1, id1 - id2);
		}
		break;
	}
	case '*': {
		int id1 = c_node(C);
		if (*(++C->pos) == '?') {
			C->pos++;
			p_atom(C, false);
			int id2 = c_node(C);
			c_set(C, id1, RE_SPT, 1, id2 + 1 - id1);
			c_set(C, id2, RE_SPT, id1 + 1 - id2, 1);
		}
		else {
			p_atom(C, false);
			int id2 = c_node(C);
			c_set(C, id1, RE_SPT, id2 + 1 - id1, 1);
			c_set(C, id2, RE_SPT, 1, id1 + 1 - id2);
		}
		break;
	}
	default: {
		p_atom(C, true);
		break;
	}
	}
}

static void p_mono(acstat_t* C) {
	/* mono -> { prefix } */
	p_prefix(C);
	next:
	switch (*C->pos) {
	case '\0':
		break; /* end of string */
	case ']': case '}':
		cerror(C, "illegal pattern.");
		break;
	case ')': case '|':
		break;
	default:
		p_prefix(C);
		goto next;
	}
}

static void p_expr(acstat_t* C) {
	/* expr -> mono { '|' mono } */
	int id1 = c_node(C);
	p_mono(C);
	if (*C->pos == '|') {
		int id2;
		do {
			C->pos++;
			id2 = c_node(C);
			c_set(C, id1, RE_SPT, C->ncode - id1, 1);
			id1 = c_node(C);
			p_mono(C);
			c_set(C, id2, RE_C_JMP, 0, C->ncode - id2);
		}
		while (*C->pos == '|');
		C->f->codes[id2].argb--;
	}
	c_erase(C, id1);
}

static void c_trim(acstat_t* C) {
	C->f->codes = C->alloc(C->context, C->f->codes, C->f->ncode * sizeof(aminsn_t), C->ncode * sizeof(aminsn_t));
	C->f->ncode = C->ncode;
	C->f->seq = C->alloc(C->context, C->f->seq, C->f->len * sizeof(char), C->ncs * sizeof(char));
	C->f->len = C->ncs;
}

static void compile(amfun_t* f, alo_State T, const char* src, size_t len) {
	acstat_t C;
	C.T = T;
	C.f = f;
	C.alloc = alo_getalloc(T, &C.context);
	C.length = 0;
	C.ngroup = 1;
	C.ncount = 0;
	C.pos = src;
	C.end = src + len;
	C.ncode = 0;
	C.ncs = 0;
	c_put(&C, RE_BEG, 0, 0);
	p_expr(&C);
	if (C.end != C.pos) {
		cerror(&C, "illegal pattern.");
	}
	/* jump folding */
	for (int i = 0; i < C.ncode; ++i) {
		aminsn_t* insn = &C.f->codes[i];
		if (insn->kind == RE_C_JMP) {
			aminsn_t* insn1 = insn;
			do {
				insn1 = insn1 + insn1->argb;
			}
			while (insn1->kind == RE_C_JMP);
			if (insn1->kind == RE_JMP)
				insn1 += insn1->argb;
			aminsn_t* insn2;
			aminsn_t* insn3 = insn;
			do {
				insn2 = insn3;
				insn3 += insn3->argb;
				insn2->kind = RE_JMP;
				insn2->argb = insn1 - insn2;
			}
			while (insn3->kind == RE_C_JMP);
		}
	}
	c_put(&C, RE_END, 0, 0);
	c_put(&C, RE_RET, 0, 0);
	f->ngroup = C.ngroup;
	f->ncounter = C.ncount;
	c_trim(&C);
}

static void destory(alo_State T, amfun_t* f) {
	alo_Alloc alloc;
	void* context;
	alloc = alo_getalloc(T, &context);
	alloc(context, f->codes, f->ncode * sizeof(aminsn_t), 0);
	alloc(context, f->seq, f->len * sizeof(char), 0);
}

static void initialize(amfun_t* f) {
	f->codes = NULL;
	f->ncode = 0;
	f->seq = NULL;
	f->len = 0;
	f->ngroup = 0;
	f->ncounter = 0;
}

static int matcher_gc(alo_State T) {
	amfun_t* f = aloE_cast(amfun_t*, alo_torawdata(T, 0));
	if (f->codes) {
		destory(T, f);
		f->codes = NULL;
	}
	return 0;
}

static amfun_t* self(alo_State T) {
	aloL_checktype(T, 0, ALO_TUSER);
	amfun_t* f = aloE_cast(amfun_t*, alo_torawdata(T, 0));
	if (f->codes == NULL) {
		aloL_error(T, "matcher already destroyed.");
	}
	return f;
}

static int matcher_test(alo_State T) {
	amfun_t* f = self(T);
	aloL_checkstring(T, 1);
	amstat_t M;
	const char* src;
	size_t len;
	src = alo_tolstring(T, 1, &len);
	mbegin(&M, T, f, src, len);
	alo_pushboolean(T, match(&M, f, src, true));
	mend(&M);
	return 1;
}

static int matcher_match(alo_State T) {
	amfun_t* f = self(T);
	amstat_t M;
	const char* src;
	size_t len;
	src = alo_tolstring(T, 1, &len);
	mbegin(&M, T, f, src, len);
	if (!match(&M, f, src, true)) {
		mend(&M);
		return 0;
	}
	alo_ensure(T, f->ngroup);
	for (int i = 0; i < f->ngroup; ++i) {
		amgroup_t* group = &M.groups[i];
		if (group->begin != NULL) {
			alo_pushlstring(T, group->begin, group->end - group->begin);
		}
		else {
			alo_pushnil(T);
		}
	}
	mend(&M);
	return f->ngroup;
}

static void aux_replaces(alo_State T, amfun_t* f, amstat_t* M, ambuf_t* buf) {
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
					aloL_error(T, "replacement ngroup out of bound.");
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

static void aux_replacef(alo_State T, __attribute__((unused)) amfun_t* f, amstat_t* M, ambuf_t* buf) {
	int index = alo_gettop(T);
	alo_push(T, 2); /* push transformer */
	alo_pushlstring(T, M->groups[0].begin, M->groups[0].end - M->groups[0].begin);
	alo_pushinteger(T, M->groups[0].begin - M->sbegin);
	alo_call(T, 2, 1);
	aloL_bwrite(T, buf, index);
	alo_drop(T);
}

static int matcher_replace(alo_State T) {
	amfun_t* f = self(T);
	void (*transformer)(alo_State, amfun_t*, amstat_t*, ambuf_t*);
	if (alo_typeid(T, 2) == ALO_TSTR) {
		transformer = aux_replaces;
	}
	else {
		aloL_checkcall(T, 2);
		transformer = aux_replacef;
	}
	amstat_t M;
	const char* src;
	size_t len;
	src = aloL_checklstring(T, 1, &len);
	mbegin(&M, T, f, src, len);
	alo_settop(T, 3);
	aloL_usebuf(T, buf,
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
	)
	mend(&M);
	return 1;
}

static int mmatch(amstat_t* M, amfun_t* nodes, const char* begin, const char* end) {
	for (const char* s = begin; s <= end; ++s) {
		if (match(M, nodes, s, 0)) {
			return true;
		}
	}
	return false;
}

static int matcher_split(alo_State T) {
	amfun_t* f = self(T);
	size_t len;
	const char* src = aloL_checklstring(T, 1, &len);
	amstat_t M;
	mbegin(&M, T, f, src, len);
	size_t n = 0;
	alo_settop(T, 2);
	alo_newlist(T, 0);
	if (mmatch(&M, f, M.sbegin, M.send)) {
		do
		{
			alo_pushlstring(T, M.sbegin, M.groups->begin - M.sbegin);
			alo_rawseti(T, 2, n++);
			M.sbegin = M.groups->end;
		}
		while (mmatch(&M, f, M.sbegin, M.send));
		alo_pushlstring(T, M.sbegin, M.send - M.sbegin);
	}
	else {
		alo_push(T, 1);
	}
	alo_rawseti(T, 2, n);
	mend(&M);
	return 1;
}

/** used to debug the generate code.
static const astr opnames[] = {
		[RE_RET] = "ret",
		[RE_CHKC] = "test",
		[RE_CHKS] = "test",
		[RE_CHKXS] = "test",
		[RE_CHKQ] = "test",
		[RE_CHKF] = "test",
		[RE_CHKL] = "test",
		[RE_SPT] = "split",
		[RE_REP] = "rep",
		[RE_REPX] = "repx",
		[RE_REPY] = "repy",
		[RE_BEG] = "begin",
		[RE_END] = "end",
		[RE_JMP] = "jmp",
		[RE_CNT] = "count"
};

static void addcodes(astate T, ambuf_t* buf, amfun_t* f) {
	for (int i = 0; i < f->ncode; ++i) {
		aminsn_t* insn = &f->codes[i];
		char s[64];
		int n = sprintf(s, "\n\t%3d %-5s ", i, opnames[insn->kind]);
		aloL_bputls(T, buf, s, n);
		switch (insn->kind) {
		case RE_BEG: case RE_END: case RE_CNT: {
			aloL_bputf(T, buf, "%d", insn->arga);
			break;
		}
		case RE_CHKC: case RE_CHKL: {
			char ch = insn->arga;
			aloL_bputf(T, buf, "%\"", &ch, 1);
			break;
		}
		case RE_CHKF: {
			char ch = insn->arga;
			aloL_bputf(T, buf, "\\%\"", &ch, 1);
			break;
		}
		case RE_CHKQ: {
			aloL_bputs(T, buf, f->seq + insn->arga);
			break;
		}
		case RE_CHKS: {
			astr st = f->seq + insn->arga;
			aloL_bputs(T, buf, "[");
			while (*st == '-') {
				aloL_bputc(T, buf, st[1]);
				aloL_bputc(T, buf, '-');
				aloL_bputc(T, buf, st[2]);
				st += 3;
			}
			while (*st) {
				if (*st == '\\') {
					do {
						aloL_bputc(T, buf, '\\');
						aloL_bputc(T, buf, *(++st));
					}
					while (*st);
				}
				else {
					aloL_bputc(T, buf, *(st++));
				}
			}
			aloL_bputs(T, buf, "]");
			break;
		}
		case RE_CHKXS: {
			astr st = f->seq + insn->arga;
			aloL_bputs(T, buf, "[^");
			while (*st == '-') {
				aloL_bputc(T, buf, st[1]);
				aloL_bputc(T, buf, '-');
				aloL_bputc(T, buf, st[2]);
				st += 3;
			}
			while (*st) {
				if (*st == '\\') {
					do {
						aloL_bputc(T, buf, '\\');
						aloL_bputc(T, buf, *(++st));
					}
					while (*st);
				}
				else {
					aloL_bputc(T, buf, *(st++));
				}
			}
			aloL_bputs(T, buf, "]");
			break;
		}
		case RE_REP: case RE_REPX: case RE_REPY: {
			aloL_bputf(T, buf, "%d %d", insn->arga, i + insn->argb);
			break;
		}
		case RE_SPT: {
			aloL_bputf(T, buf, "%d %d", i + insn->arga, i + insn->argb);
			break;
		}
		case RE_JMP: {
			aloL_bputf(T, buf, "%d", i + insn->argb);
			break;
		}
		}
	}
}
 */

static int matcher_tostr(alo_State T) {
	amfun_t* f = self(T);
	/*
	aloL_usebuf(T, buf,
		aloL_bputf(T, buf, "__matcher: { ngroup: %d, addr: %p, opcodes:", (int) f->ngroup, f);
		addcodes(T, buf, f);
		aloL_bputxs(T, buf, "\n}");
		aloL_bpushstring(T, buf);
	);
	*/
	alo_pushfstring(T, "__matcher: { address: %p }", f);
	return 1;
}

static int matcher_find(alo_State T) {
	const char* src;
	size_t len;
	src = aloL_checklstring(T, 1, &len);
	amfun_t* f = self(T);
	amstat_t M;
	a_int off = aloL_getoptinteger(T, 2, 0);
	if (off < 0) {
		off += len;
	}
	if (off > aloE_cast(a_int, len) || off < 0) {
		aloL_argerror(T, 2, "ngroup out of bound.");
	}
	mbegin(&M, T, f, src, len);
	if (mmatch(&M, f, src + off, M.send)) {
		alo_pushinteger(T, M.groups[0].begin - M.sbegin);
		alo_pushinteger(T, M.groups[0].end - M.sbegin);
		mend(&M);
		return 2;
	}
	mend(&M);
	return 0;
}

static amfun_t* preload(alo_State T) {
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

static int str_matcher(alo_State T) {
	size_t len;
	const char* src = aloL_checklstring(T, 0, &len);
	amfun_t* f = preload(T);
	compile(f, T, src, len);
	return 1;
}

static int str_match(alo_State T) {
	size_t len;
	const char* src = aloL_checklstring(T, 0, &len);
	aloL_checkstring(T, 1);
	amfun_t* f = preload(T);
	compile(f, T, src, len);
	amstat_t M;
	src = alo_tolstring(T, 1, &len);
	mbegin(&M, T, f, src, len);
	alo_pushboolean(T, match(&M, f, src, true));
	mend(&M);
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
static int str_replace(alo_State T) {
	aloL_checkstring(T, 0);
	if (aloL_getoptbool(T, 3, false)) { /* replace in regex mode. */
		alo_settop(T, 3);
		if (!aloL_callselfmeta(T, 1, "matcher")) { /* stack[3] = target->matcher */
			aloL_argerror(T, 1, "failed to call function 'string.matcher'");
		}
		if (alo_getmeta(T, 3, "replace", false) != ALO_TFUNC) {
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
		aloL_usebuf(T, buf,
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
		)
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
static int str_split(alo_State T) {
	size_t l1, l2;
	const char* s1 = aloL_checklstring(T, 0, &l1);
	const char* s2 = aloL_checklstring(T, 1, &l2);
	if (aloL_getoptbool(T, 2, false)) { /* replace in regex mode. */
		alo_settop(T, 2);
		if (!aloL_callselfmeta(T, 1, "matcher")) { /* stack[3] = target->matcher */
			aloL_argerror(T, 1, "failed to call function 'string.matcher'");
		}
		if (alo_getmeta(T, 2, "split", false) != ALO_TFUNC) {
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

int aloopen_str(alo_State T) {
	alo_getreg(T, "__basic_delegates");
	alo_rawgeti(T, -1, ALO_TSTR);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
