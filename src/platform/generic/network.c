// SPDX-License-Identifier: CC0-1.0
// SPDX-FileCopyrightText: 2022 Ryzee119

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "helpers/nano_debug.h"
#include "platform/platform.h"

void platform_network_init(void)
{
}

void platform_networkrestart(void)
{
}

void platform_networkdeinit(void)
{
}

int platform_networkget_up(void)
{
    return 1;
}

uint32_t platform_networkget_ip(char *rxbuf, uint32_t max_len)
{
    const char *no_ip = "0.0.0.0";
    strncpy(rxbuf, no_ip, max_len);
    return 0;
}

uint32_t platform_networkget_gateway(char *rxbuf, uint32_t max_len)
{
    const char *no_gw = "0.0.0.0";
    strncpy(rxbuf, no_gw, max_len);
    return 0;
}

uint32_t platform_networkget_netmask(char *rxbuf, uint32_t max_len)
{
    const char *no_nm = "0.0.0.0";
    strncpy(rxbuf, no_nm, max_len);
    return 0;
}
