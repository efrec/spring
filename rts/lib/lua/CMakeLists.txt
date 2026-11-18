# Spring supplied CMake build file

# deactivate signaling-NANs for this library
remove_flag("-fsignaling-nans")

INCLUDE_DIRECTORIES(BEFORE ../streflop include)

SET(luaSources
		"src/lapi.cpp"
		"src/lauxlib.cpp"
		"src/lbaselib.cpp"
		"src/lcode.cpp"
		"src/ldblib.cpp"
		"src/ldebug.cpp"
		"src/ldo.cpp"
		"src/ldump.cpp"
		"src/lfunc.cpp"
		"src/lgc.cpp"
		"src/linit.cpp"
		"src/liolib.cpp"
		"src/llex.cpp"
		"src/lmathlib.cpp"
		"src/lmem.cpp"
		"src/loadlib.cpp"
		"src/lobject.cpp"
		"src/lopcodes.cpp"
		"src/loslib.cpp"
		"src/lparser.cpp"
		"src/lstate.cpp"
		"src/lstring.cpp"
		"src/lstrlib.cpp"
		"src/ltable.cpp"
		"src/ltablib.cpp"
		"src/ltm.cpp"
		"src/lundump.cpp"
		"src/lvm.cpp"
		"src/lzio.cpp"
		"src/print.cpp"
		"src/lua_privileges.cpp"
		"include/LuaUserCommon.cpp"
	)

if (UNIX)
	SET(luaCompileFlags "-DLUA_USE_MKSTEMP ${PIC_FLAG}")
else (UNIX)
	SET(luaCompileFlags "${PIC_FLAG}")
endif (UNIX)

# Everything but the engine-side hooks in LuaUser.cpp, which need the rest of
# the engine to link.
ADD_LIBRARY(lua-core OBJECT EXCLUDE_FROM_ALL ${luaSources})

if (ENABLE_STREFLOP)
TARGET_LINK_LIBRARIES(lua-core PUBLIC streflop)
endif ()

target_include_directories(lua-core PUBLIC include)
SET_TARGET_PROPERTIES(lua-core PROPERTIES COMPILE_FLAGS "${luaCompileFlags}")

# Vendored lua has an intentional empty-body loop (lauxlib.cpp shebang skip).
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	target_compile_options(lua-core PRIVATE
		-Wno-empty-body
	)
endif()

ADD_LIBRARY(lua STATIC EXCLUDE_FROM_ALL "include/LuaUser.cpp")
TARGET_LINK_LIBRARIES(lua PUBLIC lua-core)
SET_TARGET_PROPERTIES(lua PROPERTIES COMPILE_FLAGS "${luaCompileFlags}")

if (BUILD_RECOIL_LUA)
	# lua-core again, with the C API that C modules load against, and the
	# standalone hooks in place of LuaUser.cpp's
	SET(recoilLuaSources
			${luaSources}
			"include/LuaUserStandalone.cpp"
			"${CMAKE_SOURCE_DIR}/rts/System/Misc/SpringTime.cpp"
		)

	if (WIN32)
		# C modules on Windows link against lua51.dll, as with stock Lua
		ADD_LIBRARY(recoil-lua-core SHARED EXCLUDE_FROM_ALL ${recoilLuaSources})
		SET_TARGET_PROPERTIES(recoil-lua-core PROPERTIES OUTPUT_NAME lua51 PREFIX "" WINDOWS_EXPORT_ALL_SYMBOLS ON)
	else (WIN32)
		ADD_LIBRARY(recoil-lua-core OBJECT EXCLUDE_FROM_ALL ${recoilLuaSources})
	endif (WIN32)

	if (ENABLE_STREFLOP)
	TARGET_LINK_LIBRARIES(recoil-lua-core PUBLIC streflop)
	endif ()

	target_include_directories(recoil-lua-core PUBLIC include)
	SET_TARGET_PROPERTIES(recoil-lua-core PROPERTIES COMPILE_FLAGS "${luaCompileFlags}" CXX_VISIBILITY_PRESET default)
	target_compile_definitions(recoil-lua-core PUBLIC LUA_USE_C_LINKAGE)
	target_compile_definitions(recoil-lua-core PRIVATE NOT_USING_CREG)

	if (UNIX)
		target_compile_definitions(recoil-lua-core PRIVATE LUA_USE_DLOPEN LUA_USE_POPEN)
		TARGET_LINK_LIBRARIES(recoil-lua-core PUBLIC ${CMAKE_DL_LIBS})
	elseif (WIN32)
		target_compile_definitions(recoil-lua-core PUBLIC LUA_WIN)
	endif ()

	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		target_compile_options(recoil-lua-core PRIVATE
			-Wno-empty-body
		)
	endif()

	ADD_EXECUTABLE(recoil-lua "src/lua.cpp")
	TARGET_LINK_LIBRARIES(recoil-lua PRIVATE recoil-lua-core)

	if (UNIX)
		target_compile_definitions(recoil-lua PRIVATE LUA_USE_ISATTY)
		SET_TARGET_PROPERTIES(recoil-lua PROPERTIES ENABLE_EXPORTS ON)
	endif (UNIX)
endif (BUILD_RECOIL_LUA)
