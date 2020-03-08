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

/* variants for string */
#define ALO_THSTRING (ALO_TSTRING | 0 << 4)
#define ALO_TISTRING (ALO_TSTRING | 1 << 4)

/* variants for function */
#define ALO_TLCF (ALO_TFUNCTION | 0 << 4)
#define ALO_TCCL (ALO_TFUNCTION | 1 << 4)
#define ALO_TACL (ALO_TFUNCTION | 2 << 4)

/* mask for reference */
#define ALO_REFERENCE 0x80

/* useful masks */
#define ALO_TMASKNUMBER (1 << ALO_TINT | 1 << ALO_TFLOAT)
#define ALO_TMASKCOLLECTION (1 << ALO_TTUPLE | 1 << ALO_TLIST | 1 << ALO_TTABLE)

/**
 ** header predefined.
 */

#define CommonHeader agct gcprev; abyte tt; abyte mark
#define RefHeader(e) CommonHeader; e; agct gclist
#define TValHeader aval_t v; abyte tt

/**
 ** type predefined, the description of types see code below.
 */

typedef struct alo_GCHead *agct;

typedef union alo_Value aval_t;
typedef struct alo_TagValue atval_t;
typedef struct alo_Capture acap;

/* index in stack */
typedef atval_t *askid_t;

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
#define rttype(o) (o)->tt

/* get type tag from object. */
#define ttype(o) gettype(rttype(o))

/* get type tag without variant bits from object. */
#define ttpnv(o) gettpnv(rttype(o))

/* type checking without specific type macros. */
#define checktag(o,t) (ttype(o) == t)
#define checktype(o,t) (ttpnv(o) == t)
#define matchtypes(o,m) (masktype(rttype(o)) & (m))

/* type checking macros. */

#define ttisnil(o) checktype(o, ALO_TNIL)
#define ttisbool(o) checktype(o, ALO_TBOOL)
#define ttisint(o) checktype(o, ALO_TINT)
#define ttisflt(o) checktype(o, ALO_TFLOAT)
#define ttisnum(o) matchtypes(o, ALO_TMASKNUMBER)
#define ttisptr(o) checktype(o, ALO_TPOINTER)
#define ttisstr(o) checktype(o, ALO_TSTRING)
#define ttishstr(o) checktag(o, ALO_THSTRING)
#define ttisistr(o) checktag(o, ALO_TISTRING)
#define ttistup(o) checktype(o, ALO_TTUPLE)
#define ttislis(o) checktype(o, ALO_TLIST)
#define ttistab(o) checktype(o, ALO_TTABLE)
#define ttiscol(o) matchtypes(o, ALO_TMASKCOLLECTION)
#define ttisfun(o) checktype(o, ALO_TFUNCTION)
#define ttislcf(o) checktag(o, ALO_TLCF)
#define ttisccl(o) checktag(o, ALO_TCCL)
#define ttisacl(o) checktag(o, ALO_TACL)
#define ttisrdt(o) checktype(o, ALO_TRAWDATA)
#define ttisthr(o) checktype(o, ALO_TTHREAD)
#define ttispro(o) checktype(o, ALO_TPROTO)
#define ttiscap(o) checktype(o, ALO_TCAPTURE)
#define ttisref(o) (rttype(o) & ALO_REFERENCE)

/* tagged value get access macros. */

