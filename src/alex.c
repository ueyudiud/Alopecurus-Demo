/*
 * alex.c
 *
 *  Created on: 2019年8月15日
 *      Author: ueyudiud
 */

#include "achr.h"
#include "astr.h"
#include "atab.h"
#include "astate.h"
#include "agc.h"
#include "abuf.h"
#include "adebug.h"
#include "avm.h"
#include "ado.h"
#include "alex.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static const atoken_t undef = { { }, TK_UNDEF };

const astr aloX_tokenid[] = {
		"alias", "break", "case", "continue", "def", "delete", "do", "else",
		"false", "for", "goto", "if", "in", "local", "new", "nil", "return",
		"struct", "then", "this", "true", "while", "'..'", "'...'", "'::'",
		"'..='", "'<-'", "'->'", "':='", "'//'", "'=='", "'!='", "'<='",
		"'>='", "'<<'", "'>>'", "'&&'", "'||'", "'+='", "'-='", "'*='",
		"'/='", "'//='", "'%='", "'^='", "'<<='", "'>>='", "'&='", "'|='",
		"'~='", "<eof>", "<identifier>", "<integer>", "<float>", "<string>"
};

#define lisnl(lex) ((lex)->ch == '\n' || (lex)->ch == '\r')

static inline int lgetc(alexer_t* lex) {
	return lex->ch = aloB_iget(lex->T, lex->in);
}

static inline void lstore(alexer_t* lex, int ch) {
	aloB_putc(lex->T, &lex->buf, ch);
}

static inline void lrewind(alexer_t* lex) {
	lex->buf.length = 0;
}

static inline astring_t* lload(alexer_t* lex) {
	astring_t* s = aloX_getstr(lex, lex->buf.array, lex->buf.length);
	lrewind(lex);
	return s;
}

static inline astr lmake(alexer_t* lex) {
	lstore(lex, '\0'); /* make a zero terminated string */
	return lex->buf.array;
}

/**
 ** initialize lexer environment, called by initialize(void) in 'astate.c'.
 */
void aloX_init(astate T) {
	for (int i = 0; i < ALO_NUMRESERVED; ++i) {
		astring_t* s = aloS_of(T, aloX_tokenid[i]);
		s->freserved = true;
		s->extra = i;
		aloG_fix(T, s);
	}
}

/**
 ** open lexer.
 */
void aloX_open(astate T, alexer_t* lex, astr src, aibuf_t* in) {
	lex->T = T;
	lex->in = in;
	aloB_bopen(lex->buf);
	lex->ch = aloB_ifill_(T, lex->in); /* get fist character */
	lex->ct = lex->nt = undef;
	lex->pl = -1;
	lex->cl = 1;
	atable_t* symbols = aloH_new(T);
	tsettab(T, T->top++, symbols);
	aloH_ensure(T, symbols, ALO_NUMRESERVED);
	for (int i = 0; i < ALO_NUMRESERVED; ++i) {
		aloH_findxset(T, symbols, aloX_tokenid[i], strlen(aloX_tokenid[i]));
	}
	lex->ss = symbols;
	lex->src = aloX_getstr(lex, src, strlen(src));
}

/**
 ** close lexer.
 */
void aloX_close(alexer_t* lex) {
	aloB_bfree(lex->T, lex->buf);
}

/**
 ** throw an error by lexer.
 */
anoret aloX_error(alexer_t* lex, astr msg) {
	aloV_pushfstring(lex->T, "%s:%d: %s", lex->src->array, lex->cl, msg);
	aloD_throw(lex->T, ThreadStateErrCompile);
}
/**
 ** get string from token index.
 */
astr aloX_tkid2str(alexer_t* lex, int id) {
	if (id == TK_ERR) {
		return "<error>";
	}
	else if (id <= TK_OFFSET) {
		aloE_assert(id == aloE_byte(id), "invalid token index.");
		return aloV_pushfstring(lex->T, "'%c'", id);
	}
	else {
		astr s = aloX_tokenid[id - (TK_OFFSET + 1)];
		return tiskey(id) ? aloV_pushfstring(lex->T, "'%s'", s) : s;
	}
}

