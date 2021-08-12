/*
 * ameta.c
 *
 *  Created on: 2019年7月22日
 *      Author: ueyudiud
 */

#define AMETA_C_
#define ALO_CORE

#include "astr.h"
#include "alis.h"
#include "atab.h"
#include "ameta.h"
#include "agc.h"
#include "adebug.h"
#include "avm.h"
#include "ado.h"

#define TM(id,name) "__"#name

ALO_VDEF const a_cstr aloT_typenames[ALO_NUMTYPE] = {
	"nil", "bool", "int", "float", "pointer", "string", "tuple",
	"list", "table", "function", "thread", "rawdata"
};

ALO_VDEF const a_cstr aloT_tmnames_[TM_N] = {
	TM_LIST
};

#undef TM

void aloT_init(alo_State T) {
	Gd(T);
	for (size_t i = 0; i < TM_N; ++i) {
		alo_Str* name = aloS_of(T, aloT_tmnames_[i]);
		aloG_fix(T, name); /* fix TM name */
		/* settle extra data */
		name->ftagname = true;
		name->extra = i;
		G->stagnames[i] = name; /* set tagged name to global */
	}
}

const alo_TVal* aloT_fastgetaux(atable_t* table, alo_Str* name, atmi id) {
	const alo_TVal* out = aloH_getis(table, name);
	if (tisnil(out)) {
		table->reserved |= 1 << id; /* marked TM is not presents in table. */
		return NULL;
	}
	return out;
}

static void callbin(alo_State T, const alo_TVal* f, const alo_TVal* t1, const alo_TVal* t2) {
	ptrdiff_t oldtop = getstkoff(T, T->top);
	/* assume extra stack to place values */
	aloT_vmput3(T, f, t1, t2);
	aloD_call(T, T->top - 3, 1); /* call function */
	T->top = putstkoff(T, oldtop); /* restore to old top of stack, the result will be here */
}

static void callunr(alo_State T, const alo_TVal* f, const alo_TVal* t1) {
	ptrdiff_t oldtop = getstkoff(T, T->top);
	/* assume extra stack to place values */
	aloT_vmput2(T, f, t1);
	aloD_call(T, T->top - 2, 1); /* call function */
	T->top = putstkoff(T, oldtop); /* restore to old top of stack, the result will be here */
}

atable_t** aloT_getpmt(const alo_TVal* o) {
	switch (ttpnv(o)) {
	case ALO_TLIST   : return &taslis(o)->metatable;
	case ALO_TTABLE  : return &tastab(o)->metatable;
	case ALO_TUSER: return &tgetrdt(o)->metatable;
	default          : return NULL;
	}
}

/**
 ** get field in meta table with lookup.
 */
static const alo_TVal* aloT_fullgetis(alo_State T, atable_t* table, alo_Str* name) {
	aloE_assert(table, "meta table should not be NULL.");
	const alo_TVal *result, *lookup;
	Gd(T);
	find:
	if (!tisnil(result = aloH_getis(table, name))) {
		return result;
	}
	if ((lookup = aloT_gfastget(G, table, TM_LOOK))) {
		switch (ttpnv(lookup)) {
		case ALO_TTABLE: /* find method in parent meta table */
			table = tastab(lookup);
			goto find;
		case ALO_TLIST: { /* find method in MRO */
			alo_List* mro = taslis(lookup);
			if (mro->length > 0) {
				alo_TVal* i = mro->array + mro->length;
				while (--i >= mro->array) {
					switch (ttpnv(i)) {
					case ALO_TTABLE: /* static target */
						result = aloH_getis(tastab(i), name);
						break; /* no tagged method found */
					case ALO_TFUNC: { /* dynamic target */
						callunr(T, i, &tnewstr(name));
						result = T->top;
						break; /* no tagged method found */
					}
					default:
						continue; /* illegal MRO value */
					}
					if (!tisnil(result)) { /* find target? */
						return result;
					}
				}
			}
			break;
		}
		case ALO_TFUNC: {
			callunr(T, lookup, &tnewstr(name));
			if (!tisnil(T->top)) { /* find target? */
				return T->top;
			}
			break;
		}
		default: /* no available lookup value */
			break;
		}
	}
	return NULL;
}