#define tgetbool(o) aloE_check(ttisbool(o), "'"#o"' is not boolean value", (o)->v.b)
#define tgetint(o) aloE_check(ttisint(o), "'"#o"' is not integer value", (o)->v.i)
#define tgetflt(o) aloE_check(ttisflt(o), "'"#o"' is not float value", (o)->v.n)
#define tgetnum(o) aloE_check(ttisnum(o), "'"#o"' is not number value", ttisint(o) ? aloE_flt(tgetint(o)) : tgetflt(o))
#define tgetlcf(o) aloE_check(ttislcf(o), "'"#o"' is not light C function value", (o)->v.f)
#define tgetptr(o) aloE_check(ttisptr(o), "'"#o"' is not integer value", tgetrptr(o))
#define tgetcap(o) aloE_check(ttiscap(o), "'"#o"' is not capture", (o)->v.c)
#define tgetref(o) aloE_check(ttisref(o), "'"#o"' is not reference value", (o)->v.g)
#define tgetstr(o) aloE_check(ttisstr(o), "'"#o"' is not string value", g2s(tgetref(o)))
#define tgettup(o) aloE_check(ttistup(o), "'"#o"' is not tuple value", g2a(tgetref(o)))
#define tgetlis(o) aloE_check(ttislis(o), "'"#o"' is not list value", g2l(tgetref(o)))
#define tgettab(o) aloE_check(ttistab(o), "'"#o"' is not table value", g2m(tgetref(o)))
#define tgetclo(o) aloE_check(ttisfun(o), "'"#o"' is not closure value", g2c(tgetref(o)))
#define tgetthr(o) aloE_check(ttisthr(o), "'"#o"' is not thread value", g2t(tgetref(o)))
#define tgetrdt(o) aloE_check(ttisrdt(o), "'"#o"' is not raw data value", g2r(tgetref(o)))

#define trefbool(o) (*aloE_check(ttisbool(o), "'"#o"' is not boolean value", &(o)->v.b))
#define trefint(o) (*aloE_check(ttisint(o), "'"#o"' is not integer value", &(o)->v.i))
#define trefflt(o) (*aloE_check(ttisflt(o), "'"#o"' is not float value", &(o)->v.f))

#define tgetrptr(o) (o)->v.p

/* tagged value construct macros. */

#define ALOO_NILBODY {}, ALO_TNIL

#define tnewnil() (atval_t) { ALOO_NILBODY }
#define tnewbool(x) (atval_t) { { .b = (x) }, ALO_TBOOL }
#define tnewint(x) (atval_t) { { .i = (x) }, ALO_TINT }
#define tnewflt(x) (atval_t) { { .n = (x) }, ALO_TFLOAT }
#define tnewlcf(x) (atval_t) { { .f = (x) }, ALO_TLCF }
#define tnewptr(x) (atval_t) { { .p = (x) }, ALO_TPOINTER }
#define tnewcap(x) (atval_t) { { .c = (x) }, ALO_TCAPTURE }
#define tnewref(x,t) (atval_t) { { .g = r2g(x) }, wrf(t) }
#define tnewrefx(x) tnewref(x, rttype(x))
#define tnewstr tnewrefx

/* tagged value set access macros. */

#define trsetval(o,x...) (*(o) = x)
#define trsetvalx(T,o,x...) ({ atval_t* io = (o); trsetval(io, x); checklive(T, io); })
#define tsetnil(o)     trsetval(o, aloO_nil)
#define tsetbool(o,x)  trsetval(o, tnewbool(x))
#define tsetint(o,x)   trsetval(o, tnewint(x))
#define tsetflt(o,x)   trsetval(o, tnewflt(x))
#define tsetlcf(o,x)   trsetval(o, tnewlcf(x))
#define tsetptr(o,x)   trsetval(o, tnewptr(x))
#define tsetcap(o,x)   trsetval(o, tnewcap(x))
#define tsetref(T,o,x) trsetvalx(T, o, tnewref(x, rttype(x)))
#define tsetstr(T,o,x) trsetvalx(T, o, tnewref(x, rttype(x)))
#define tsettup(T,o,x) trsetvalx(T, o, tnewref(x, ALO_TTUPLE))
#define tsetlis(T,o,x) trsetvalx(T, o, tnewref(x, ALO_TLIST))
#define tsettab(T,o,x) trsetvalx(T, o, tnewref(x, ALO_TTABLE))
#define tsetclo(T,o,x) trsetvalx(T, o, tnewref(x, rttype(x)))
#define tsetthr(T,o,x) trsetvalx(T, o, tnewref(x, ALO_TTHREAD))
#define tsetrdt(T,o,x) trsetvalx(T, o, tnewref(x, ALO_TRAWDATA))

