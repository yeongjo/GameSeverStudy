#include "LuaUtil.h"

//#pragma comment(lib, "lua54.lib")
bool DebugLua(lua_State* L, int err) {
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

bool CallLuaFunction(lua_State* L, int argCount, int resultCount) {
	int errindex = 0;
	const int res = lua_pcall(L, argCount, resultCount, errindex);
	return DebugLua(L, res);
}