const alo_TVal* aloT_gettm(alo_State T, const alo_TVal* self, atmi id, int dolookup) {
	atable_t* table = aloT_getmt(self);
	if (table == NULL)
		return NULL;
	Gd(T);
	if (dolookup) {
		return aloT_fullgetis(T, table, G->stagnames[id]);
	}
	else {
		const alo_TVal* result = aloH_getis(table, G->stagnames[id]);
		return tisnil(result) ? NULL : result;
	}
}

const alo_TVal* aloT_fastget(alo_State T, const alo_TVal* self, atmi id) {
	atable_t* table = aloT_getmt(self);
	return aloT_gfastget(T->g, table, id);
}

static const alo_TVal* fastgetx(alo_State T, atable_t* table, atmi id) {
	const alo_TVal* result;
	Gd(T);
	find: /* find tagged method directly */
	if ((result = aloT_gfastget(G, table, id))) {
		return result;
	}
	/* failed get tagged method directly, call 'lookup' method */
	const alo_TVal* lookup = aloT_gfastget(G, table, TM_LOOK);
	if (lookup) {
		switch (ttpnv(lookup)) {
		case ALO_TTABLE: /* find method in parent meta table */
			table = tastab(lookup);
			if (table == NULL) {
				break;
			}
			goto find;
		case ALO_TLIST: { /* find method in MRO */
			alo_List* mro = taslis(lookup);
			alo_TVal* i = mro->array + mro->length;
			while (--i >= mro->array) {
				switch (ttpnv(i)) {
				case ALO_TTABLE: /* static target */
					if ((result = aloT_gfastget(G, tastab(i), id))) {
						return result;
					}
					break; /* no tagged method found */
				case ALO_TFUNC: { /* dynamic target */
					callunr(T, i, &tnewstr(G->stagnames[id]));
					if (!tisnil(T->top)) { /* find target? */
						return T->top;
					}
					break; /* no tagged method found */
				}
				default:
					break; /* illegal MRO value */
				}
			}
			break;
		}
		case ALO_TFUNC: {
			callunr(T, lookup, &tnewstr(G->stagnames[id]));
			if (!tisnil(T->top)) { /* find target? */
				return T->top;
			}
			break;
		}
		default: /* no available lookup value */
			break;
		}
	}
	return NULL;
}

const alo_TVal* aloT_fastgetx(alo_State T, const alo_TVal* self, atmi id) {
	aloE_assert(id < ALO_NUMTM && id != TM_LOOK, "invalid fast call tagged method id.");
	atable_t* table = aloT_getmt(self);
	return table ? fastgetx(T, table, id) : NULL;
}

const alo_TVal* aloT_index(alo_State T, const alo_TVal* self, const alo_TVal* key) {
	const alo_TVal *meta, *result;
	meta = aloT_fastgetx(T, self, TM_GET);
	if (meta == NULL) {
		return NULL;
	}
	find:
	switch (ttpnv(meta)) {
	case ALO_TFUNC: {
		callbin(T, meta, self, key);
		return T->top;
	}
	case ALO_TTABLE: {
		if (!tisnil(result = aloH_get(T, tastab(meta), key))) {
			return result;
		}
		self = meta;
		meta = aloT_fastgetx(T, self, TM_GET);
		if (meta != NULL) {
			goto find;
		}
		break;
	}
	}
	return aloO_tnil;
}

static const alo_TVal* lookup_mro(alo_State T, const alo_TVal* self, const alo_TVal* key, alo_List* mro) {
	if (mro->length == 0)
		return NULL;
	const alo_TVal* result;
	alo_TVal* i = mro->array + mro->length;
	while (--i >= mro->array) {
		switch (ttpnv(i)) {
		case ALO_TTABLE: /* static target */
			if (!tisnil(result = aloH_get(T, tastab(i), key))) {
				return result;
			}
			break; /* no tagged method found */
		case ALO_TFUNC: { /* dynamic target */
			callbin(T, i, self, key);
			if (!tisnil(T->top)) { /* find target? */
				return T->top;
			}
			break; /* no tagged method found */
		}
		default:
			break; /* illegal MRO value */
		}
	}
	return NULL;
}

