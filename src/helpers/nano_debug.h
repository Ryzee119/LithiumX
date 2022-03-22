// SPDX-License-Identifier: CC0-1.0
// SPDX-FileCopyrightText: 2022 Ryzee119

#pragma once

#include <stddef.h>

enum{

    LEVEL_TRACE,
    LEVEL_WARN,
    LEVEL_ERROR,
    LEVEL_NONE
};

#ifndef NANO_DEBUG_LEVEL
#define NANO_DEBUG_LEVEL LEVEL_WARN
#endif

void lvgl_putstring(const char *buf);
void nano_debug(int level, const char *format, ...);
