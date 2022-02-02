include "common.lua"

solution "iDebugTool"
    QtSlnConfigs {"ordered"}
    include "openssl.lua"
    include "libzip.lua"
    include "libplist.lua"
    include "libusbmuxd.lua"
    include "libimobiledevice.lua"
    include "libimobiledevice-glue.lua"

project "iDebugTool"
    kind "WindowedApp"
    AppName "iDebugTool"
    AppCompany "hazmi-e205 Indonesia"
    AppCopyright ("Copyright (c) hazmi-e205 Indonesia " .. os.date("%Y"))
    AppDescription "The cross platform of iOS debugging tool"
    AppIcon "bulb.ico"

    local info_str = io.readfile("../info.json")
    info_json, err = json.decode(info_str)
    AppVersion (info_json.version)

    QtResources
    {
        "../info.json",
        "bulb.ico",
    }

    files
    {
        "../Src/**.h",
        "../Src/**.cpp",
        "../Src/**.ui",
    }

    includedirs
    {
        "../Externals/libimobiledevice",
        "../Externals/libimobiledevice/include",
        "../Externals/libimobiledevice-glue/include",
        "../Externals/libplist/include",
        "../Externals/openssl/include",
        "../Externals/libusbmuxd/include",
        "../Externals/libzip/lib",
        "../Externals/libzip/build",
        "../Externals/zlib",
    }

    links
    {
        "crypto",
        "openssl",
        "cnary",
        "plist",
        "imobiledevice-glue",
        "usbmuxd",
        "imobiledevice",
        "zip",
        "zlib",
    }

    if IsWindows() then
        links
        {
            "Iphlpapi",
            "Ws2_32",
            "Ole32",
        }

        filter {"Debug*"}
            libdirs
            {
                "$$(PWD)/crypto/debug",
                "$$(PWD)/openssl/debug",
                "$$(PWD)/cnary/debug",
                "$$(PWD)/plist/debug",
                "$$(PWD)/imobiledevice-glue/debug",
                "$$(PWD)/usbmuxd/debug",
                "$$(PWD)/imobiledevice/debug",
                "$$(PWD)/zip/debug",
                "$$(PWD)/zlib/debug",
            }
        filter {"Release*"}
            libdirs
            {
                "$$(PWD)/crypto/release",
                "$$(PWD)/openssl/release",
                "$$(PWD)/cnary/release",
                "$$(PWD)/plist/release",
                "$$(PWD)/imobiledevice-glue/release",
                "$$(PWD)/usbmuxd/release",
                "$$(PWD)/imobiledevice/release",
                "$$(PWD)/zip/release",
                "$$(PWD)/zlib/release",
            }
        filter {}
    else
        links
        {
            "dl",
        }

        libdirs
        {
            "$$(PWD)/../crypto",
            "$$(PWD)/../openssl",
            "$$(PWD)/../cnary",
            "$$(PWD)/../plist",
            "$$(PWD)/../imobiledevice-glue",
            "$$(PWD)/../usbmuxd",
            "$$(PWD)/../imobiledevice",
            "$$(PWD)/../zip",
            "$$(PWD)/../zlib",
        }
    end