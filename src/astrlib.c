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

/* max match group number */
#define MAX_GROUP_SIZE 64

/* max pattern match in greedy mode */
#define MAX_MATCH_DEPTH 1024

/* no node */
#define NO_NODE (-1)

typedef struct alo_MatchNode amnode_t;

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

struct alo_MatchNode {
	union {
		void* ptr;
		astr str; /* string data */
		amnode_t** arr; /* array of children */
	};
	int data; /* integral data */
	abyte type; /* node type */
	abyte mode; /* node mode */
	abyte extra; /* extra data */
};

/* matching group */
typedef struct alo_MatchGroup {
	astr begin, end; /* begin and end of matched string */
} amgroup_t;

/* match state */
typedef struct alo_MatchState {
	astate T;
	astr sbegin, send; /* begin and end of source */
	int depth; /* rule depth */
	amgroup_t* groups; /* group buffer */
} amstat_t;

/* compile state */
typedef struct alo_CompileState {
	astate T;
	aalloc alloc;
	void* context;
	amnode_t* node; /* root node */
	astr pos; /* current position */
	astr end; /* end of source */
	int index; /* group index */
	struct {
		size_t length;
		char array[256];
	}; /* string buffer */
} acstat_t;

/* end of string */
#define EOS (-1)

#define sgetc(M,s) ((M)->send == (s) ? EOS : *((s)++))

/* matching continuation */
typedef void* amcon_t;

struct alo_MatchContinuation {
	astr (*handle)(amstat_t*, astr, void*);
	abyte context[];
};

#define makec(n,f,t,as...) struct { typeof(f)* _f; t _as; } n = { f, { as } };

#define callc(M,c,s) \
	({ \
		struct alo_MatchContinuation* _con = aloE_cast(struct alo_MatchContinuation*, c); \
		_con ? _con->handle(M, s, _con->context) : s; \
	})

/* matching function */
typedef astr (*amfun_t)(amstat_t*, amnode_t*, astr, amcon_t);

/* matcher function */
static astr matchx(amstat_t*, amnode_t*, astr, amcon_t);

#define matchs(M,node,s,con) \
	((node)->mode == M_NORMAL ? handles[(node)->type] : matchx)(M, node, s, con)

/* character used in class */
#define CLASS_CHAR "acdglpsuwx"

/* character used for modifier */
#define MODIFIER_CHAR "+*?"

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

static const amfun_t handles[];

static astr m_bol(amstat_t* M, __attribute__((unused)) amnode_t* node, astr s, amcon_t con) {
	return (s == M->sbegin || s[-1] == '\r' || s[-1] == '\n') ? callc(M, con, s) : NULL;
}

static astr m_eol(amstat_t* M, __attribute__((unused)) amnode_t* node, astr s, amcon_t con) {
	return (s == M->send || s[1] == '\r' || s[1] == '\n') ? callc(M, con, s) : NULL;
}

static astr m_chr(amstat_t* M, amnode_t* node, astr s, amcon_t con) {
	return node->data == sgetc(M, s) ? callc(M, con, s) : NULL;
}

static astr m_str(amstat_t* M, amnode_t* node, astr s, amcon_t con) {
	astr p = node->str;
	next:
	switch (*p) {
	case '\0': break; /* end of string */
	case '\\': {
		if (!match_class(sgetc(M, s), *(p + 1))) {
			return NULL;
		}
		p += 2;
		goto next;
	}
	default: {
		if (sgetc(M, s) != *p) {
			return NULL;
		}
		p += 1;
		goto next;
	}
	}
	return callc(M, con, s);
}

static astr m_set(amstat_t* M, amnode_t* node, astr s, amcon_t con) {
	astr p = node->str;
	int ch = sgetc(M, s);
	if (ch == EOS) {
		return NULL;
	}
	while (*p) {
		if (*p == '\\') {
			if (match_class(*(p + 1), ch) != node->extra) {
				return callc(M, con, s);
			}
			p += 1;
		}
		if (*(p + 1) == '-') {
			if ((*p <= ch && ch <= *(p + 2)) != node->extra) {
				return callc(M, con, s);
			}
			p += 3;
		}
		else {
			if ((*p == ch) != node->extra) {
				return callc(M, con, s);
			}
			p += 1;
		}
	}
	return NULL;
}