#define tsetobj(T,o1,o2) ({ atval_t* io = (o1); *io = *(o2); checklive(T, io); })

#define tchgbool(o,x) ({ atval_t* io = (o); aloE_check(ttisbool(io), "'"#o"' is not boolean value", (io)->v.b = (x)); })
#define tchgint(o,x)  ({ atval_t* io = (o); aloE_check(ttisint(io) , "'"#o"' is not integer value", (io)->v.i = (x)); })
#define tchgflt(o,x)  ({ atval_t* io = (o); aloE_check(ttisflt(io) , "'"#o"' is not float value"  , (io)->v.n = (x)); })

/* internal test macros. */

#define rfittt(o) (ttype(o) == rttype((o)->v.g))
#define checklive(T,o) aloE_assert(!ttisref(o) || \
		(rfittt(o) && ((T) == NULL || aloG_isalive(aloE_cast(astate, T)->g, tgetref(o)))), "'"#o"' might not be alive.")

/* GC target access macros. */
#define g2s(g) aloE_check(ttisstr(g), "'"#g"' is not string value"  , aloE_cast(astring_t* , g))
#define g2a(g) aloE_check(ttistup(g), "'"#g"' is not tuple value"   , aloE_cast(atuple_t*  , g))
#define g2l(g) aloE_check(ttislis(g), "'"#g"' is not list value"    , aloE_cast(alist_t*   , g))
#define g2m(g) aloE_check(ttistab(g), "'"#g"' is not table value"   , aloE_cast(atable_t*  , g))
#define g2c(g) aloE_check(ttisfun(g), "'"#g"' is not closure value" , aloE_cast(aclosure_t*, g))
#define g2t(g) aloE_check(ttisthr(g), "'"#g"' is not thread value"  , aloE_cast(athread_t* , g))
#define g2r(g) aloE_check(ttisrdt(g), "'"#g"' is not raw data value", aloE_cast(arawdata_t*, g))
#define g2p(g) aloE_check(ttispro(g), "'"#g"' is not prototype"     , aloE_cast(aproto_t*  , g))

#define r2g(g) aloE_cast(agct, g)
#define wrf(t) ((t) | ALO_REFERENCE)

/**
 ** types definition.
 */

/**
 ** GC header for all collectible objects.
 */
struct alo_GCHead {
	RefHeader();
};

/**
 ** value union without tag
 */
union alo_Value {
	int b;
	aint i;
	afloat n;
	acfun f;
	amem p;
	agct g;
	acap* c;
};

/**
 ** tagged value
 */
struct alo_TagValue {
	TValHeader;
};

/**
 ** string value
 */
typedef struct alo_String astring_t;

struct alo_String {
	CommonHeader;
	abyte shtlen;
	abyte extra;
	union {
		struct {
			abyte fhash : 1; /* mark hash value are already calculated. */
			abyte freserved : 1; /* mark string value is reserved word. */
			abyte ftagname : 1; /* mark string value tagged method name. */
		};
		abyte flags;
	};
	ahash_t hash;
	union {
		astring_t* shtlist;
		size_t lnglen;
	};
	char array[];
};

typedef struct alo_Table atable_t;

/**
 ** raw data value.
 */
typedef struct alo_RawData {
	RefHeader();
	atable_t* metatable;
	size_t length;
	abyte array[];
} arawdata_t;

/**
 ** tuple value.
 */
typedef struct alo_Tuple {
	RefHeader(uint16_t length);
	atval_t array[];
} atuple_t;

/**
 ** list value.
 */
typedef struct alo_List {
	RefHeader();
	atable_t* metatable;
	size_t capacity;
	size_t length;
	atval_t* array;
} alist_t;