/**
 ** get string from token.
 */
astr aloX_token2str(alexer_t* lex, atoken_t* token) {
	switch (token->t) {
	case TK_INTEGER:
		return aloV_pushfstring(lex->T, "%i", token->d.i);
	case TK_FLOAT:
		return aloV_pushfstring(lex->T, "%f", token->d.f);
	case TK_STRING:
		return aloV_pushfstring(lex->T, "\"%\"\"", token->d.s->array, aloS_len(token->d.s));
	case TK_IDENT:
		return token->d.s->array;
	default:
		return aloX_tkid2str(lex, token->t);
	}
}

/**
 ** get cached string.
 */
astring_t* aloX_getstr(alexer_t* lex, const char* src, size_t len) {
	atable_t* symbols = lex->ss;
	return tgetstr(aloH_findxset(lex->T, symbols, src, len)); /* get string from key of entry */
}

static anoret lerror(alexer_t* lex, astr msg, ...) {
	va_list varg;
	va_start(varg, msg);
	astr s = aloV_pushvfstring(lex->T, msg, varg);
	va_end(varg);
	aloX_error(lex, s);
}

static int peekidt(alexer_t* lex, union alo_TokenData* data) {
	do {
		lstore(lex, lex->ch);
	}
	while (aisident(lgetc(lex)));
	data->s = lload(lex); /* build string */
	return data->s->freserved ? data->s->extra + (TK_OFFSET + 1) : TK_IDENT;
}

static int peeknum(alexer_t* lex, union alo_TokenData* data) {
	if (lex->ch == '0') {
		switch (lgetc(lex)) {
		case 'x': {
			lstore(lex, '0');
			lstore(lex, 'x');
			while (aisxdigit(lgetc(lex))) {
				lstore(lex, lex->ch);
			}
			if (lex->ch == '.') {
				lstore(lex, '.');
				while (aisxdigit(lgetc(lex))) {
					lstore(lex, lex->ch);
				}
				if (lex->ch != 'p' && lex->ch != 'P') {
					goto flt_val;
				}
				hex_exp:
				lstore(lex, 'p');
				switch (lgetc(lex)) {
				case '+': case '-':
					lstore(lex, lex->ch);
					lgetc(lex);
					break;
				}
				while (aisdigit(lex->ch)) {
					lstore(lex, lex->ch);
					lgetc(lex);
				}
				goto flt_val;
			}
			else if (lex->ch == 'p' || lex->ch == 'P') {
				goto hex_exp;
			}
			goto int_val;
		}
		}
		lstore(lex, '0');
		goto number;
	}
number:
	while (aisdigit(lex->ch)) {
		lstore(lex, lex->ch);
		lgetc(lex);
	}
	if (lex->ch == '.') {
		lstore(lex, '.');
		while (aisdigit(lgetc(lex))) {
			lstore(lex, lex->ch);
		}
		if (lex->ch == 'e' || lex->ch == 'E') {
			dec_exp:
			lstore(lex, 'e');
			switch (lgetc(lex)) {
			case '+': case '-':
				lstore(lex, lex->ch);
				lgetc(lex);
				break;
			}
			while (aisdigit(lex->ch)) {
				lstore(lex, lex->ch);
				lgetc(lex);
			}
		}
		goto flt_val;
	}
	else if (lex->ch == 'e' || lex->ch == 'E') {
		goto dec_exp;
	}
	goto int_val;

	int_val: {
		lstore(lex, '\0');
		aint value = strtoll(lmake(lex), NULL, 0);
		if (errno) {
			lerror(lex, "fail to parse integer value: %s", strerror(errno));
			return TK_ERR;
		}
		lrewind(lex);
		data->i = value;
		return TK_INTEGER;
	}

	flt_val: {
		afloat value = strtod(lmake(lex), NULL);
		if (errno) {
			lerror(lex, "fail to parse float value: %s", strerror(errno));
			return TK_ERR;
		}
		lrewind(lex);
		data->f = value;
		return TK_FLOAT;
	}
}