struct context_sub {
	astr begin;
	amcon_t con;
	abyte index;
};

static astr subcon(amstat_t* M, astr s, struct context_sub* context) {
	M->groups[context->index] = (amgroup_t) { context->begin, s }; /* settle group position */
	return callc(M, context->con, s);
}

static astr m_sub(amstat_t* M, amnode_t* node, astr s, amcon_t con) {
	makec(con1, subcon, struct context_sub, s, con, node->extra);
	astr e;
	amgroup_t* group = &M->groups[node->extra];
	group->begin = NULL;
	for (int i = 0; i < node->data; ++i) {
		if ((e = matchs(M, node->arr[i], s, node->extra > 0 ? &con1 : con))) {
			group->begin = group->begin ?: s;
			return e;
		}
	}
	return NULL;
}

struct context_seq {
	amnode_t* node;
	amcon_t con;
	int index;
};

static astr seqcheck(amstat_t*, amnode_t*, astr, amcon_t, int);

static astr seqcon(amstat_t* M, astr s, struct context_seq* context) {
	if (M->depth-- == 0) {
		aloL_error(M->T, "matching string is too complex.");
	}
	return seqcheck(M, context->node, s, context->con, context->index + 1);
}

static astr seqcheck(amstat_t* M, amnode_t* node, astr s, amcon_t con, int index) {
	makec(con1, seqcon, struct context_seq, node, con, index);
	return matchs(M, node->arr[index], s, index + 1 == node->data ? con : &con1);
}

static astr m_seq(amstat_t* M, amnode_t* node, astr s, amcon_t con) {
	return seqcheck(M, node, s, con, 0);
}

static const amfun_t handles[] = { NULL, m_bol, m_eol, m_chr, m_str, m_set, m_sub, m_seq };

struct context_greedy {
	amfun_t handle;
	amnode_t* node;
	amcon_t con;
};

static astr greedycon(amstat_t* M, astr s, struct context_greedy* context) {
	astr e = context->handle(M, context->node, s, NULL);
	return e && (e = greedycon(M, e, context)) ? e : callc(M, context->con, s);
}

static astr matchx(amstat_t* M, amnode_t* node, astr s, amcon_t con) {
	if (M->depth-- == 0) {
		aloL_error(M->T, "matching string is too complex.");
	}
	amfun_t handle = handles[node->type];
	switch (node->mode) {
	case M_NORMAL: {
		return handle(M, node, s, con);
	}
	case M_MORE: {
		if (!(s = handle(M, node, s, NULL))) {
			return NULL;
		}
	case M_ANY: {}
		astr t;
		do if ((t = callc(M, con, s))) {
			return t;
		}
		while ((s = handle(M, node, s, NULL)));
		return NULL;
	}
	case M_MOREG: case M_ANYG: {
		makec(con1, greedycon, struct context_greedy, handle, node, con);
		return handle(M, node, s, &con1) ?: node->mode == M_ANYG ? callc(M, con, s) : NULL;
	}
	case M_OPT: {
		return callc(M, con, s) ?: handle(M, node, s, con);
	}
	case M_OPTG: {
		return handle(M, node, s, con) ?: callc(M, con, s);
	}
	default: { /* illegal error */
		return NULL;
	}
	}
}

static void minit(amstat_t* M, astate T, astr src, size_t len, amgroup_t* groups) {
	M->T = T;
	M->sbegin = src;
	M->send = src + len;
	M->groups = groups;
}

struct context_full {
	astr end;
};

static astr fullcon(__attribute__((unused)) amstat_t* M, astr s, struct context_full* context) {
	return s == context->end ? s : NULL;
}

