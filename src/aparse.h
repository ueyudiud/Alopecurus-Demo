/*
 * aparse.h
 *
 * script parsing.
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

ALO_IFUN int aloP_parse(alo_State, a_cstr, aibuf_t*, alo_Proto**, alo_Str**);

enum {
	SYMBOL_LOC,
	SYMBOL_VAR
};

typedef struct alo_Symbol {
	alo_Str* name; /* symbol name */
	int type; /* symbol type */
	int index; /* symbol index */
	uint16_t layer; /* layer of function defined symbol */
} asymbol;

typedef struct {
	alo_Str* name; /* label name */
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
	CV_UNBOX /* for unbox variable, pattern: _@f(...) */
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
	struct { /* function name cache */
		alo_Str** a;
		size_t c;
	} fn;
	altable_t jp; /* list of pending jump instruction */
	altable_t lb; /* list of active labels */
	alo_Proto* p; /* root prototype */
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
	E_CONCAT, /* concat string, c.s = concat starting index, c.l = concat length */
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
		a_int i; /* integer value */
		a_float f; /* float value */
		alo_Str* s; /* string value */
		int g; /* generic use */
		struct { /* index data */
			uint16_t o; /* owner index */
			uint16_t k; /* key index */
			a_byte fo: 1; /* owner type */
			a_byte fk: 1; /* key type */
		} d;
		struct { /* array data */
			uint16_t s; /* start index */
			uint16_t l; /* length */
		} a;
	} v;
	int lt, lf; /* unpredicted label for true/false */
} aestat_t;

#define NO_CASEVAR (-1)

/* not-free variable for partial function */
struct alo_CaseVar {
	alo_Str* name; /* variable name */
	aestat_t expr; /* unbox/match expression */
	size_t nchild; /* number of child variables */
	int parent; /* index of parent node */
	int prev; /* index of previous node */
	int next; /* index of next node */
	int line; /* nested line */
	uint16_t src; /* source index */
	a_byte type; /* variable type */
	a_byte flag;
};

struct alo_Block {
	ablock_t* prev; /* previous block */
	a_cstr lname; /* label name */
	size_t flabel; /* index of first label in this block */
	size_t fjump; /* index of first jump in this block */
	size_t fsymbol; /* first symbol index */
	int lcon; /* index of continue */
	int lout; /* index of jump from break */
	uint16_t nlocal; /* last index of local or temporary variable at beginning of this block */
	a_byte incap; /* true when some local variable in this block is capture */
	a_byte loop; /* true when block is loop */
};

struct alo_FuncState {
	alexer_t* l; /* lexer */
	alo_Proto* p; /* prototype */
	afstat_t* e; /* enclosing function */
	apdata_t* d; /* protected data */
	ablock_t* b; /* current block */
	int ncode;
	int nconst;
	int nchild;
	int nlocvar; /* number of named local variables */
	int nline; /* number of line number */
	int cjump; /* current label for jump */
	int lastline; /* line number for last instruction */
	int firstsym; /* first symbol */
	uint16_t ncap;
	uint16_t layer; /* layer of nested function depth */
	uint16_t firsttemp; /* first temporary variable offset */
	uint16_t firstlocal; /* first local value offset */
	uint16_t freelocal; /* first free local variable */
	ablock_t broot[1]; /* root block */
};

#endif /* APARSE_H_ */
