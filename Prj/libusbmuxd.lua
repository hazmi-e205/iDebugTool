include "common.lua"

project "usbmuxd"
    kind "StaticLib"

    files
    {
        "../Externals/libusbmuxd/include/**.h",
        "../Externals/libusbmuxd/src/**.c",
        "../Externals/libusbmuxd/src/**.h",
        "../Externals/libusbmuxd/*.h",
    }

    includedirs
    {
        "../Externals/libusbmuxd",
        "../Externals/libusbmuxd/include",
        "../Externals/libplist/include",
        "../Externals/libimobiledevice-glue/include",
    }

    defines
    {
        "HAVE_CONFIG_H",
        "LIBPLIST_STATIC",
    }

    buildoptions
    {
        "-Wno-unused-parameter",
    }