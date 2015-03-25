/*
 * Copyright (c) 2014 Francisco Zamora-Martinez (pakozm@gmail.com)
 * Copyright (c) 2007-2009 Neil Richardson (nrich@iinet.net.au)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 *
 * Table construction helper functions
 *
 * LUA_PUSH_ATTRIB_*  creates string indexed (hashmap)
 * LUA_PUSH_ARRAY_*   creates integer indexed (array)
 *
 */

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#if !defined(LUA_VERSION_NUM) || (LUA_VERSION_NUM < 501)
#error "Needs Lua 5.1 or greater"
#endif

#if LUA_VERSION_NUM < 502
#define lua_rawlen lua_objlen
#endif
};

#define UNUSED_VARIABLE(x) (void)(x)

/* this was removed in Lua 5.2 */
LUALIB_API int luaL_typeerror (lua_State *L, int narg, const char *tname);

#if LUA_VERSION_NUM < 502
LUALIB_API void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup);
#endif

#define LUA_PUSH_ATTRIB_INT(n, v) \
    lua_pushstring(L, n); \
    lua_pushinteger(L, v); \
    lua_rawset(L, -3);

#define LUA_PUSH_ATTRIB_FLOAT(n, v) \
    lua_pushstring(L, n); \
    lua_pushnumber(L, v); \
    lua_rawset(L, -3);

#define LUA_PUSH_ATTRIB_STRING(n, v) \
    lua_pushstring(L, n); \
    lua_pushstring(L, v); \
    lua_rawset(L, -3);

#define LUA_PUSH_ATTRIB_BOOL(n, v) \
    lua_pushstring(L, n); \
    lua_pushboolean(L, v); \
    lua_rawset(L, -3);

#define LUA_PUSH_ATTRIB_NIL(n) \
    lua_pushstring(L, n); \
    lua_pushnil(L); \
    lua_rawset(L, -3);



#define LUA_PUSH_ARRAY_INT(n, v) \
    lua_pushinteger(L, v); \
    lua_rawseti(L, -2, n); \
    n++;

#define LUA_PUSH_ARRAY_FLOAT(n, v) \
    lua_pushnumber(L, v); \
    lua_rawseti(L, -2, n); \
    n++;

#define LUA_PUSH_ARRAY_STRING(n, v) \
    lua_pushstring(L, v); \
    lua_rawseti(L, -2, n); \
    n++;

#define LUA_PUSH_ARRAY_BOOL(n, v) \
    lua_pushboolean(L, v); \
    lua_rawseti(L, -2, n); \
    n++;

#define LUA_PUSH_ARRAY_NIL(n) \
    lua_pushnil(L); \
    lua_rawseti(L, -2, n); \
    n++;


#ifdef _WIN32
    #define LM_EXPORT __declspec(dllexport)
#else
    #define LM_EXPORT
#endif
#include <mongo/bson/bsonobj.h>
#include "..\AsyncTaskMgr.h"
#include "..\LuaEngine.h"


void bson_to_lua(lua_State *L, const mongo::BSONObj &obj);
void lua_push_value(lua_State *L, const mongo::BSONElement &elem);


inline std::string gs(const char msg[], ...){
	va_list args;
	va_start(args, msg);
	std::auto_ptr<char> tempstr(new char[1024 * 1024]);
	_vsnprintf(tempstr.get(), 1024 * 1024, msg, args);
	va_end(args);
	return tempstr.get();
}

template<class T>
inline void LuaPush(lua_State *L, T v){
	lua_tinker::push(L, v);
}


inline void LuaPush(lua_State *L, const std::string& v){
	lua_pushlstring(L, v.c_str(), v.size());
}

inline void LuaPush(lua_State *L, const mongo::BSONElement & v){
	lua_push_value(L, v);
}
inline void LuaPush(lua_State *L, const mongo::BSONObj & v){
	bson_to_lua(L, v);
}

template<class T>
void LuaPush(lua_State *L, const std::list<T>& v){
	lua_newtable(L);
	int i = 1;
	for (auto it = v.begin(); it != v.end(); ++it, ++i) {
		lua_pushnumber(L, i);
		LuaPush(L, *it);
		lua_settable(L, -3);
	}
}
template<class T>
void LuaPush(lua_State *L, const std::vector<T>& v){
	lua_newtable(L);
	int i = 1;
	for (auto it = v.begin(); it != v.end(); ++it, ++i) {
		lua_pushnumber(L, i);
		LuaPush(L, *it);
		lua_settable(L, -3);
	}
}

void asyncLuaFunc(std::function<void()> func);
bool isLuaCoroutine(lua_State *L);


template<class T1>
int mongoResult(lua_State *L, T1 v1){
	if (isLuaCoroutine(L)){
		AsyncTaskMgr::instance().post([=](){
			LuaPush(L, v1);
			int ret = lua_resume(L, 1);
			if (ret == LUA_ERRRUN){
				lua_tinker::on_error(L);
			}
		});
		return 0;
	}
	LuaPush(L, v1);
	return 1;
}

template<class T1, class T2>
int mongoResult(lua_State *L, T1 v1, T2 v2){
	if (isLuaCoroutine(L)){
		AsyncTaskMgr::instance().post([=](){
			LuaPush(L, v1);
			LuaPush(L, v2);
			int ret = lua_resume(L, 2);
			if (ret == LUA_ERRRUN){
				lua_tinker::on_error(L);
			}
		});
		return 0;
	}
	LuaPush(L, v1);
	LuaPush(L, v2);
	return 2;
}



template<class _Fty>
int mongoTask(lua_State *L, _Fty && func){
	if (isLuaCoroutine(L)){
		AsyncTaskMgr::instance().start(func);
		return lua_yield(L, 0);
	}
	return func();
}


template<class _Fty>
int mongoTask(lua_State *L, _Fty && func, const char * fmt1){
	if (isLuaCoroutine(L)){
		AsyncTaskMgr::instance().start(
			[=](){
			try{ return func(); }
			catch (std::exception &e){
				return mongoResult(L, false, gs(fmt1, e.what()));
			}
		});
		return lua_yield(L, 0);
	}
	return func();
}

template<class _Fty>
int mongoTask(lua_State *L, _Fty && func, const char * fmt1, const char * fmt2){
	if (isLuaCoroutine(L)){
		AsyncTaskMgr::instance().start(
			[=](){
			try{ return func(); }
			catch (std::exception &e){
				return mongoResult(L, false, gs(fmt1, fmt2, e.what()));
			}
		});
		return lua_yield(L, 0);
	}
	return func();
}

template<class _Fty>
int mongoTask(lua_State *L, _Fty && func, const char * fmt1, const char * fmt2, const char * fmt3){
	if (isLuaCoroutine(L)){
		AsyncTaskMgr::instance().start(
			[=](){
			try{ return func(); }
			catch (std::exception &e){
				return mongoResult(L, false, gs(fmt1, fmt2, e.what()));
			}
		});
		return lua_yield(L, 0);
	}
	return func();
}


#define MONGO_LUA_ERR_PATH \
	lua_pushboolean(L, 0); \
	lua_pushstring(L, "!!!!!! error , unknow path"); \
	return 2;
