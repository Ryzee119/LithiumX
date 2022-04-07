// SPDX-License-Identifier: CC0-1.0
// SPDX-FileCopyrightText: 2022 Ryzee119

#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0

// Compile nanoprintf in this translation unit.
#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf/nanoprintf.h"

#include "helpers/nano_debug.h"
#include <stdarg.h>
#include <string.h>

#ifdef NXDK
#include <xboxkrnl/xboxkrnl.h>
#include <hal/debug.h>

//Can printf debug info on Xbox SMBus. Use a device to read it at I2C addr 0x01.
//My Arduino Atmega32u4 bridge code: https://gist.github.com/Ryzee119/5a48f5fc68690c947fef3143887cc7dd
#ifdef DBG_I2C
#define I2C_ADDR 0x01
static void i2c_printf(const char *format, ...)
{
    static char buff[512];
    va_list args;
    va_start(args, format);
    npf_vsnprintf(buff, sizeof(buff),format, args);
    va_end(args);
    int len = strlen(buff);
    for (int i = 0; i < len; i++)
    {
        HalWriteSMBusValue(I2C_ADDR << 1, 0x0C, FALSE, buff[i]);
    }
}
#define ppf i2c_printf
#else //DBG_I2C
//Normal xbox debug is out of superio. Start xemu with `-device lpc47m157 -serial stdio`
#define ppf DbgPrint
#endif
#else //NXDK
//On PC builds, just output of stdio.
#include <stdio.h>
#define ppf printf
#endif

void lvgl_putstring(const char *buf)
{
    ppf("%s", buf);
}

static void npf_putchar(int c, void *ctx)
{
    (void)ctx;
    ppf("%c", c);
}

void nano_debug(int level, const char *format, ...)
{
    if (level < NANO_DEBUG_LEVEL)
    {
        return;
    }
    va_list argList;
    va_start(argList, format);
    npf_vpprintf(npf_putchar, NULL, format, argList);
    va_end(argList);
}
