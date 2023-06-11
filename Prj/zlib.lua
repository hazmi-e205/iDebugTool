include "common.lua"

project "zlib"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}

    files
    {
        "../Externals/zlib/*.h",
        "../Externals/zlib/*.c",
    }

project "minizip"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}

    files
    {
        "../Externals/zlib/contrib/minizip/**.h",
        "../Externals/zlib/contrib/minizip/**.c",
    }

    excludes
    {
        "../Externals/zlib/contrib/minizip/miniunz.**",
        "../Externals/zlib/contrib/minizip/minizip.**",
    }

    includedirs
    {
        "../Externals/zlib/contrib/minizip",
        "../Externals/zlib",
    }

    if IsWindows() == false then
        excludes
        {
            "../Externals/zlib/contrib/minizip/iowin32.**",
        }
    end