static int match(amstat_t* M, amnode_t* node, astr s, int full) {
	M->depth = MAX_MATCH_DEPTH;
	amgroup_t* group = &M->groups[0];
	group->begin = group->end = NULL;
	astr begin = s, end = M->send;
	makec(con1, fullcon, struct context_full, end);
	end = matchx(M, node, s, full ? &con1 : NULL);
	if (end) {
		group->begin = group->begin ?: begin;
		group->end = group->end ?: end;
		return true;
	}
	return false;
}

static void destory(astate T, amnode_t* node) {
	if (node == NULL) {
		return;
	}
	void* context;
	aalloc alloc = alo_getalloc(T, &context);
	switch (node->type) {
	case T_STR: case T_SET:
		alloc(context, (void*) node->str, node->data * sizeof(char), 0);
		break;
	case T_SUB: case T_SEQ:
		for (int i = 0; i < node->data; ++i) {
			destory(T, node->arr[i]);
		}
		alloc(context, node->arr, node->data * sizeof(amnode_t*), 0);
		break;
	}
	alloc(T, node, sizeof(amnode_t), 0);
}

#define cerror(C,msg,args...) { destory((C)->T, (C)->node); aloL_error((C)->T, msg, ##args); }

static amnode_t* construct(acstat_t* C, int type) {
	amnode_t* node = C->alloc(C->context, NULL, 0, sizeof(amnode_t));
	if (node == NULL) {
		cerror(C, "no enough memory.");
	}
	node->type = type;
	node->mode = M_NORMAL;
	node->extra = 0;
	node->data = 0;
	node->ptr = NULL;
	return node;
}

static void trim(acstat_t* C, amnode_t* node, int len) {
	node->arr = C->alloc(C->context, node->arr, node->data * sizeof(amnode_t*), len * sizeof(amnode_t*));
	node->data = len;
}

static amnode_t** alloc(acstat_t* C, amnode_t* node, int len) {
	if (len == node->data) {
		int newsize = node->data * 2;
		if (newsize < 4) {
			newsize = 4;
		}
		amnode_t** children = C->alloc(C->context, node->arr, node->data * sizeof(amnode_t*), newsize * sizeof(amnode_t*));
		if (children == NULL) {
			destory(C->T, node);
			aloL_error(C->T, "no enough memory.");
		}
		for (int i = len; i < newsize; ++i) {
			children[i] = NULL;
		}
		node->arr = children;
		node->data = newsize;
	}
	return &node->arr[len];
}

static void build(acstat_t* C, amnode_t* node) {
	int length = C->length;
	C->length = 0;
	void* block = C->alloc(C->context, NULL, 0, length + 1);
	memcpy(block, C->array, length);
	char* s = (char*) block;
	s[length] = '\0';
	node->str = s;
	node->data = length;
}

static void cput(acstat_t* C, int ch) {
	if (C->length >= 256) {
		cerror(C, "pattern too long.");
	}
	C->array[C->length++] = ch;
}

static void p_str(acstat_t* C, amnode_t** pnode) {
	int flag = (*C->pos == '\\') && strchr(CLASS_CHAR, *(C->pos + 1));
	next:
	switch (*C->pos) {
	case '+': case '*': case '?': case '(': case ')':
	case '[': case '^': case '$': case '|':
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
		cput(C, '\\');
		cput(C, C->pos[1]);
		C->pos += 2;
		goto next;
	}
	single: default: {
		if (C->length > 0 && strchr(MODIFIER_CHAR, *(C->pos + 1))) { /* modifier found, skip */
			break;
		}
		cput(C, *C->pos);
		C->pos += 1;
		goto next;
	}
	}
	if (flag || C->length > 1) {
		amnode_t* node = *pnode = construct(C, T_STR);
		build(C, node);
	}
	else {
		amnode_t* node = *pnode = construct(C, T_CHR);
		node->data = C->array[0];
		C->length = 0;
	}
}

