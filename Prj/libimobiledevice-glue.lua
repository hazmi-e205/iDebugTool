include "common.lua"

project "imobiledevice-glue"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}

    files
    {
        "../Externals/libimobiledevice-glue/include/**.h",
        "../Externals/libimobiledevice-glue/src/**.c",
        "../Externals/libimobiledevice-glue/src/**.h",
        "../Externals/libimobiledevice-glue/*.h",
    }

    includedirs
    {
        "../Externals/libimobiledevice-glue",
        "../Externals/libimobiledevice-glue/include",
        "../Externals/libplist/include",
    }

    defines
    {
        "HAVE_CONFIG_H",
    }

    buildoptions
    {
        "-Wno-nonnull-compare",
        "-Wno-unused-variable",
        "-Wno-unused-parameter",
        "-Wno-incompatible-pointer-types",
        "-Wno-extra",
        "-Wno-cast-function-type",
        "-Wall",
    }