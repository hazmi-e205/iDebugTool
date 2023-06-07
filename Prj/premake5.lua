include "common.lua"

solution "iDebugTool"
    QtSlnConfigs {"ordered"}
    include "openssl.lua"
    include "libzip.lua"
    include "libplist.lua"
    include "libusbmuxd.lua"
    include "libimobiledevice.lua"
    include "libimobiledevice-glue.lua"
    include "macholib.lua"
    include "zsign.lua"
    include "bit7z.lua"

project "SelfUpdater"
    kind "ConsoleApp"
    AppName "SelfUpdater"
    AppCompany "hazmi-e205 Indonesia"
    AppCopyright ("Copyright (c) hazmi-e205 Indonesia " .. os.date("%Y"))
    AppDescription "Self Updater for iOS Debugging Tool"
    AppIcon "../Assets/technical.ico"

    local info_str = io.readfile("../info.json")
    info_json, err = json.decode(info_str)
    AppVersion (info_json.version)

    files
    {
        "../Updater/**.h",
        "../Updater/**.cpp",
    }

    includedirs
    {
        "../Src",
    }

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

    QtModules
    {
        "network",
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
        "../Externals/zsign",
        "../Externals/mingw-patch",
        "../Externals/bit7z/include",
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
        "libzip",
        "zlib",
        "MachOLib",
        "minizip",
        "zsign",
        "mingw-patch",
        "bit7z",
    }

    libdirs
    {
        "../Build/" .. GetPathFromPlatform() .. "/libs",
    }

    local copyext = "$$PWD/../../../Scripts/copyext.py"
    local copysrc = "$$PWD/../../../Build/" .. GetPathFromPlatform() .. "/libs"
    local copydst = "$$PWD/../../../Build/" .. GetPathFromPlatform() .. "/bin"
    if IsWindows() then
        links
        {
            "Iphlpapi",
            "Ws2_32",
            "Ole32",
        }
        prelinkcommands {"python " .. copyext .. " .dll " .. copysrc .. " " .. copydst}
    else
        links
        {
            "dl",
        }
        prelinkcommands {"python " .. copyext .. " .so " .. copysrc .. " " .. copydst}
    end