#pragma once
#include <crtdbg.h>
#include <cstdio>
#include <iostream>

#include "lualib.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
//#pragma comment(lib, "lua54.lib")

template <typename T, typename U>
void asTable(lua_State* L, T begin, U end) {
	lua_newtable(L);
	for (size_t i = 0; begin != end; ++begin, ++i) {
		lua_pushinteger(L, i + 1);
		lua_pushnumber(L, *begin);
		lua_settable(L, -3);
	}
}

template <typename T, typename U>
void fromTable(lua_State* L, T begin, U end) {
	_ASSERT(lua_istable(L, -1));
	for (size_t i = 0; begin != end; ++begin, ++i) {
		lua_pushinteger(L, i + 1);
		lua_gettable(L, -2);
		*begin = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
}

inline bool DebugLua(lua_State* L, int err) {
	if (err) {
		auto msg = lua_tostring(L, -1);
		std::cout << "LUA error in exec: " << msg << std::endl;
		lua_pop(L, 1);

		switch (err) {
		case LUA_ERRFILE:
			printf("couldn't open the given file\n");
		case LUA_ERRSYNTAX:
			printf("syntax error during pre-compilation\n");
			luaL_traceback(L, L, msg, 1);
			printf("%s\n", lua_tostring(L, -1));
		case LUA_ERRMEM:
			printf("memory allocation error\n");
		case LUA_ERRRUN:
		{
			const char* msg = lua_tostring(L, -1);
			luaL_traceback(L, L, msg, 1);
			printf("LUA_ERRRUN %s\n", lua_tostring(L, -1));
		}
		case LUA_ERRERR:
			printf("error while running the error handler function\n");
		default:
			printf("unknown error %i\n", err);
		}
		return false;
	}
	return true;
}

inline bool CallLuaFunction(lua_State* L, int argCount, int resultCount) {
	int errindex = 0;
	const int res = lua_pcall(L, argCount, resultCount, errindex);
	return DebugLua(L, res);
}