/**
 ** entry of table value.
 */
typedef struct alo_Entry {
	atval_t value;
	union {
		struct {
			TValHeader;
		};
		atval_t key;
	};
	int prev;
	int next;
	ahash_t hash;
} aentry_t;

/**
 ** table value.
 */
struct alo_Table {
	RefHeader(abyte reserved); /* for fast searching tagged methods. */
	atable_t* metatable;
	size_t capacity;
	size_t length;
	aentry_t* array;
	aentry_t* lastfree; /* the starting slot of searching next free slot, any free slot are after this slot. */
};

/**
 ** thread value, defined in astate.h
 */
typedef struct alo_Thread athread_t;

typedef struct alo_Proto aproto_t;

/**
 ** captured reference, only appear in closure capture value.
 */
struct alo_Capture {
	atval_t* p;
	size_t counter;
	union {
		atval_t slot;
		struct {
			acap* prev;
			int mark; /* capture GC marker, true when can be traced. */
		};
	};
};

#define aloO_isclosed(c) ((c)->p == &(c)->slot)

/**
 ** closure value.
 */
typedef struct alo_Closure {
	RefHeader(abyte length);
	union {
		struct {
			aproto_t* proto;
		} a;
		struct {
			acfun handle;
		} c;
	};
	atval_t delegate; /* for Alopecurus closure, real begin of capture array */
	atval_t array[];
} aclosure_t;

typedef struct alo_LocalVariable {
	astring_t* name;
	size_t start;
	size_t end;
	uint16_t index;
} alocvar_t;

typedef struct alo_CaptureInfo {
	astring_t* name;
	uint16_t index;
	abyte finstack : 1;
} acapinfo_t;

typedef struct alo_LineInfo {
	size_t begin;
	int line;
} alineinfo_t;

/**
 ** prototype.
 */
struct alo_Proto {
	RefHeader(
		union {
			struct {
				abyte fvararg : 1; /* is variable parameters prototype */
			};
			abyte flags;
		}
	);
	size_t ncode; /* code size */
	size_t nconst; /* constant size */
	size_t nlineinfo; /* line information size */
	size_t nchild; /* children prototype count */
	astring_t* name;
	uint16_t nargs; /* fixed parameters count */
	uint16_t nstack; /* max stack size */
	uint16_t ncap; /* capture count */
	uint16_t nlocvar; /* local variable count */
	int linefdef; /* first line defined (debug) */
	int lineldef; /* last line defined (debug) */
	atval_t* consts;
	ainsn_t* code; /* opcodes */
	alineinfo_t* lineinfo; /* line informations (debug) */
	alocvar_t* locvars; /* local variable informations (debug) */
	acapinfo_t* captures; /* capture informations */
	aproto_t** children;
	aclosure_t* cache; /* closure cache */
	astring_t* src; /* (debug) */
};

/**
 ** constants and function.
 */

/* nil value */
extern const atval_t aloO_nil;

#define aloO_tnil (&aloO_nil)

#define aloO_boolhash(v) ((v) ? 0x41 : 0x31)
#define aloO_inthash(v) aloE_cast(ahash_t, v)
ALO_IFUN ahash_t aloO_flthash(afloat);
ALO_IFUN int aloO_str2int(astr, atval_t*);
ALO_IFUN int aloO_str2num(astr, atval_t*);
ALO_IFUN int aloO_flt2int(afloat, aint*, int);
ALO_IFUN int aloO_tostring(astate, awriter, void*, const atval_t*);
ALO_IFUN void aloO_escape(astate, awriter, void*, const char*, size_t);
ALO_IFUN const atval_t* aloO_get(astate, const atval_t*, const atval_t*);

ALO_IFUN int alo_format(astate, awriter, void*, astr, ...);
ALO_IFUN int alo_vformat(astate, awriter, void*, astr, va_list);

#endif /* AOBJ_H_ */
