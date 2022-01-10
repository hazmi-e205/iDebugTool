newoption
{
	trigger = "to",
	value   = "path",
	description = "Set the output location for the generated files"
}

function IsQt()
    return _ACTION == "Qt"
end

if IsQt() then
	include "qt.lua"
end

function GetPathFromPlatform(action)
    local act = action or _ACTION

    if act == "Qt" then
    	return "Qt"
    end
end

function GetDefaultPlatforms(options)
	local platList = {}

	if IsQt() then
		table.insert(platList, "x64")
	end

	return platList
end

if IsQt() then
    system "Windows"
	language "C++"

	local options = options or {}

	if not _OPTIONS["to"] then
		_OPTIONS["to"] = GetPathFromPlatform()
	end
	location ( _OPTIONS["to"] )
	
	local _configs = _OPTIONS["configs"]
	if _configs then
       _configs = split(_configs, ",")
	end
	local configurationsList = _configs or options.configurations or { "Debug", "Release" }
	configurations { configurationsList }

	local _platforms = _OPTIONS["platforms"]
	if _platforms then
		_platforms = split(_platforms, ",")
	end
	local platformsList = _platforms or GetDefaultPlatforms(options)
	platforms { platformsList }

	QtConfigs {"c++11"}
	filter {"kind:ConsoleApp"}
		QtConfigs {"console"}
	filter {"kind:StaticLib"}
		QtConfigs {"staticlib"}
	filter {}
end