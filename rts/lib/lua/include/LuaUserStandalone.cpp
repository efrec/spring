/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */

// LuaUser hooks for the standalone interpreter, in place of LuaUser.cpp.

#include <cstdio>
#include <cstdlib>

#include "lib/streflop/streflop_cond.h"

#include "lua.h"
#include "LuaUser.h"

#include "System/Misc/SpringTime.h"


// userstate locks are compiled out of the engine too (ENABLE_USERSTATE_LOCKS)
void LuaCreateMutex(lua_State*) {}
void LuaDestroyMutex(lua_State*) {}
void LuaLinkMutex(lua_State*, lua_State*) {}
void LuaMutexLock(lua_State*) {}
void LuaMutexUnlock(lua_State*) {}


static FILE* StandaloneFOpen(lua_State*, const char* path, const char* mode) { return fopen(path, mode); }
static int StandaloneSystem(lua_State*, const char* command) { return system(command); }
static int StandaloneRemove(lua_State*, const char* path) { return remove(path); }
static int StandaloneRename(lua_State*, const char* oldPath, const char* newPath) { return rename(oldPath, newPath); }

void LuaStandaloneInit(lua_State* L)
{
#if STREFLOP_ENABLED
	streflop::streflop_init<streflop::Simple>();
#endif

	// os.clock
	spring_clock::PushTickRate(false);
	spring_time::setstarttime(spring_time::gettime(true));

	// io and os file access, which the engine leaves disabled or routes through its VFS
	lua_set_fopen(L, StandaloneFOpen);
	lua_set_system(L, StandaloneSystem);
	lua_set_remove(L, StandaloneRemove);
	lua_set_rename(L, StandaloneRename);
}
