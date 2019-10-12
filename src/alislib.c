/*
 * alislib.c
 *
 * library for list.
 *
 *  Created on: 2019年8月9日
 *      Author: ueyudiud
 */

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

/* masks for tagged methods */

/**
 ** return true if target is contained in list.
 ** prototype: list.contains(self, target)
 */
static int list_contains(astate T) {
	aloL_checktype(T, 0, ALO_TLIST);
	size_t n = alo_rawlen(T, 0);
	int top = alo_gettop(T);
	for (size_t i = 0; i < n; ++i) {
		alo_rawgeti(T, 0, i);
		for (int j = 1; j < top; ++j) {
			if (alo_equal(T, -1, j)) {
				alo_pushboolean(T, true);
				return 1;
			}
		}
		alo_drop(T);
	}
	alo_pushboolean(T, false);
	return 1;
}

/**
 ** transform elements in list by function and
 ** put them into a new list if create is true.
 ** prototype: list.map(self, function,[create])
 */
static int list_map(astate T) {
	aloL_checktype(T, 0, ALO_TLIST);
	aloL_checkcall(T, 1);
	int flag = aloL_getoptbool(T, 2, true);
	alo_settop(T, 2);
	size_t n = alo_rawlen(T, 0);
	if (flag) { /* copy to a new list */
		alo_newlist(T, n);
	}
	else { /* put in it self */
		alo_push(T, 0);
	}
	for (size_t i = 0; i < n; ++i) {
		alo_push(T, 1);
		alo_rawgeti(T, 0, i);
		alo_call(T, 1, 1);
		alo_rawseti(T, 2, i);
	}
	return 1;
}

/**
 ** filter elements by function and put them into
 ** a new list if create is true.
 ** prototype: list.filter(self, function,[create])
 */
static int list_filter(astate T) {
	aloL_checktype(T, 0, ALO_TLIST);
	aloL_checkcall(T, 1);
	int flag = aloL_getoptbool(T, 2, true);
	alo_settop(T, 2);
	size_t n = alo_rawlen(T, 0);
	if (flag) { /* copy to a new list */
		alo_newlist(T, n);
	}
	else { /* put in it self */
		alo_push(T, 0);
	}
	size_t j = 0;
	for (size_t i = 0; i < n; ++i) {
		alo_rawgeti(T, 0, i);
		alo_push(T, 1);
		alo_push(T, 3);
		alo_call(T, 1, 1);
		if (alo_toboolean(T, 4)) {
			alo_drop(T);
			alo_rawseti(T, 2, j++);
		}
		else {
			alo_settop(T, -2);
		}
	}
	alo_triml(T, 2, j); /* remove extra slot in array */
	return 1;
}

/**
 ** fold all elements together by order, return () if
 ** no element exist.
 ** prototype: list.fold(self, function)
 */
static int list_fold(astate T) {
	aloL_checktype(T, 0, ALO_TLIST);
	aloL_checkcall(T, 1);
	alo_settop(T, 2);
	size_t len = alo_rawlen(T, 0);
	switch (len) {
	case 0:
		alo_pushunit(T);
		break;
	case 1:
		alo_rawgeti(T, 0, 0);
		break;
	case 2:
		alo_rawgeti(T, 0, 0);
		alo_rawgeti(T, 0, 1);
		alo_call(T, 2, 1);
		break;
	default:
		alo_rawgeti(T, 0, 0);
		for (aint i = 1; i < len; ++i) {
			alo_push(T, 1);
			alo_push(T, 2);
			alo_rawgeti(T, 0, i);
			alo_call(T, 2, 1);
			alo_pop(T, 2);
		}
		break;
	}
	return 1;
}

/**
 ** fold all elements from left to right.
 ** prototype: list.foldl(self, head, function)
 */
static int list_foldl(astate T) {
	aloL_checktype(T, 0, ALO_TLIST);
	aloL_checkcall(T, 2);
	alo_settop(T, 3);
	size_t len = alo_rawlen(T, 0);
	switch (len) {
	case 0:
		alo_drop(T);
		break;
	case 1:
		alo_push(T, 1);
		alo_rawgeti(T, 0, 0);
		alo_call(T, 2, 1);
		break;
	default:
		for (aint i = 0; i < len; ++i) {
			alo_push(T, 2);
			alo_push(T, 1);
			alo_rawgeti(T, 0, i);
			alo_call(T, 2, 1);
			alo_pop(T, 1);
		}
		alo_drop(T);
		break;
	}
	return 1;
}

/**
 ** fold all elements from right to left.
 ** prototype: list.foldr(self, head, function)
 */
