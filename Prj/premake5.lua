include "common.lua"

solution "iDebugTool"
    QtSlnConfigs {"ordered"}
    external "qicstable"
        location  ("../Externals")
		kind "None"
    
    include "openssl.lua"
    include "libzip.lua"
    include "libplist.lua"
    include "libusbmuxd.lua"
    include "libimobiledevice.lua"
    include "libimobiledevice-glue.lua"
    include "macholib.lua"

project "iDebugTool"
    kind "WindowedApp"
    AppName "iDebugTool"
    AppCompany "hazmi-e205 Indonesia"
    AppCopyright ("Copyright (c) hazmi-e205 Indonesia " .. os.date("%Y"))
    AppDescription "iOS Debugging Tool"
    AppIcon "../Assets/bulb.ico"

    local info_str = io.readfile("../info.json")
    info_json, err = json.decode(info_str)
    AppVersion (info_json.version)

    QtIncludes
    {
        "../Externals/qicstable/qicstable_config.pri",
    }

    QtModules
    {
        "network",
        "xml",
    }

    QtResources
    {
        "../info.json",
        "../Assets/**",
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
        "../Externals/zlib/contrib/minizip",
        "../Externals/MachOLib",
        "../Externals/qicstable/include",
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
        "MachOLib",
        "unzip",
        "qicstable_d",
    }

    libdirs
    {
        "../Build/" .. GetPathFromPlatform() .. "/libs",
        "../Externals/qicstable/lib",
    }

    if IsWindows() then
        links
        {
            "Iphlpapi",
            "Ws2_32",
            "Ole32",
        }
    else
        links
        {
            "dl",
        }
    end