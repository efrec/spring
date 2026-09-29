/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include <cinttypes>

#include "lib/streflop/streflop_cond.h"

#include "LuaInclude.h"
#include "Lua/LuaAllocState.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaMemPool.h"

#include "System/GlobalRNG.h"
#include "System/SpringMath.h"

#if (ENABLE_USERSTATE_LOCKS != 0)
	#include "System/UnorderedMap.hpp"
	#include "System/Threading/SpringThreading.h"
#endif

#include "System/Log/ILog.h"
#include "System/Misc/SpringTime.h"

#if defined(DEDICATED) || defined(UNITSYNC) || defined(BUILDING_AI)
#error liblua should be built only once!
#endif

#ifndef __archBits__
#error __archBits__ undefined
#endif




///////////////////////////////////////////////////////////////////////////
// Custom (Unsynced) Random Number Generator

extern CGlobalUnsyncedRNG lguRNG; // shared with math.random, see LuaUserCommon.cpp




///////////////////////////////////////////////////////////////////////////
// Custom Lua Mutexes

#if (ENABLE_USERSTATE_LOCKS != 0)
static spring::unsynced_map<lua_State*, bool> coroutines;
static spring::unsynced_map<lua_State*, spring::recursive_mutex*> mutexes;

static spring::recursive_mutex* GetLuaMutex(lua_State* L)
{
	assert(mutexes[L] == nullptr);
	return new spring::recursive_mutex();
}
#endif


void LuaCreateMutex(lua_State* L)
{
#if (ENABLE_USERSTATE_LOCKS != 0)
	luaContextData* lcd = GetLuaContextData(L);

	if (lcd == nullptr)
		return; // CLuaParser

	assert(lcd != nullptr);

	spring::recursive_mutex* mutex = GetLuaMutex(L);
	lcd->luamutex = mutex;
	mutexes[L] = mutex;
#endif
}

void LuaDestroyMutex(lua_State* L)
{
#if (ENABLE_USERSTATE_LOCKS != 0)
	if (GetLuaContextData(L) == nullptr)
		return; // CLuaParser

	assert(GetLuaContextData(L) != nullptr);

	if (coroutines.find(L) != coroutines.end()) {
		mutexes.erase(L);
		coroutines.erase(L);
		return;
	}

	lua_unlock(L);
	assert(mutexes.find(L) != mutexes.end());
	spring::recursive_mutex* mutex = GetLuaContextData(L)->luamutex;
	assert(mutex);
	delete mutex;
	mutexes.erase(L);
	//TODO erase all related coroutines too?
#endif
}


void LuaLinkMutex(lua_State* L_parent, lua_State* L_child)
{
#if (ENABLE_USERSTATE_LOCKS != 0)
	luaContextData* plcd = GetLuaContextData(L_parent);
	luaContextData* clcd = GetLuaContextData(L_child);

	assert(plcd != nullptr);
	assert(clcd != nullptr);
	assert(plcd == clcd);

	coroutines[L_child] = true;
	mutexes[L_child] = plcd->luamutex;
#endif
}


void LuaMutexLock(lua_State* L)
{
#if (ENABLE_USERSTATE_LOCKS != 0)

	if (GetLuaContextData(L) == nullptr)
		return; // CLuaParser

	spring::recursive_mutex* mutex = GetLuaContextData(L)->luamutex;

	if (mutex->try_lock())
		return;

	mutex->lock();
#endif
}

void LuaMutexUnlock(lua_State* L)
{
#if (ENABLE_USERSTATE_LOCKS != 0)
	if (GetLuaContextData(L) == nullptr)
		return; // CLuaParser

	spring::recursive_mutex* mutex = GetLuaContextData(L)->luamutex;
	mutex->unlock();
#endif
}


void LuaMutexYield(lua_State* L)
{
#if (ENABLE_USERSTATE_LOCKS != 0)
	assert(GetLuaContextData(L));
	/*mutexes[L]->unlock();
	if (!mutexes[L]->try_lock()) {
		// only yield if another thread is waiting for the mutex
		spring::this_thread::yield();
		mutexes[L]->lock();
	}*/

	static int count = 0;

	if (count-- <= 0)
		count = 30;

	LuaMutexUnlock(L);

	if (count == 30)
		spring::this_thread::yield();

	LuaMutexLock(L);
#endif
}




///////////////////////////////////////////////////////////////////////////
//

static const char* spring_lua_get_handle_name(const CLuaHandle* h) {
	return ((h != nullptr)? (h->GetName()).c_str(): "<null>");
}

const char* spring_lua_get_handle_name(lua_State* L)
{
	const luaContextData* lcd = GetLuaContextData(L);

	if (lcd != nullptr)
		return (spring_lua_get_handle_name(lcd->owner));

	return "";
}




