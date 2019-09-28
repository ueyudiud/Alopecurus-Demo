/*
 * astrlib.c
 *
 * library for string.
 *
 *  Created on: 2019年7月31日
 *      Author: ueyudiud
 */

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

#include <ctype.h>
#include <string.h>

#define MAX_HEAPBUF_LEN 256

/**
 ** reverse string.
 ** prototype: string.reverse(string)
 */
static int str_reverse(astate T) {
	size_t len;
	const char* src = aloL_checklstring(T, 0, &len);
	if (len < MAX_HEAPBUF_LEN) { /* for short string, use heap memory */
		char buf[len];
		for (size_t i = 0; i < len; ++i) {
			buf[i] = src[len - 1 - i];
		}
		alo_pushlstring(T, buf, len);
	}
	else { /* or use memory buffer instead */
		ambuf_t buf;
		aloL_bempty(T, &buf);
		aloL_bcheck(&buf, len);
		for (size_t i = 0; i < len; ++i) {
			aloL_bsetc(&buf, i, src[len - 1 - i]);
		}
		aloL_bsetlen(&buf, len);
		aloL_bpushstring(&buf);
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
	T_STR, /* string, data = pattern length, str = pattern */
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

typedef struct alo_MatchGroup {
	astr begin, end; /* begin and end of matched string */
} amgroup_t;

typedef struct alo_MatchState {
	astate T;
	astr sbegin, send; /* begin and end of source */
	int depth; /* rule depth */
	amgroup_t* groups; /* group buffer */
} amstat_t;

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
typedef int (*amcon_t)(astr);

/* matching function */
typedef int (*amfun_t)(amstat_t*, amnode_t*, amgroup_t*, astr, amcon_t);

/* matcher function */

static int nocon(astr s) {
	return true;
}

static int matchx(amstat_t*, amnode_t*, amgroup_t*, astr, int(*)(astr));

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

static int m_bol(amstat_t* M, amnode_t* node, amgroup_t* group, astr s, int (*con)(astr)) {
	return (s == M->sbegin || s[-1] == '\r' || s[-1] == '\n') && con(s);
}

static int m_eol(amstat_t* M, amnode_t* node, amgroup_t* group, astr s, int (*con)(astr)) {
	return (s == M->send || s[1] == '\r' || s[1] == '\n') && con(s);
}

static int m_chr(amstat_t* M, amnode_t* node, amgroup_t* group, astr s, int (*con)(astr)) {
	return node->data == sgetc(M, s) && con(s);
}

static int m_str(amstat_t* M, amnode_t* node, amgroup_t* group, astr s, int (*con)(astr)) {
	astr p = node->str;
	next:
	switch (*p) {
	case '\0': break; /* end of string */
	case '<':
		group->begin = node->str;
		break;
	case '>':
		group->end = node->str;
		break;
	case '\\': {
		if (!match_class(sgetc(M, s), *(p + 1))) {
			return false;
		}
		p += 2;
		goto next;
	}
	default: {
		if (sgetc(M, s) != *p) {
			return false;
		}
		p += 1;
		goto next;
	}
	}
	return con(s);
}

static int m_set(amstat_t* M, amnode_t* node, amgroup_t* group, astr s, int (*con)(astr)) {
	astr p = node->str;
	int ch = sgetc(M, s);
	while (*p) {
		if (*p == '\\') {
			if (match_class(*(p + 1), ch) != node->extra) {
				return con(s);
			}
			p += 1;
		}
		if (*(p + 1) == '-') {
			if ((*p < ch && ch < *(p + 2)) != node->extra) {
				return con(s);
			}
			p += 3;
		}
		else {
			if ((*p == ch) != node->extra) {
				return con(s);
			}
			p += 1;
		}
	}
	return false;
}

static int m_sub(amstat_t* M, amnode_t* node, amgroup_t* group, astr s, int (*con)(astr)) {
	int con1(astr result) {
		if (!group->end) {
			group->end = result;
		}
		return con(result);
	}

	for (size_t i = 0; i < node->data; ++i) {
		if (node->extra >= 0) {
			group = &M->groups[node->extra];
			group->begin = s;
			group->end = NULL;
		}

		if (matchx(M, node->arr[i], group, s, con1)) {
			return true;
		}
	}
	return false;
}

static int m_seq(amstat_t* M, amnode_t* node, amgroup_t* group, astr s, int (*con)(astr)) {
	int check(astr s, int index) {
		int con1(astr s1) {
			if (M->depth-- == 0) {
				aloL_error(M->T, 2, "matching string is too complex.");
			}
			int result = index + 1 == node->data ? con(s1) : check(s1, index + 1);
			return result;
		}
		return matchx(M, node->arr[index], group, s, con1);
	}
	return check(s, 0);
}

static int matchx(amstat_t* M, amnode_t* node, amgroup_t* group, astr s, int (*con)(astr)) {
	static const amfun_t handles[] = { NULL, m_bol, m_eol, m_chr, m_str, m_set, m_sub, m_seq };
	int norm_con(astr result) {
		s = result;
		return true;
	}
	if (M->depth-- == 0) {
		aloL_error(M->T, 2, "matching string is too complex.");
	}
	amfun_t handle = handles[node->type];
	switch (node->mode) {
	case M_NORMAL: {
		return handle(M, node, group, s, con);
	}
	case M_MORE: {
		if (!handle(M, node, group, s, norm_con)) {
			return false;
		}
	case M_ANY:
		do if (con(s)) {
			return true;
		}
		while (handle(M, node, group, s, norm_con));
		return false;
	}
	case M_MOREG: case M_ANYG: {
		int greedy_con(astr result) {
			if (handle(M, node, group, result, norm_con) && greedy_con(s)) {
				return true;
			}
			return con(result);
		}
		return handle(M, node, group, s, greedy_con) || (node->mode == M_ANYG && con(s));
	}
	case M_OPT: {
		return con(s) || handle(M, node, group, s, con);
	}
	case M_OPTG: {
		return handle(M, node, group, s, con) || con(s);
	}
	default: { /* illegal error */
		return false;
	}
	}
}

static void minit(amstat_t* M, astate T, astr src, size_t len, amgroup_t* groups) {
	M->T = T;
	M->sbegin = src;
	M->send = src + len;
	M->groups = groups;
}

static int match(amstat_t* M, amnode_t* node, astr s) {
	M->depth = MAX_MATCH_DEPTH;
	M->groups[0].begin = s;
	M->groups[0].end = NULL;
	return matchx(M, node, NULL, s, nocon);
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

#define cerror(C,msg,args...) { destory((C)->T, (C)->node); aloL_error((C)->T, 2, msg, ##args); }

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
			aloL_error(C->T, 2, "no enough memory.");
		}
		for (int i = len; i < newsize; ++i) {
			children[i] = NULL;
		}
		node->arr = children;
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
	case '+': case '*': case '?': case '(':
	case '[': case '^': case '$': case '|':
		break;
	case '\0':
		if (C->pos == C->end) {
			break;
		}
		goto single;
	single: default:
		if (C->length > 0 && strchr(MODIFIER_CHAR, *(C->pos + 1))) { /* modifier found, skip */
			break;
		}
		cput(C, *C->pos);
		C->pos += 1;
		goto next;
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
		break;
	}
	case '$': {
		*node = construct(C, T_EOL);
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
	switch (*C->pos) {
	case '\0':
		if (C->pos == C->end) { /* end of string */
			break;
		}
		goto single;
	single: default:
		p_suffix(C, alloc(C, *node, len++));
		break;
	case ']': case '}':
		cerror(C, "illegal pattern.");
		break;
	case ')':
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
	Matcher matcher;
	size_t len;
	const char* src = alo_tolstring(T, 0, &len);
	compile(&matcher, T, src, len);
	amstat_t M;
	amgroup_t groups[matcher.ngroup];
	src = alo_tolstring(T, 1, &len);
	minit(&M, T, src, len, groups);
	alo_pushboolean(T, match(&M, matcher.node, src) && M.groups[0].end - M.groups[0].begin == len);
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
	Matcher* matcher = aloE_cast(Matcher*, alo_torawdata(T, 0));
	if (matcher->node == NULL) {
		aloL_error(T, 2, "matcher already destroyed.");
	}
	return matcher;
}

static int matcher_match(astate T) {
	Matcher* matcher = self(T);
	amstat_t M;
	amgroup_t groups[matcher->ngroup];
	const char* src;
	size_t len;
	src = alo_tolstring(T, 1, &len);
	minit(&M, T, src, len, groups);
	int flag = match(&M, matcher->node, src);
	alo_pushboolean(T, flag && groups[0].end - groups[0].begin == len);
	return 1;
}

static int str_matcher(astate T) {
	Matcher* matcher = alo_newobj(T, Matcher);
	matcher->node = NULL;
	matcher->ngroup = 0;
	if (aloL_getsimpleclass(T, "__matcher")) {
		static const acreg_t funcs[] = {
				{ "__del", matcher_gc },
				{ "__call", matcher_match },
				{ NULL, NULL }
		};
		aloL_setfuns(T, -1, funcs);
	}
	alo_setmetatable(T, -2);
	size_t len;
	const char* src = alo_tolstring(T, 0, &len);
	compile(matcher, T, src, len);
	return 1;
}

static const acreg_t mod_funcs[] = {
	{ "match", str_match },
	{ "matcher", str_matcher },
	{ "reverse", str_reverse },
	{ NULL, NULL }
};

int aloopen_strlib(astate T) {
	alo_bind(T, "string.reverse", str_reverse);
	alo_bind(T, "string.match", str_match);
	alo_bind(T, "string.matcher", str_matcher);
	alo_getreg(T, "__basic_delegates");
	alo_rawgeti(T, -1, ALO_TSTRING);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
