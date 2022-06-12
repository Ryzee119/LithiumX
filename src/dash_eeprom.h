// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _EEPROM_H
#define _EEPROM_H

#ifdef __cplusplus
extern "C" {
#endif

void eeprom_init(void);
void eeprom_deinit(void);
void eeprom_open(void);

#ifdef __cplusplus
}
#endif

#endif
