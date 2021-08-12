/*
 * aobj.h
 *
 *  Created on: 2019年7月21日
 *      Author: ueyudiud
 */

#ifndef AOBJ_H_
#define AOBJ_H_

#include "art.h"

/**
 ** tagged variants.
 */

#define maketag(t,v) ((t) | (v) << 4)

enum {
	/* variants for string */
	ALO_VHSTR = maketag(ALO_TSTR, 0),
	ALO_VISTR = maketag(ALO_TSTR, 1),
	/* variants for function */
	ALO_VLCF = maketag(ALO_TFUNC, 0),
	ALO_VCCL = maketag(ALO_TFUNC, 1),
	ALO_VACL = maketag(ALO_TFUNC, 3)
};

/* mask for reference */
#define ALO_REF_BIT 0x80

/* useful masks */
#define ALO_TMASK_NUMBER (1 << ALO_TINT | 1 << ALO_TFLOAT)
#define ALO_TMASK_COLLECTION (1 << ALO_TTUPLE | 1 << ALO_TLIST | 1 << ALO_TTABLE)

/**
 ** header predefined.
 */

#define ALO_OBJ_HEADER alo_Obj* gcprev; a_byte tt; a_byte mark
#define ALO_AGGR_HEADER(e) ALO_OBJ_HEADER; e; alo_Obj* gclist
#define ALO_TVAL_HEADER alo_Val v; a_byte tt

/**
 ** type predefined, the description of types see code below.
 */

typedef struct alo_Obj alo_Obj;

typedef union alo_Value alo_Val;
typedef struct alo_TagValue alo_TVal;
typedef struct alo_Capture alo_Capture;

/* index in stack */
typedef alo_TVal *alo_StkId;

/* get type tag from raw type tag. */
#define gettype(tt) ((tt) & 0x7F)

/* get type tag without variant bits from raw type tag. */
#define gettpnv(tt) ((tt) & 0x0F)

/* mask type with specific type tags. */
#define masktype(tt) (1 << gettpnv(tt))

/**
 ** the object in following, may refer to GC target or tagged value.
 */

/* get raw type tag from object. */
#define _rttype(o) ((o)->tt)

/* get type tag from object. */
#define ttype(o) gettype(_rttype(o))

/* get type tag without variant bits from object. */
#define ttpnv(o) gettpnv(_rttype(o))

/* type checking without specific type macros. */
#define checktag(o,t) (ttype(o) == (t))
#define checktype(o,t) (ttpnv(o) == (t))
#define matchtypes(o,m) (masktype(_rttype(o)) & (m))

/* type checking macros. */

#define tisnil(o) checktype(o, ALO_TNIL)
#define tisbool(o) checktype(o, ALO_TBOOL)
#define tisint(o) checktype(o, ALO_TINT)
#define tisflt(o) checktype(o, ALO_TFLOAT)
#define tisnum(o) matchtypes(o, ALO_TMASK_NUMBER)
#define tisptr(o) checktype(o, ALO_TPTR)
#define tisstr(o) checktype(o, ALO_TSTR)
#define tishstr(o) checktag(o, ALO_VHSTR)
#define tisistr(o) checktag(o, ALO_VISTR)
#define tistup(o) checktype(o, ALO_TTUPLE)
#define tislis(o) checktype(o, ALO_TLIST)
#define tistab(o) checktype(o, ALO_TTABLE)
#define tiscol(o) matchtypes(o, ALO_TMASK_COLLECTION)
#define tisfun(o) checktype(o, ALO_TFUNC)
#define tisclo(o) ((_rttype(o) & 0x1F) == (ALO_TFUNC | 1 << 4))
#define tislcf(o) checktag(o, ALO_VLCF)
#define tisccl(o) checktag(o, ALO_VCCL)
#define tisacl(o) checktag(o, ALO_VACL)
#define tisusr(o) checktype(o, ALO_TUSER)
#define tisthr(o) checktype(o, ALO_TTHREAD)
#define tispro(o) checktype(o, ALO_TPROTO)
#define ttiscap(o) checktype(o, ALO_TCAPTURE)
#define tisref(o) (_rttype(o) & ALO_REF_BIT)

/* tagged value get access macros. */

