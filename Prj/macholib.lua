include "common.lua"

project "unzip"
    kind "StaticLib"

    files
    {
        "../Externals/zlib/contrib/minizip/**.h",
        "../Externals/zlib/contrib/minizip/**.c",
    }

    excludes
    {
        "../Externals/zlib/contrib/minizip/miniunz.**",
        "../Externals/zlib/contrib/minizip/minizip.**",
    }

    includedirs
    {
        "../Externals/zlib/contrib/minizip",
    }

project "MachOLib"
    kind "StaticLib"

    files
    {
        "../Externals/MachOLib/**.h",
        "../Externals/MachOLib/**.c",
        "../Externals/MachOLib/**.hpp",
        "../Externals/MachOLib/**.cpp",
    }

    includedirs
    {
        "../Externals/MachOLib",
        "../Externals/MachOLib/libiberty",
        "../Externals/zlib",
        "../Externals/zlib/contrib/minizip",
        -- "../Externals/rapidjson/include",
    }