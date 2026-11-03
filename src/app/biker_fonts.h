// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
#pragma once
// Schriften des Biker: Montserrat aus der eingebauten TrueType-Datei (assets/fonts, SIL OFL) in 18, 22, 32 und 48 Punkt mit Umlauten,
// ß, ° und dem ganzen Latin-1-Bereich. Die alten PicoPixel-Schriften font_montserrat_18/22/32/48 haben keine Umlaute; sie werden zur Laufzeit
// ersetzt (uiSchriftenTauschen) und dienen als Rueckfall, falls die TrueType-Schrift nicht angelegt werden kann.
#include "../ui/fonts/fonts.h"
#ifdef __cplusplus
extern "C" {
#endif
extern const lv_font_t* bikerFont18;
extern const lv_font_t* bikerFont22;
extern const lv_font_t* bikerFont32;
extern const lv_font_t* bikerFont48;
void bikerSchriftenInit(void);   // einmal nach lv_init(), mit gehaltenem LVGL-Lock
#ifdef __cplusplus
}
#endif
#define BIKER_F18 (*bikerFont18)
#define BIKER_F22 (*bikerFont22)
#define BIKER_F32 (*bikerFont32)
#define BIKER_F48 (*bikerFont48)