///////////////////////////////////////////////////////////////////////////
// Custom Memory Allocator
//
static constexpr const char* LUA_OOM_FMT_STR = "[%s][handle=%s][OOM] synced=%d {alloced,maximum}={" _STPF_ "," _STPF_ "}bytes\n";

// tracks allocations across all states
static SLuaAllocState gLuaAllocState = {{0}, {0}, {0}, {0}};
static SLuaAllocError gLuaAllocError = {};

void spring_lua_alloc_log_error(const luaContextData* lcd)
{
	const CLuaHandle* lho = lcd->owner;

	const char* lhn = spring_lua_get_handle_name(lho);

	SLuaAllocState& s = gLuaAllocState;
	SLuaAllocError& e = gLuaAllocError;

	if (e.msgPtr == nullptr)
		e.msgPtr = &e.msgBuf[0];

	// append to buffer until it fills up or get_error is called
	e.msgPtr += SNPRINTF(e.msgPtr, sizeof(e.msgBuf) - (e.msgPtr - &e.msgBuf[0]), LUA_OOM_FMT_STR, __func__, lhn, lcd->synced, s.allocedBytes.load(), SLuaAllocLimit::MAX_ALLOC_BYTES);
}

void* spring_lua_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
	luaContextData* lcd = static_cast<luaContextData*>(ud);
	SLuaAllocState* las = &lcd->allocState;
	LuaMemPool* lmp = lcd->memPool;

	gLuaAllocState.allocedBytes -= osize;
	gLuaAllocState.allocedBytes += nsize;
	las->allocedBytes -= osize;
	las->allocedBytes += nsize;

	if (nsize == 0) {
		// deallocation; must return NULL
		lmp->Free(ptr, osize);
		return nullptr;
	}

	if ((nsize > osize) && (gLuaAllocState.allocedBytes.load() > SLuaAllocLimit::MAX_ALLOC_BYTES)) {
		// (re)allocation
		// better kill Lua than whole engine; instant desync if synced handle
		// NOTE: this will trigger luaD_throw, which calls exit(EXIT_FAILURE)
		spring_lua_alloc_log_error(lcd);
		return nullptr;
	}

	// ptr is NULL if and only if osize is zero
	// behaves like realloc when nsize!=0 and osize!=0 (ptr != NULL)
	// behaves like malloc when nsize!=0 and osize==0 (ptr == NULL)
#if LUA_MEASURE_ALLOC_TIME == 1
	const spring_time t0 = spring_gettime();
#endif
	void* mem = lmp->Realloc(ptr, nsize, osize);
#if LUA_MEASURE_ALLOC_TIME == 1
	const spring_time t1 = spring_gettime();
#endif

	gLuaAllocState.numLuaAllocs += 1;
#if LUA_MEASURE_ALLOC_TIME == 1
	gLuaAllocState.luaAllocTime += (t1 - t0).toMicroSecsi();
	las->luaAllocTime += (t1 - t0).toMicroSecsi();
#endif
	las->numLuaAllocs += 1;

	return mem;
}

void spring_lua_alloc_get_stats(SLuaAllocState* state)
{
	state->allocedBytes.store(gLuaAllocState.allocedBytes.load());
	state->numLuaAllocs.store(gLuaAllocState.numLuaAllocs.load());
	state->luaAllocTime.store(gLuaAllocState.luaAllocTime.load());

#if (ENABLE_USERSTATE_LOCKS != 0)
	state->numLuaStates.store(mutexes.size() - coroutines.size();
#else
	state->numLuaStates.store(LuaMemPool::GetPoolCount());
#endif
}

bool spring_lua_alloc_skip_gc(float gcLoadMult)
{
	// randomly skip a GC cycle with probability 1 - (weighted memory load ratio)
	const float rawLoadRatio = float(gLuaAllocState.allocedBytes.load()) / float(SLuaAllocLimit::MAX_ALLOC_BYTES);
	const float modLoadRatio = gcLoadMult * rawLoadRatio;
	return (lguRNG.NextFloat() > modLoadRatio);
}

bool spring_lua_alloc_get_error(SLuaAllocError* error)
{
	if (gLuaAllocError.msgBuf[0] == 0)
		return false;

	// copy and clear
	std::memcpy(error->msgBuf, gLuaAllocError.msgBuf, sizeof(error->msgBuf));
	std::memset(gLuaAllocError.msgBuf, 0, sizeof(gLuaAllocError.msgBuf));

	gLuaAllocError.msgPtr = &gLuaAllocError.msgBuf[0];
	return true;
}

void spring_lua_alloc_update_stats(int clearStatsFrame)
{
	gLuaAllocState.numLuaAllocs.store(gLuaAllocState.numLuaAllocs * (1 - clearStatsFrame));
	gLuaAllocState.luaAllocTime.store(gLuaAllocState.luaAllocTime * (1 - clearStatsFrame));
}
