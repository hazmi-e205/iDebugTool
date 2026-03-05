include "common.lua"

project "lwip"
    kind "StaticLib"
    QtConfigs {"force_debug_info"}
    language "C"

    files
    {
        -- Core TCP/IP stack
        "../Externals/lwip/src/core/init.c",
        "../Externals/lwip/src/core/def.c",
        "../Externals/lwip/src/core/dns.c",
        "../Externals/lwip/src/core/inet_chksum.c",
        "../Externals/lwip/src/core/ip.c",
        "../Externals/lwip/src/core/mem.c",
        "../Externals/lwip/src/core/memp.c",
        "../Externals/lwip/src/core/netif.c",
        "../Externals/lwip/src/core/pbuf.c",
        "../Externals/lwip/src/core/raw.c",
        "../Externals/lwip/src/core/stats.c",
        "../Externals/lwip/src/core/sys.c",
        "../Externals/lwip/src/core/altcp.c",
        "../Externals/lwip/src/core/altcp_alloc.c",
        "../Externals/lwip/src/core/altcp_tcp.c",
        "../Externals/lwip/src/core/tcp.c",
        "../Externals/lwip/src/core/tcp_in.c",
        "../Externals/lwip/src/core/tcp_out.c",
        "../Externals/lwip/src/core/timeouts.c",
        "../Externals/lwip/src/core/udp.c",

        -- IPv4
        "../Externals/lwip/src/core/ipv4/acd.c",
        "../Externals/lwip/src/core/ipv4/autoip.c",
        "../Externals/lwip/src/core/ipv4/dhcp.c",
        "../Externals/lwip/src/core/ipv4/etharp.c",
        "../Externals/lwip/src/core/ipv4/icmp.c",
        "../Externals/lwip/src/core/ipv4/igmp.c",
        "../Externals/lwip/src/core/ipv4/ip4.c",
        "../Externals/lwip/src/core/ipv4/ip4_addr.c",
        "../Externals/lwip/src/core/ipv4/ip4_frag.c",

        -- IPv6
        "../Externals/lwip/src/core/ipv6/dhcp6.c",
        "../Externals/lwip/src/core/ipv6/ethip6.c",
        "../Externals/lwip/src/core/ipv6/icmp6.c",
        "../Externals/lwip/src/core/ipv6/inet6.c",
        "../Externals/lwip/src/core/ipv6/ip6.c",
        "../Externals/lwip/src/core/ipv6/ip6_addr.c",
        "../Externals/lwip/src/core/ipv6/ip6_frag.c",
        "../Externals/lwip/src/core/ipv6/mld6.c",
        "../Externals/lwip/src/core/ipv6/nd6.c",

        -- API (minimal - only err.c needed for NO_SYS=1)
        "../Externals/lwip/src/api/err.c",

        -- Network interface
        "../Externals/lwip/src/netif/ethernet.c",

        -- Headers
        "../Externals/lwip/src/include/**.h",
    }

    includedirs
    {
        "../Externals/lwip/src/include",
        -- lwipopts.h will be in libinstruments src/connection/
        "../Externals/libinstruments/src/connection",
    }

    buildoptions
    {
        "-Wno-unused-parameter",
        "-Wno-missing-field-initializers",
        "-Wno-sign-compare",
        "-Wno-address",
        "-Wno-extra",
    }
