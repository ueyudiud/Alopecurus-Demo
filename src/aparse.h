/*
 * aparse.h
 *
 *  Created on: 2019年7月30日
 *      Author: ueyudiud
 */

#ifndef APARSE_H_
#define APARSE_H_

#include "aobj.h"
#include "abuf.h"

typedef struct alo_FuncState afstat_t;
typedef struct alo_CaseVar acasevar_t;
typedef struct alo_Block ablock_t;
typedef struct alo_PData apdata_t;
typedef struct alo_Lexer alexer_t;

int aloP_parse(astate, astr, aibuf_t*, aproto_t**);

enum {
	SYMBOL_FIX,
	SYMBOL_VAR,
	SYMBOL_LOC
};

typedef struct alo_Symbol {
	astring_t* name; /* symbol name */
	int type; /* symbol type */
	int index; /* symbol index */
	uint16_t layer; /* layer of function defined symbol */
} asymbol;

typedef struct {
	astring_t* name; /* label name */
	size_t pc; /* position in code */
	int line; /* line number of label */
	uint16_t layer; /* the function layer */
	uint16_t nactvar; /* number of local variable */
} alabel;

typedef struct {
	alabel* a;
	size_t c;
	size_t l;
} altable_t;

enum {
	CV_NORMAL, /* free variable, pattern: _ */
	CV_MATCH, /* match variable, pattern: ?_ */
	CV_UNBOX, /* for unbox variable, pattern: _@f(...) */
	CV_UNBOXSEQ, /* for unbox sequence, pattern: _@f[...] */
};

/* the protected data needed used in parser */
struct alo_PData {
	struct { /* symbol table */
		asymbol* a;
		size_t c;
		size_t l;
	} ss;
	struct { /* cast variable table */
		acasevar_t* a;
		size_t c;
		size_t l;
		size_t nchild;
	} cv;
	altable_t jp; /* list of pending jump instruction */
	altable_t lb; /* list of active labels */
	aproto_t* p; /* root prototype */
};

enum {
	E_VOID, /* void expression */
	E_NIL, /* nil constant */
	E_TRUE, /* true constant */
	E_FALSE, /* false constant */
	E_INTEGER, /* integer constant, i = numeral integer value */
	E_FLOAT, /* float constant, f = numeral float value */
	E_STRING, /* string constant, s = string value */

	E_LOCAL, /* local variable, g = register index */
	E_CAPTURE, /* capture variable, g = capture index */
	E_INDEXED, /* indexed variable,
	              d.o = owner index
	              d.k = key index */
	E_VARARG, /* variable argument */
	E_CALL, /* variable result from function call, g = pc of instruction */
	E_UNBOX, /* variable result from unbox, g = pc of instruction */
	E_JMP, /* jumping to label when false, g = pc of corresponding jump instruction */

	E_CONST, /* any kinds of constant, g = constant index */
	E_FIXED, /* register value, g = register index */
	E_ALLOC /* wait for allocating a register, g = pc of instruction */
};

/* expression state */
typedef struct alo_ExprStat {
	int t;
	union {
		aint i; /* integer value */
		afloat f; /* float value */
		astring_t* s; /* string value */
		size_t g; /* generic use */
		struct {
			uint16_t o; /* owner index */
			uint16_t k; /* key index */
			abyte fo: 1; /* owner type */
			abyte fk: 1; /* key type */
		} d;
	} v;
	int lt, lf; /* unpredicted label for true/false */
} aestat_t;

#define NO_CASEVAR (-1)

/* not-free variable for partial function */
struct alo_CaseVar {
	astring_t* name; /* variable name */
	aestat_t expr; /* unbox/match expression */
	size_t nchild; /* number of child variables */
	int parent; /* index of parent node */
	int prev; /* index of previous node */
	int next; /* index of next node */
	int line; /* nested line */
	uint16_t src; /* source index*/
	uint16_t dest; /* destination variable index */
	abyte type; /* variable type */
	abyte flag;
};

struct alo_FuncState {
	alexer_t* l; /* lexer */
	aproto_t* p; /* prototype */
	afstat_t* e; /* enclosing function */
	apdata_t* d; /* protected data */
	ablock_t* b; /* current block */
	size_t ncode;
	size_t nconst;
	size_t nchild;
	size_t firstlocal; /* first local variable offset */
	int cjump; /* current label for jump */
	uint16_t ncap;
	uint16_t layer; /* layer of nested function depth */
	uint16_t freelocal; /* first free local variable */
	uint16_t nactvar; /* number of active variables */
	uint16_t nlocvar; /* number of named local variables */
};

#endif /* APARSE_H_ */
