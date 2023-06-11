include "common.lua"

project "zsign"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}

    files
    {
        "../Externals/zsign/**.h",
        "../Externals/zsign/**.c",
        "../Externals/zsign/**.cpp",
    }

    excludes
    {
        "../Externals/zsign/zsign.*",
    }

    includedirs
    {
        "../Externals/zsign",
        "../Externals/openssl/include",
    }

    if IsWindows() then
        defines {"WINDOWS"}
        includedirs {"../Externals/mingw-patch"}
    end

if IsWindows() then
    project "mingw-patch"
        kind "StaticLib"
        QtConfigs {"force_debug_info"}
        files
        {
            "../Externals/mingw-patch/**.h",
            "../Externals/mingw-patch/**.c",
        }

        includedirs
        {
            "../Externals/mingw-patch",
        }
end