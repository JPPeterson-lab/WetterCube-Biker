// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Daten → Oberfläche: Bildschirm "Heute" des WetterCube Biker, Bootscreen und Einrichtungs-Hinweis
#include "biker_fonts.h"
#include "app_state.h"
#include "biker.h"
#include <time.h>
#include <math.h>
#include "../ui/screens.h"
#include "../ui/fonts/fonts.h"

// Alle Setter ändern nur, was sich wirklich geändert hat: jede Änderung löst sonst eine Neuzeichnung aus.
static void setLabel(lv_obj_t* obj, const char* text) {
  if (obj && strcmp(lv_label_get_text(obj), text) != 0) lv_label_set_text(obj, text);
}
static void setLabelFarbe(lv_obj_t* obj, uint32_t rgb, const char* text) {
  if (!obj) return;
  setLabel(obj, text);
  lv_color_t c = lv_color_hex(rgb);
  if (lv_color_to32(lv_obj_get_style_text_color(obj, LV_PART_MAIN)) != lv_color_to32(c)) lv_obj_set_style_text_color(obj, c, 0);
}
static void komma(char* s) { for (; *s; s++) if (*s == '.') *s = ','; }

// ── Stunden-Balken (Score der nächsten 12 Stunden) ───────────────────────────
static const int BALKEN_N = 12;
static lv_obj_t* balkenObj[BALKEN_N];
static lv_obj_t* balkenLbl[BALKEN_N];

