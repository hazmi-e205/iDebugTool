include "common.lua"

project "MachOLib"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}
    
    if IsLinux() or IsMac() then
        defines {"HAVE_DECL_BASENAME"}
    end

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
    }