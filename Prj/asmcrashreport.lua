include "common.lua"

project "asmCrashReport"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}

    files
    {
        "../Externals/asmCrashReport/src/**",
    }

    includedirs
    {
        "../Externals/asmCrashReport/src",
    }