static void peeksstr(alexer_t* lex, union alo_TokenData* data) {
	while (lgetc(lex) != '\'') {
		if (lisnl(lex) || lex->ch == EOB) {
			lerror(lex, "unclosed string value.");
		}
		lstore(lex, lex->ch);
	}
	lgetc(lex);
	data->s = lload(lex); /* build string */
}

static void peekdstr(alexer_t* lex, union alo_TokenData* data) {
	lgetc(lex);
	next:
	switch (lex->ch) {
	case EOB:
		lerror(lex, "unclosed string value");
		break;
	case '\r':
		if (lgetc(lex) == '\n') {
			lgetc(lex);
		}
		goto newline;
	case '\n':
		lgetc(lex);
	newline:
		while (lex->ch == ' ' || lex->ch == '\t') {
			lgetc(lex);
		}
		if (lex->ch == '\\') {
			lgetc(lex);
		}
		lstore(lex, '\n');
		lex->cl++;
		goto next;
	case '\\':
		switch (lgetc(lex)) {
		case 'a': lstore(lex, '\a'); break;
		case 'b': lstore(lex, '\b'); break;
		case 'f': lstore(lex, '\f'); break;
		case 'n': lstore(lex, '\n'); break;
		case 'r': lstore(lex, '\r'); break;
		case 't': lstore(lex, '\t'); break;
		case '\'': lstore(lex, '\''); break;
		case '\"': lstore(lex, '\"'); break;
		case '\\': lstore(lex, '\\'); break;
		case 'x': {
			if (!aisxdigit(lgetc(lex))) {
				lerror(lex, "illegal escape character");
			}
			int ch = axdigit(lex->ch);
			if (aisxdigit(lgetc(lex))) {
				ch = ch << 4 | axdigit(lex->ch);
				lgetc(lex);
			}
			lstore(lex, ch);
			goto next;
		}
		case '0' ... '3': {
			int ch = lex->ch - '0';
			if ('0' <= lgetc(lex) && lex->ch <= '7') {
				ch = ch << 3 | (lex->ch - '0');
				if ('0' <= lgetc(lex) && lex->ch <= '7') {
					ch = ch << 3 | (lex->ch - '0');
					lgetc(lex);
				}
			}
			lstore(lex, ch);
			goto next;
		}
		}
		lgetc(lex);
		goto next;
	default:
		lstore(lex, lex->ch);
		lgetc(lex);
		goto next;
	case '"':
		break;
	}
	lgetc(lex);
	data->s = lload(lex); /* build string */
}

