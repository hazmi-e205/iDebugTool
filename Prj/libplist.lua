include "common.lua"

project "cnary"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}

    files
    {
        "../Externals/libplist/libcnary/include/**.h",
        "../Externals/libplist/libcnary/**.c",
    }

    excludes
    {
        "../Externals/libplist/libcnary/cnary.c",
    }

    includedirs
    {
        "../Externals/libplist/libcnary/include",
    }

project "plist"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}

    files
    {
        "../Externals/libplist/include/**.h",
        "../Externals/libplist/src/**.c",
        "../Externals/libplist/src/**.cpp",
        "../Externals/libplist/src/**.h",
        "../Externals/libplist/*.h",
    }

    includedirs
    {
        "../Externals/libplist",
        "../Externals/libplist/include",
        "../Externals/libplist/src",
        "../Externals/libplist/libcnary/include",
    }

    defines
    {
        "LIBPLIST_STATIC",
        "HAVE_CONFIG_H",
    }

    buildoptions
    {
        "-Wno-unused-variable",
        "-Wno-unused-parameter",
        "-Wno-extra",
    }