static int list_foldr(astate T) {
	aloL_checktype(T, 0, ALO_TLIST);
	aloL_checkcall(T, 2);
	alo_settop(T, 3);
	size_t len = alo_rawlen(T, 0);
	switch (len) {
	case 0:
		alo_drop(T);
		break;
	case 1:
		alo_push(T, 1);
		alo_rawgeti(T, 0, 0);
		alo_call(T, 2, 1);
		break;
	default:
		for (aint i = len - 1; i >= 0; --i) {
			alo_push(T, 2);
			alo_rawgeti(T, 0, i);
			alo_push(T, 1);
			alo_call(T, 2, 1);
			alo_pop(T, 1);
		}
		alo_drop(T);
		break;
	}
	return 1;
}

/* maximum number of elements should use insert sort */
#define INSERET_THRESHOLD 32

/* maximum number of elements should use quick sort */
#define QUICK_THRESHOLD 128

/* maximum count mark list has local structure */
#define STRUCTURE_THRESHOLD (QUICK_THRESHOLD / 5)

static int stkcmp(astate T, size_t i, size_t j) {
	if (alo_isnonnil(T, 1)) { /* use function */
		alo_push(T, 1);
		alo_push(T, i);
		alo_push(T, j);
		alo_call(T, 2, 1);
		int f = alo_toboolean(T, -1);
		alo_drop(T);
		return f;
	}
	else { /* use natural order */
		return alo_compare(T, i, j, ALO_OPLE);
	}
}

static int elecmp(astate T, size_t i, size_t j, int put) {
	alo_rawgeti(T, 0, i);
	alo_rawgeti(T, 0, j);
	if (!stkcmp(T, 2, 3)) {
		if (put) {
			alo_rawseti(T, 0, i);
			alo_rawseti(T, 0, j);
		}
		else {
			alo_settop(T, -2);
		}
		return false;
	}
	alo_settop(T, -2);
	return true;
}

static void swapele(astate T, size_t i, size_t j) {
	alo_rawgeti(T, 0, i);
	alo_rawgeti(T, 0, j);
	alo_rawseti(T, 0, i);
	alo_rawseti(T, 0, j);
}

static void isortaux(astate T, size_t s, size_t l) {
	switch (l) {
	case 0: case 1:
		break;
	case 2:
		elecmp(T, s, s + 1, true);
		break;
	default: {
		alo_rawgeti(T, 0, s);
		size_t a, b, m, t;
		for (size_t i = 1; i < l; ++i) {
			alo_rawgeti(T, 0, s + i);
			a = 2;
			t = b = 2 + i;
			/* do binary insert */
			while (a < b) {
				m = (a + b) / 2;
				if (!stkcmp(T, m, t)) {
					b = m;
				}
				else {
					a = m + 1;
				}
			}
			alo_insert(T, a);
		}
		for (aint i = l - 1; i >= 0; --i) {
			alo_rawseti(T, 0, s + i);
		}
		break;
	}
	}
}

static void qsortaux(astate T, size_t l, size_t h) {
	size_t len = h - l + 1;
	if (len < INSERET_THRESHOLD) { /* if only a few elements present, use insert sort */
		isortaux(T, l, len);
	}
	else {
		alo_rawgeti(T, 0, l); /* get basis */
		/* split elements to two group divided by basis */
		size_t a = l + 1, b = h;
		while (a < b) {
			alo_rawgeti(T, 0, a);
			if (stkcmp(T, 2, 3)) {
				alo_rawgeti(T, 0, b);
				alo_rawseti(T, 0, a);
				alo_rawseti(T, 0, b);
				b--;
			}
			else {
				alo_drop(T);
				a++;
			}
		}
		alo_rawgeti(T, 0, a);
		if (stkcmp(T, 2, 3)) {
			a -= 1;
			alo_drop(T);
			alo_rawgeti(T, 0, a);
		}
		alo_rawseti(T, 0, l);
		alo_rawseti(T, 0, a);
		/* recursive call */
		qsortaux(T, l, a - 1);
		qsortaux(T, a + 1, h);
	}
}

static void mergeaux(astate T, size_t l, size_t m, size_t h) {
	size_t i;
	alo_rawgeti(T, 0, m); /* push basis value into stack */
	for (; l < m; ++l) {
		alo_rawgeti(T, 0, l);
		if (!stkcmp(T, 3, 2)) {
			alo_settop(T, -2);
			break;
		}
		alo_drop(T);
	}
	/* start merge */
	for (; l < m; ++l) {
		alo_rawgeti(T, 0, l);
		alo_rawgeti(T, 0, m);
		if (!stkcmp(T, 2, 3)) {
			alo_rawseti(T, 0, l);
			for (i = m; i < h; ++i) {
				alo_rawgeti(T, 0, i + 1);
				if (stkcmp(T, 2, 3)) {
					alo_drop(T);
					break;
				}
				alo_rawseti(T, 0, i);
			}
			alo_rawseti(T, 0, i);
		}
		else {
			alo_settop(T, -2);
		}
	}
}