#define tasbool(o) aloE_check(tisbool(o), "'"#o"' is not boolean value", (o)->v.b)
#define tasint(o) aloE_check(tisint(o), "'"#o"' is not integer value", (o)->v.i)
#define tasflt(o) aloE_check(tisflt(o), "'"#o"' is not float value", (o)->v.n)
#define taslcf(o) aloE_check(tislcf(o), "'"#o"' is not light C function value", (o)->v.f)
#define tasptr(o) aloE_check(tisptr(o), "'"#o"' is not pointer value", tgetrptr(o))
#define tasobj(o) aloE_check(tisref(o), "'"#o"' is not reference value", (o)->v.g)
#define tasstr(o) aloE_check(tisstr(o), "'"#o"' is not string value", g2s(tasobj(o)))
#define tastup(o) aloE_check(tistup(o), "'"#o"' is not tuple value", g2a(tasobj(o)))
#define taslis(o) aloE_check(tislis(o), "'"#o"' is not list value", g2l(tasobj(o)))
#define tastab(o) aloE_check(tistab(o), "'"#o"' is not table value", g2m(tasobj(o)))
#define tasclo(o) aloE_check(tisfun(o), "'"#o"' is not closure value", g2c(tasobj(o)))
#define tgetacl(o) aloE_check(tisacl(o), "'"#o"' is not Alopecurus closure value", g2ac(tasobj(o)))
#define tgetccl(o) aloE_check(tisccl(o), "'"#o"' is not C closure value", g2cc(tasobj(o)))
#define tgetthr(o) aloE_check(tisthr(o), "'"#o"' is not thread value", g2t(tasobj(o)))
#define tgetrdt(o) aloE_check(tisusr(o), "'"#o"' is not raw data value", g2r(tasobj(o)))

#define ttonum(o) aloE_check(tisnum(o), "'"#o"' is not number value", tisint(o) ? aloE_flt(tasint(o)) : tasflt(o))

#define trefbool(o) (*aloE_check(tisbool(o), "'"#o"' is not boolean value", &(o)->v.b))
#define trefint(o) (*aloE_check(tisint(o), "'"#o"' is not integer value", &(o)->v.i))
#define trefflt(o) (*aloE_check(tisflt(o), "'"#o"' is not float value", &(o)->v.f))

#define tgetrptr(o) (o)->v.p

/* tagged value construct macros. */

#define NIL_BODY {}, ALO_TNIL

#define tnewnil() (alo_TVal) { NIL_BODY }
#define tnewbool(x) (alo_TVal) { { .b = (x) }, ALO_TBOOL }
#define tnewint(x) (alo_TVal) { { .i = (x) }, ALO_TINT }
#define tnewflt(x) (alo_TVal) { { .n = (x) }, ALO_TFLOAT }
#define tnewlcf(x) (alo_TVal) { { .f = (x) }, ALO_VLCF }
#define tnewptr(x) (alo_TVal) { { .p = (x) }, ALO_TPTR }
#define tnewref(x,t) (alo_TVal) { { .g = r2g(x) }, wrf(t) }
#define tnewrefx(x) tnewref(x, _rttype(x))
#define tnewstr tnewrefx

/* tagged value set access macros. */

#define trsetval(o,x...) (*(o) = x)
#define trsetvalx(T,o,x...) ({ alo_TVal* io = (o); trsetval(io, x); checklive(T, io); })
#define tsetnil(o)     trsetval(o, aloO_nil)
#define tsetbool(o,x)  trsetval(o, tnewbool(x))
#define tsetint(o,x)   trsetval(o, tnewint(x))
#define tsetflt(o,x)   trsetval(o, tnewflt(x))
#define tsetlcf(o,x)   trsetval(o, tnewlcf(x))
#define tsetptr(o,x)   trsetval(o, tnewptr(x))
#define tsetref(T,o,x) trsetvalx(T, o, tnewref(x, _rttype(x)))
#define tsetstr(T,o,x) trsetvalx(T, o, tnewref(x, _rttype(x)))
#define tsettup(T,o,x) trsetvalx(T, o, tnewref(x, ALO_TTUPLE))
#define tsetlis(T,o,x) trsetvalx(T, o, tnewref(x, ALO_TLIST))
#define tsettab(T,o,x) trsetvalx(T, o, tnewref(x, ALO_TTABLE))
#define tsetclo(T,o,x) trsetvalx(T, o, tnewref(x, _rttype(x)))
#define tsetacl(T,o,x) trsetvalx(T, o, tnewref(x, ALO_VACL))
#define tsetccl(T,o,x) trsetvalx(T, o, tnewref(x, ALO_VCCL))
#define tsetthr(T,o,x) trsetvalx(T, o, tnewref(x, ALO_TTHREAD))
#define tsetrdt(T,o,x) trsetvalx(T, o, tnewref(x, ALO_TUSER))

