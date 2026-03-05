include "common.lua"

project "picoquic"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}
    language "C"

    files
    {
        "../Externals/picoquic/picoquic/bbr.c",
        "../Externals/picoquic/picoquic/bbr1.c",
        "../Externals/picoquic/picoquic/bytestream.c",
        "../Externals/picoquic/picoquic/c4.c",
        "../Externals/picoquic/picoquic/cc_common.c",
        "../Externals/picoquic/picoquic/config.c",
        "../Externals/picoquic/picoquic/cubic.c",
        "../Externals/picoquic/picoquic/ech.c",
        "../Externals/picoquic/picoquic/error_names.c",
        "../Externals/picoquic/picoquic/fastcc.c",
        "../Externals/picoquic/picoquic/frames.c",
        "../Externals/picoquic/picoquic/intformat.c",
        "../Externals/picoquic/picoquic/logger.c",
        "../Externals/picoquic/picoquic/logwriter.c",
        "../Externals/picoquic/picoquic/loss_recovery.c",
        "../Externals/picoquic/picoquic/newreno.c",
        "../Externals/picoquic/picoquic/pacing.c",
        "../Externals/picoquic/picoquic/packet.c",
        "../Externals/picoquic/picoquic/paths.c",
        "../Externals/picoquic/picoquic/performance_log.c",
        "../Externals/picoquic/picoquic/picohash.c",
        "../Externals/picoquic/picoquic/picoquic_lb.c",
        "../Externals/picoquic/picoquic/picoquic_ptls_openssl.c",
        "../Externals/picoquic/picoquic/picoquic_ptls_minicrypto.c",
        "../Externals/picoquic/picoquic/picosplay.c",
        "../Externals/picoquic/picoquic/port_blocking.c",
        "../Externals/picoquic/picoquic/prague.c",
        "../Externals/picoquic/picoquic/quicctx.c",
        "../Externals/picoquic/picoquic/register_all_cc_algorithms.c",
        "../Externals/picoquic/picoquic/sacks.c",
        "../Externals/picoquic/picoquic/sender.c",
        "../Externals/picoquic/picoquic/sim_link.c",
        "../Externals/picoquic/picoquic/siphash.c",
        "../Externals/picoquic/picoquic/spinbit.c",
        "../Externals/picoquic/picoquic/ticket_store.c",
        "../Externals/picoquic/picoquic/timing.c",
        "../Externals/picoquic/picoquic/token_store.c",
        "../Externals/picoquic/picoquic/tls_api.c",
        "../Externals/picoquic/picoquic/transport.c",
        "../Externals/picoquic/picoquic/unified_log.c",
        "../Externals/picoquic/picoquic/util.c",

        -- Headers
        "../Externals/picoquic/picoquic/**.h",
    }

    -- Exclude socket layer (libinstruments handles its own I/O) and optional backends
    excludes
    {
        "../Externals/picoquic/picoquic/picoquic_ptls_fusion.c",
        "../Externals/picoquic/picoquic/picoquic_mbedtls.c",
        "../Externals/picoquic/picoquic/memory_log.c",
        "../Externals/picoquic/picoquic/winsockloop.c",
        "../Externals/picoquic/picoquic/sockloop.c",
    }

    includedirs
    {
        "../Externals/picoquic/picoquic",
        "../Externals/picotls/include",
        "../Externals/picotls/deps/cifra/src",
        "../Externals/picotls/deps/cifra/src/ext",
        "../Externals/picotls/deps/micro-ecc",
        "../Externals/openssl/include",
    }

    defines
    {
        "PTLS_WITHOUT_FUSION",
        "DISABLE_DEBUG_PRINTF",
    }

    buildoptions
    {
        "-Wno-unused-parameter",
        "-Wno-missing-field-initializers",
        "-Wno-sign-compare",
        "-Wno-extra",
        "-Wno-type-limits",
        "-Wno-unknown-pragmas",
    }

    
    if IsWindows() then
        defines { "_WINDOWS", "gettimeofday=gettimeofday", "_WIN32_WINNT=0x0A00" }
    else
        defines { "_GNU_SOURCE" }
    end