static void balkenAnlegen() {
  lv_obj_t* c = objects.containerstundenbalken;
  if (!c || balkenObj[0]) return;
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  const int w = 40, abstand = 8;
  for (int i = 0; i < BALKEN_N; i++) {
    balkenObj[i] = lv_obj_create(c);
    lv_obj_remove_style_all(balkenObj[i]);
    lv_obj_set_style_bg_opa(balkenObj[i], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(balkenObj[i], 4, 0);
    lv_obj_set_width(balkenObj[i], w);
    lv_obj_set_pos(balkenObj[i], i * (w + abstand) + 2, 0);
    balkenLbl[i] = lv_label_create(c);
    lv_obj_set_style_text_font(balkenLbl[i], &BIKER_F18, 0);
    lv_obj_set_style_text_color(balkenLbl[i], lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_align(balkenLbl[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(balkenLbl[i], w + 8, 22);
    lv_obj_set_pos(balkenLbl[i], i * (w + abstand) - 2, 56);
  }
}

static void balkenSetzen(const BikerPunkt& p, int ab) {
  balkenAnlegen();
  const int hoehe = 52;
  for (int i = 0; i < BALKEN_N; i++) {
    if (!balkenObj[i]) return;
    const int idx = ab + i;
    if (idx >= p.n) { lv_obj_add_flag(balkenObj[i], LV_OBJ_FLAG_HIDDEN); setLabel(balkenLbl[i], ""); continue; }
    lv_obj_clear_flag(balkenObj[i], LV_OBJ_FLAG_HIDDEN);
    const int sc = bikerScore(p.h[idx], bikerCfg.fahrtKmh);
    int hh = sc * hoehe / 100; if (hh < 4) hh = 4;
    lv_obj_set_height(balkenObj[i], hh);
    lv_obj_set_y(balkenObj[i], hoehe - hh);
    lv_obj_set_style_bg_color(balkenObj[i], lv_color_hex(bikerFarbe(sc)), 0);
    struct tm t; localtime_r(&p.h[idx].t, &t);
    char b[8]; snprintf(b, sizeof(b), "%02d", t.tm_hour);
    setLabel(balkenLbl[i], b);
  }
}

// ── Datum und Uhrzeit ────────────────────────────────────────────────────────
// Kopfzeile: Uhrzeit gross (rechts), Datum in der Mitte davor, Ortsname links
static void hinweiseSetzen(const BikerPunkt& p, int idx);
static lv_obj_t* s_datumTag = nullptr;
static void kopfzeileEinrichten() {
  if (s_datumTag || !objects.labeldatum || !objects.screenheute) return;
  lv_obj_set_pos(objects.labeldatum, 844, 2);
  lv_obj_set_size(objects.labeldatum, 166, 56);
  lv_obj_set_style_text_font(objects.labeldatum, &BIKER_F48, 0);
  lv_obj_set_style_text_align(objects.labeldatum, LV_TEXT_ALIGN_RIGHT, 0);
  s_datumTag = lv_label_create(objects.screenheute);
  lv_obj_set_style_text_font(s_datumTag, &BIKER_F32, 0);
  lv_obj_set_style_text_color(s_datumTag, lv_color_hex(0x111111), 0);
  lv_obj_set_style_text_align(s_datumTag, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(s_datumTag, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(s_datumTag, 190, 40);
  lv_obj_set_pos(s_datumTag, 650, 12);
  // Warn-Button breiter, Quellenhinweis daneben
  if (objects.buttonheutewarnung) lv_obj_set_width(objects.buttonheutewarnung, 246);
  if (objects.labelquelleheute) {   // zwei Zeilen, damit die Quellenangabe nicht abgeschnitten wird
    lv_obj_set_pos(objects.labelquelleheute, 600, 538);
    lv_obj_set_size(objects.labelquelleheute, 300, 50);
    lv_label_set_long_mode(objects.labelquelleheute, LV_LABEL_LONG_WRAP);
    lv_label_set_text(objects.labelquelleheute, "Quelle: Deutscher Wetterdienst\nOpen-Meteo.com");
    lv_obj_set_style_text_align(objects.labelquelleheute, LV_TEXT_ALIGN_CENTER, 0);
  }
}
static void setDatumUhrzeit(const struct tm& ti) {
  static const char* WT[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  kopfzeileEinrichten();
  char b[24];
  snprintf(b, sizeof(b), "%02d:%02d", ti.tm_hour, ti.tm_min);
  setLabel(objects.labeldatum, b);
  snprintf(b, sizeof(b), "%s %02d.%02d.", WT[ti.tm_wday], ti.tm_mday, ti.tm_mon + 1);
  setLabel(s_datumTag, b);
}

void aktualisiereUhr() {
  struct tm ti;
  if (!getLocalTime(&ti, 0)) return;
  LvLock lock;
  setDatumUhrzeit(ti);
  const BikerPunkt* p = bikerHeim;
  const int idx = p ? bikerIndexJetzt(*p, time(nullptr)) : -1;
  if (p && idx >= 0) hinweiseSetzen(*p, idx);   // Regen-Nowcast zeitlich nachfuehren
}

void aktualisiereHinweise() { aktualisiereUhr(); }   // Regen-, Frost- und UV-Hinweise stehen im Warn-Button auf dem Bildschirm "Heute"



// ── Hinweise (Regen, Frost, UV) ──────────────────────────────────────────────
// Erscheinen in der Titelzeile des Empfehlungs-Feldes. Die Schalter stehen in den Einstellungen (cfg.hinweis_regen/_frost/_uv).
static bool hinweisRegen(const BikerPunkt& p, char* buf, size_t n) {
  const float SCHWELLE = 0.1f;   // mm je 15 Min. (ab hier zaehlt ein Slot als Regen)
  const time_t jetzt = time(nullptr);
  if (p.nc_n <= 0 || jetzt < 1700000000L || jetzt - p.nc_start[0] >= 6 * 3600L) return false;
  int aktuell = -1;
  for (int i = 0; i < p.nc_n; i++) if (jetzt >= p.nc_start[i] && jetzt < p.nc_start[i] + 900) { aktuell = i; break; }
  if (aktuell >= 0 && p.nc_mm[aktuell] >= SCHWELLE) {
    int ende = -1;
    for (int i = aktuell + 1; i < p.nc_n; i++) if (p.nc_mm[i] < SCHWELLE) { ende = i; break; }
    if (ende < 0) { snprintf(buf, n, "Es regnet"); return true; }
    int m = (int)((p.nc_start[ende] - jetzt + 59) / 60);
    snprintf(buf, n, "Regen noch %d Min.", m < 1 ? 1 : m);
    return true;
  }
  for (int i = 0; i < p.nc_n; i++) {
    if (p.nc_start[i] + 900 <= jetzt) continue;
    if (p.nc_mm[i] >= SCHWELLE) { int m = (int)((p.nc_start[i] - jetzt + 59) / 60); snprintf(buf, n, "Regen in %d Min.", m < 1 ? 1 : m); return true; }
  }
  return false;
}
bool bikerRegenHinweis(char* buf, size_t n) { return bikerHeim && bikerHeim->geladen && hinweisRegen(*bikerHeim, buf, n); }
static bool hinweisFrost(const BikerPunkt& p, int idx, char* buf, size_t n) {
  float tmin = 99; int ab = -1;
  for (int i = idx; i <= idx + 12 && i < p.n; i++) { if (p.h[i].temp < tmin) tmin = p.h[i].temp; if (ab < 0 && p.h[i].temp <= 0.0f) ab = i; }
  if (ab < 0) return false;
  const int t = (int)floorf(tmin + 0.5f);
  if (ab == idx) { snprintf(buf, n, "Frost, bis %d °C", t); return true; }
  struct tm tt; localtime_r(&p.h[ab].t, &tt);
  snprintf(buf, n, "Frost ab %02d:00, %d °C", tt.tm_hour, t);
  return true;
}
static bool hinweisUv(const BikerPunkt& p, int idx, char* buf, size_t n) {
  float uv = 0;
  for (int i = idx; i <= idx + 3 && i < p.n; i++) if (p.h[i].uv > uv) uv = p.h[i].uv;
  if (uv < 6.0f) return false;
  snprintf(buf, n, "UV %s (%d)", uv >= 11.0f ? "extrem" : uv >= 8.0f ? "sehr hoch" : "hoch", (int)floorf(uv + 0.5f));
  return true;
}
// Erster aktiver Hinweis (und wie viele es sind) fuer den Warn-Button auf "Heute" (ui_warn.cpp)
static char s_hinweisText[96] = ""; static uint32_t s_hinweisFarbe = 0; static int s_hinweisAnzahl = 0;
bool hinweisFuerButton(char* buf, size_t n, uint32_t* farbe) {
  if (!s_hinweisAnzahl) return false;
  if (s_hinweisAnzahl > 1) snprintf(buf, n, "%s +%d", s_hinweisText, s_hinweisAnzahl - 1); else snprintf(buf, n, "%s", s_hinweisText);
  *farbe = s_hinweisFarbe;
  return true;
}
static void hinweiseSetzen(const BikerPunkt& p, int idx) {
  char b[3][48]; int k = 0; uint32_t farbe = 0x5A6B7B;
  if (!warnRuheAktiv()) {
    if (cfg.hinweis_regen && hinweisRegen(p, b[k], sizeof(b[k]))) { if (!k) farbe = 0x1D6FD6; k++; }
    if (cfg.hinweis_frost && hinweisFrost(p, idx, b[k], sizeof(b[k]))) { if (!k) farbe = 0x0A8FB0; k++; }
    if (cfg.hinweis_uv && hinweisUv(p, idx, b[k], sizeof(b[k]))) { if (!k) farbe = 0xC8501E; k++; }
  }
  s_hinweisAnzahl = k;
  if (k) { strlcpy(s_hinweisText, b[0], sizeof(s_hinweisText)); s_hinweisFarbe = farbe; }
  setLabelFarbe(objects.labelempfehlungtitel, 0x5A6B7B, "Empfehlung");
  aktualisiereDwdWarnBtn();
}

// ── Bildschirm "Heute" ───────────────────────────────────────────────────────
void aktualisiereUI() {
  LvLock lock;
  aktualisiereStandort();
  aktualisiereVerlauf();
  aktualisiereWoche();
  aktualisiereWetter();
  struct tm ti;
  time_t jetzt = time(nullptr);
  if (getLocalTime(&ti, 0)) setDatumUhrzeit(ti);

  const BikerPunkt* p = bikerHeim;
  const int idx = p ? bikerIndexJetzt(*p, jetzt) : -1;
  if (!p || idx < 0) {
    setLabelFarbe(objects.labelscorezahl, 0x5A6B7B, "--");
    setLabel(objects.labelscorestufe, "Keine Daten");
    return;
  }
  const BikerStunde& h = p->h[idx];
  const int sc = bikerScore(h, bikerCfg.fahrtKmh);
  const uint32_t farbe = bikerFarbe(sc);
  char b[96];

  // Score
  if (objects.arcscore) {
    lv_arc_set_value(objects.arcscore, sc);
    lv_obj_set_style_arc_color(objects.arcscore, lv_color_hex(farbe), LV_PART_INDICATOR);
  }
  snprintf(b, sizeof(b), "%d", sc);
  setLabelFarbe(objects.labelscorezahl, farbe, b);
  setLabel(objects.labelscorestufe, bikerStufe(sc));

  // Beste Fahrzeit: heute ab jetzt, sonst morgen
  int von = 0, bis = 0, schnitt = 0;
  int tagNr = 0;
  bool ok = bikerBesteFahrzeit(*p, 0, jetzt, &von, &bis, &schnitt);
  if (!ok) { tagNr = 1; ok = bikerBesteFahrzeit(*p, 1, jetzt, &von, &bis, &schnitt); }
  setLabel(objects.labelbestetitel, tagNr == 0 ? "Beste Fahrzeit heute" : "Beste Fahrzeit morgen");
  if (ok) {
    struct tm a, e;
    localtime_r(&p->h[von].t, &a);
    time_t ende = p->h[bis - 1].t + 3600; localtime_r(&ende, &e);
    snprintf(b, sizeof(b), "%02d:00 - %02d:00", a.tm_hour, e.tm_hour == 0 ? 24 : e.tm_hour);
    setLabelFarbe(objects.labelbestewert, bikerFarbe(schnitt), b);
    struct tm sn; time_t su = p->untergang[tagNr]; localtime_r(&su, &sn);
    snprintf(b, sizeof(b), "Score im Schnitt %d, Sonnenuntergang %02d:%02d", schnitt, sn.tm_hour, sn.tm_min);
    setLabel(objects.labelbestedetail, su ? b : "");
  } else {
    setLabelFarbe(objects.labelbestewert, 0xD01818, "keine");
    setLabel(objects.labelbestedetail, "Heute und morgen kein gutes Fenster");
  }

  // Temperatur und Fahrtwind-Gefühl
  snprintf(b, sizeof(b), "%.0f °C", h.temp);
  setLabelFarbe(objects.labeltempwert, h.temp < 5 ? 0x1D6FD6 : h.temp > 30 ? 0xD01818 : 0x00A838, b);
  snprintf(b, sizeof(b), "Fahrtwind-Gefühl %.0f °C", bikerFahrtwindTemp(h.temp, bikerCfg.fahrtKmh));
  setLabel(objects.labeltempdetail, b);

  // Wind
  snprintf(b, sizeof(b), "%.0f km/h", h.wind);
  setLabelFarbe(objects.labelwindwert, h.gust >= 60 ? 0xD01818 : h.gust >= 40 ? 0xE8A000 : 0x00A838, b);
  snprintf(b, sizeof(b), "Böen %.0f km/h, %s", h.gust, windRichtung(h.wdir));
  setLabel(objects.labelwinddetail, b);

  // Regen
  snprintf(b, sizeof(b), "%.1f mm", h.mm); komma(b);
  setLabelFarbe(objects.labelregenwert, h.mm >= 1.0f ? 0xD01818 : h.mm >= 0.1f ? 0xE8A000 : 0x00A838, b);
  int regenIdx = -1;
  for (int i = idx; i < p->n && i < idx + 24; i++) if (p->h[i].mm >= 0.1f) { regenIdx = i; break; }
  if (regenIdx < 0) snprintf(b, sizeof(b), "%d %%, 24 Std. trocken", h.pop);
  else if (regenIdx == idx) snprintf(b, sizeof(b), "%d %%, es regnet", h.pop);
  else { struct tm r; localtime_r(&p->h[regenIdx].t, &r); snprintf(b, sizeof(b), "%d %%, trocken bis %02d Uhr", h.pop, r.tm_hour); }
  setLabel(objects.labelregendetail, b);

  // Sicht und Wolken
  if (h.vis >= 20) snprintf(b, sizeof(b), "> 20 km"); else snprintf(b, sizeof(b), "%.0f km", h.vis);
  setLabelFarbe(objects.labelsichtwert, h.vis < 1 ? 0xD01818 : h.vis < 5 ? 0xE8A000 : 0x00A838, b);
  snprintf(b, sizeof(b), "Wolken %d %%, Feuchte %d %%", h.cloud, h.hum);
  setLabel(objects.labelsichtdetail, b);

  hinweiseSetzen(*p, idx);
  balkenSetzen(*p, idx);
  bikerEmpfehlung(*p, idx, idx + 8, b, sizeof(b));
  setLabel(objects.labelempfehlungtext, b);
}

// ── Bootscreen und Einrichtungs-Hinweis ──────────────────────────────────────
void uiBoot(const char* text, int pct) {
  LvLock lock;
  setLabel(objects.labelstatus, text);
  if (objects.barwifi) lv_bar_set_value(objects.barwifi, pct, LV_ANIM_OFF);
}

static lv_obj_t* hinweisScreen = nullptr;
static lv_obj_t* hinweisTitel  = nullptr;
static lv_obj_t* hinweisText   = nullptr;

void uiHinweis(const char* titel, const char* text) {
  LvLock lock;
  if (!hinweisScreen) {
    hinweisScreen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(hinweisScreen, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(hinweisScreen, LV_OPA_COVER, 0);

    hinweisTitel = lv_label_create(hinweisScreen);
    lv_obj_set_style_text_font(hinweisTitel, &BIKER_F32, 0);
    lv_obj_set_style_text_color(hinweisTitel, lv_color_hex(0xf5a623), 0);
    lv_obj_align(hinweisTitel, LV_ALIGN_TOP_MID, 0, 50);

    hinweisText = lv_label_create(hinweisScreen);
    lv_label_set_long_mode(hinweisText, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hinweisText, 860);
    lv_obj_set_style_text_font(hinweisText, &BIKER_F22, 0);
    lv_obj_set_style_text_color(hinweisText, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_align(hinweisText, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hinweisText, LV_ALIGN_TOP_MID, 0, 130);
  }
  lv_label_set_text(hinweisTitel, titel);
  lv_label_set_text(hinweisText, text);
  lv_scr_load(hinweisScreen);
}
