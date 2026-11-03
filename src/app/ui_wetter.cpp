// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Bildschirm "Wetter": aktuelles Wetter wie auf dem frueheren Hauptscreen, die naechsten vier Stunden, Wind, Luftdruck, Feuchte, Sicht, Sonne und UV,
// dazu zwei waehlbare Kaesten (Modus je Kasten in den Einstellungen, Antippen schaltet zum naechsten Modus):
// 1 Fahrtwind-Gefuehl, 2 Regen in Kuerze, 3 Glaette und Frost, 4 Sonne und Blendung, 5 Aktive Route, 6 Gebiete (bestes am Wochenende), 0 aus.
// Aussehen, Lage und Farben der Elemente kommen aus PicoPixel (ScreenWetter); der Code setzt Texte, Schriften, Symbole und Farben.
#include "app_state.h"
#include "biker.h"
#include "biker_fonts.h"
#include "route.h"
#include "gebiete.h"
#include "../ui/screens.h"
#include "../ui/images.h"
#include "../icons/wetter.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define WETTER_MODULE 7   // 0 bis 6

static bool s_bereit = false;

static void setT(lv_obj_t* o, const char* t) { if (o && strcmp(lv_label_get_text(o), t) != 0) lv_label_set_text(o, t); }
static void setTF(lv_obj_t* o, uint32_t rgb, const char* t) { if (!o) return; setT(o, t); lv_obj_set_style_text_color(o, lv_color_hex(rgb), 0); }
static void komma(char* s) { for (; *s; s++) if (*s == '.') *s = ','; }
static void uhr(time_t t, char* out, size_t n) { struct tm l; localtime_r(&t, &l); snprintf(out, n, "%02d:%02d", l.tm_hour, l.tm_min); }

static const char* wmoText(int c) {
  switch (c) {
    case 0: return "Klar";
    case 1: return "Überwiegend klar";
    case 2: return "Teils bewölkt";
    case 3: return "Bedeckt";
    case 45: case 48: return "Nebel";
    case 51: case 53: case 55: return "Nieselregen";
    case 56: case 57: return "Gefrierender Nieselregen";
    case 61: return "Leichter Regen";
    case 63: return "Regen";
    case 65: return "Starker Regen";
    case 66: case 67: return "Gefrierender Regen";
    case 71: return "Leichter Schneefall";
    case 73: return "Schneefall";
    case 75: return "Starker Schneefall";
    case 77: return "Schneegriesel";
    case 80: return "Leichte Regenschauer";
    case 81: return "Regenschauer";
    case 82: return "Heftige Regenschauer";
    case 85: case 86: return "Schneeschauer";
    case 95: return "Gewitter";
    case 96: case 99: return "Gewitter mit Hagel";
    default: return "Wetter";
  }
}

static void einrichten();

// Modul zeigt ein Symbol (56 px) oder verbirgt es
static void modulIcon(lv_obj_t* img, const lv_img_dsc_t* d) {
  if (!img) return;
  if (!d) { lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN); return; }
  lv_obj_clear_flag(img, LV_OBJ_FLAG_HIDDEN);
  wetterIconZeichnen(img, d, 56, 420, 17);
}

struct ModulZiel { lv_obj_t* box; lv_obj_t* titel; lv_obj_t* wert; lv_obj_t* detail; lv_obj_t* icon; int* modus; };
static ModulZiel s_modul[2];

