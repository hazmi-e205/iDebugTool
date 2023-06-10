include "common.lua"

project "asmCrashReport"
    kind "StaticLib"

    files
    {
        "../Externals/asmCrashReport/src/**",
    }

    includedirs
    {
        "../Externals/asmCrashReport/src",
    }