static void msortaux(astate T, size_t l, size_t h) {
	size_t len = h - l + 1;
	if (h - l < INSERET_THRESHOLD) {
		isortaux(T, l, len);
	}
	else {
		size_t m = (l + h) / 2;
		/* recursive call */
		msortaux(T, l, m);
		msortaux(T, m + 1, h);
		/* merge elements together */
		mergeaux(T, l, m + 1, h);
	}
}

static void sortaux(astate T, size_t l) {
	alo_ensure(T, (l >= INSERET_THRESHOLD ? INSERET_THRESHOLD : l) + 5);
	if (l <= QUICK_THRESHOLD) { /* if elements are less enough to use quick sort */
		qsortaux(T, 0, l - 1);
	}
	else {
		size_t i, j;
		/**
		 ** the count of structure, the structure is representation of increasing or
		 ** decreasing sequence, it will make merge sort faster than quick sort.
		 */
		size_t count = 0;
		for (i = 0; i < l - 1; ++i) {
			if (elecmp(T, i, i + 1, false)) {
				do if (++i == l - 1) {
					break;
				}
				while (elecmp(T, i, i + 1, false));
			}
			else {
				j = i;
				do if (++i == l - 1) {
					break;
				}
				while (!elecmp(T, i, i + 1, false));
				for (size_t k = i; j < k; j++, k--) { /* swap object */
					swapele(T, j, k);
				}
			}
			count += 1;
		}
		if (count <= STRUCTURE_THRESHOLD) {
			msortaux(T, 0, l - 1);
		}
		else {
			qsortaux(T, 0, l - 1);
		}
	}
}

/**
 ** sort list.
 ** prototype: list.sort(self,[comparator])
 */
static int list_sort(astate T) {
	aloL_checktype(T, 0, ALO_TLIST);
	if (alo_isnonnil(T, 1)) {
		aloL_checkcall(T, 1);
	}
	else {
		alo_settop(T, 2); /* use natural order */
	}
	size_t len = alo_rawlen(T, 0); /* get list length */
	sortaux(T, len);
	alo_push(T, 0); /* push list */
	return 1;
}

static void l_mkstr_checkarg(astate T) {
	aloL_checktype(T, 0, ALO_TLIST);
	switch (alo_gettop(T)) {
	case 1:
		alo_pushstring(T, ", ");
		goto left;
	case 2:
		aloL_checkstring(T, 1);
		left:
		alo_pushstring(T, "[");
		goto right;
	case 3:
		aloL_checkstring(T, 2);
		right:
		alo_pushstring(T, "]");
		break;
	case 4:
		break;
	default:
		alo_settop(T, 4);
		break;
	}
	alo_getreg(T, "tostring");
}

/**
 ** make list to string.
 ** prototype: list.mkstr(self,[sep,[left,[right]]])
 */
static int list_mkstr(astate T) {
	ambuf_t buf;
	l_mkstr_checkarg(T);
	aloL_bempty(T, &buf);
	size_t n = alo_rawlen(T, 0);
	aloL_bwrite(&buf, 2);
	for (size_t i = 0; i < n; ++i) {
		if (i != 0) {
			aloL_bwrite(&buf, 1);
		}
		alo_push(T, 4); /* push tostring function */
		alo_rawgeti(T, 0, i);
		alo_call(T, 1, 1);
		aloL_bwrite(&buf, -1);
		alo_drop(T);
	}
	aloL_bwrite(&buf, 3);
	aloL_bpushstring(&buf);
	return 1;
}

static const acreg_t mod_funcs[] = {
	{ "contains", list_contains },
	{ "filter", list_filter },
	{ "fold", list_fold },
	{ "foldl", list_foldl },
	{ "foldr", list_foldr },
	{ "map", list_map },
	{ "mkstr", list_mkstr },
	{ "sort", list_sort },
	{ NULL, NULL }
};

int aloopen_lislib(astate T) {
	alo_bind(T, "list.contains", list_contains);
	alo_bind(T, "list.filter", list_filter);
	alo_bind(T, "list.fold", list_fold);
	alo_bind(T, "list.foldl", list_foldl);
	alo_bind(T, "list.foldr", list_foldr);
	alo_bind(T, "list.map", list_map);
	alo_bind(T, "list.mkstr", list_mkstr);
	alo_bind(T, "list.sort", list_sort);
	alo_getreg(T, "__basic_delegates");
	alo_rawgeti(T, -1, ALO_TLIST);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