static void modulRendern(const ModulZiel& z, const BikerPunkt& p, int idx, time_t jetzt) {
  const BikerStunde& h = p.h[idx];
  char b[128], c[32], d[32];
  const int m = *z.modus;
  if (m == 0) { lv_obj_add_flag(z.box, LV_OBJ_FLAG_HIDDEN); return; }
  lv_obj_clear_flag(z.box, LV_OBJ_FLAG_HIDDEN);
  switch (m) {
    case 1: {   // Fahrtwind-Gefuehl bei verschiedenen Reisetempi
      const float f = bikerFahrtwindTemp(h.temp, bikerCfg.fahrtKmh);
      snprintf(b, sizeof(b), "%.0f °C", f);
      setT(z.titel, "Fahrtwind-Gefühl");
      setTF(z.wert, f < 5 ? 0x1D6FD6 : f > 30 ? 0xD01818 : 0x00A838, b);
      if (h.temp > 10.0f) snprintf(b, sizeof(b), "Über 10 °C kein Windchill (bei %.0f km/h)", bikerCfg.fahrtKmh);
      else snprintf(b, sizeof(b), "50 km/h %.0f °C, 80 km/h %.0f °C, 100 km/h %.0f °C", bikerFahrtwindTemp(h.temp, 50), bikerFahrtwindTemp(h.temp, 80), bikerFahrtwindTemp(h.temp, 100));
      setT(z.detail, b);
      modulIcon(z.icon, &thermometer);
      break;
    }
    case 2: {   // Regen in Kuerze (Nowcast, 15-Minuten-Schritte der naechsten 3 Stunden)
      setT(z.titel, "Regen in Kürze");
      float summe = 0; int erst = -1;
      for (int i = 0; i < p.nc_n; i++) {
        if (p.nc_start[i] + 900 <= jetzt) continue;
        summe += p.nc_mm[i];
        if (erst < 0 && p.nc_mm[i] >= 0.1f) erst = i;
      }
      if (p.nc_n == 0) { setTF(z.wert, 0x5A6B7B, "keine Daten"); setT(z.detail, ""); }
      else if (erst < 0) { setTF(z.wert, 0x00A838, "trocken"); snprintf(b, sizeof(b), "3 Stunden ohne Regen"); setT(z.detail, b); }
      else {
        const int min = (int)((p.nc_start[erst] - jetzt) / 60);
        if (min <= 0) snprintf(b, sizeof(b), "es regnet"); else snprintf(b, sizeof(b), "in %d Min.", min);
        setTF(z.wert, summe >= 1.0f ? 0xD01818 : 0xE8A000, b);
        snprintf(b, sizeof(b), "nächste 3 Stunden %.1f mm", summe); komma(b);
        setT(z.detail, b);
      }
      modulIcon(z.icon, &rain);
      break;
    }
    case 3: {   // Glaette und Frost in den naechsten 14 Stunden
      setT(z.titel, "Glätte und Frost");
      float tmin = 99; int hmin = 0; bool glatt = false;
      for (int i = idx; i < p.n && i < idx + 14; i++) {
        const BikerStunde& s = p.h[i];
        if (s.temp < tmin) { tmin = s.temp; struct tm l; localtime_r(&s.t, &l); hmin = l.tm_hour; }
        if (s.temp <= 3.0f && (s.mm >= 0.1f || s.hum > 90)) glatt = true;
      }
      if (tmin <= 0.0f) { setTF(z.wert, 0xD01818, glatt ? "Glätte, Frost" : "Frost"); }
      else if (glatt) setTF(z.wert, 0xE8A000, "Glätte möglich");
      else if (tmin <= 3.0f) setTF(z.wert, 0xE8A000, "Kalt, kein Frost");
      else setTF(z.wert, 0x00A838, "kein Frost");
      snprintf(b, sizeof(b), "Tiefstwert %.0f °C um %02d Uhr", tmin, hmin); komma(b);
      setT(z.detail, b);
      modulIcon(z.icon, &thermometer);
      break;
    }
    case 4: {   // Sonne: hell bis, tiefe Sonne (Blendung)
      setT(z.titel, "Sonne und Blendung");
      const int tag = idx / 24;
      const time_t auf = p.aufgang[tag], unter = p.untergang[tag];
      b[0] = 0; c[0] = 0;
      if (auf && jetzt < auf) { uhr(auf, c, sizeof(c)); snprintf(b, sizeof(b), "hell ab %s", c); }
      else if (unter && jetzt < unter) { uhr(unter, c, sizeof(c)); snprintf(b, sizeof(b), "hell bis %s", c); }
      else if (tag + 1 < 7 && p.aufgang[tag + 1]) { uhr(p.aufgang[tag + 1], c, sizeof(c)); snprintf(b, sizeof(b), "hell ab %s", c); }
      else snprintf(b, sizeof(b), "dunkel");
      setTF(z.wert, unter && jetzt >= unter ? 0x5A6B7B : 0xC8501E, b);
      // tiefe Sonne (unter 15 Grad): Beginn oder Ende im verbleibenden Tageslicht suchen
      d[0] = 0; snprintf(d, sizeof(d), "keine tiefe Sonne mehr");
      if (auf && unter && jetzt < unter) {
        const time_t t0 = jetzt < auf ? auf : jetzt;
        double hh, az; sonnenStand(stdLat(), stdLon(), t0 + 120, &hh, &az);
        const bool tief0 = hh < 15.0;
        for (time_t t = t0 + 120; t < unter; t += 600) {
          sonnenStand(stdLat(), stdLon(), t, &hh, &az);
          const bool tief = hh < 15.0;
          if (tief != tief0) { uhr(t, c, sizeof(c)); snprintf(d, sizeof(d), tief0 ? "Tiefe Sonne bis %s" : "Tiefe Sonne ab %s", c); break; }
        }
        if (tief0 && strcmp(d, "keine tiefe Sonne mehr") == 0) snprintf(d, sizeof(d), "Tiefe Sonne bis Sonnenuntergang");
        if (d[0] == 'T') { snprintf(b, sizeof(b), "%s, Blendung möglich", d); setT(z.detail, b); } else setT(z.detail, d);
      } else setT(z.detail, "");
      modulIcon(z.icon, &wx_clear_day);
      break;
    }
    case 5: {   // aktive Route
      setT(z.titel, "Aktive Route");
      if (routeAkt && routeAkt->route && routeErg.gueltig) {
        snprintf(b, sizeof(b), "Score %d", routeErg.gesamt);
        setTF(z.wert, bikerFarbe(routeErg.gesamt), b);
        uhr(routeErg.abfahrt, c, sizeof(c));
        snprintf(b, sizeof(b), "%s, Abfahrt %s", routeAkt->route->name, c);
        setT(z.detail, b);
      } else { setTF(z.wert, 0x5A6B7B, "keine"); setT(z.detail, "Unter Routen eine Route wählen"); }
      modulIcon(z.icon, nullptr);
      break;
    }
    case 6: {   // bestes Gebiet am Wochenende
      setT(z.titel, "Bestes Gebiet, Wochenende");
      int r[1];
      if (gebieteRang(2, r, 1) > 0) {
        const Gebiet& g = gebiete[r[0]];
        setTF(z.wert, bikerFarbe(g.score[2]), g.name);
        snprintf(b, sizeof(b), "Score %d, bis %.0f °C", g.score[2], g.tmax[2]);
        setT(z.detail, b);
      } else { setTF(z.wert, 0x5A6B7B, "offen"); setT(z.detail, "Gebiete werden berechnet"); }
      modulIcon(z.icon, nullptr);
      break;
    }
    default: break;
  }
}

