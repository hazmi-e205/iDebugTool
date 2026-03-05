include "common.lua"

project "instruments"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}

    files
    {
        -- Public headers
        "../Externals/libinstruments/include/**.h",

        -- Utilities
        "../Externals/libinstruments/src/util/**.h",
        "../Externals/libinstruments/src/util/**.cpp",

        -- NSKeyedArchiver
        "../Externals/libinstruments/src/nskeyedarchiver/**.h",
        "../Externals/libinstruments/src/nskeyedarchiver/**.cpp",

        -- DTX protocol
        "../Externals/libinstruments/src/dtx/**.h",
        "../Externals/libinstruments/src/dtx/**.cpp",

        -- Connection
        "../Externals/libinstruments/src/connection/**.h",
        "../Externals/libinstruments/src/connection/**.cpp",

        -- Services
        "../Externals/libinstruments/src/services/**.h",
        "../Externals/libinstruments/src/services/**.cpp",

        -- Facade
        "../Externals/libinstruments/src/instruments.cpp",
    }

    includedirs
    {
        "../Externals/libinstruments/include",
        "../Externals/libinstruments/src",
        "../Externals/libinstruments/src/connection",  -- lwipopts.h, arch/cc.h
        "../Externals/libplist/include",
        "../Externals/libimobiledevice/include",
        "../Externals/libimobiledevice-glue/include",
        "../Externals/libusbmuxd/include",
        "../Externals/openssl/include",
        "../Externals/picoquic/picoquic",
        "../Externals/picotls/include",
        "../Externals/picotls/picotlsvs/picotls",
        "../Externals/lwip/src/include",
    }

    defines
    {
        "INSTRUMENTS_STATIC",
        "INSTRUMENTS_HAS_QUIC",
        "INSTRUMENTS_ENABLE_LOGGING",
        "LIBIMOBILEDEVICE_STATIC",
        "LIBUSBMUXD_STATIC",
        "LIBPLIST_STATIC",
        "LIMD_GLUE_STATIC",
    }

    buildoptions
    {
        "-Wno-unused-parameter",
        "-Wno-missing-field-initializers",
    }

    if IsWindows() then
        defines { "_WINDOWS", "gettimeofday=gettimeofday" }
    end
