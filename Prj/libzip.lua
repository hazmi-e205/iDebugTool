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

project "libzip"
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