static void modulTippen(lv_event_t* e) {
  const int i = (int)(intptr_t)lv_event_get_user_data(e);
  *s_modul[i].modus = (*s_modul[i].modus % (WETTER_MODULE - 1)) + 1;   // 1 bis 6, danach wieder von vorn; "Aus" (0) nur in der Web-Oberflaeche (ein ausgeblendeter Kasten laesst sich nicht mehr antippen)
  speichereCfg();
  aktualisiereWetter();
}

static void einrichten() {
  if (s_bereit || !objects.screenwetter) return;
  s_bereit = true;
  auto f = [](lv_obj_t* o, const lv_font_t* font) { if (o) lv_obj_set_style_text_font(o, font, 0); };
  f(objects.labelwetterort, &BIKER_F32);
  f(objects.labelwetterdatum, &BIKER_F32);
  f(objects.labeljetzttemp, &BIKER_F48);
  f(objects.labeljetzttext, &BIKER_F32);
  f(objects.labeljetztgefuehlt, &BIKER_F22);
  f(objects.labelmodul1titel, &BIKER_F18); f(objects.labelmodul1wert, &BIKER_F32); f(objects.labelmodul1detail, &BIKER_F18);
  f(objects.labelmodul2titel, &BIKER_F18); f(objects.labelmodul2wert, &BIKER_F32); f(objects.labelmodul2detail, &BIKER_F18);
  lv_obj_t* zeit[4] = {objects.labelstunde1zeit, objects.labelstunde2zeit, objects.labelstunde3zeit, objects.labelstunde4zeit};
  lv_obj_t* temp[4] = {objects.labelstunde1temp, objects.labelstunde2temp, objects.labelstunde3temp, objects.labelstunde4temp};
  lv_obj_t* det[4] = {objects.labelstunde1detail, objects.labelstunde2detail, objects.labelstunde3detail, objects.labelstunde4detail};
  for (int i = 0; i < 4; i++) { f(zeit[i], &BIKER_F22); f(temp[i], &BIKER_F32); f(det[i], &BIKER_F18); }
  lv_obj_t* dt[6] = {objects.labeldetail1titel, objects.labeldetail2titel, objects.labeldetail3titel, objects.labeldetail4titel, objects.labeldetail5titel, objects.labeldetail6titel};
  lv_obj_t* dw[6] = {objects.labeldetail1wert, objects.labeldetail2wert, objects.labeldetail3wert, objects.labeldetail4wert, objects.labeldetail5wert, objects.labeldetail6wert};
  for (int i = 0; i < 6; i++) { f(dt[i], &BIKER_F18); f(dw[i], &BIKER_F22); }
  f(objects.labelquellewetter, &BIKER_F18);
  // Geometrie (Positionen relativ zum Elternteil): Abstaende und Hoehen passend zu den Schriften, damit nichts ueberlappt
  auto g = [](lv_obj_t* o, int x, int y, int w, int h) { if (o) { lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h); } };
  g(objects.containerwetterjetzt, 10, 58, 500, 190);   // Achtung: objects.containerjetzt gehoert zum Bildschirm Abfahrt, dieser Kasten heisst ContainerWetterJetzt
  g(objects.labeljetzttemp, 156, 12, 330, 60); g(objects.labeljetzttext, 156, 76, 330, 40); g(objects.labeljetztgefuehlt, 14, 150, 472, 34);
  g(objects.containermodul1, 520, 58, 494, 91); g(objects.containermodul2, 520, 157, 494, 91);
  g(objects.labelmodul1titel, 14, 4, 340, 22); g(objects.labelmodul1wert, 14, 24, 340, 40); g(objects.labelmodul1detail, 14, 64, 464, 22);
  g(objects.labelmodul2titel, 14, 4, 340, 22); g(objects.labelmodul2wert, 14, 24, 340, 40); g(objects.labelmodul2detail, 14, 64, 464, 22);
  lv_obj_t* kasten[4] = {objects.containerstunde1, objects.containerstunde2, objects.containerstunde3, objects.containerstunde4};
  const int kx[4] = {10, 264, 518, 772}, kw[4] = {244, 244, 244, 242};
  for (int i = 0; i < 4; i++) { g(kasten[i], kx[i], 256, kw[i], 170); g(zeit[i], 10, 4, 222, 28); g(temp[i], 10, 100, 222, 38); g(det[i], 10, 140, 222, 22); }
  g(objects.containerdetails, 10, 434, 1004, 106);
  const int sx[3] = {14, 348, 682};
  for (int k = 0; k < 6; k++) { const int y = k < 3 ? 2 : 54; g(dt[k], sx[k % 3], y, 310, 22); g(dw[k], sx[k % 3], y + 22, 310, 28); }
  // Textfarben: PicoPixel-Labels ohne Stil sind sehr hell, deshalb hier setzen (dunkel fuer Haupttext, grau fuer Titel und Quelle)
  auto dunkel = [](lv_obj_t* o) { if (o) lv_obj_set_style_text_color(o, lv_color_hex(0x111111), 0); };
  auto grau = [](lv_obj_t* o) { if (o) lv_obj_set_style_text_color(o, lv_color_hex(0x5A6B7B), 0); };
  dunkel(objects.labelwetterort); dunkel(objects.labelwetterdatum); dunkel(objects.labeljetzttext); dunkel(objects.labeljetztgefuehlt);
  dunkel(objects.labelmodul1detail); dunkel(objects.labelmodul2detail);
  for (int i = 0; i < 4; i++) { dunkel(zeit[i]); dunkel(det[i]); }
  for (int i = 0; i < 6; i++) { grau(dt[i]); dunkel(dw[i]); }
  grau(objects.labelmodul1titel); grau(objects.labelmodul2titel); grau(objects.labelquellewetter);
  s_modul[0] = {objects.containermodul1, objects.labelmodul1titel, objects.labelmodul1wert, objects.labelmodul1detail, objects.imagemodul1icon, &cfg.wetter_modul1};
  s_modul[1] = {objects.containermodul2, objects.labelmodul2titel, objects.labelmodul2wert, objects.labelmodul2detail, objects.imagemodul2icon, &cfg.wetter_modul2};
  for (int i = 0; i < 2; i++) if (s_modul[i].box) { lv_obj_add_flag(s_modul[i].box, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(s_modul[i].box, modulTippen, LV_EVENT_CLICKED, (void*)(intptr_t)i); }
}

