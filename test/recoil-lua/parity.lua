-- This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html

-- Prints how Lua behaves where Recoil differs from stock Lua 5.1, so the
-- output of recoil-lua can be compared line for line with the engine's.
--
-- Standalone:  recoil-lua parity.lua | grep -o 'parity: .*' > standalone.txt
-- Engine:      copy into a game's LuaUI/Widgets/, start any game, quit, then
--              grep -o 'parity: .*' infolog.txt > engine.txt
-- Then:        diff standalone.txt engine.txt
--
-- Running it under stock lua5.1 shows what the Recoil changes are.
-- math.random is left out: the unsynced generator is sequenced by its own
-- address, so it is not reproducible between runs, in the engine either.

local function Run(emit)
	local function check(name, f)
		local ok, value = pcall(f)
		if not ok then
			value = "error: " .. (tostring(value):gsub("^.-:%d+: ", ""))
		end
		emit("parity: " .. name .. " = " .. tostring(value))
	end

	check("_VERSION", function() return _VERSION end)

	-- lua_Number is float
	check("tostring(16777217)", function() return tostring(16777217) end)
	check("tostring(123456789)", function() return tostring(123456789) end)
	check("0.1 + 0.2 == 0.3", function() return 0.1 + 0.2 == 0.3 end)
	check("tonumber('1e39')", function() return tonumber("1e39") end)

	-- number to string conversion (spring_lua_ftoa)
	check("tostring(0.1)", function() return tostring(0.1) end)
	check("tostring(1/3)", function() return tostring(1/3) end)
	check("tostring(12345.678)", function() return tostring(12345.678) end)
	check("tostring(math.pi)", function() return tostring(math.pi) end)
	check("tostring(1e20)", function() return tostring(1e20) end)
	check("tostring(1.5e-7)", function() return tostring(1.5e-7) end)
	check("tostring(-0.0)", function() return tostring(-0.0) end)
	check("tostring(math.huge)", function() return tostring(math.huge) end)

	-- string.format (spring_lua_format)
	check("format('%.3f', 1/3)", function() return string.format("%.3f", 1/3) end)
	check("format('%+8.2f', 2.25)", function() return string.format("%+8.2f", 2.25) end)
	check("format('%g', 1e20)", function() return string.format("%g", 1e20) end)
	check("format('%e', 12345.678)", function() return string.format("%e", 12345.678) end)
	check("format('%d', 3.7)", function() return string.format("%d", 3.7) end)

	-- streflop math
	check("math.sqrt(2)", function() return string.format("%.9f", math.sqrt(2)) end)
	check("math.sin(1)", function() return string.format("%.9f", math.sin(1)) end)
	check("math.exp(1)", function() return string.format("%.9f", math.exp(1)) end)
	check("math.log(10)", function() return string.format("%.9f", math.log(10)) end)
	check("math.atan2(1, 2)", function() return string.format("%.9f", math.atan2(1, 2)) end)
	check("2^0.5", function() return string.format("%.9f", 2^0.5) end)
	check("math.fmod(7.5, 2)", function() return math.fmod(7.5, 2) end)

	-- REPORT_LUANAN: library functions reject inf and nan
	check("format('%f', 1/0)", function() return string.format("%f", 1/0) end)

	-- __pairs backported from Lua 5.2
	check("__pairs", function()
		local t = setmetatable({}, {__pairs = function(self)
			return function(_, k) if k == nil then return 1, "metamethod" end end, self, nil
		end})
		for _, v in pairs(t) do return v end
		return "ignored"
	end)

	-- loadstring only accepts source, not bytecode
	check("loadstring(string.dump(f))", function()
		local chunk, err = loadstring(string.dump(function() return 1 end))
		return chunk and "loaded" or err
	end)
end

if widget then
	function widget:GetInfo()
		return {
			name    = "recoil-lua parity",
			desc    = "Prints Lua behaviour for comparison with recoil-lua",
			license = "GNU GPL, v2 or later",
			layer   = 0,
			enabled = true,
		}
	end

	function widget:Initialize()
		Run(Spring.Echo)
		widgetHandler:RemoveWidget(self)
	end
else
	Run(print)
end
