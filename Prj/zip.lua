include "common.lua"

project "zip"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}

    files
    {
        "../Externals/zip/src/**.h",
        "../Externals/zip/src/**.c",
    }

    includedirs
    {
        "../Externals/zip/src",
    }