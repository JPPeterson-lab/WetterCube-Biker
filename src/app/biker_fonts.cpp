// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Schriften aus der eingebauten Montserrat-TrueType-Datei (lv_tiny_ttf), siehe biker_fonts.h
#include "biker_fonts.h"
#include <lvgl.h>
#include <stdio.h>

extern const uint8_t ttf_start[] asm("_binary_Montserrat_Regular_ttf_start");
extern const uint8_t ttf_end[]   asm("_binary_Montserrat_Regular_ttf_end");

const lv_font_t* bikerFont18 = &font_montserrat_18;
const lv_font_t* bikerFont22 = &font_montserrat_22;
const lv_font_t* bikerFont32 = &font_montserrat_32;
const lv_font_t* bikerFont48 = &font_montserrat_32;   // die 48er PicoPixel-Schrift wird nicht mehr exportiert (kein Label bindet sie)

void bikerSchriftenInit(void) {
  const size_t n = (size_t)(ttf_end - ttf_start);
  struct { const lv_font_t** ziel; int px; } f[4] = {{&bikerFont18, 18}, {&bikerFont22, 22}, {&bikerFont32, 32}, {&bikerFont48, 48}};
  for (auto& e : f) {
    lv_font_t* neu = lv_tiny_ttf_create_data(ttf_start, n, e.px);
    if (neu) {
      neu->fallback = *e.ziel;   // Zeichen ausserhalb von Latin-1 (zum Beispiel Symbole) aus der alten Schrift
      *e.ziel = neu;
    } else printf("[Schrift] %d px: TrueType-Schrift nicht angelegt, alte Schrift bleibt\n", e.px);
  }
}