#define tsetobj(T,o1,o2) ({ alo_TVal* io = (o1); *io = *(o2); checklive(T, io); })

#define tchgbool(o,x) ({ alo_TVal* io = (o); aloE_check(tisbool(io), "'"#o"' is not boolean value", (io)->v.b = (x)); })
#define tchgint(o,x)  ({ alo_TVal* io = (o); aloE_check(tisint(io) , "'"#o"' is not integer value", (io)->v.i = (x)); })
#define tchgflt(o,x)  ({ alo_TVal* io = (o); aloE_check(tisflt(io) , "'"#o"' is not float value"  , (io)->v.n = (x)); })

/* internal test macros. */

#define rfittt(o) (ttype(o) == _rttype((o)->v.g))
#define checklive(T,o) aloE_assert(!tisref(o) || \
		(rfittt(o) && ((T) == NULL || aloG_isalive(aloE_cast(alo_State, T)->g, tasobj(o)))), "'"#o"' might not be alive.")

/* GC target access macros. */
#define g2s(g) aloE_check(tisstr(g), "'"#g"' is not string value"  , aloE_cast(alo_Str* , g))
#define g2a(g) aloE_check(tistup(g), "'"#g"' is not tuple value"   , aloE_cast(alo_Tuple*, g))
#define g2l(g) aloE_check(tislis(g), "'"#g"' is not list value"    , aloE_cast(alo_List*, g))
#define g2m(g) aloE_check(tistab(g), "'"#g"' is not table value"   , aloE_cast(atable_t*, g))
#define g2c(g) aloE_check(tisfun(g), "'"#g"' is not closure value" , aloE_cast(alo_Closure*, g))
#define g2t(g) aloE_check(tisthr(g), "'"#g"' is not thread value"  , aloE_cast(alo_Thread* , g))
#define g2r(g) aloE_check(tisusr(g), "'"#g"' is not raw data value", aloE_cast(alo_User*, g))
#define g2p(g) aloE_check(tispro(g), "'"#g"' is not prototype"     , aloE_cast(alo_Proto*  , g))

#define g2ac(g) aloE_check(tisacl(g), "'"#g"' is not closure value" , aloE_cast(alo_ACl*, g))
#define g2cc(g) aloE_check(tisccl(g), "'"#g"' is not closure value" , aloE_cast(alo_CCl*, g))

#define r2g(g) aloE_cast(alo_Obj*, g)
#define wrf(t) ((t) | ALO_REF_BIT)

/**
 ** types definition.
 */

/**
 ** GC header for all collectible objects.
 */
struct alo_Obj {
	ALO_AGGR_HEADER();
};

/**
 ** value union without tag
 */
union alo_Value {
	int b;
	a_int i;
	a_float n;
	a_cfun f;
	a_mem p;
	alo_Obj* g;
};

/**
 ** tagged value
 */
struct alo_TagValue {
	ALO_TVAL_HEADER;
};

/**
 ** string value
 */
typedef struct alo_Str alo_Str;

struct alo_Str {
	ALO_OBJ_HEADER;
	a_byte shtlen;
	a_byte extra;
	union {
		struct {
			a_byte fhash : 1; /* mark hash value are already calculated. */
			a_byte freserved : 1; /* mark string value is reserved word. */
			a_byte ftagname : 1; /* mark string value tagged method name. */
		};
		a_byte flags;
	};
	a_hash hash;
	union {
		alo_Str* shtlist;
		size_t lnglen;
	};
	char array[];
};

typedef struct alo_Table atable_t;

/**
 ** raw data value.
 */
typedef struct alo_User {
	ALO_AGGR_HEADER();
	atable_t* metatable;
	size_t length;
	a_byte array[];
} alo_User;

/**
 ** tuple value.
 */
typedef struct alo_Tuple {
	ALO_AGGR_HEADER(uint16_t length);
	alo_TVal array[];
} alo_Tuple;

/**
 ** list value.
 */
