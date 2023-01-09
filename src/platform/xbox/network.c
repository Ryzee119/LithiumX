// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "helpers/nano_debug.h"
#include "platform/platform.h"

#include <xboxkrnl/xboxkrnl.h>
#include <windows.h>
#include <stdio.h>
#include <time.h>

void xbox_sntp_set_time(uint32_t ntp_s)
{
#define NT_EPOCH_TIME_OFFSET ((LONGLONG)(369 * 365 + 89) * 24 * 3600)

    LARGE_INTEGER xbox_nt_time, ntp_nt_time;

    KeQuerySystemTime(&xbox_nt_time);
    ntp_nt_time.QuadPart = ((uint64_t)ntp_s + NT_EPOCH_TIME_OFFSET) * 10000000;
    nano_debug(LEVEL_TRACE, "Time synced to SNTP server. Changed by %d ms\n",
               (LONG)(ntp_nt_time.LowPart - xbox_nt_time.LowPart) / 10000);
    NtSetSystemTime(&ntp_nt_time, NULL);
}

#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "lwip/dhcp.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/tcp.h"
#include "lwip/apps/sntp.h"

err_t nvnetif_init(struct netif *netif);
static struct netif netif;
const ip4_addr_t *ip;
static ip4_addr_t ipaddr, netmask, gw;

#if (1)
#include "ftpd/ftp.h"
#else //nxdk-ftp lib
#include "nxdk-ftp/ftp.h"
static ftpServer *ftp_server;
static ftpConfig conf;
#endif

static void ftp_thread(void *arg)
{
    LWIP_UNUSED_ARG(arg);
    nano_debug(LEVEL_TRACE, "Waiting for DHCP...\n");
    while (dhcp_supplied_address(&netif) == 0)
    {
        Sleep(100);
    }
    nano_debug(LEVEL_TRACE, "IP address.. %s\n", ip4addr_ntoa(netif_ip4_addr(&netif)));
    nano_debug(LEVEL_TRACE, "Mask........ %s\n", ip4addr_ntoa(netif_ip4_netmask(&netif)));
    nano_debug(LEVEL_TRACE, "Gateway..... %s\n", ip4addr_ntoa(netif_ip4_gw(&netif)));

    #if(1)
    ftp_server();
    #else
    ftpConfig conf;
    conf.enable = true;
    conf.username = "xb1ox";
    conf.password = "xb1ox";
    conf.port = 21;
    ftp_server = new_ftpServer(&conf);
    while(1)
    {
        run_ftpServer(ftp_server);
        Sleep(1);
    }
    delete_ftpServer(ftp_server);
    #endif
}

//Callback when tcpip init is done. This should be in the lwip thread context, so remaining init is safe to call
static void tcpip_init_done(void *arg)
{
    (void)arg;
    //FIXME, may not want to use DHCP all the time
    IP4_ADDR(&gw, 0, 0, 0, 0);
    IP4_ADDR(&ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    netif_add(&netif, &ipaddr, &netmask, &gw, NULL, nvnetif_init, tcpip_input);
    netif_set_default(&netif);
    netif_set_up(&netif);
    dhcp_start(&netif);
    sys_thread_new("ftp_server", ftp_thread, NULL, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

void platform_network_init(void)
{
    tcpip_init(tcpip_init_done, NULL);
}

void platform_network_restart(void)
{
}

void platform_network_deinit(void)
{

}

int platform_network_get_up(void)
{
    return 1;
}

uint32_t platform_network_get_ip(char *rxbuf, uint32_t max_len)
{
    const char *no_ip = "0.0.0.0";
    if (!netif_is_up(&netif) || !netif_is_link_up(&netif))
    {
        strncpy(rxbuf, no_ip, max_len);
        return 0;
    }
    ip4addr_ntoa_r(netif_ip4_addr(&netif), rxbuf, max_len);
    return 1;
}

uint32_t platform_network_get_gateway(char *rxbuf, uint32_t max_len)
{
    const char *no_gw = "0.0.0.0";
    if (!netif_is_up(&netif) || !netif_is_link_up(&netif))
    {
        strncpy(rxbuf, no_gw, max_len);
        return 0;
    }
    uint32_t _gw = ip4_addr_get_u32(netif_ip4_gw(&netif));
    snprintf(rxbuf, max_len, "%d.%d.%d.%d\n", (_gw & 0xff), ((_gw >> 8) & 0xff), ((_gw >> 16) & 0xff), (_gw >> 24));
    return 1;
}

uint32_t platform_network_get_netmask(char *rxbuf, uint32_t max_len)
{
    const char *no_nm = "0.0.0.0";
    if (!netif_is_up(&netif) || !netif_is_link_up(&netif))
    {
        strncpy(rxbuf, no_nm, max_len);
        return 0;
    }
    uint32_t nm = ip4_addr_get_u32(netif_ip4_netmask(&netif));
    snprintf(rxbuf, max_len, "%d.%d.%d.%d\n\r", (nm & 0xff), ((nm >> 8) & 0xff), ((nm >> 16) & 0xff), (nm >> 24));
    return 1;
}
