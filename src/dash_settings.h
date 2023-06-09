// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _DASH_SETTINGS_H
#define _DASH_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lithiumx.h"

void dash_settings_open(void);
void dash_settings_apply(void);
void dash_settings_read(void);

#ifdef __cplusplus
}
#endif

#endif
