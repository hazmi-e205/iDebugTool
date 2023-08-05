include "common.lua"

if IsWindows() then
    project "usb"
        kind "StaticLib"
        QtConfigs {"force_debug_info"}
        defines {"HAVE_SYS_TIME_H"}

        files
        {
            "../Externals/libusb/libusb/*.h",
            "../Externals/libusb/libusb/*.c",
            "../Externals/libusb/libusb/os/*windows*.c",
        }

        includedirs
        {
            "../Externals/libusb/libusb",
        }

    project "usb-win32"
        kind "StaticLib"
        QtConfigs {"force_debug_info"}

        files
        {
            "../Externals/libusb-win32/libusb/src/error.*",
            "../Externals/libusb-win32/libusb/src/registry.*",
            "../Externals/libusb-win32/libusb/src/libusb-win32_version.h",
            "../Externals/libusb-win32/libusb/src/lusb0_usb.h",
            "../Externals/libusb-win32/libusb/src/usbi.h",
            "../Externals/libusb-win32/libusb/src/descriptors.c",
            "../Externals/libusb-win32/libusb/src/install.c",
            "../Externals/libusb-win32/libusb/src/usb.c",
            "../Externals/libusb-win32/libusb/src/windows.c",
        }

        includedirs
        {
            "../Externals/libusb-win32/libusb/src",
        }
        
    project "usbmuxd-win32"
        kind "StaticLib"
        QtConfigs {"force_debug_info"}
        defines
        {
            "LIBPLIST_STATIC",
            "HAVE_LIBIMOBILEDEVICE",
            "HAVE_ENUM_IDEVICE_CONNECTION_TYPE",
            "PACKAGE_NAME=\\\\\\\"usbmuxd\\\\\\\"",
            "PACKAGE_STRING=\\\\\\\"usbmuxd\\\\\\\"",
            "PACKAGE_VERSION=\\\\\\\"1.0.0\\\\\\\"",
            "PACKAGE_URL=\\\\\\\"usbmuxd\\\\\\\"",
            "PACKAGE_BUGREPORT=\\\\\\\"usbmuxd\\\\\\\"",
        }

        files
        {
            "../Externals/usbmuxd-win32/src/*.h",
            "../Externals/usbmuxd-win32/src/*.c",
        }

        includedirs
        {
            "../Externals/libimobiledevice/include",
            "../Externals/mingw-patch",
            "../Externals/libusb-win32/libusb/src",
            "../Externals/libusb/libusb",
            "../Externals/libplist/include",
        }
end