static const alo_TVal* lookupenv(alo_State T, const alo_TVal* self, const alo_TVal* key) {
	const alo_TVal* env = T->frame->env;
	if (tistab(env)) { /* check by delegate function */
		alo_TVal t = tnewstr(T->g->stagnames[TM_DLGT]);
		const alo_TVal* result = aloH_getis(tastab(env), T->g->stagnames[TM_DLGT]);
		if (result == aloO_tnil) {
			result = aloT_index(T, env, &t);
		}
		if (result) {
			aloT_vmput3(T, result, self, key);
			aloD_callnoyield(T, T->top - 3, 1);
			if (!tisnil(--T->top)) { /* found object? */
				return T->top;
			}
		}
	}
	return NULL;
}

/**
 ** look up method in meta table.
 */
const alo_TVal* aloT_lookup(alo_State T, const alo_TVal* self, const alo_TVal* key) {
	const alo_TVal* result;
	atable_t* table = aloT_getmt(self);
	if (table) { /* check in meta table */
		find: /* find tagged method directly */
		if (!tisnil(result = aloH_get(T, table, key))) {
			return result;
		}
		/* failed get tagged method directly, call 'lookup' method */
		const alo_TVal* lookup = aloT_gfastget(T->g, table, TM_LOOK);
		if (lookup) {
			switch (ttpnv(lookup)) {
			case ALO_TTABLE: /* find method in parent meta table */
				table = tastab(lookup);
				goto find;
			case ALO_TLIST: { /* find method in MRO */
				if ((result = lookup_mro(T, self, key, taslis(lookup)))) {
					return result;
				}
				break;
			}
			case ALO_TFUNC: {
				callbin(T, lookup, self, key);
				if (!tisnil(T->top)) { /* find target? */
					return T->top;
				}
				break;
			}
			default: /* no available lookup value */
				break;
			}
		}
	}
	return lookupenv(T, self, key);
}

/**
 ** call unary method.
 */
void aloT_callunr(alo_State T, const alo_TVal* f, const alo_TVal* t1, alo_TVal* o) {
	callunr(T, f, t1);
	tsetobj(T, o, T->top); /* move object to output slot */
}

/**
 ** call binary method between two values.
 */
void aloT_callbin(alo_State T, const alo_TVal* f, const alo_TVal* t1, const alo_TVal* t2, alo_TVal* o) {
	callbin(T, f, t1, t2);
	tsetobj(T, T->top, o); /* move object to output slot */
}

/**
 ** call compare method between two values.
 */
int aloT_callcmp(alo_State T, const alo_TVal* f, const alo_TVal* t1, const alo_TVal* t2) {
	callbin(T, f, t1, t2);
	return aloV_getbool(T->top); /* cast object to boolean value */
}

/**
 ** try take meta unary operation, return true if operation is exists.
 */
int aloT_tryunr(alo_State T, const alo_TVal* t1, atmi id) {
	const alo_TVal* tm = aloT_gettm(T, t1, id, true);
	if (tm) {
		callunr(T, tm, t1);
		return true;
	}
	return false;
}

/**
 ** try take meta binary operation, return true if operation is exists.
 */
int aloT_trybin(alo_State T, const alo_TVal* t1, const alo_TVal* t2, atmi id) {
	const alo_TVal* tm = aloT_gettm(T, t1, id, true);
	if (tm) {
		callbin(T, tm, t1, t2);
		return true;
	}
	return false;
}

/**
 ** try call meta get length function, return true if function is exists.
 */
int aloT_trylen(alo_State T, const alo_TVal* t, atable_t* table) {
	aloE_assert(aloT_getmt(t) == table, "meta table mismatched.");
	if (table == NULL)
		return false;
	const alo_TVal* tm = fastgetx(T, table, TM_LEN);
	if (tm) {
		callunr(T, tm, t);
		return true;
	}
	return false;
}
