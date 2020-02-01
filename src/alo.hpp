/*
 * alo.hpp
 *
 * Alopecurus API for C++
 *
 *  Created on: 2019年10月2日
 *      Author: ueyudiud
 */

#ifndef ALO_HPP_
#define ALO_HPP_

extern "C" {

#include "alo.h"
#include "aaux.h"
#include "alibs.h"

struct alo_Iterator {
	aindex_t _index;
	ptrdiff_t _counter;

	alo_Iterator(aindex_t index) : _index(index), _counter(ALO_ITERATE_BEGIN) { }

	alo_Iterator(const alo_Iterator&) = delete;
};

struct alo_VM {
	astate _T;

	alo_VM(astate T) noexcept : _T(T) { }

	alo_VM(const alo_VM&) noexcept = default;

	void setpanic(acfun fun) {
		alo_setpanic(_T, fun);
	}

	aalloc getalloc(void*& context) {
		return alo_getalloc(_T, &context);
	}

	aalloc getalloc() {
		return alo_getalloc(_T, nullptr);
	}

	void setalloc(aalloc allocf, void* context = nullptr) {
		alo_setalloc(_T, allocf, context);
	}

	void bind(astr name, acfun fun) {
		alo_bind(_T, name, fun);
	}

	aindex_t absindex(aindex_t index) {
		return alo_absindex(_T, index);
	}

	int ensure(size_t size) {
		return alo_ensure(_T, size);
	}

	aindex_t gettop() {
		return alo_gettop(_T);
	}

	void settop(aindex_t top) {
		alo_settop(_T, top);
	}

	void copy(aindex_t from, aindex_t to) {
		alo_copy(_T, from, to);
	}

	void push(aindex_t index) {
		alo_push(_T, index);
	}

	void pop(aindex_t index) {
		alo_pop(_T, index);
	}

	void erase(aindex_t index = -1) {
		alo_erase(_T, index);
	}

	void insert(aindex_t index) {
		alo_insert(_T, index);
	}

	void xmove(alo_VM& other, size_t size) {
		alo_xmove(_T, other._T, size);
	}

	bool isnone(aindex_t index) {
		return alo_isnone(_T, index);
	}

	bool isnothing(aindex_t index) {
		return alo_isnothing(_T, index);
	}

	bool isnonnil(aindex_t index) {
		return alo_isnonnil(_T, index);
	}

	bool isboolean(aindex_t index) {
		return alo_isboolean(_T, index);
	}

	bool isinteger(aindex_t index) {
		return alo_isinteger(_T, index);
	}

	bool isnumber(aindex_t index) {
		return alo_isnumber(_T, index);
	}

	bool iscfunction(aindex_t index) {
		return alo_iscfunction(_T, index);
	}

	bool israwdata(aindex_t index) {
		return alo_israwdata(_T, index);
	}

	int gettypeid(aindex_t index) {
		return alo_typeid(_T, index);
	}

	astr gettypename(aindex_t index) {
		return alo_typename(_T, index);
	}

	astr gettypeidname(int id) {
		return alo_tpidname(_T, id);
	}

	bool toboolean(aindex_t index) {
		return alo_toboolean(_T, index);
	}

	aint tointeger(aindex_t index) {
		return alo_tointeger(_T, index);
	}

	aint tointeger(aindex_t index, int& psuccess) {
		return alo_tointegerx(_T, index, &psuccess);
	}

	afloat tonumber(aindex_t index) {
		return alo_tonumber(_T, index);
	}

	afloat tonumber(aindex_t index, int& psuccess) {
		return alo_tonumberx(_T, index, &psuccess);
	}

	astr tostring(aindex_t index) {
		return alo_tolstring(_T, index, nullptr);
	}

	astr tostring(aindex_t index, size_t& size) {
		return alo_tolstring(_T, index, &size);
	}

	acfun tocfunction(aindex_t index) {
		return alo_tocfunction(_T, index);
	}

	amem torawdata(aindex_t index) {
		return alo_torawdata(_T, index);
	}

	astate tothread(aindex_t index) {
		return alo_tothread(_T, index);
	}

	void* getrawptr(aindex_t index) {
		return alo_rawptr(_T, index);
	}

	size_t getrawlen(aindex_t index) {
		return alo_rawlen(_T, index);
	}

	bool equal(aindex_t id1, aindex_t id2) {
		return alo_equal(_T, id1, id2);
	}

	void arith(int type) {
		alo_arith(_T, type);
	}

	bool compare(aindex_t id1, aindex_t id2, int type) {
		return alo_compare(_T, id1, id2, type);
	}

	void pushnil() {
		alo_pushnil(_T);
	}

	void pushbool(bool value) {
		alo_pushboolean(_T, value);
	}

	void pushinteger(aint value) {
		alo_pushinteger(_T, value);
	}

	void pushnumber(afloat value) {
		alo_pushnumber(_T, value);
	}

	astr pushstring(const char* src, size_t len) {
		return alo_pushlstring(_T, src, len);
	}

	astr pushstring(astr value) {
		return alo_pushstring(_T, value);
	}

	astr pushfstring(astr fmt, ...) {
		va_list varg;
		va_start(varg, fmt);
		astr result = alo_pushvfstring(_T, fmt, varg);
		va_end(varg);
		return result;
	}

	astr pushvfstring(astr fmt, va_list varg) {
		return alo_pushvfstring(_T, fmt, varg);
	}

	void pushcfunction(acfun value) {
		alo_pushlightcfunction(_T, value);
	}

	void pushcfunction(acfun fun, size_t size) {
		alo_pushcclosure(_T, fun, size);
	}

	void pushpointer(void* value) {
		alo_pushpointer(_T, value);
	}

	int pushthread() {
		return alo_pushthread(_T);
	}

	void rawcat(size_t size) {
		alo_rawcat(_T, size);
	}

	bool inext(alo_Iterator& itr) {
		return alo_inext(_T, itr._index, &itr._counter);
	}

	void iremove(alo_Iterator& itr) {
		alo_iremove(_T, itr._index, itr._counter);
	}

	int rawget(aindex_t index) {
		return alo_rawget(_T, index);
	}

	int rawgeti(aindex_t index, aint key) {
		return alo_rawgeti(_T, index, key);
	}

	int rawgets(aindex_t index, astr key) {
		return alo_rawgets(_T, index, key);
	}

	int get(aindex_t index) {
		return alo_get(_T, index);
	}

	int gets(aindex_t index, astr key) {
		return alo_gets(_T, index, key);
	}

	int put(aindex_t index) {
		return alo_put(_T, index);
	}

	bool getmetatable(aindex_t index) {
		return alo_getmetatable(_T, index);
	}

	int getmeta(aindex_t index, astr name, bool lookup) {
		return alo_getmeta(_T, index, name, lookup);
	}

	bool getdelegate(aindex_t index) {
		return alo_getdelegate(_T, index);
	}

	int getregistry(astr key) {
		return alo_getreg(_T, key);
	}

	amem newdata(size_t size) {
		return alo_newdata(_T, size);
	}

	void newtuple(size_t size) {
		alo_newtuple(_T, size);
	}

	void newlist(size_t size) {
		alo_newlist(_T, size);
	}

	void newtable(size_t size) {
		alo_newtable(_T, size);
	}

	void trim(aindex_t index) {
		alo_trim(_T, index);
	}

	void trim(aindex_t index, size_t length) {
		alo_triml(_T, index, length);
	}

	void rawset(aindex_t index, bool take = false) {
		alo_rawsetx(_T, index, take);
	}

	void rawseti(aindex_t index, aint key) {
		alo_rawseti(_T, index, key);
	}

	void rawsets(aindex_t index, astr key) {
		alo_rawsets(_T, index, key);
	}

	int rawrem(aindex_t index) {
		return alo_rawrem(_T, index);
	}

	void set(aindex_t index, bool take = false) {
		alo_setx(_T, index, take);
	}

	int remove(aindex_t index) {
		return alo_remove(_T, index);
	}

	bool setmetatable(aindex_t index) {
		return alo_setmetatable(_T, index);
	}

	bool setdelegate(aindex_t index) {
		return alo_setdelegate(_T, index);
	}

	void call(int narg, int nres, akfun kfun = nullptr, void* kctx = nullptr) {
		alo_callk(_T, narg, nres, kfun, kctx);
	}

	int pcall(int narg, int nres, akfun kfun = nullptr, void* kctx = nullptr) {
		return alo_pcallk(_T, narg, nres, kfun, kctx);
	}

	int compile(astr name, astr src, areader reader, void* context) {
		return alo_compile(_T, name, src, reader, context);
	}

	int load(astr src, areader reader, void* context = nullptr) {
		return alo_load(_T, src, reader, context);
	}

	int save(awriter writer, void* context = nullptr, bool debug = false) {
		return alo_save(_T, writer, context, debug);
	}

	anoret throwexcept() {
		alo_throw(_T);
	}

	size_t memused() {
		return alo_memused(_T);
	}

	void fullgc() {
		alo_fullgc(_T);
	}

	int format(awriter writer, void* context, astr fmt, ...) {
		va_list varg;
		va_start(varg, fmt);
		int result = alo_vformat(_T, writer, context, fmt, varg);
		va_end(varg);
		return result;
	}

	int vformat(awriter writer, void* context, astr fmt, va_list varg) {
		return alo_vformat(_T, writer, context, fmt, varg);
	}

	void getframe(astr what, aframeinfo_t& info) {
		alo_getframe(_T, what, &info);
	}

	bool getprevframe(astr what, aframeinfo_t& info) {
		return alo_prevframe(_T, what, &info);
	}

	operator astate() const {
		return _T;
	}
};

}

#endif /* ALO_HPP_ */
