include "common.lua"

project "picotls"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}
    language "C"

    -- picotls-core + picotls-openssl sources
    files
    {
        -- Core TLS implementation
        "../Externals/picotls/lib/picotls.c",
        "../Externals/picotls/lib/hpke.c",
        "../Externals/picotls/lib/pembase64.c",
        "../Externals/picotls/lib/asn1.c",

        -- OpenSSL backend (works with OpenSSL 1.1.x)
        "../Externals/picotls/lib/openssl.c",

        -- Minicrypto backend (needed by picoquic for some operations)
        "../Externals/picotls/lib/cifra.c",
        "../Externals/picotls/lib/cifra/x25519.c",
        "../Externals/picotls/lib/cifra/chacha20.c",
        "../Externals/picotls/lib/cifra/aes128.c",
        "../Externals/picotls/lib/cifra/aes256.c",
        "../Externals/picotls/lib/cifra/random.c",
        "../Externals/picotls/lib/minicrypto-pem.c",
        "../Externals/picotls/lib/uecc.c",
        "../Externals/picotls/lib/ffx.c",

        -- Cifra crypto primitives
        "../Externals/picotls/deps/micro-ecc/uECC.c",
        "../Externals/picotls/deps/cifra/src/aes.c",
        "../Externals/picotls/deps/cifra/src/blockwise.c",
        "../Externals/picotls/deps/cifra/src/chacha20.c",
        "../Externals/picotls/deps/cifra/src/chash.c",
        "../Externals/picotls/deps/cifra/src/curve25519.c",
        "../Externals/picotls/deps/cifra/src/drbg.c",
        "../Externals/picotls/deps/cifra/src/hmac.c",
        "../Externals/picotls/deps/cifra/src/gcm.c",
        "../Externals/picotls/deps/cifra/src/gf128.c",
        "../Externals/picotls/deps/cifra/src/modes.c",
        "../Externals/picotls/deps/cifra/src/poly1305.c",
        "../Externals/picotls/deps/cifra/src/sha256.c",
        "../Externals/picotls/deps/cifra/src/sha512.c",

        -- Headers
        "../Externals/picotls/include/**.h",
    }

    includedirs
    {
        "../Externals/picotls/include",
        "../Externals/picotls/picotlsvs/picotls",
        "../Externals/picotls/deps/cifra/src/ext",
        "../Externals/picotls/deps/cifra/src",
        "../Externals/picotls/deps/micro-ecc",
        "../Externals/openssl/include",
    }

    buildoptions
    {
        "-Wno-unused-parameter",
        "-Wno-missing-field-initializers",
        "-Wno-sign-compare",
        "-Wno-cast-function-type",
        "-Wno-incompatible-pointer-types",
    }

    
    if IsWindows() then
        defines { "_WINDOWS", "gettimeofday=gettimeofday" }
    else
        defines { "_GNU_SOURCE" }
        buildoptions { "-pthread" }
    end
