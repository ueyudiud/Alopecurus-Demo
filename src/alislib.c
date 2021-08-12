/*
 * alislib.c
 *
 * library for list.
 *
 *  Created on: 2019年8月9日
 *      Author: ueyudiud
 */

#define ALISLIB_C_
#define ALO_LIB

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

/* masks for tagged methods */

/**
 ** allocate new list.
 ** prototype: list.__alloc()
 */
static int list_alloc(alo_State T) {
	alo_newlist(T, 0);
	return 1;
}

/**
 ** create a new list with capacity (the list is already allocated).
 ** prototype: list.__new(initial_capacity)
 */
static int list_new(alo_State T) {
	aloL_checktype(T, 0, ALO_TLIST);
	if (!alo_isnothing(T, 1)) {
		if (alo_typeid(T, 1) == ALO_TLIST) {
			size_t n = alo_rawlen(T, 1);
			alo_sizehint(T, 0, n);
			for (size_t i = 0; i < n; ++i) {
				alo_rawgeti(T, 1, i);
				alo_add(T, 0);
			}
		}
		else {
			alo_sizehint(T, 0, aloL_checkinteger(T, 1));
		}
	}
	return 0;
}

/**
 ** add elements into list.
 ** prototype: list.add(self, elements...)
 */
static int list_add(alo_State T) {
	aloL_checktype(T, 0, ALO_TLIST);
	size_t n = alo_gettop(T) - 1;
	alo_sizehint(T, 0, n);
	for (size_t i = 0; i < n; ++i) {
		alo_push(T, i + 1);
		alo_add(T, 0);
	}
	return 0;
}

/**
 ** clear element in list.
 ** prototype: list.clear(self)
 */
static int list_clear(alo_State T) {
	aloL_checktype(T, 0, ALO_TLIST);
	alo_rawclr(T, 0);
	return 0;
}

/**
 ** return true if target is contained in list.
 ** prototype: list.contains(self, target)
 */