static void p_chset(acstat_t* C, amnode_t** pnode) {
	amnode_t* node = *pnode = construct(C, T_SET);
	if (*C->pos == '^') {
		node->extra = true;
		C->pos += 1;
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
	build(C, node);
}

static void p_expr(acstat_t*, amnode_t**);

static void p_atom(acstat_t* C, amnode_t** node) {
	/* atom -> '(' expr ')' | '[' chset ']' */
	switch (*C->pos) {
	case '(': {
		int index = C->index++;
		C->pos += 1;
		p_expr(C, node);
		(*node)->extra = index;
		if (*C->pos != ')') {
			cerror(C, "illegal pattern.");
		}
		C->pos += 1;
		break;
	}
	case '[': {
		C->pos += 1;
		p_chset(C, node);
		C->pos += 1;
		break;
	}
	case '^': {
		*node = construct(C, T_BOL);
		C->pos += 1;
		break;
	}
	case '$': {
		*node = construct(C, T_EOL);
		C->pos += 1;
		break;
	}
	default: {
		p_str(C, node);
		break;
	}
	}
}

static void p_suffix(acstat_t* C, amnode_t** node) {
	/* suffix -> atom [ ('+'|'*'|'?') ['?'] ] */
	p_atom(C, node);
	switch (*C->pos) {
	case '?':
		(*node)->mode = M_OPTG;
		C->pos += 1;
		goto greedy;
	case '+':
		(*node)->mode = M_MOREG;
		C->pos += 1;
		goto greedy;
	case '*':
		(*node)->mode = M_ANYG;
		C->pos += 1;
		goto greedy;
	greedy:
		if (*C->pos == '?') {
			(*node)->mode &= ~0x8; /* add greedy mark */
			C->pos += 1;
		}
		break;
	}
}

static void p_mono(acstat_t* C, amnode_t** node) {
	/* mono -> { suffix } */
	*node = construct(C, T_SEQ);
	p_suffix(C, alloc(C, *node, 0));
	size_t len = 1;
	next:
	switch (*C->pos) {
	case '\0':
		if (C->pos == C->end) { /* end of string */
			break;
		}
		goto single;
	single: default:
		p_suffix(C, alloc(C, *node, len++));
		goto next;
	case ']': case '}':
		cerror(C, "illegal pattern.");
		break;
	case ')': case '|':
		break;
	}
	if (len != 1) {
		trim(C, *node, len);
	}
	else {
		amnode_t* n = *node;
		*node = n->arr[0];
		n->arr[0] = NULL;
		destory(C->T, n);
	}
}

static void p_expr(acstat_t* C, amnode_t** node) {
	/* expr -> mono { '|' mono } */
	*node = construct(C, T_SUB);
	p_mono(C, alloc(C, *node, 0));
	size_t len = 1;
	while (*C->pos == '|') {
		C->pos += 1;
		p_mono(C, alloc(C, *node, len++));
	}
	trim(C, *node, len);
}

typedef struct {
	amnode_t* node; /* root node */
	size_t ngroup; /* number of group */
} Matcher;

static void compile(Matcher* matcher, astate T, const char* src, size_t len) {
	acstat_t C;
	C.T = T;
	C.alloc = alo_getalloc(T, &C.context);
	C.length = 0;
	C.index = 1;
	C.pos = src;
	C.end = src + len;
	C.node = NULL;
	p_expr(&C, &C.node);
	if (C.end != C.pos) {
		cerror(&C, "illegal pattern.");
	}
	C.node->extra = 0;
	matcher->node = C.node;
	matcher->ngroup = C.index;
}

static int str_match(astate T) {
	aloL_checkstring(T, 0);
	aloL_checkstring(T, 1);
	Matcher matcher;
	size_t len;
	const char* src = alo_tolstring(T, 0, &len);
	compile(&matcher, T, src, len);
	amstat_t M;
	amgroup_t groups[matcher.ngroup];
	src = alo_tolstring(T, 1, &len);
	minit(&M, T, src, len, groups);
	alo_pushboolean(T, match(&M, matcher.node, src, true));
	destory(T, matcher.node);
	return 1;
}

static int matcher_gc(astate T) {
	Matcher* matcher = aloE_cast(Matcher*, alo_torawdata(T, 0));
	if (matcher->node) {
		destory(T, matcher->node);
		matcher->node = NULL;
	}
	return 0;
}

static Matcher* self(astate T) {
	aloL_checktype(T, 0, ALO_TRAWDATA);
	Matcher* matcher = aloE_cast(Matcher*, alo_torawdata(T, 0));
	if (matcher->node == NULL) {
		aloL_error(T, "matcher already destroyed.");
	}
	return matcher;
}

static int matcher_test(astate T) {
	Matcher* matcher = self(T);
	aloL_checkstring(T, 1);
	amstat_t M;
	amgroup_t groups[matcher->ngroup];
	const char* src;
	size_t len;
	src = alo_tolstring(T, 1, &len);
	minit(&M, T, src, len, groups);
	alo_pushboolean(T, match(&M, matcher->node, src, true));
	return 1;
}

static int matcher_match(astate T) {
	Matcher* matcher = self(T);
	amstat_t M;
	amgroup_t groups[matcher->ngroup];
	const char* src;
	size_t len;
	src = alo_tolstring(T, 1, &len);
	minit(&M, T, src, len, groups);
	if (!match(&M, matcher->node, src, true)) {
		return 0;
	}
	alo_ensure(T, matcher->ngroup);
	for (size_t i = 0; i < matcher->ngroup; ++i) {
		amgroup_t* group = &groups[i];
		if (group->begin != NULL) {
			alo_pushlstring(T, group->begin, group->end - group->begin);
		}
		else {
			alo_pushnil(T);
		}
	}
	return matcher->ngroup;
}

static void aux_replaces(astate T, Matcher* matcher, amstat_t* M, ambuf_t* buf) {
	size_t len;
	const char* s1 = alo_tolstring(T, 2, &len);
	const char* s0 = s1;
	const char* const se = s0 + len;
	while (s1 < se) {
		if (*s1 == '$') { /* escape character? */
			if (s1 != s0) {
				aloL_bputls(T, buf, s0, s1 - s0); /* put normal sub sequence before */
			}
			size_t index;
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
				if (index >= matcher->ngroup) {
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

static void aux_replacef(astate T, __attribute__((unused)) Matcher* matcher, amstat_t* M, ambuf_t* buf) {
	int index = alo_gettop(T);
	alo_push(T, 2); /* push transformer */
	alo_pushlstring(T, M->groups[0].begin, M->groups[0].end - M->groups[0].begin);
	alo_pushinteger(T, M->groups[0].begin - M->sbegin);
	alo_call(T, 2, 1);
	aloL_bwrite(T, buf, index);
	alo_drop(T);
}

static int matcher_replace(astate T) {
	Matcher* matcher = self(T);
	void (*transformer)(astate, Matcher*, amstat_t*, ambuf_t*);
	if (alo_typeid(T, 2) == ALO_TSTRING) {
		transformer = aux_replaces;
	}
	else {
		aloL_checkcall(T, 2);
		transformer = aux_replacef;
	}
	amstat_t M;
	amgroup_t groups[matcher->ngroup];
	const char* src;
	size_t len;
	src = aloL_checklstring(T, 1, &len);
	minit(&M, T, src, len, groups);
	alo_settop(T, 3);
	aloL_usebuf(T, buf) {
		const char* s1 = M.sbegin;
		const char* s0 = s1;
		while (s1 <= M.send) {
			if (match(&M, matcher->node, s1, false)) {
				if (s1 != s0) {
					aloL_bputls(T, buf, s0, s1 - s0);
				}
				(*transformer)(T, matcher, &M, buf);
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

static int mmatch(amstat_t* M, amnode_t* nodes, const char* begin, const char* end) {
	for (const char* s = begin; s <= end; ++s) {
		if (match(M, nodes, s, false)) {
			return true;
		}
	}
	return false;
}

static int matcher_split(astate T) {
	Matcher* matcher = self(T);
	size_t len;
	const char* src = aloL_checklstring(T, 1, &len);
	amstat_t M;
	amgroup_t groups[matcher->ngroup];
	minit(&M, T, src, len, groups);
	size_t n = 0;
	alo_settop(T, 2);
	alo_newlist(T, 0);
	if (mmatch(&M, matcher->node, M.sbegin, M.send)) {
		do
		{
			alo_pushlstring(T, M.sbegin, groups->begin - M.sbegin);
			alo_rawseti(T, 2, n++);
			M.sbegin = groups->end;
		}
		while (mmatch(&M, matcher->node, M.sbegin, M.send));
		alo_pushlstring(T, M.sbegin, M.send - M.sbegin);
	}
	else {
		alo_push(T, 1);
	}
	alo_rawseti(T, 2, n);
	return 1;
}

static void addnodeinfo(astate T, amnode_t* node, ambuf_t* buf) {
	switch (node->type) {
	case T_END: break;
	case T_BOL:
		aloL_bputc(T, buf, '^');
		break;
	case T_EOL:
		aloL_bputc(T, buf, '$');
		break;
	case T_CHR:
		aloL_bputf(T, buf, "%u", node->data);
		break;
	case T_STR:
		aloL_bputf(T, buf, "%\"", node->str, aloE_cast(size_t, node->data));
		break;
	case T_SUB:
		aloL_bputc(T, buf, '(');
		for (int i = 0; i < node->data; ++i) {
			addnodeinfo(T, node->arr[i], buf);
			aloL_bputc(T, buf, '|');
		}
		buf->len -= 1;
		aloL_bputc(T, buf, ')');
		break;
	case T_SEQ:
		for (int i = 0; i < node->data; ++i) {
			addnodeinfo(T, node->arr[i], buf);
		}
		break;
	case T_SET:
		aloL_bputf(T, buf, "[%\"]", node->str, aloE_cast(size_t, node->data));
		break;
	}
	switch (node->mode) {
	case M_MORE: aloL_bputs(T, buf, "+?"); break;
	case M_MOREG: aloL_bputs(T, buf, "+"); break;
	case M_ANY: aloL_bputs(T, buf, "*?"); break;
	case M_ANYG: aloL_bputs(T, buf, "*"); break;
	case M_OPT: aloL_bputs(T, buf, "??"); break;
	case M_OPTG: aloL_bputs(T, buf, "?"); break;
	}
}

static int matcher_tostr(astate T) {
	Matcher* matcher = self(T);
	aloL_usebuf(T, buf) {
		addnodeinfo(T, matcher->node, buf);
		alo_pushfstring(T, "__matcher: { ngroup: %d, expr: %q }", (int) matcher->ngroup, buf->buf, buf->len);
	}
	return 1;
}

static int matcher_find(astate T) {
	const char* src;
	size_t len;
	src = aloL_checklstring(T, 1, &len);
	Matcher* matcher = self(T);
	amstat_t M;
	amgroup_t groups[matcher->ngroup];
	aint off = aloL_getoptinteger(T, 2, 0);
	if (off < 0) {
		off += len;
	}
	if (off > aloE_cast(aint, len) || off < 0) {
		aloL_argerror(T, 2, "index out of bound.");
	}
	minit(&M, T, src, len, groups);
	if (mmatch(&M, matcher->node, src + off, M.send)) {
		alo_pushinteger(T, groups[0].begin - M.sbegin);
		alo_pushinteger(T, groups[0].end - M.sbegin);
		return 2;
	}
	return 0;
}

static Matcher* preload(astate T) {
	Matcher* matcher = alo_newobject(T, Matcher);
	matcher->node = NULL;
	matcher->ngroup = 0;
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
	return matcher;
}

static int str_matcher(astate T) {
	size_t len;
	const char* src = aloL_checklstring(T, 0, &len);
	Matcher* matcher = preload(T);
	compile(matcher, T, src, len);
	return 1;
}

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

int aloopen_strlib(astate T) {
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
