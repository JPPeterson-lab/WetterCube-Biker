// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Systembereich im Menue-Bildschirm (neben den Einstellungen): WLAN, Adresse, Version und Update, Speicher, Laufzeit, Neustart,
// Speicherkarte, Datenlaufwerk. Der Rahmen (ContainerSystem) kommt aus PicoPixel, der Inhalt entsteht im Code.
#include "biker_fonts.h"
#include "app_state.h"
#include "../ui/screens.h"
#include "../ui/colors.h"
#include "../ui/fonts/fonts.h"
#include "../storage/storage.h"
#include "../storage/daten.h"

static const int ZEILEN = 8;
static lv_obj_t* s_name[ZEILEN]; static lv_obj_t* s_wert[ZEILEN];
static char s_text[ZEILEN][96];
static bool s_bereit = false;

static uint32_t txtFarbe()  { return theme_colors[active_theme_index][THEME_COLOR_TEXT] & 0xFFFFFF; }
static uint32_t nebenFarbe() { return active_theme_index == THEME_DARK ? 0xE2E8F0 : 0x334155; }

static lv_obj_t* beschriftung(lv_obj_t* eltern, const lv_font_t* f, int x, int y, int w, const char* t) {
  lv_obj_t* l = lv_label_create(eltern);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  lv_obj_set_width(l, w); lv_obj_set_height(l, lv_font_get_line_height(f)); lv_obj_set_pos(l, x, y);
  lv_obj_set_style_text_font(l, f, 0);
  lv_label_set_text(l, t);
  return l;
}

static void einrichten() {
  lv_obj_t* host = objects.containersystem;
  if (s_bereit || !host) return;
  s_bereit = true;
  lv_obj_clear_flag(host, LV_OBJ_FLAG_SCROLLABLE);
  static const char* NAMEN[ZEILEN] = {"WLAN", "Adresse", "Version", "Speicher", "Laufzeit", "Neustart", "Karte", "Daten"};
  lv_obj_t* titel = beschriftung(host, &BIKER_F22, 14, 6, 480, "System");
  lv_obj_set_style_text_color(titel, lv_color_hex(txtFarbe()), 0);
  for (int i = 0; i < ZEILEN; i++) {
    s_name[i] = beschriftung(host, &BIKER_F18, 14, 38 + i * 28, 130, NAMEN[i]);
    s_wert[i] = beschriftung(host, &BIKER_F18, 150, 38 + i * 28, 366, "");
    s_text[i][0] = 0;
  }
}

static void zeileSetzen(int i, const char* t) {
  if (strcmp(s_text[i], t) == 0) return;
  strlcpy(s_text[i], t, sizeof(s_text[i]));
  lv_label_set_text(s_wert[i], t);
}

static void aktualisieren() {
  einrichten();
  if (objects.labelversion) lv_label_set_text(objects.labelversion, FIRMWARE_VERSION);
  if (!s_bereit) return;
  for (int i = 0; i < ZEILEN; i++) {
    lv_obj_set_style_text_color(s_name[i], lv_color_hex(nebenFarbe()), 0);
    lv_obj_set_style_text_color(s_wert[i], lv_color_hex(txtFarbe()), 0);
  }
  char b[96];
  if (WiFi.status() == WL_CONNECTED) snprintf(b, sizeof(b), "%s, %d dBm", WiFi.SSID().c_str(), (int)WiFi.RSSI()); else snprintf(b, sizeof(b), "nicht verbunden");
  zeileSetzen(0, b);
  if (WiFi.status() == WL_CONNECTED) snprintf(b, sizeof(b), "%s", WiFi.localIP().toString().c_str()); else snprintf(b, sizeof(b), "-");
  zeileSetzen(1, b);
  if (cfg.update_modus == 0)      snprintf(b, sizeof(b), "%s, Updates aus", FIRMWARE_VERSION);
  else if (cfg.update_modus == 1) snprintf(b, sizeof(b), "%s", FIRMWARE_VERSION);   // Modus 1: kein Update-Hinweis am Display
  else snprintf(b, sizeof(b), "%s, %s", FIRMWARE_VERSION, updateVerfuegbar ? (String("Update ") + updateNeueVersion + " verfügbar").c_str() : "aktuell");
  zeileSetzen(2, b);
  snprintf(b, sizeof(b), "%u KB intern, %.1f MB PSRAM frei", (unsigned)(ESP.getFreeHeap() / 1024), ESP.getFreePsram() / 1048576.0);
  zeileSetzen(3, b);
  { unsigned long s = millis() / 1000UL; unsigned long d = s / 86400UL, h = (s / 3600UL) % 24, m = (s / 60UL) % 60;
    if (d) snprintf(b, sizeof(b), "%lu Tg. %lu Std. %lu Min.", d, h, m); else snprintf(b, sizeof(b), "%lu Std. %lu Min.", h, m); }
  zeileSetzen(4, b);
  zeileSetzen(5, resetGrundText.length() ? resetGrundText.c_str() : "-");
  zeileSetzen(6, speicherStatusText());
  snprintf(b, sizeof(b), "%s", datenStatusText());
  zeileSetzen(7, b);
}

void uiSystemErzwingen() { aktualisieren(); }

static void zeitgeber(lv_timer_t*) { if (lv_scr_act() == objects.screenmenu) aktualisieren(); }
static void menueGeoeffnet(lv_event_t*) { aktualisieren(); }

void uiSystemInit() {
  LvLock lock;
  if (!objects.screenmenu || !objects.containersystem) return;
  lv_obj_add_event_cb(objects.screenmenu, menueGeoeffnet, LV_EVENT_SCREEN_LOAD_START, nullptr);
  lv_timer_create(zeitgeber, 2000, nullptr);
}
