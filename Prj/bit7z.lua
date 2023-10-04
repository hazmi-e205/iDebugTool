include "common.lua"

project "7z"
    kind "SharedLib"

    files
    {
        "../Externals/7zip/C/*.h",
        "../Externals/7zip/C/*.c",
        "../Externals/7zip/CPP/Common/*.h",
        "../Externals/7zip/CPP/Common/*.cpp",
        "../Externals/7zip/CPP/7zip/Common/*.h",
        "../Externals/7zip/CPP/7zip/Common/*.cpp",
        "../Externals/7zip/CPP/7zip/Archive/**.h",
        "../Externals/7zip/CPP/7zip/Archive/**.cpp",
        "../Externals/7zip/CPP/7zip/Compress/*.h",
        "../Externals/7zip/CPP/7zip/Compress/*.cpp",
        "../Externals/7zip/CPP/7zip/Crypto/*.h",
        "../Externals/7zip/CPP/7zip/Crypto/*.cpp",
    }

    excludes
    {
        "../Externals/7zip/C/7zAlloc.*",
        "../Externals/7zip/C/7zArcIn.*",
        "../Externals/7zip/C/7zBuf.*",
        "../Externals/7zip/C/7zDec.*",
        "../Externals/7zip/C/7zFile.*",
        "../Externals/7zip/C/DllSecur.*",
        "../Externals/7zip/C/Lzma86Dec.*",
        "../Externals/7zip/C/Lzma86Enc.*",
        "../Externals/7zip/C/LzmaLib.*",
        "../Externals/7zip/CPP/Common/CksumReg.*",
        "../Externals/7zip/CPP/Common/CommandLineParser.*",
        "../Externals/7zip/CPP/Common/C_FileIO.*",
        "../Externals/7zip/CPP/Common/Lang.*",
        "../Externals/7zip/CPP/Common/ListFileUtils.*",
        "../Externals/7zip/CPP/Common/MyWindows.*",
        "../Externals/7zip/CPP/Common/Random.*",
        "../Externals/7zip/CPP/Common/StdInStream.*",
        "../Externals/7zip/CPP/Common/StdOutStream.*",
        "../Externals/7zip/CPP/Common/TextConfig.*",
        "../Externals/7zip/CPP/7zip/Common/FilePathAutoRename.*",
        "../Externals/7zip/CPP/7zip/Common/**StdAfx.**",
        "../Externals/7zip/CPP/7zip/Archive/DllExports.*",
        "../Externals/7zip/CPP/7zip/Archive/**StdAfx.**",
        "../Externals/7zip/CPP/7zip/Compress/DllExports2Compress.*",
        "../Externals/7zip/CPP/7zip/Compress/DllExportsCompress.*",
        "../Externals/7zip/CPP/7zip/Compress/ImplodeHuffmanDecoder.*",
    }

    includedirs
    {
        "../Externals/7zip/C",
        "../Externals/7zip/CPP",
        "../Externals/7zip/CPP/Common",
        "../Externals/7zip/CPP/7zip/Common",
        "../Externals/7zip/CPP/7zip/Archive",
        "../Externals/7zip/CPP/7zip/Compress",
        "../Externals/7zip/CPP/7zip/Crypto",
    }

    defines
    {
        "EXTERNAL_CODECS",
        "_7ZIP_LARGE_PAGES",
        "_REENTRANT",
        "_FILE_OFFSET_BITS=64",
        "_LARGEFILE_SOURCE",
        "UNICODE",
        "_UNICODE",
    }
    
    if IsWindows() then
        files
        {
            "../Externals/7zip/CPP/Windows/Synchronization.*",
            "../Externals/7zip/CPP/Windows/FileDir.*",
            "../Externals/7zip/CPP/Windows/FileFind.*",
            "../Externals/7zip/CPP/Windows/FileIO.*",
            "../Externals/7zip/CPP/Windows/FileName.*",
            "../Externals/7zip/CPP/Windows/PropVariant.*",
            "../Externals/7zip/CPP/Windows/PropVariantConv.*",
            "../Externals/7zip/CPP/Windows/PropVariantUtils.*",
            "../Externals/7zip/CPP/Windows/System.*",
            "../Externals/7zip/CPP/Windows/TimeUtils.*",
        }

        includedirs
        {
            "../Externals/7zip/CPP/Windows",
        }

        links
        {
            "oleaut32",
            "uuid",
            "advapi32",
            "User32",
            "Ole32",
            "Gdi32",
            "Comctl32",
            "Comdlg32",
            "htmlhelp",
        }
    else
        links
        {
            "dl",
        }
    end

project "bit7z"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}
    defines {"BIT7Z_USE_STANDARD_FILESYSTEM"}

    files
    {
        "../Externals/bit7z/include/**.hpp",
        "../Externals/bit7z/src/**.cpp",
        "../Externals/bit7z/src/**.hpp",
    }

    excludes
    {
    }

    includedirs
    {
        "../Externals/bit7z/include",
        "../Externals/bit7z/include/bit7z",
        "../Externals/bit7z/src",
        "../Externals/7zip/CPP",
    }