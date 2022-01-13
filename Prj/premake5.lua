include "common.lua"

solution "iDebugTool"
    QtSlnConfigs {"ordered"}

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
        }

        if os.host() ~= "linux" then
            excludes
            {
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
        end

        defines
        {
            "WIN32_LEAN_AND_MEAN",
            "L_ENDIAN",
            "OPENSSL_PIC",
            "OPENSSLDIR=\\\"\\\\\\\"/usr/local/ssl\\\\\\\"\\\"",
        }

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
            "WIN32_LEAN_AND_MEAN",
            "L_ENDIAN",
            "OPENSSL_PIC",
            "OPENSSLDIR=\\\"\\\\\\\"/usr/local/ssl\\\\\\\"\\\"",
        }

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

    project "cnary"
        kind "StaticLib"

        files
        {
            "../Externals/libplist/libcnary/include/**.h",
            "../Externals/libplist/libcnary/**.c",
        }

        excludes
        {
            "../Externals/libplist/libcnary/cnary.c",
        }

        includedirs
        {
            "../Externals/libplist/libcnary/include",
        }

    project "plist"
        kind "StaticLib"

        files
        {
            "../Externals/libplist/include/**.h",
            "../Externals/libplist/src/**.c",
            "../Externals/libplist/src/**.cpp",
            "../Externals/libplist/src/**.h",
            "../Externals/libplist/*.h",
        }

        includedirs
        {
            "../Externals/libplist",
            "../Externals/libplist/include",
            "../Externals/libplist/src",
            "../Externals/libplist/libcnary/include",
        }

        defines
        {
            "HAVE_CONFIG_H",
        }

        buildoptions
        {
            "-Wno-unused-variable",
            "-Wno-unused-parameter",
            "-Wno-extra",
        }

    project "imobiledevice-glue"
        kind "StaticLib"
    
        files
        {
            "../Externals/libimobiledevice-glue/include/**.h",
            "../Externals/libimobiledevice-glue/src/**.c",
            "../Externals/libimobiledevice-glue/src/**.h",
            "../Externals/libimobiledevice-glue/*.h",
        }

        includedirs
        {
            "../Externals/libimobiledevice-glue",
            "../Externals/libimobiledevice-glue/include",
            "../Externals/libplist/include",
        }

        defines
        {
            "HAVE_CONFIG_H",
        }

        buildoptions
        {
            "-Wno-nonnull-compare",
            "-Wno-unused-variable",
            "-Wno-unused-parameter",
            "-Wno-incompatible-pointer-types",
            "-Wno-extra",
            "-Wno-cast-function-type",
            "-Wall",
        }

    project "usbmuxd"
        kind "StaticLib"

        files
        {
            "../Externals/libusbmuxd/include/**.h",
            "../Externals/libusbmuxd/src/**.c",
            "../Externals/libusbmuxd/src/**.h",
            "../Externals/libusbmuxd/*.h",
        }

        includedirs
        {
            "../Externals/libusbmuxd",
            "../Externals/libusbmuxd/include",
            "../Externals/libplist/include",
            "../Externals/libimobiledevice-glue/include",
        }

        defines
        {
            "HAVE_CONFIG_H",
        }

        buildoptions
        {
            "-Wno-unused-parameter",
        }

    project "imobiledevice"
        kind "StaticLib"
    
        files
        {
            "../Externals/libimobiledevice/include/**.h",
            "../Externals/libimobiledevice/src/**.c",
            "../Externals/libimobiledevice/src/**.h",
            "../Externals/libimobiledevice/*.h",
            "../Externals/libimobiledevice/common/*.c",
            "../Externals/libimobiledevice/common/*.c",
        }

        includedirs
        {
            "../Externals/libimobiledevice",
            "../Externals/libimobiledevice/include",
            "../Externals/libimobiledevice/common",
            "../Externals/libimobiledevice-glue/include",
            "../Externals/libplist/include",
            "../Externals/openssl/include",
            "../Externals/libusbmuxd/include",
        }

        defines
        {
            "HAVE_CONFIG_H",
        }

        buildoptions
        {
            "-Wno-pointer-to-int-cast",
            "-Wno-int-to-pointer-cast",
            "-Wno-unused-variable",
            "-Wno-unused-parameter",
            "-Wno-implicit-function-declaration",
        }

    project "iDebugTool"
        kind "WindowedApp"
        AppName "iDebugTool"
        AppCompany "hazmi-e205 Indonesia"
        AppCopyright "Copyright (c) hazmi-e205 Indonesia 2022"
        AppDescription "The cross platform of iOS debugging tool"
        AppIcon "bulb.ico"

        QtResources
        {
            "../info.json",
        }

        files
        {
            "../Src/**.h",
            "../Src/**.cpp",
            "../Src/**.ui",
        }

        includedirs
        {
            "../Externals/libimobiledevice",
            "../Externals/libimobiledevice/include",
            "../Externals/libimobiledevice-glue/include",
            "../Externals/libplist/include",
            "../Externals/openssl/include",
            "../Externals/libusbmuxd/include",
        }

        links
        {
            "Iphlpapi",
            "Ws2_32",
            "Ole32",
            "crypto",
            "openssl",
            "cnary",
            "plist",
            "imobiledevice-glue",
            "usbmuxd",
            "imobiledevice",
        }

        filter {"Debug*"}
            libdirs
            {
                "$$(PWD)/Crypto/debug",
                "$$(PWD)/Openssl/debug",
                "$$(PWD)/Cnary/debug",
                "$$(PWD)/Plist/debug",
                "$$(PWD)/imobiledevice-glue/debug",
                "$$(PWD)/Usbmuxd/debug",
                "$$(PWD)/imobiledevice/debug",
            }
        filter {"Release*"}
            libdirs
            {
                "$$(PWD)/Crypto/release",
                "$$(PWD)/Openssl/release",
                "$$(PWD)/Cnary/release",
                "$$(PWD)/Plist/release",
                "$$(PWD)/imobiledevice-glue/release",
                "$$(PWD)/Usbmuxd/release",
                "$$(PWD)/imobiledevice/release",
            }
        filter {}