// Aufrufer haelt den LVGL-Lock
void aktualisiereWetter() {
  if (!objects.screenwetter) return;
  einrichten();
  struct tm ti;
  char b[96];
  static const char* WT[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  if (getLocalTime(&ti, 0)) { snprintf(b, sizeof(b), "%s %02d.%02d.   %02d:%02d", WT[ti.tm_wday], ti.tm_mday, ti.tm_mon + 1, ti.tm_hour, ti.tm_min); setT(objects.labelwetterdatum, b); }
  setT(objects.labelwetterort, stdName().c_str());
  const BikerPunkt* p = bikerHeim;
  const time_t jetzt = time(nullptr);
  const int idx = p ? bikerIndexJetzt(*p, jetzt) : -1;
  if (!p || idx < 0) { setTF(objects.labeljetzttemp, 0x5A6B7B, "-- °C"); setT(objects.labeljetzttext, "Keine Daten"); setT(objects.labeljetztgefuehlt, ""); return; }
  const BikerStunde& h = p->h[idx];

  // Jetzt
  snprintf(b, sizeof(b), "%.0f °C", h.temp);
  setTF(objects.labeljetzttemp, h.temp < 5 ? 0x1D6FD6 : h.temp > 30 ? 0xD01818 : 0x00A838, b);
  setT(objects.labeljetzttext, wmoText(h.wmo));
  snprintf(b, sizeof(b), "Gefühlt %.0f °C, Fahrtwind-Gefühl %.0f °C", h.feels, bikerFahrtwindTemp(h.temp, bikerCfg.fahrtKmh));
  setT(objects.labeljetztgefuehlt, b);
  wetterIconZeichnen(objects.imagejetztsymbol, wmoZuImage(h.wmo, h.tag), 128, 14, 14);

  // naechste vier Stunden
  lv_obj_t* zeit[4] = {objects.labelstunde1zeit, objects.labelstunde2zeit, objects.labelstunde3zeit, objects.labelstunde4zeit};
  lv_obj_t* sym[4] = {objects.imagestunde1symbol, objects.imagestunde2symbol, objects.imagestunde3symbol, objects.imagestunde4symbol};
  lv_obj_t* temp[4] = {objects.labelstunde1temp, objects.labelstunde2temp, objects.labelstunde3temp, objects.labelstunde4temp};
  lv_obj_t* det[4] = {objects.labelstunde1detail, objects.labelstunde2detail, objects.labelstunde3detail, objects.labelstunde4detail};
  for (int k = 0; k < 4; k++) {
    const int i = idx + 1 + k;
    if (i >= p->n) { setT(zeit[k], ""); setT(temp[k], ""); setT(det[k], ""); if (sym[k]) lv_obj_add_flag(sym[k], LV_OBJ_FLAG_HIDDEN); continue; }
    const BikerStunde& s = p->h[i];
    struct tm l; localtime_r(&s.t, &l);
    snprintf(b, sizeof(b), "%02d:00", l.tm_hour); setT(zeit[k], b);
    snprintf(b, sizeof(b), "%.0f °C", s.temp); setTF(temp[k], s.temp < 5 ? 0x1D6FD6 : s.temp > 30 ? 0xD01818 : 0x00A838, b);
    snprintf(b, sizeof(b), "%s %.0f km/h, %.1f mm", windRichtung(s.wdir), s.wind, s.mm); komma(b); setT(det[k], b);
    if (sym[k]) { lv_obj_clear_flag(sym[k], LV_OBJ_FLAG_HIDDEN); wetterIconZeichnen(sym[k], wmoZuImage(s.wmo, s.tag), 64, k == 3 ? 89 : 90, 34); }
  }

  // Details
  lv_obj_t* dw[6] = {objects.labeldetail1wert, objects.labeldetail2wert, objects.labeldetail3wert, objects.labeldetail4wert, objects.labeldetail5wert, objects.labeldetail6wert};
  snprintf(b, sizeof(b), "%.0f km/h aus %s", h.wind, windRichtung(h.wdir)); setTF(dw[0], h.wind >= 50 ? 0xD01818 : h.wind >= 30 ? 0xE8A000 : 0x111111, b);
  snprintf(b, sizeof(b), "%.0f km/h", h.gust); setTF(dw[1], h.gust >= 60 ? 0xD01818 : h.gust >= 40 ? 0xE8A000 : 0x111111, b);
  if (h.druck > 0) {
    const float dr = idx >= 3 && p->h[idx - 3].druck > 0 ? h.druck - p->h[idx - 3].druck : 0;
    snprintf(b, sizeof(b), "%.0f hPa, %s", h.druck, dr >= 1.0f ? "steigend" : dr <= -1.0f ? "fallend" : "gleich");
  } else snprintf(b, sizeof(b), "keine Daten");
  setTF(dw[2], 0x111111, b);
  snprintf(b, sizeof(b), "%d %%", h.hum); setTF(dw[3], 0x111111, b);
  if (h.vis >= 20) snprintf(b, sizeof(b), "> 20 km"); else snprintf(b, sizeof(b), "%.0f km", h.vis);
  setTF(dw[4], h.vis < 1 ? 0xD01818 : h.vis < 5 ? 0xE8A000 : 0x111111, b);
  const time_t su = p->untergang[idx / 24]; char su5[8] = "";
  if (su) uhr(su, su5, sizeof(su5));
  snprintf(b, sizeof(b), "UV %.0f, Sonne bis %s", h.uv, su5); setTF(dw[5], h.uv >= 6 ? 0xD01818 : h.uv >= 3 ? 0xE8A000 : 0x111111, su ? b : "UV --");

  // wählbare Kästen
  for (int i = 0; i < 2; i++) if (s_modul[i].box) modulRendern(s_modul[i], *p, idx, jetzt);
}
