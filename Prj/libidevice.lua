include "common.lua"

project "libnskeyedarchiver"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}

    files
    {
        "../Externals/libnskeyedarchiver/include/**.hpp",
        "../Externals/libnskeyedarchiver/src/**.cpp",
    }

    includedirs
    {
        "../Externals/libnskeyedarchiver/include",
        "../Externals/libplist/include",
    }

    defines
    {
        "LIBPLIST_STATIC",
    }

project "libidevice"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}

    files
    {
        "../Externals/libidevice/include/**.h",
        "../Externals/libidevice/src/**.cpp",
    }

    includedirs
    {
        "../Externals/libidevice/include",
        "../Externals/libnskeyedarchiver/include",
        "../Externals/libplist/include",
        "../Externals/libimobiledevice/include",
    }

    defines
    {
        "LIBPLIST_STATIC",
    }