static int list_contains(alo_State T) {
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
static int list_map(alo_State T) {
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
static int list_filter(alo_State T) {
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
static int list_fold(alo_State T) {
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
		for (size_t i = 1; i < len; ++i) {
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
static int list_foldl(alo_State T) {
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
		for (size_t i = 0; i < len; ++i) {
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
static int list_foldr(alo_State T) {
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
		for (a_int i = len - 1; i >= 0; --i) {
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
#define INSERET_THRESHOLD 40

/* maximum number of elements should use quick sort */
#define QUICK_THRESHOLD 128

/* maximum number of elements should use middle value instead of middle of list */
#define PIVOT_THRESHOLD 700

/* maximum count mark list has local structure */
#define STRUCTURE_THRESHOLD (QUICK_THRESHOLD / 5)

static void set2(alo_State T, size_t a, size_t b) {
	alo_rawseti(T, 0, a);
	alo_rawseti(T, 0, b);
}

/**
 ** compare value on stack.
 */
static int stkcmp(alo_State T, size_t i, size_t j) {
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
		return alo_compare(T, i, j, ALO_OPLT);
	}
}

static int elecmp(alo_State T, size_t i, size_t j, int put) {
	alo_rawgeti(T, 0, i);
	alo_rawgeti(T, 0, j);
	if (!stkcmp(T, 2, 3)) {
		if (put) {
			set2(T, i, j);
		}
		else {
			alo_settop(T, -2);
		}
		return false;
	}
	alo_settop(T, -2);
	return true;
}

static void swapele(alo_State T, size_t i, size_t j) {
	alo_rawgeti(T, 0, i);
	alo_rawgeti(T, 0, j);
	set2(T, i, j);
}

/**
 ** do binary insert in stack.
 */
static void insert(alo_State T, size_t n) {
	size_t l = 2, h = 2 + n;
	size_t t = h;
	/* do binary insert */
	while (l < h) {
		size_t m = (l + h) / 2;
		if (!stkcmp(T, m, t)) {
			h = m;
		}
		else {
			l = m + 1;
		}
	}
	alo_insert(T, l);
}

/**
 ** insert sort.
 */
static void isortaux(alo_State T, size_t s, size_t l) {
	switch (l) {
	case 0: case 1: /* don't need sort */
		break;
	case 2:
		elecmp(T, s, s + 1, true); /* swap only */
		break;
	default: { /* sort on stack */
		alo_rawgeti(T, 0, s);
		for (size_t i = 1; i < l; ++i) {
			alo_rawgeti(T, 0, s + i);
			/* do binary insert */
			insert(T, i);
		}
		for (a_int i = l - 1; i >= 0; --i) {
			alo_rawseti(T, 0, s + i);
		}
		break;
	}
	}
}

/**
 ** find pivot for quick sort and put it in front of list.
 ** the result is the mid-value of 5 samples.
 */
static void find_pivot(alo_State T, size_t l, size_t n) {
	/* sort 5 samples in list */
	alo_rawgeti(T, 0, l);
	for (size_t i = 1; i <= 4; ++i) {
		alo_rawgeti(T, 0, l + n * i / 4);
		insert(T, i);
	}
	/* put samples back into list */
	alo_rawseti(T, 0, l + n      );
	alo_rawseti(T, 0, l + n * 3/4);
	alo_rawseti(T, 0, l          ); /* the pivot slot */
	alo_rawseti(T, 0, l + n * 2/4);
	alo_rawseti(T, 0, l + n * 1/4);
}

/**
 ** split part into two sub parts.
 */
static size_t partition(alo_State T, size_t l, size_t h) {
	alo_rawgeti(T, 0, l); /* get pivot */
	size_t a = l, b = h + 1;
	while (true) {
		while (alo_rawgeti(T, 0, ++a), stkcmp(T, 3, 2)) {
			if (a + 1 == b)
				goto end;
			alo_drop(T);
		}
		if (a + 1 == b)
			goto end;
		while (alo_rawgeti(T, 0, --b), stkcmp(T, 2, 4)) {
			alo_drop(T);
			if (a + 1 == b)
				goto end;
		}
		if (a + 1 == b) {
			alo_drop(T);
			break;
		}
		set2(T, a, b);
	}
	end:
	set2(T, l, a);
	return a;
}

/**
 ** quick sort list.
 ** the insert sort will used if length is less than INSERET_THRESHOLD
 */
static void qsortaux(alo_State T, size_t l, size_t h) {
	while (true) {
		size_t len = h - l + 1;
		if (len < INSERET_THRESHOLD) { /* if only a few elements present, use insert sort */
			return isortaux(T, l, len);
		}
		else {
			/* find a pivot for quick sort */
			find_pivot(T, l, h - l);
			/* split elements to two group divided by basis */
			size_t p = partition(T, l, h);
			/* recursive call the smaller interval */
			if (p - l > h - p) {
				qsortaux(T, p + 1, h);
				h = p - 1;
			}
			else {
				qsortaux(T, l, p - 1);
				l = p + 1;
			}
		}
	}
}

/**
 ** merge sort list.
 */
static void mergeaux(alo_State T, size_t l, size_t m, size_t h) {
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

static void msortaux(alo_State T, size_t l, size_t h) {
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

static void sortaux(alo_State T, size_t l) {
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
static int list_sort(alo_State T) {
	aloL_checktype(T, 0, ALO_TLIST);
	if (alo_isnonnil(T, 1)) {
		aloL_checkcall(T, 1);
		alo_settop(T, 2);
	}
	else {
		alo_pushnil(T); /* use natural order */
	}
	size_t len = alo_rawlen(T, 0); /* get list length */
	sortaux(T, len);
	alo_push(T, 0); /* push list */
	return 1;
}

#define DEFAULT_SEP ", "
#define DEFAULT_LEFT "["
#define DEFAULT_RIGHT "]"

#define defaults(s,l,x) aloE_void((s) = ""x, (l) = (sizeof(x) / sizeof(char) - 1))

/**
 ** make list to string.
 ** prototype: list.mkstr(self,[sep]|[left,seq,right])
 */
static int list_mkstr(alo_State T) {
	aloL_checktype(T, 0, ALO_TLIST);
	size_t l1, l2, l3, l4;
	const char *s1, *s2, *s3, *s4;
	switch (alo_gettop(T)) {
	case 1:
		defaults(s1, l1, DEFAULT_SEP);
		goto lr;
	case 2:
		s1 = aloL_checklstring(T, 1, &l1);
		lr:
		defaults(s2, l2, DEFAULT_LEFT);
		defaults(s3, l3, DEFAULT_RIGHT);
		break;
	case 3:
		aloL_error(T, "illegal arguments: wrong argument count.");
		break;
	default:
		s1 = aloL_checklstring(T, 2, &l1);
		s2 = aloL_checklstring(T, 1, &l2);
		s3 = aloL_checklstring(T, 3, &l3);
		break;
	}
	size_t n = alo_rawlen(T, 0);
	aloL_usebuf(T, buf,
		if (n == 0) {
			aloL_bputls(T, buf, s2, l2);
			aloL_bputls(T, buf, s3, l3);
		}
		else {
			aloL_bputls(T, buf, s2, l2);
			/* append first element */
			alo_rawgeti(T, 0, 0);
			s4 = aloL_tostring(T, -1, &l4);
			aloL_bputls(T, buf, s4, l4);
			alo_settop(T, -2);
			/* append forward elements */
			for (size_t i = 1; i < n; ++i) {
				aloL_bputls(T, buf, s1, l1);
				alo_rawgeti(T, 0, i);
				s4 = aloL_tostring(T, -1, &l4);
				aloL_bputls(T, buf, s4, l4);
				alo_settop(T, -2);
			}
			aloL_bputls(T, buf, s3, l3);
		}
		aloL_bpushstring(T, buf);
	)
	return 1;
}

static const acreg_t mod_funcs[] = {
	{ "add", list_add },
	{ "contains", list_contains },
	{ "clear", list_clear },
	{ "filter", list_filter },
	{ "fold", list_fold },
	{ "foldl", list_foldl },
	{ "foldr", list_foldr },
	{ "map", list_map },
	{ "mkstr", list_mkstr },
	{ "sort", list_sort },
	{ "__alloc", list_alloc },
	{ "__new", list_new },
	{ NULL, NULL }
};

int aloopen_lis(alo_State T) {
	alo_getreg(T, "__basic_delegates");
	alo_rawgeti(T, -1, ALO_TLIST);
	aloL_setfuns(T, -1, mod_funcs);
	return 1;
}
