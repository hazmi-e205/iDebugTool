include "common.lua"

project "zsign"
    kind "StaticLib"

    files
    {
        "../Externals/zsign/**.h",
        "../Externals/zsign/**.c",
        "../Externals/zsign/**.cpp",
    }

    excludes
    {
        "../Externals/zsign/zsign.*",
    }

    includedirs
    {
        "../Externals/zsign",
        "../Externals/openssl/include",
    }

    if IsWindows() then
        defines {"WINDOWS"}
        excludes
        {
            "../Externals/zsign/win32/test.c",
        }
    else
        excludes
        {
            "../Externals/zsign/win32/**",
        }
    end