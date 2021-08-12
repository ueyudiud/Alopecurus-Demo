/*
 * alex.h
 *
 * lexical analyzer.
 *
 *  Created on: 2019年8月14日
 *      Author: ueyudiud
 */

#ifndef ALEX_H_
#define ALEX_H_

#include "abuf.h"
#include "aobj.h"
#include "aparse.h"

typedef struct alo_Token atoken_t;

enum {
	TK_ERR = -1,
	/* empty token */
	TK_UNDEF = 0,
	/* normal token */
	TK_OFFSET = UCHAR_MAX,
	/* keywords */
	TK_ALIAS, TK_BREAK, TK_CASE, TK_CONTINUE, TK_DEF, TK_DELETE, TK_DO, TK_ELSE,
	TK_FALSE, TK_FOR, TK_GOTO, TK_IF, TK_IN, TK_LOCAL, TK_MATCH, TK_NEW, TK_NIL,
	TK_RETURN, TK_STRUCT, TK_THEN, TK_THIS, TK_TRUE, TK_WHILE,
	/* operators */
	TK_BDOT, TK_TDOT, TK_BCOL, TK_ABDOT, TK_LARR, TK_RARR, TK_ACOL, TK_IDIV,
	TK_QCOL,
	TK_EQ, TK_NE, TK_LE, TK_GE, TK_SHL, TK_SHR, TK_AND, TK_OR,
	TK_AADD, TK_ASUB, TK_AMUL, TK_ADIV, TK_AIDIV, TK_AMOD, TK_APOW, TK_ASHL,
	TK_ASHR, TK_AAND, TK_AOR, TK_AXOR,
	/* special token */
	TK_EOF,
	/* literal and symbols */
	TK_IDENT, TK_INTEGER, TK_FLOAT, TK_STRING
};

ALO_VDEC const a_cstr aloX_tokenid[];

#define ALO_NUMRESERVED (TK_WHILE - TK_OFFSET)

#define tiskey(t) ((t) > TK_OFFSET && (t) <= TK_WHILE)

ALO_IFUN void aloX_init(alo_State);
ALO_IFUN void aloX_open(alo_State, alexer_t*, a_cstr, aibuf_t*);
ALO_IFUN void aloX_close(alexer_t*);
ALO_IFUN a_none aloX_error(alexer_t*, a_cstr);
ALO_IFUN a_cstr aloX_tkid2str(alexer_t*, int);
ALO_IFUN a_cstr aloX_token2str(alexer_t*, atoken_t*);
ALO_IFUN alo_Str* aloX_getstr(alexer_t*, const char*, size_t);
ALO_IFUN int aloX_poll(alexer_t*);
ALO_IFUN int aloX_forward(alexer_t*);

/* extra token data */
union alo_TokenData {
	a_int i;
	a_float f;
	alo_Str* s;
};

/* token */
struct alo_Token {
	union alo_TokenData d; /* token data */
	int t;
};

struct alo_Lexer {
	alo_State T;
	alo_Str* src; /* source name */
	aibuf_t* in; /* input buffer */
	atable_t* ss; /* used string */
	int ch; /* current character */
	atoken_t ct, nt; /* current token and next token */
	int pl, cl; /* previous and current line number */
	afstat_t* f; /* current function state */
	ambuf_t buf[1];
};

#endif /* ALEX_H_ */
