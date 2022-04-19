// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "helpers/nano_debug.h"
#include "platform/platform.h"

#include <windows.h>
#include "lwip/opt.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "lwip/dhcp.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/tcp.h"
#include "ftpd/ftp.h"

err_t nvnetif_init(struct netif *netif);
static struct netif netif;
const ip4_addr_t *ip;
static ip4_addr_t ipaddr, netmask, gw;

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
    ftp_server();
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
    sys_thread_new("http_server_netconn", ftp_thread, NULL, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
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
    //pcapif_shutdown(&netif);
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
