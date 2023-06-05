include "common.lua"

project "7z"
    kind "SharedLib"

    files
    {
        "../Externals/bit7z/third_party/7-zip/C/*.h",
        "../Externals/bit7z/third_party/7-zip/C/*.c",
        "../Externals/bit7z/third_party/7-zip/CPP/Common/*.h",
        "../Externals/bit7z/third_party/7-zip/CPP/Common/*.cpp",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Common/*.h",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Common/*.cpp",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Archive/**.h",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Archive/**.cpp",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Compress/*.h",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Compress/*.cpp",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Crypto/*.h",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Crypto/*.cpp",
    }

    excludes
    {
        "../Externals/bit7z/third_party/7-zip/C/7zAlloc.*",
        "../Externals/bit7z/third_party/7-zip/C/7zArcIn.*",
        "../Externals/bit7z/third_party/7-zip/C/7zBuf.*",
        "../Externals/bit7z/third_party/7-zip/C/7zDec.*",
        "../Externals/bit7z/third_party/7-zip/C/7zFile.*",
        "../Externals/bit7z/third_party/7-zip/C/DllSecur.*",
        "../Externals/bit7z/third_party/7-zip/C/Lzma86Dec.*",
        "../Externals/bit7z/third_party/7-zip/C/Lzma86Enc.*",
        "../Externals/bit7z/third_party/7-zip/C/LzmaLib.*",
        "../Externals/bit7z/third_party/7-zip/CPP/Common/CksumReg.*",
        "../Externals/bit7z/third_party/7-zip/CPP/Common/CommandLineParser.*",
        "../Externals/bit7z/third_party/7-zip/CPP/Common/C_FileIO.*",
        "../Externals/bit7z/third_party/7-zip/CPP/Common/Lang.*",
        "../Externals/bit7z/third_party/7-zip/CPP/Common/ListFileUtils.*",
        "../Externals/bit7z/third_party/7-zip/CPP/Common/MyWindows.*",
        "../Externals/bit7z/third_party/7-zip/CPP/Common/Random.*",
        "../Externals/bit7z/third_party/7-zip/CPP/Common/StdInStream.*",
        "../Externals/bit7z/third_party/7-zip/CPP/Common/StdOutStream.*",
        "../Externals/bit7z/third_party/7-zip/CPP/Common/TextConfig.*",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Common/FilePathAutoRename.*",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Common/FileStreams.*",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Common/**StdAfx.**",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Archive/DllExports.*",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Archive/**StdAfx.**",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Compress/DllExports2Compress.*",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Compress/DllExportsCompress.*",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Compress/ImplodeHuffmanDecoder.*",
    }

    includedirs
    {
        "../Externals/bit7z/third_party/7-zip/C",
        "../Externals/bit7z/third_party/7-zip/CPP",
        "../Externals/bit7z/third_party/7-zip/CPP/Common",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Common",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Archive",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Compress",
        "../Externals/bit7z/third_party/7-zip/CPP/7zip/Crypto",
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
            "../Externals/bit7z/third_party/7-zip/CPP/Windows/Synchronization.*",
            "../Externals/bit7z/third_party/7-zip/CPP/Windows/FileDir.*",
            "../Externals/bit7z/third_party/7-zip/CPP/Windows/FileFind.*",
            "../Externals/bit7z/third_party/7-zip/CPP/Windows/FileIO.*",
            "../Externals/bit7z/third_party/7-zip/CPP/Windows/FileName.*",
            "../Externals/bit7z/third_party/7-zip/CPP/Windows/PropVariant.*",
            "../Externals/bit7z/third_party/7-zip/CPP/Windows/PropVariantConv.*",
            "../Externals/bit7z/third_party/7-zip/CPP/Windows/PropVariantUtils.*",
            "../Externals/bit7z/third_party/7-zip/CPP/Windows/System.*",
            "../Externals/bit7z/third_party/7-zip/CPP/Windows/TimeUtils.*",
        }

        includedirs
        {
            "../Externals/bit7z/third_party/7-zip/CPP/Windows",
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
        "../Externals/bit7z/third_party/7-zip/CPP",
    }