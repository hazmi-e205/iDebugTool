newoption
{
	trigger = "to",
	value   = "path",
	description = "Set the output location for the generated files"
}

newoption
{
	trigger = "target",
	value   = "String",
	description = "Target of platform"
}

function IsTarget(_target)
	if _OPTIONS["target"] then
		return _OPTIONS["target"] == _target
	end
    return os.host() == _target
end

function IsLinux()
    return IsTarget("linux")
end

function IsWindows()
    return IsTarget("windows")
end

function IsMac()
    return IsTarget("macosx")
end

function IsQt()
    return _ACTION == "Qt"
end

if IsQt() then
	include "qt.lua"
end

function GetPathFromPlatform(action)
	local act = action or _ACTION
	if _OPTIONS["to"] then
		return _OPTIONS["to"]
	elseif act == "Qt" then
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

	QtConfigs {"c++11"}
	filter {"kind:ConsoleApp"}
		QtConfigs {"console"}
	filter {"kind:StaticLib"}
		QtConfigs {"staticlib"}
	filter {}

	filter {"kind:ConsoleApp or WindowedApp or SharedLib"}
		targetdir ("../Build/" .. GetPathFromPlatform() .. "/bin/")
	filter {"kind:StaticLib"}
		targetdir ("../Build/" .. GetPathFromPlatform() .. "/libs/")
	filter {}

	filter {"Debug*"}
		targetsuffix "_d"
	filter {}
end

-- default configuration
local options = options or {}

local _platformPath = GetPathFromPlatform()
location (_platformPath)

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