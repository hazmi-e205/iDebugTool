include "common.lua"

project "crypto"
    kind "StaticLib"

    files
    {
        "../Externals/openssl/include/**.h",
        "../Externals/openssl/crypto/**.c",
        "../Externals/openssl/crypto/**.h",
        "../Externals/openssl/*.h",
    }

    excludes
    {
        "../Externals/openssl/crypto/armcap.c",
        "../Externals/openssl/crypto/dllmain.c",
        "../Externals/openssl/crypto/LPdir_nyi.c",
        "../Externals/openssl/crypto/LPdir_unix.c",
        "../Externals/openssl/crypto/LPdir_vms.c",
        "../Externals/openssl/crypto/LPdir_win.c",
        "../Externals/openssl/crypto/LPdir_win32.c",
        "../Externals/openssl/crypto/LPdir_wince.c",
        "../Externals/openssl/crypto/ppccap.c",
        "../Externals/openssl/crypto/s390xcap.c",
        "../Externals/openssl/crypto/sparcv9cap.c",
        "../Externals/openssl/crypto/aes/aes_x86core.c",
        "../Externals/openssl/crypto/bn/rsaz_exp.c",
        "../Externals/openssl/crypto/bn/asm/x86_64-gcc.c",
        "../Externals/openssl/crypto/des/ncbc_enc.c",
        "../Externals/openssl/crypto/ec/ecp_nistz256.c",
        "../Externals/openssl/crypto/ec/ecp_nistz256_table.c",
        "../Externals/openssl/crypto/engine/eng_devcrypto.c",
        "../Externals/openssl/crypto/md2/md2_dgst.c",
        "../Externals/openssl/crypto/md2/md2_one.c",
        "../Externals/openssl/crypto/poly1305/poly1305_base2_44.c",
        "../Externals/openssl/crypto/poly1305/poly1305_ieee754.c",
        "../Externals/openssl/crypto/rc5/rc5cfb64.c",
        "../Externals/openssl/crypto/rc5/rc5ofb64.c",
        "../Externals/openssl/crypto/rc5/rc5_ecb.c",
        "../Externals/openssl/crypto/rc5/rc5_enc.c",
        "../Externals/openssl/crypto/rc5/rc5_skey.c",
        "../Externals/openssl/crypto/engine/eng_all.c",
        "../Externals/openssl/crypto/engine/eng_cnf.c",
        "../Externals/openssl/crypto/engine/eng_ctrl.c",
        "../Externals/openssl/crypto/engine/eng_dyn.c",
        "../Externals/openssl/crypto/engine/eng_err.c",
        "../Externals/openssl/crypto/engine/eng_fat.c",
        "../Externals/openssl/crypto/engine/eng_init.c",
        "../Externals/openssl/crypto/engine/eng_lib.c",
        "../Externals/openssl/crypto/engine/eng_list.c",
        "../Externals/openssl/crypto/engine/eng_openssl.c",
        "../Externals/openssl/crypto/engine/eng_pkey.c",
        "../Externals/openssl/crypto/engine/eng_rdrand.c",
        "../Externals/openssl/crypto/engine/eng_table.c",
        "../Externals/openssl/crypto/engine/tb_asnmth.c",
        "../Externals/openssl/crypto/engine/tb_cipher.c",
        "../Externals/openssl/crypto/engine/tb_dh.c",
        "../Externals/openssl/crypto/engine/tb_digest.c",
        "../Externals/openssl/crypto/engine/tb_dsa.c",
        "../Externals/openssl/crypto/engine/tb_eckey.c",
        "../Externals/openssl/crypto/engine/tb_pkmeth.c",
        "../Externals/openssl/crypto/engine/tb_rand.c",
        "../Externals/openssl/crypto/engine/tb_rsa.c",
    }

    defines
    {
        "L_ENDIAN",
        "OPENSSL_PIC",
        "OPENSSLDIR=\\\"\\\\\\\"/usr/local/ssl\\\\\\\"\\\"",
    }

    if IsWindows() then
        defines {"WIN32_LEAN_AND_MEAN"}
    end

    includedirs
    {
        "../Externals/openssl",
        "../Externals/openssl/include",
        "../Externals/openssl/ssl",
        "../Externals/openssl/crypto",
        "../Externals/openssl/crypto/asn1",
        "../Externals/openssl/crypto/evp",
        "../Externals/openssl/crypto/modes",
        "../Externals/openssl/crypto/ec/curve448",
        "../Externals/openssl/crypto/ec/curve448/arch_32",
    }

    buildoptions
    {
        "-Wno-unused-variable",
        "-Wno-unused-parameter",
        "-Wno-missing-field-initializers",
        "-Wno-cast-function-type",
        "-Wno-overflow",
        "-Wno-incompatible-pointer-types",
        "-Wno-extra",
        "-Wno-sign-compare",
        "-Wall",
    }

project "openssl"
    kind "StaticLib"

    files
    {
        "../Externals/openssl/include/**.h",
        "../Externals/openssl/ssl/**.c",
        "../Externals/openssl/ssl/**.h",
        "../Externals/openssl/*.h",
    }

    defines
    {
        "L_ENDIAN",
        "OPENSSL_PIC",
        "OPENSSLDIR=\\\"\\\\\\\"/usr/local/ssl\\\\\\\"\\\"",
    }

    if IsWindows() then
        defines {"WIN32_LEAN_AND_MEAN"}
    end

    includedirs
    {
        "../Externals/openssl",
        "../Externals/openssl/include",
        "../Externals/openssl/ssl",
        "../Externals/openssl/crypto",
    }

    buildoptions
    {
        "-Wno-unused-variable",
        "-Wno-unused-parameter",
        "-Wno-missing-field-initializers",
        "-Wno-cast-function-type",
        "-Wno-overflow",
        "-Wno-incompatible-pointer-types",
        "-Wno-extra",
        "-Wno-sign-compare",
        "-Wall",
    }