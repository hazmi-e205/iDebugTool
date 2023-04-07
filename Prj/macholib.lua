include "common.lua"

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