typedef struct alo_List {
	ALO_AGGR_HEADER();
	atable_t* metatable;
	size_t capacity;
	size_t length;
	alo_TVal* array;
} alo_List;

/**
 ** entry of table value.
 */
typedef struct alo_Entry {
	alo_TVal value;
	union {
		struct {
			ALO_TVAL_HEADER;
		};
		alo_TVal key;
	};
	int prev;
	int next;
	a_hash hash;
} alo_Entry;

/**
 ** table value.
 */
struct alo_Table {
	ALO_AGGR_HEADER(a_byte reserved); /* for fast searching tagged methods. */
	atable_t* metatable;
	size_t capacity;
	size_t length;
	alo_Entry* array;
	alo_Entry* lastfree; /* the starting slot of searching next free slot, any free slot are after this slot. */
};

/**
 ** thread value, defined in astate.h
 */
typedef struct alo_Thread alo_Thread;

typedef struct alo_Proto alo_Proto;

/**
 ** captured reference, only appear in closure capture value.
 */
struct alo_Capture {
	alo_TVal* p;
	size_t counter;
	union {
		alo_TVal slot;
		struct {
			alo_Capture* prev;
			int mark; /* capture GC marker, true when can be traced. */
		};
	};
};

#define aloO_isclosed(c) ((c)->p == &(c)->slot)

#define ClosureHeader ALO_AGGR_HEADER(a_byte length; a_byte fenv : 1)

/**
 ** Alopecurus closure value.
 */
typedef struct alo_AClosure {
	ClosureHeader;
	alo_Proto* proto;
	alo_Capture* array[];
} alo_ACl;

/**
 ** C closure value.
 */
typedef struct alo_CClosure {
	ClosureHeader;
	a_cfun handle;
	alo_TVal array[];
} alo_CCl;

/**
 ** closure value.
 */
typedef struct {
	union {
		struct { ClosureHeader; };
		alo_ACl a;
		alo_CCl c;
	};
} alo_Closure;

typedef struct alo_LocVarInfo {
	alo_Str* name;
	int start;
	int end;
	uint16_t index;
} alo_LocVarInfo;

typedef struct alo_CapInfo {
	alo_Str* name;
	uint16_t index;
	a_byte finstack : 1;
} alo_CapInfo;

typedef struct alo_LineInfo {
	int begin;
	int line;
} alo_LineInfo;

/**
 ** prototype.
 */
struct alo_Proto {
	ALO_AGGR_HEADER(
		union {
			struct {
				a_byte fvararg : 1; /* is variable parameters prototype */
			};
			a_byte flags;
		}
	);
	int ncode; /* code size */
	int nconst; /* constant size */
	int nlineinfo; /* line information size */
	int nchild; /* children prototype count */
	alo_Str* name;
	uint16_t nargs; /* fixed parameters count */
	uint16_t nstack; /* max stack size */
	uint16_t ncap; /* capture count */
	uint16_t nlocvar; /* local variable count */
	int linefdef; /* first line defined (debug) */
	int lineldef; /* last line defined (debug) */
	alo_TVal* consts;
	a_insn* code; /* opcodes */
	alo_LineInfo* lineinfo; /* line informations (debug) */
	alo_LocVarInfo* locvars; /* local variable informations (debug) */
	alo_CapInfo* captures; /* capture informations */
	alo_Proto** children;
	alo_ACl* cache; /* closure cache */
	alo_Str* src; /* (debug) */
};

/**
 ** constants and function.
 */

/* nil value */
ALO_VDEC const alo_TVal aloO_nil;

#define aloO_tnil (&aloO_nil)

#define aloO_boolhash(v) ((v) ? 0x41 : 0x31)
#define aloO_inthash(v) aloE_cast(a_hash, v)

ALO_IFUN a_hash aloO_flthash(a_float);
ALO_IFUN int aloO_str2int(a_cstr, alo_TVal*);
ALO_IFUN int aloO_str2num(a_cstr, alo_TVal*);
ALO_IFUN int aloO_flt2int(a_float, a_int*, int);
ALO_IFUN int aloO_tostring(alo_State, alo_Writer, void*, const alo_TVal*);
ALO_IFUN int aloO_escape(alo_State, alo_Writer, void*, const char*, size_t);
ALO_IFUN const alo_TVal* aloO_get(alo_State, const alo_TVal*, const alo_TVal*);

#endif /* AOBJ_H_ */
