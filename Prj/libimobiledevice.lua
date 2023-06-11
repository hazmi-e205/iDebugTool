include "common.lua"

project "imobiledevice"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}

    files
    {
        "../Externals/libimobiledevice/include/**.h",
        "../Externals/libimobiledevice/src/**.c",
        "../Externals/libimobiledevice/src/**.h",
        "../Externals/libimobiledevice/*.h",
        "../Externals/libimobiledevice/common/*.c",
        "../Externals/libimobiledevice/common/*.c",
    }

    includedirs
    {
        "../Externals/libimobiledevice",
        "../Externals/libimobiledevice/include",
        "../Externals/libimobiledevice/common",
        "../Externals/libimobiledevice-glue/include",
        "../Externals/libplist/include",
        "../Externals/openssl/include",
        "../Externals/libusbmuxd/include",
    }

    defines
    {
        "HAVE_CONFIG_H",
        "LIBPLIST_STATIC",
    }

    buildoptions
    {
        "-Wno-pointer-to-int-cast",
        "-Wno-int-to-pointer-cast",
        "-Wno-unused-variable",
        "-Wno-unused-parameter",
        "-Wno-implicit-function-declaration",
    }