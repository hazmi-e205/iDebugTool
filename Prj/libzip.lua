include "common.lua"

project "zlib"
    kind "StaticLib"

    files
    {
        "../Externals/zlib/*.h",
        "../Externals/zlib/*.c",
    }

project "minizip"
    kind "StaticLib"

    files
    {
        "../Externals/zipper/minizip/**.h",
        "../Externals/zipper/minizip/**.c",
    }

    excludes
    {
        "../Externals/zipper/minizip/miniunz.**",
        "../Externals/zipper/minizip/minizip.**",
    }

    includedirs
    {
        "../Externals/zipper/minizip",
    }

project "zipper"
    kind "StaticLib"

    files
    {
        "../Externals/zipper/zipper/*.h",
        "../Externals/zipper/zipper/*.cpp",
    }

    includedirs
    {
        "../Externals/zipper/zipper",
        "../Externals/zipper/minizip",
    }

    if IsWindows() then
        defines {"USE_WINDOWS"}
        includedirs {"../Externals/mingw-patch"}
    end

project "zip"
    kind "StaticLib"

    files
    {
        "../Externals/libzip/lib/**.h",
        "../Externals/libzip/lib/**.c",
        "../Externals/libzip/build/*.h",
        "../Externals/libzip/build/lib/*.c",
    }

    excludes
    {
        "../Externals/libzip/lib/zip_algorithm_bzip2.c",
        "../Externals/libzip/lib/zip_algorithm_xz.c",
        "../Externals/libzip/lib/zip_algorithm_zstd.c",
        "../Externals/libzip/lib/zip_crypto_commoncrypto.c",
        "../Externals/libzip/lib/zip_crypto_gnutls.c",
        "../Externals/libzip/lib/zip_crypto_mbedtls.c",
        "../Externals/libzip/lib/zip_crypto_win.c",
        "../Externals/libzip/lib/zip_random_uwp.c",
        "../Externals/libzip/lib/zip_source_file_win32**",
    }

    includedirs
    {
        "../Externals/zlib",
        "../Externals/libzip/lib",
        "../Externals/libzip/build",
        "../Externals/openssl/include",
    }

    if IsWindows() then
        excludes
        {
            "../Externals/libzip/lib/zip_random_unix.c",
        }
    else
        excludes
        {
            "../Externals/libzip/lib/zip_random_win32.c",
        }
    end