static int peek(alexer_t* lex, union alo_TokenData* data) {
	next:
	switch (lex->ch) {
	case EOB: /* end of buffer */
		lgetc(lex);
		return TK_EOF;
	case ' ':
		lgetc(lex);
		goto next;
	case '\t':
		lgetc(lex);
		goto next;
	case '\f':
		lgetc(lex);
		goto next;
	case ':':
		switch (lgetc(lex)) {
		case '=':
			lgetc(lex);
			return TK_ACOL;
		case ':':
			lgetc(lex);
			return TK_BCOL;
		}
		return ':';
	case '.':
		switch (lgetc(lex)) {
		case '.':
			switch (lgetc(lex)) {
			case '.':
				lgetc(lex);
				return TK_TDOT;
			}
			return TK_BDOT;
		case '=':
			lgetc(lex);
			return TK_ABDOT;
		}
		return '.';
	case '+':
		switch (lgetc(lex)) {
		case '=':
			lgetc(lex);
			return TK_AADD;
		}
		return '+';
	case '-':
		switch (lgetc(lex)) {
		case '=':
			lgetc(lex);
			return TK_ASUB;
		case '>':
			lgetc(lex);
			return TK_RARR;
		case '-':
			lgetc(lex);
			while (!lisnl(lex) && lex->ch != EOB) {
				lgetc(lex);
			}
			goto next;
		}
		return '-';
	case '*':
		switch (lgetc(lex)) {
		case '=':
			lgetc(lex);
			return TK_AMUL;
		}
		return '*';
	case '/':
		switch (lgetc(lex)) {
		case '/':
			lgetc(lex);
			switch (lex->ch) {
			case '=':
				lgetc(lex);
				return TK_AIDIV;
			}
			return TK_IDIV;
		case '=':
			lgetc(lex);
			return TK_ADIV;
		}
		return '/';
	case '%':
		switch (lgetc(lex)) {
		case '=':
			lgetc(lex);
			return TK_AMOD;
		}
		return '%';
	case '^':
		switch (lgetc(lex)) {
		case '=':
			lgetc(lex);
			return TK_APOW;
		}
		return '^';
	case '<':
		switch (lgetc(lex)) {
		case '-':
			lgetc(lex);
			return TK_LARR;
		case '=':
			lgetc(lex);
			return TK_LE;
		case '<':
			switch (lgetc(lex)) {
			case '=':
				lgetc(lex);
				return TK_ASHL;
			}
			return TK_SHL;
		}
		return '<';
	case '>':
		switch (lgetc(lex)) {
		case '=':
			lgetc(lex);
			return TK_GE;
		case '>':
			lgetc(lex);
			switch (lex->ch) {
			case '=':
				lgetc(lex);
				return TK_ASHR;
			}
			return TK_SHR;
		}
		return '>';
	case '=':
		switch (lgetc(lex)) {
		case '=':
			lgetc(lex);
			return TK_EQ;
		}
		return '=';
	case '&':
		switch (lgetc(lex)) {
		case '=':
			lgetc(lex);
			return TK_AAND;
		case '&':
			lgetc(lex);
			return TK_AND;
		}
		return '&';
	case '|':
		switch (lgetc(lex)) {
		case '=':
			lgetc(lex);
			return TK_AOR;
		case '|':
			lgetc(lex);
			return TK_OR;
		}
		return '|';
	case '~':
		switch (lgetc(lex)) {
		case '=':
			lgetc(lex);
			return TK_AXOR;
		}
		return '~';
	case '!':
		switch (lgetc(lex)) {
		case '=':
			lgetc(lex);
			return TK_NE;
		}
		return '!';
	case '\r': {
		if (lgetc(lex) == '\n') { /* '\r\n' */
			lgetc(lex);
		}
		goto newline;
	}
	case '\n': { /* new line */
		lgetc(lex);
		newline:
		lex->cl++; /* increase line number. */
		goto next;
	}
	case 'A' ... 'Z':
	case 'a' ... 'z':
	case '_':
		return peekidt(lex, data);
	case '0' ... '9':
		return peeknum(lex, data);
	case '\'':
		peeksstr(lex, data);
		return TK_STRING;
	case '\"':
		peekdstr(lex, data);
		return TK_STRING;
	default: {
		int ch = lex->ch;
		lgetc(lex);
		return ch;
	}
	}
	return TK_ERR;
}

int aloX_poll(alexer_t* lex) {
	if (lex->nt.t) {
		lex->ct = lex->nt;
		lex->nt.t = TK_UNDEF;
	} else {
		lex->pl = lex->cl;
		lex->ct.t = peek(lex, &lex->ct.d);
	}
	return lex->ct.t;
}

int aloX_forward(alexer_t* lex) {
	if (!lex->nt.t) {
		lex->nt.t = peek(lex, &lex->nt.d);
	}
	return lex->nt.t;
}
