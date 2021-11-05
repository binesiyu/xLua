#ifndef __LUA_LSPROTO_H_
#define __LUA_LSPROTO_H_

#if defined(_WINDLL)
#  if defined(LUA_LIB)
#    define SPROTO_LUA_API __declspec(dllexport)
#  else
#    define SPROTO_LUA_API __declspec(dllimport)
#  endif
#else
#  define SPROTO_LUA_API
#endif

#if __cplusplus
extern "C" {
#endif

#include "lauxlib.h"

SPROTO_LUA_API int luaopen_sproto_core(lua_State *L);

#if __cplusplus
}
#endif

#endif
