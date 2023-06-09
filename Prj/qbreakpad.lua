include "common.lua"

project "qBreakpad"
    kind "StaticLib"

    QtModules
    {
        "network",
    }

    if IsWindows() then
        files
        {
            "../Externals/qBreakpad/third_party/breakpad/src/client/windows/crash_generation/crash_generation_client.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/windows/handler/exception_handler.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/windows/guid_string.*",
        }
    end

    if IsMac() or IsLinux() then
        files
        {
            "../Externals/qBreakpad/third_party/breakpad/src/client/minidump_file_writer.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/convert_UTF.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/md5.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/string_conversion.*",
        }
    end

    if IsLinux() then
        files
        {
            "../Externals/qBreakpad/third_party/breakpad/src/client/linux/crash_generation/crash_generation_client.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/linux/dump_writer_common/thread_info.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/linux/dump_writer_common/ucontext_reader.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/linux/handler/exception_handler.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/linux/handler/minidump_descriptor.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/linux/log/log.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/linux/microdump_writer/microdump_writer.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/linux/minidump_writer/linux_core_dumper.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/linux/minidump_writer/linux_dumper.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/linux/minidump_writer/linux_ptrace_dumper.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/linux/minidump_writer/minidump_writer.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/linux/breakpad_getcontext.S",
            "../Externals/qBreakpad/third_party/breakpad/src/common/linux/elfutils.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/linux/file_id.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/linux/guid_creator.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/linux/linux_libc_support.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/linux/memory_mapped_file.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/linux/safe_readlink.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/convert_UTF.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/md5.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/string_conversion.*",
        }
    end

    if IsMac() then
        files
        {
            "../Externals/qBreakpad/third_party/breakpad/src/client/mac/crash_generation/crash_generation_client.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/mac/handler/breakpad_nlist_64.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/mac/handler/dynamic_images.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/mac/handler/exception_handler.*",
            "../Externals/qBreakpad/third_party/breakpad/src/client/mac/handler/minidump_generator.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/mac/MachIPC.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/mac/bootstrap_compat.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/mac/file_id.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/mac/macho_id.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/mac/macho_utilities.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/mac/macho_walker.*",
            "../Externals/qBreakpad/third_party/breakpad/src/common/mac/string_utilities.*",
        }
    end

    files
    {
        "../Externals/qBreakpad/handler/singletone/call_once.h",
        "../Externals/qBreakpad/handler/singletone/singleton.h",
        "../Externals/qBreakpad/handler/QBreakpadHandler.*",
        "../Externals/qBreakpad/handler/QBreakpadHttpUploader.*",
    }

    includedirs
    {
        "../Externals/qBreakpad/third_party/breakpad/src",
        "../Externals/qBreakpad/third_party/breakpad",
        "../Externals/qBreakpad/handler",
    }
