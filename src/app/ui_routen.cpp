// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Routen am Geraet: Eingabe mit Bildschirmtastatur, Liste, Routenwetter (Schemakarte, Abschnitte), Hoehe/Wind/Sonne, Abfahrtszeit-Planer.
// PicoPixel liefert die Rahmen, Listen, Chips, Karte und Balken entstehen hier im Code.
#include "biker_fonts.h"
#include "app_state.h"
#include "route.h"
#include "navigation.h"
#include "../storage/daten.h"
#include "../ui/screens.h"
#include "../ui/fonts/fonts.h"
#include <math.h>
#include "gebiete.h"
#include "kacheln.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const lv_font_t* F18() { return &BIKER_F18; }
static const lv_font_t* F22() { return &BIKER_F22; }
static const lv_font_t* F32() { return &BIKER_F32; }

static lv_obj_t* lab(lv_obj_t* eltern, const lv_font_t* f, uint32_t farbe, int x, int y, int w, int h, const char* text, lv_text_align_t al = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t* l = lv_label_create(eltern);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(farbe), 0);
  lv_obj_set_style_text_align(l, al, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(l, w, h);
  lv_obj_set_pos(l, x, y);
  lv_label_set_text(l, text);
  return l;
}
static void setLabel(lv_obj_t* o, const char* t) { if (o && strcmp(lv_label_get_text(o), t) != 0) lv_label_set_text(o, t); }
static void setLabelFarbe(lv_obj_t* o, uint32_t rgb, const char* t) { if (!o) return; setLabel(o, t); lv_obj_set_style_text_color(o, lv_color_hex(rgb), 0); }
static void kommaText(char* s) { for (; *s; s++) if (*s == '.') *s = ','; }
static void zeitText(time_t t, char* b, size_t n, bool mitTag) {
  static const char* WT[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  struct tm tm; localtime_r(&t, &tm);
  if (mitTag) snprintf(b, n, "%s %02d:%02d", WT[tm.tm_wday], tm.tm_hour, tm.tm_min); else snprintf(b, n, "%02d:%02d", tm.tm_hour, tm.tm_min);
}
static lv_obj_t* ebene(lv_obj_t* eltern) {   // durchsichtige Zeichenflaeche ueber die ganze Flaeche des Eltern-Objekts
  lv_obj_t* e = lv_obj_create(eltern);
  lv_obj_remove_style_all(e);
  lv_obj_set_size(e, lv_pct(100), lv_pct(100));
  lv_obj_clear_flag(e, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(e, LV_OBJ_FLAG_CLICKABLE);   // Beruehrungen gehen an das darunterliegende Feld
  return e;
}

// ── Auftraege (Rechnen im Hintergrund, Anzeige darueber) ─────────────────────
static lv_obj_t* s_decke = nullptr;       // Fenster ueber allem: "Rechne ..."
static lv_obj_t* s_deckeText = nullptr;
static lv_timer_t* s_poll = nullptr;
static int  s_nachFertig = 0;             // 0 = nichts, 1 = zum Routenwetter wechseln
static int  s_fehlerRest = 0;

static void deckeEinrichten() {
  if (s_decke) return;
  s_decke = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(s_decke);
  lv_obj_set_size(s_decke, 1024, 600);
  lv_obj_set_style_bg_color(s_decke, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(s_decke, LV_OPA_60, 0);
  lv_obj_add_flag(s_decke, LV_OBJ_FLAG_CLICKABLE);   // faengt Beruehrungen ab
  lv_obj_t* kasten = lv_obj_create(s_decke);
  lv_obj_remove_style_all(kasten);
  lv_obj_set_size(kasten, 700, 200);
  lv_obj_center(kasten);
  lv_obj_set_style_bg_color(kasten, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(kasten, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(kasten, 12, 0);
  s_deckeText = lab(kasten, F32(), 0x111111, 20, 20, 660, 160, "", LV_TEXT_ALIGN_CENTER);
  lv_label_set_long_mode(s_deckeText, LV_LABEL_LONG_WRAP);
  lv_obj_add_flag(s_decke, LV_OBJ_FLAG_HIDDEN);
}

static void routeWetterAnzeigen();

static void pollCb(lv_timer_t* t) {
  if (routeStatus == RS_BESCHAEFTIGT) { setLabel(s_deckeText, routeStatusText); return; }
  if (routeStatus == RS_FERTIG) {
    lv_obj_add_flag(s_decke, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(t);
    routeStatus = RS_LEER;
    if (s_nachFertig == 1) { routeWetterAnzeigen(); nav_goto(SCREEN_ID_SCREENROUTEWETTER); }
    s_nachFertig = 0;
    return;
  }
  if (routeStatus == RS_FEHLER) {   // Fehlertext einige Sekunden zeigen
    setLabelFarbe(s_deckeText, 0xB00000, routeStatusText);
    if (s_fehlerRest == 0) s_fehlerRest = 12;
    if (--s_fehlerRest == 0) { lv_obj_add_flag(s_decke, LV_OBJ_FLAG_HIDDEN); lv_timer_pause(t); routeStatus = RS_LEER; setLabelFarbe(s_deckeText, 0x111111, ""); }
    return;
  }
  lv_obj_add_flag(s_decke, LV_OBJ_FLAG_HIDDEN);
  lv_timer_pause(t);
}

static void auftragAnzeigen(const char* text) {
  deckeEinrichten();
  setLabelFarbe(s_deckeText, 0x111111, text);
  lv_obj_clear_flag(s_decke, LV_OBJ_FLAG_HIDDEN);
  s_fehlerRest = 0;
  if (!s_poll) s_poll = lv_timer_create(pollCb, 400, nullptr);
  lv_timer_resume(s_poll);
}

// ── Bildschirmtastatur (deutsche Belegung mit Umlauten) ──────────────────────
static lv_obj_t* s_kb = nullptr;
static lv_obj_t* s_ta = nullptr;
static bool      s_gross = false;

static const char* MAP_KLEIN[] = {
  "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
  "q", "w", "e", "r", "t", "z", "u", "i", "o", "p", "ü", "\n",
  "a", "s", "d", "f", "g", "h", "j", "k", "l", "ö", "ä", "\n",
  "Umsch", "y", "x", "c", "v", "b", "n", "m", "ß", "Entf", "\n",
  "Leer", ",", ".", "-", "OK", ""
};
static const char* MAP_GROSS[] = {
  "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
  "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P", "Ü", "\n",
  "A", "S", "D", "F", "G", "H", "J", "K", "L", "Ö", "Ä", "\n",
  "Umsch", "Y", "X", "C", "V", "B", "N", "M", "ß", "Entf", "\n",
  "Leer", ",", ".", "-", "OK", ""
};
static const int IDX_LEER = 10 + 11 + 11 + 10;

static void sugAnstossen();
static void sugVerbergen();
static void tastaturZu() {
  sugVerbergen();
  if (s_kb) lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
  if (s_ta) lv_obj_clear_state(s_ta, LV_STATE_FOCUSED);
  s_ta = nullptr;
}
static void kartenSetzen(bool gross) {
  s_gross = gross;
  lv_btnmatrix_set_map(s_kb, gross ? MAP_GROSS : MAP_KLEIN);
  lv_btnmatrix_set_btn_width(s_kb, IDX_LEER, 5);
}
static void cbTaste(lv_event_t* e) {
  lv_obj_t* kb = lv_event_get_target(e);
  uint32_t id = lv_btnmatrix_get_selected_btn(kb);
  if (id == LV_BTNMATRIX_BTN_NONE) return;
  const char* t = lv_btnmatrix_get_btn_text(kb, id);
  if (!t) return;
  if (!strcmp(t, "OK")) { tastaturZu(); return; }
  if (!strcmp(t, "Umsch")) { kartenSetzen(!s_gross); return; }
  if (!s_ta) return;
  if (!strcmp(t, "Entf")) lv_textarea_del_char(s_ta);
  else if (!strcmp(t, "Leer")) lv_textarea_add_text(s_ta, " ");
  else { lv_textarea_add_text(s_ta, t); if (s_gross) kartenSetzen(false); }
  sugAnstossen();
}
static void cbFeld(lv_event_t* e) {
  lv_obj_t* ta = lv_event_get_target(e);
  if (s_ta && s_ta != ta) lv_obj_clear_state(s_ta, LV_STATE_FOCUSED);
  s_ta = ta;
  lv_obj_add_state(ta, LV_STATE_FOCUSED);
  if (lv_textarea_get_text(ta)[0] == 0) kartenSetzen(true);
  lv_obj_clear_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
}
static void feldEinrichten(lv_obj_t* ta, const char* platzhalter) {
  if (!ta) return;
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_placeholder_text(ta, platzhalter);
  lv_obj_set_style_text_font(ta, F22(), 0);
  lv_obj_set_style_text_font(ta, F22(), LV_PART_TEXTAREA_PLACEHOLDER);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0xFFFFFF), 0);   // helles Feld wie der Rest der Oberflaeche (das Farbthema machte es dunkel)
  lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(0x111111), 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(0x7B8794), LV_PART_TEXTAREA_PLACEHOLDER);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(0x94A3B8), 0);
  lv_obj_add_flag(ta, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ta, cbFeld, LV_EVENT_CLICKED, nullptr);
}

// ── Routen-Eingabe: Abfahrt, Tempo, Berechnen ────────────────────────────────
static int s_abfOffset = 0;   // 0 = jetzt, sonst Stunden: 1 = naechste volle Stunde

static void abfahrtSetzen() {
  if (s_abfOffset <= 0) { routeAbfahrt = 0; return; }
  time_t jetzt = time(nullptr);
  routeAbfahrt = (jetzt / 3600 + 1) * 3600 + (time_t)(s_abfOffset - 1) * 3600;
}
static void eingabeAnzeigen() {
  char b[40];
  if (s_abfOffset <= 0) snprintf(b, sizeof(b), "Jetzt"); else zeitText(routeAbfahrt, b, sizeof(b), true);
  setLabel(objects.labelabfahrtwert, b);
  snprintf(b, sizeof(b), "%d km/h", routeTempo);
  setLabel(objects.labeltempowert, b);
}
static void cbAbfMinus(lv_event_t*) { if (s_abfOffset > 0) s_abfOffset--; abfahrtSetzen(); eingabeAnzeigen(); }
static void cbAbfPlus(lv_event_t*)  { if (s_abfOffset < 47) s_abfOffset++; abfahrtSetzen(); eingabeAnzeigen(); }
static void cbAbfJetzt(lv_event_t*) { s_abfOffset = 0; abfahrtSetzen(); eingabeAnzeigen(); }
static void cbTempoMinus(lv_event_t*) { if (routeTempo > 30) routeTempo -= 5; eingabeAnzeigen(); }
static void cbTempoPlus(lv_event_t*)  { if (routeTempo < 120) routeTempo += 5; eingabeAnzeigen(); }

static void cbBerechnen(lv_event_t*) {
  tastaturZu();
  String s = lv_textarea_get_text(objects.textareastart), z = lv_textarea_get_text(objects.textareaziel), v = lv_textarea_get_text(objects.textareavia);
  s.trim(); z.trim(); v.trim();
  if (s.isEmpty() || z.isEmpty()) {
    deckeEinrichten();
    routeStatus = RS_FEHLER; snprintf(routeStatusText, sizeof(routeStatusText), "Bitte Start und Ziel eingeben.");
    s_nachFertig = 0; auftragAnzeigen("");
    return;
  }
  const bool meiden = objects.switchautobahn && lv_obj_has_state(objects.switchautobahn, LV_STATE_CHECKED);
  abfahrtSetzen();
  if (!routeAuftragNeu(String(), s, z, v, meiden)) { routeStatus = RS_FEHLER; snprintf(routeStatusText, sizeof(routeStatusText), "Es läuft schon eine Berechnung."); auftragAnzeigen(""); return; }
  s_nachFertig = 1;
  auftragAnzeigen("Starte ...");
}

// ── Routenliste ──────────────────────────────────────────────────────────────
static lv_obj_t* s_liste = nullptr;

static void cbRouteWaehlen(lv_event_t* e) {
  const char* id = (const char*)lv_event_get_user_data(e);
  abfahrtSetzen();
  if (!routeAuftragWaehlen(id)) return;
  s_nachFertig = 1;
  auftragAnzeigen("Lade Route ...");
}
static void listeAufbauen();
static void cbRouteLoeschen(lv_event_t* e) {
  const char* id = (const char*)lv_event_get_user_data(e);
  if (routeAkt && routeAkt->route && !strcmp(routeAkt->route->id, id)) routeAktivSetzen(nullptr);
  routeLoeschen(id);
  listeAufbauen();
}

static char s_ids[ROUTE_MAX_GESPEICHERT][12];

static void listeAufbauen() {
  if (!objects.containerroutenliste) return;
  if (!s_liste) { s_liste = ebene(objects.containerroutenliste); lv_obj_add_flag(s_liste, LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(s_liste, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_scroll_dir(s_liste, LV_DIR_VER); }
  lv_obj_clean(s_liste);
  const int n = routenAnzahl();
  if (objects.labelroutenleer) { if (n) lv_obj_add_flag(objects.labelroutenleer, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(objects.labelroutenleer, LV_OBJ_FLAG_HIDDEN); }
  for (int i = 0; i < n && i < ROUTE_MAX_GESPEICHERT; i++) {
    char name[40]; float km; uint32_t sek; bool gpx;
    if (!routenInfo(i, s_ids[i], sizeof(s_ids[i]), name, sizeof(name), &km, &sek, &gpx)) continue;
    const bool aktiv = routeAkt && routeAkt->route && !strcmp(routeAkt->route->id, s_ids[i]);
    lv_obj_t* z = lv_obj_create(s_liste);
    lv_obj_remove_style_all(z);
    lv_obj_set_size(z, 518, 56);
    lv_obj_set_pos(z, 6, 6 + i * 62);
    lv_obj_set_style_bg_color(z, lv_color_hex(aktiv ? 0xBEE6F8 : 0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(z, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(z, 8, 0);
    lv_obj_set_style_border_width(z, 1, 0);
    lv_obj_set_style_border_color(z, lv_color_hex(0x94A3B8), 0);
    lv_obj_clear_flag(z, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(z, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(z, cbRouteWaehlen, LV_EVENT_CLICKED, s_ids[i]);
    lab(z, F22(), 0x111111, 12, 4, 494, 26, name);
    char b[64]; snprintf(b, sizeof(b), "%.0f km%s", km, gpx ? ", GPX-Datei" : "");
    lab(z, F18(), 0x5A6B7B, 12, 30, 400, 22, b);
    lv_obj_t* d = lv_btn_create(s_liste);
    lv_obj_set_size(d, 96, 56);
    lv_obj_set_pos(d, 530, 6 + i * 62);
    lv_obj_set_style_bg_color(d, lv_color_hex(0xB00000), 0);
    lv_obj_set_style_radius(d, 8, 0);
    lv_obj_set_style_pad_all(d, 0, 0);   // der Standard-Innenabstand der Taste schnitt das Wort ab
    lv_obj_add_event_cb(d, cbRouteLoeschen, LV_EVENT_CLICKED, s_ids[i]);
    lv_obj_t* dl = lv_label_create(d);
    lv_label_set_text(dl, "Löschen");
    lv_obj_set_style_text_font(dl, F18(), 0);
    lv_obj_set_style_text_color(dl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(dl);
  }
  uint64_t gesamt = 0, frei = 0;
  char b[100];
  if (datenInfo(&gesamt, &frei)) snprintf(b, sizeof(b), "%.1f MB frei im Gerät (%d von %d Routen)", frei / 1048576.0, n, ROUTE_MAX_GESPEICHERT); else snprintf(b, sizeof(b), "Datenlaufwerk nicht bereit");
  kommaText(b);
  setLabel(objects.labelspeicherwert, b);
}

// ── Routenwetter ─────────────────────────────────────────────────────────────
static lv_obj_t* s_kartenEbene = nullptr;
static lv_point_t* s_linienPunkte[ROUTE_MAX_ABSCHNITTE] = {};
static lv_img_dsc_t s_kartenDsc;
static uint8_t* s_kartenBuf = nullptr;
static char s_kartenId[12] = "";

void uiKarteVerwerfen() { LvLock lock; s_kartenId[0] = 0; }

static void karteZeichnen(const RAnalyse* a) {
  lv_obj_t* host = objects.containerkarte;
  if (!host) return;
  if (!s_kartenEbene) { s_kartenEbene = ebene(host); }
  lv_obj_clean(s_kartenEbene);
  for (int i = 0; i < ROUTE_MAX_ABSCHNITTE; i++) if (s_linienPunkte[i]) { heap_caps_free(s_linienPunkte[i]); s_linienPunkte[i] = nullptr; }
  if (objects.labelkarteplatzhalter) lv_obj_add_flag(objects.labelkarteplatzhalter, LV_OBJ_FLAG_HIDDEN);
  if (!a || !a->route) {
    if (objects.imagekarte) lv_obj_add_flag(objects.imagekarte, LV_OBJ_FLAG_HIDDEN);
    if (objects.labelkarteplatzhalter) lv_obj_clear_flag(objects.labelkarteplatzhalter, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  const Route* r = a->route;
  const int W = KARTE_B, H = KARTE_H;
  float laMin = 90, laMax = -90, loMin = 180, loMax = -180;
  for (int i = 0; i < r->n; i++) { if (r->p[i].lat < laMin) laMin = r->p[i].lat; if (r->p[i].lat > laMax) laMax = r->p[i].lat; if (r->p[i].lon < loMin) loMin = r->p[i].lon; if (r->p[i].lon > loMax) loMax = r->p[i].lon; }

  // Kartenbild aus dem Browser (OpenStreetMap-Kacheln) vorhanden? Dann Web-Mercator wie im Browser, sonst Schemakarte
  const int zoom = karteZoom(r->id);
  bool mitBild = false;
  if (zoom >= 3 && objects.imagekarte) {
    if (strcmp(s_kartenId, r->id) != 0 || !s_kartenBuf || karteBildLaden(r->id, false, &s_kartenDsc) != s_kartenBuf) {
      if (s_kartenBuf) { lv_img_set_src(objects.imagekarte, nullptr); s_kartenBuf = nullptr; }
      s_kartenBuf = karteBildLaden(r->id, false, &s_kartenDsc);
      strlcpy(s_kartenId, r->id, sizeof(s_kartenId));
    }
    if (s_kartenBuf) {
      lv_img_set_src(objects.imagekarte, &s_kartenDsc);
      lv_obj_set_pos(objects.imagekarte, 0, 0);
      lv_obj_clear_flag(objects.imagekarte, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_background(objects.imagekarte);
      mitBild = true;
    }
  }
  if (!mitBild && objects.imagekarte) lv_obj_add_flag(objects.imagekarte, LV_OBJ_FLAG_HIDDEN);

  double cx = 0, cy = 0; float sk = 1, ox = 0, oy = 0, cosB = 1;
  if (mitBild) {
    double x0, y0, x1, y1;
    mercatorPx(laMax, loMin, zoom, &x0, &y0); mercatorPx(laMin, loMax, zoom, &x1, &y1);
    cx = (x0 + x1) / 2; cy = (y0 + y1) / 2;
  } else {
    const int M = 22;
    cosB = cosf((laMin + laMax) * 0.5f * (float)M_PI / 180.0f);
    float bx = (loMax - loMin) * cosB, by = laMax - laMin;
    if (bx < 0.01f) bx = 0.01f;
    if (by < 0.01f) by = 0.01f;
    sk = fminf((W - 2 * M) / bx, (H - 2 * M) / by);
    ox = (W - bx * sk) * 0.5f; oy = (H - by * sk) * 0.5f;
  }
  auto px = [&](const RoutePunkt& p) {
    lv_point_t q;
    if (mitBild) { double x, y; mercatorPx(p.lat, p.lon, zoom, &x, &y); q.x = (lv_coord_t)lround(x - cx + W / 2.0); q.y = (lv_coord_t)lround(y - cy + H / 2.0); }
    else { q.x = (lv_coord_t)(ox + (p.lon - loMin) * cosB * sk); q.y = (lv_coord_t)(oy + (laMax - p.lat) * sk); }
    return q;
  };
  for (int s = 0; s < a->n; s++) {
    const int i0 = a->a[s].i0, i1 = a->a[s].i1 < r->n ? a->a[s].i1 : r->n - 1;
    const int cnt = i1 - i0 + 1;
    if (cnt < 2) continue;
    s_linienPunkte[s] = (lv_point_t*)heap_caps_malloc(sizeof(lv_point_t) * cnt, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_linienPunkte[s]) continue;
    for (int k = 0; k < cnt; k++) s_linienPunkte[s][k] = px(r->p[i0 + k]);
    if (mitBild) {   // heller Rand unter der Linie, damit sie auf der Karte lesbar bleibt
      lv_obj_t* rand = lv_line_create(s_kartenEbene);
      lv_line_set_points(rand, s_linienPunkte[s], cnt);
      lv_obj_set_style_line_width(rand, 11, 0);
      lv_obj_set_style_line_rounded(rand, true, 0);
      lv_obj_set_style_line_color(rand, lv_color_hex(0xFFFFFF), 0);
    }
    lv_obj_t* ln = lv_line_create(s_kartenEbene);
    lv_line_set_points(ln, s_linienPunkte[s], cnt);
    lv_obj_set_style_line_width(ln, 7, 0);
    lv_obj_set_style_line_rounded(ln, true, 0);
    lv_obj_set_style_line_color(ln, lv_color_hex(routeErg.gueltig && s < routeErg.n ? bikerFarbe(routeErg.score[s]) : 0x5A6B7B), 0);
  }
  auto marke = [&](const RoutePunkt& p, uint32_t farbe, const char* t) {
    lv_point_t q = px(p);
    lv_obj_t* o = lv_obj_create(s_kartenEbene);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, 24, 24);
    lv_obj_set_pos(o, q.x - 12, q.y - 12);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(farbe), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(0xFFFFFF), 0);
    lab(o, F18(), 0xFFFFFF, -2, 0, 28, 24, t, LV_TEXT_ALIGN_CENTER);
  };
  marke(r->p[0], 0x1D6FD6, "S");
  marke(r->p[r->n - 1], 0x111111, "Z");
  if (mitBild) {
    lv_obj_t* q = lab(s_kartenEbene, F18(), 0x333333, W - 300, H - 24, 300, 22, "© OpenStreetMap-Mitwirkende", LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_style_bg_color(q, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(q, LV_OPA_70, 0);
  } else lab(s_kartenEbene, F18(), 0x5A6B7B, 8, H - 26, 560, 22, kachelnOffen() > 0 || karteStatusLaeuft() ? "Kartenbild wird geladen ..." : "Schemakarte (kein Kartenbild vorhanden)");
}

static void chipsZeichnen(const RAnalyse* a) {
  lv_obj_t* c = objects.containerabschnittechips;
  if (!c) return;
  lv_obj_clean(c);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  if (!a || !routeErg.gueltig) return;
  const int n = routeErg.n, breite = 962;
  int w = breite / n - 6; if (w > 150) w = 150;
  const int start = (breite - n * (w + 6)) / 2;
  for (int i = 0; i < n; i++) {
    lv_obj_t* k = lv_obj_create(c);
    lv_obj_remove_style_all(k);
    lv_obj_set_size(k, w, 94);
    lv_obj_set_pos(k, start + i * (w + 6), 2);
    lv_obj_set_style_bg_color(k, lv_color_hex(bikerFarbe(routeErg.score[i])), 0);
    lv_obj_set_style_bg_opa(k, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(k, 8, 0);
    char b[24];
    snprintf(b, sizeof(b), w >= 70 ? "km %d" : "%d", (int)a->a[i].km0);
    lab(k, F18(), 0xFFFFFF, 0, 4, w, 22, b, LV_TEXT_ALIGN_CENTER);
    snprintf(b, sizeof(b), "%d", routeErg.score[i]);
    lab(k, F32(), 0xFFFFFF, 0, 28, w, 36, b, LV_TEXT_ALIGN_CENTER);
    snprintf(b, sizeof(b), routeErg.mm[i] >= 0.1f ? "%.0f° Regen" : "%.0f°", routeErg.temp[i]);
    if (w < 80 && routeErg.mm[i] >= 0.1f) snprintf(b, sizeof(b), "%.0f°*", routeErg.temp[i]);
    lab(k, F18(), 0xFFFFFF, 0, 68, w, 22, b, LV_TEXT_ALIGN_CENTER);
  }
}

static void routeWetterAnzeigen() {
  if (!routeAkt || !routeAkt->route) {
    setLabel(objects.labelrwtitel, "Keine Route gewählt");
    setLabel(objects.labelrwinfo, "Wähle in der Routenliste eine Route oder lege eine neue an.");
    karteZeichnen(nullptr); chipsZeichnen(nullptr);
    return;
  }
  const Route* r = routeAkt->route;
  const RErgebnis& e = routeErg;
  char b[160], t1[16], t2[16];
  setLabel(objects.labelrwtitel, r->name);
  const int min = (int)lroundf(r->km / e.tempo * 60.0f);
  zeitText(e.abfahrt, t1, sizeof(t1), false); zeitText(e.ankunft, t2, sizeof(t2), false);
  snprintf(b, sizeof(b), "%.0f km, %d Std. %d Min. bei %d km/h, Abfahrt %s, Ankunft %s", r->km, min / 60, min % 60, e.tempo, t1, t2);
  setLabel(objects.labelrwinfo, b);

  snprintf(b, sizeof(b), "%d", e.gesamt);
  setLabelFarbe(objects.labelrwscorezahl, bikerFarbe(e.gesamt), b);
  setLabel(objects.labelrwscoretext, e.gesamt >= 75 ? "Gute Bedingungen" : e.gesamt >= 55 ? "Okay, Abschnitte schwächer" : e.gesamt >= 35 ? "Mäßig, Teile ungünstig" : "Schlecht, besser verschieben");
  snprintf(b, sizeof(b), "Score der Route bei Abfahrt %s", t1);
  setLabel(objects.labelrwscorekopf, b);

  if (lroundf(e.tmin) == lroundf(e.tmax)) snprintf(b, sizeof(b), "%.0f °C", e.tmin); else snprintf(b, sizeof(b), "%.0f bis %.0f °C", e.tmin, e.tmax);
  setLabel(objects.labelrwwert1, b);
  if (e.regenAbschnitte == 0) snprintf(b, sizeof(b), "keiner"); else { snprintf(b, sizeof(b), "%.1f mm ab km %d", e.mmSumme / e.n, e.erstesRegenKm); kommaText(b); }
  setLabelFarbe(objects.labelrwwert2, e.regenAbschnitte ? 0xC8501E : 0x111111, b);
  snprintf(b, sizeof(b), "%.0f km/h", e.boeMax);
  setLabel(objects.labelrwwert3, b);
  const float g = -e.langsMittel;
  snprintf(b, sizeof(b), "%s %.0f km/h", g > 0 ? "Gegen" : "Rücken", fabsf(g));
  setLabel(objects.labelrwwert4, b);
  snprintf(b, sizeof(b), "Abschnitte (je %.0f km), Score und Wetter zur Durchfahrtszeit", routeAkt->a[0].km1 - routeAkt->a[0].km0);
  setLabel(objects.labelabschnittetitel, b);
  karteZeichnen(routeAkt);
  chipsZeichnen(routeAkt);
}

// ── Hoehe, Wind, Sonne ───────────────────────────────────────────────────────
static void profilZeichnen(const RAnalyse* a) {
  lv_obj_t* c = objects.containerprofilzeichnung;
  if (!c) return;
  lv_obj_clean(c);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  static lv_point_t* pts = nullptr;
  if (pts) { heap_caps_free(pts); pts = nullptr; }
  if (!a || !a->route || !routeErg.gueltig) return;
  const Route* r = a->route;
  const int W = 618, H = 208, L = 72, R = 10, O = 76, U = 24;   // oben zwei Zeilen: Temperatur und Wind je Abschnitt
  // Messpunkte: GPX-Hoehen (bis 90 Stueck) oder die Hoehen der Abschnitte
  int n = 0; float km[96], h[96];
  if (a->hoeheAusGpx) {
    float cum = 0; int schritt = r->n / 90 + 1;
    for (int i = 0; i < r->n && n < 95; i++) {
      if (i > 0) { float dla = (r->p[i].lat - r->p[i - 1].lat), dlo = (r->p[i].lon - r->p[i - 1].lon) * cosf(r->p[i].lat * 0.01745f); cum += sqrtf(dla * dla + dlo * dlo) * 111.2f; }
      if (i % schritt == 0 && r->p[i].ele != -32768) { km[n] = cum; h[n] = r->p[i].ele; n++; }
    }
  }
  if (n < 2) {
    n = 0; km[n] = 0; h[n] = a->a[0].hoehe; n++;
    for (int i = 0; i < a->n && n < 94; i++) { km[n] = a->a[i].kmMitte; h[n] = a->a[i].hoehe; n++; }
    km[n] = r->km; h[n] = a->a[a->n - 1].hoehe; n++;
  }
  float hMin = h[0], hMax = h[0];
  for (int i = 1; i < n; i++) { if (h[i] < hMin) hMin = h[i]; if (h[i] > hMax) hMax = h[i]; }
  if (hMax - hMin < 40) hMax = hMin + 40;
  const float kmMax = km[n - 1] > 1 ? km[n - 1] : 1;
  pts = (lv_point_t*)heap_caps_malloc(sizeof(lv_point_t) * n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!pts) return;
  for (int i = 0; i < n; i++) { pts[i].x = (lv_coord_t)(L + km[i] / kmMax * (W - L - R)); pts[i].y = (lv_coord_t)(O + (1 - (h[i] - hMin) / (hMax - hMin)) * (H - O - U)); }
  // Achsenmarken
  char b[24];
  snprintf(b, sizeof(b), "%.0f m", hMax); lab(c, F18(), 0x5A6B7B, 0, O - 10, L - 6, 22, b, LV_TEXT_ALIGN_RIGHT);
  snprintf(b, sizeof(b), "%.0f m", hMin); lab(c, F18(), 0x5A6B7B, 0, H - U - 12, L - 6, 22, b, LV_TEXT_ALIGN_RIGHT);
  snprintf(b, sizeof(b), "%.0f km", kmMax); lab(c, F18(), 0x5A6B7B, W - 110, H - 22, 100, 22, b, LV_TEXT_ALIGN_RIGHT);
  lab(c, F18(), 0x5A6B7B, L, H - 22, 60, 22, "0 km");
  lv_obj_t* ln = lv_line_create(c);
  lv_line_set_points(ln, pts, n);
  lv_obj_set_style_line_width(ln, 4, 0);
  lv_obj_set_style_line_color(ln, lv_color_hex(0x1D6FD6), 0);
  lv_obj_set_style_line_rounded(ln, true, 0);
  // Temperatur und Wind je Abschnitt (hoechstens acht Marken). Wind in Fahrtrichtung: Pfeil nach oben = Rueckenwind (gruen), nach unten = Gegenwind (rot)
  lab(c, F18(), 0x5A6B7B, 0, 2, L - 6, 22, "°C", LV_TEXT_ALIGN_RIGHT);
  lab(c, F18(), 0x5A6B7B, 0, 38, L - 6, 22, "Wind", LV_TEXT_ALIGN_RIGHT);
  lab(c, F18(), 0x5A6B7B, L + 70, H - 22, 340, 22, "Wind: oben = Rücken, unten = Gegen", LV_TEXT_ALIGN_CENTER);
  static lv_point_t pfeil[8][5];   // die Linien lesen ihre Punkte beim Zeichnen: muessen bestehen bleiben
  int pn = 0;
  const int schritt = (a->n + 7) / 8;
  for (int i = 0; i < a->n && pn < 8; i += schritt) {
    snprintf(b, sizeof(b), "%.0f°", routeErg.temp[i]);
    const int x = (int)(L + a->a[i].kmMitte / kmMax * (W - L - R)) - 20;
    lab(c, F18(), routeErg.temp[i] <= 1 ? 0x0A8FB0 : 0x111111, x, 2, 40, 22, b, LV_TEXT_ALIGN_CENTER);
    const float l = routeErg.langs[i];
    const int xm = x + 20, ym = 38;
    const bool kaum = fabsf(l) < 3.0f, rueck = l > 0;
    const uint32_t farbe = kaum ? 0x5A6B7B : rueck ? 0x00A838 : 0xD01818;
    lv_point_t* p = pfeil[pn++];
    int np;
    if (kaum) { p[0] = {(lv_coord_t)(xm - 7), (lv_coord_t)ym}; p[1] = {(lv_coord_t)(xm + 7), (lv_coord_t)ym}; np = 2; }
    else {
      const int sg = rueck ? 1 : -1;   // Rueckenwind: Spitze oben; Gegenwind: gespiegelt
      const int dy[5] = {-3, -9, -3, -9, 9};
      const int dx[5] = {-5, 0, 5, 0, 0};
      for (int k = 0; k < 5; k++) { p[k].x = (lv_coord_t)(xm + dx[k]); p[k].y = (lv_coord_t)(ym + sg * dy[k]); }
      np = 5;
    }
    lv_obj_t* ln = lv_line_create(c);
    lv_line_set_points(ln, p, np);
    lv_obj_set_style_line_width(ln, 3, 0);
    lv_obj_set_style_line_color(ln, lv_color_hex(farbe), 0);
    lv_obj_set_style_line_rounded(ln, true, 0);
    snprintf(b, sizeof(b), "%.0f", fabsf(l));
    lab(c, F18(), farbe, x, 50, 40, 22, b, LV_TEXT_ALIGN_CENTER);
  }
}

static void hoeheAnzeigen() {
  if (!routeAkt || !routeErg.gueltig) { profilZeichnen(nullptr); return; }
  const RAnalyse* a = routeAkt; const RErgebnis& e = routeErg;
  char b[120], t[16];
  snprintf(b, sizeof(b), "Max. %d m", a->hMax); setLabel(objects.labelhoehewert, b);
  snprintf(b, sizeof(b), "Min. %d m, Anstieg %d m%s", a->hMin, a->anstieg, a->hoeheAusGpx ? "" : " (grob)"); setLabel(objects.labelhoehedetail, b);
  if (e.frostKm >= 0) { snprintf(b, sizeof(b), "Frost ab km %d", e.frostKm); setLabelFarbe(objects.labelfrostwert, 0xB00000, b); }
  else setLabelFarbe(objects.labelfrostwert, 0x00A838, "Kein Frost");
  snprintf(b, sizeof(b), "Tiefste Temperatur %.0f °C", e.frostMinTemp); setLabel(objects.labelfrostdetail, b);
  const float l = e.langsMittel;
  snprintf(b, sizeof(b), "%s %.0f km/h", l < 0 ? "Gegenwind" : "Rückenwind", fabsf(l)); setLabel(objects.labelwindwert_1, b);
  snprintf(b, sizeof(b), "Seitenwind bei Böen bis %.0f km/h", e.querMax);
  if (fabsf(l) >= 8.0f) { char r[56]; snprintf(r, sizeof(r), ". Rückweg: %s.", l < 0 ? "Rückenwind" : "Gegenwind"); strlcat(b, r, sizeof(b)); }   // dieselbe Strecke zurueck dreht den Wind
  setLabel(objects.labelwinddetail_1, b);
  zeitText(e.ankunft, t, sizeof(t), false);
  snprintf(b, sizeof(b), "Ankunft %s", t); setLabel(objects.labelsonnewert, b);
  char u[16]; zeitText(e.sonnenuntergang, u, sizeof(u), false);
  snprintf(b, sizeof(b), "Sonnenuntergang %s. %s", u, e.imHellenAn ? "Du kommst im Hellen an." : "Du kommst nach Sonnenuntergang an!");
  setLabelFarbe(objects.labelsonnedetail, e.imHellenAn ? 0x111111 : 0xB00000, b);
  if (e.blendKm >= 0) { snprintf(b, sizeof(b), "ab km %d", e.blendKm); setLabelFarbe(objects.labelblendwert, 0xC8501E, b);
    snprintf(b, sizeof(b), "Tiefe Sonne (%.0f°) in Fahrtrichtung: Sonnenbrille bereithalten.", e.sonneHoeheBlend); setLabel(objects.labelblenddetail, b); }
  else { setLabelFarbe(objects.labelblendwert, 0x00A838, "Keine"); setLabel(objects.labelblenddetail, "Tiefe Sonne in Fahrtrichtung: nicht auf dieser Route."); }
  profilZeichnen(a);
}

// ── Abfahrtszeit-Planer ──────────────────────────────────────────────────────
static void cbBalken(lv_event_t* ev);
// Pause-Tipp unten auf dem Abfahrt-Bildschirm (rechts neben dem Zurueck-Pfeil), fuer die gewaehlte Abfahrt
static lv_obj_t* s_pauseLbl = nullptr;
static void pauseAnzeigen() {
  if (!objects.screenabfahrt) return;
  if (!s_pauseLbl) {
    s_pauseLbl = lab(objects.screenabfahrt, F18(), 0xC8501E, 120, 536, 880, 52, "");
    lv_label_set_long_mode(s_pauseLbl, LV_LABEL_LONG_WRAP);
  }
  RPause p; char t[200] = "";
  if (routeAkt && routeErg.gueltig) { routePauseRechnen(routeAkt, routeErg.abfahrt, routeErg.tempo, &p); routePauseText(p, t, sizeof(t)); }
  setLabel(s_pauseLbl, t);
}

static void planerAnzeigen() {
  lv_obj_t* c = objects.containerplanerbalken;
  if (!c) return;
  lv_obj_clean(c);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  if (!routeAkt || routeBesteIdx < 0) {
    lab(c, F22(), 0x5A6B7B, 20, 20, 900, 60, "Keine Route gewählt oder der Zeitraum der Vorhersage reicht nicht für die Strecke.");
    setLabel(objects.labelbestzeitwert, "-"); setLabel(objects.labelbestzeitdetail, ""); setLabel(objects.labeljetztwert, "-"); setLabel(objects.labeljetztdetail, "");
    pauseAnzeigen();
    return;
  }
  const int W = 962, hoehe = 236, bw = 16, ab = W / PLANER_STUNDEN;
  for (int k = 0; k < PLANER_STUNDEN; k++) {
    const int sc = routePlaner[k];
    const time_t dep = routePlanerStart + (time_t)k * 3600;
    lv_obj_t* o = lv_obj_create(c);
    lv_obj_remove_style_all(o);
    int hh = sc < 0 ? 6 : sc * hoehe / 100; if (hh < 6) hh = 6;
    lv_obj_set_size(o, bw, hh);
    lv_obj_set_pos(o, 4 + k * ab, hoehe - hh + 2);
    lv_obj_set_style_bg_color(o, lv_color_hex(sc < 0 ? 0xC9D3DD : bikerFarbe(sc)), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, 3, 0);
    if (k == routeBesteIdx) { lv_obj_set_style_border_width(o, 3, 0); lv_obj_set_style_border_color(o, lv_color_hex(0x111111), 0); }
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(o, 2);
    lv_obj_add_event_cb(o, cbBalken, LV_EVENT_CLICKED, (void*)(intptr_t)k);
    struct tm tm; localtime_r(&dep, &tm);
    if (tm.tm_hour % 6 == 0 || k == 0) {
      char b[12]; static const char* WT[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
      if (tm.tm_hour == 0) snprintf(b, sizeof(b), "%s", WT[tm.tm_wday]); else snprintf(b, sizeof(b), "%02d", tm.tm_hour);
      lab(c, F18(), 0x111111, 4 + k * ab - 14, hoehe + 6, bw + 28, 22, b, LV_TEXT_ALIGN_CENTER);
    }
  }
  // Beste Abfahrt und "jetzt"
  RErgebnis e;
  const time_t best = routePlanerStart + (time_t)routeBesteIdx * 3600;
  routeBewerten(routeAkt, best, routeTempo, &e);
  char b[140], z[24]; zeitText(best, z, sizeof(z), true);
  setLabelFarbe(objects.labelbestzeitwert, bikerFarbe(e.gesamt), z);
  snprintf(b, sizeof(b), "Score %d, %s, %s", e.gesamt, e.regenAbschnitte ? "Regen auf Teilen der Strecke" : "kein Regen", e.imHellenAn ? "Ankunft im Hellen" : "Ankunft im Dunkeln");
  setLabel(objects.labelbestzeitdetail, b);
  RErgebnis j; routeBewerten(routeAkt, time(nullptr), routeTempo, &j);
  snprintf(b, sizeof(b), "Score %d", j.gesamt); setLabelFarbe(objects.labeljetztwert, bikerFarbe(j.gesamt), b);
  if (j.erstesRegenKm >= 0) snprintf(b, sizeof(b), "Regenrisiko ab Kilometer %d", j.erstesRegenKm); else snprintf(b, sizeof(b), "Kein Regen auf der Strecke");
  setLabel(objects.labeljetztdetail, b);
  setLabel(objects.labelabinfo, "Score der ganzen Route je Abfahrtszeit, nächste 48 Stunden. Balken antippen setzt die Abfahrt.");
  pauseAnzeigen();
}

static void cbBalken(lv_event_t* ev) {
  const int k = (int)(intptr_t)lv_event_get_user_data(ev);
  if (!routeAkt || routePlaner[k] < 0) return;
  routeAbfahrt = routePlanerStart + (time_t)k * 3600;
  routeBewerten(routeAkt, routeAbfahrt, routeTempo, &routeErg);
  char z[24], b[80]; zeitText(routeAbfahrt, z, sizeof(z), true);
  snprintf(b, sizeof(b), "Abfahrt gesetzt: %s (Score %d). Siehe Routenwetter.", z, routeErg.gesamt);
  setLabel(objects.labelabinfo, b);
}


// ── Ortsvorschlaege beim Tippen (Photon: Strassen, Hausnummern, Orte) ────────
static lv_obj_t* s_sugPanel = nullptr;
static lv_obj_t* s_sugLbl[5];
static lv_obj_t* s_sugBtn[5];
static OrtVorschlag s_sug[5];
static volatile int s_sugN = 0, s_sugSeq = 0, s_sugGezeigt = 0, s_sugFeld = 0;
static volatile bool s_sugLaeuft = false;
static char s_sugFrage[80];
static lv_timer_t* s_sugDeb = nullptr;
static lv_timer_t* s_sugPoll = nullptr;

static int feldIndex(lv_obj_t* ta) { return ta == objects.textareastart ? 0 : ta == objects.textareavia ? 1 : 2; }

static void sugVerbergen() { if (s_sugPanel) lv_obj_add_flag(s_sugPanel, LV_OBJ_FLAG_HIDDEN); s_sugN = 0; s_sugGezeigt = s_sugSeq; }

static void sugTask(void*) {
  OrtVorschlag tmp[5];
  const String frage = s_sugFrage;
  const int n = photonSuche(frage, tmp, 5);
  for (int i = 0; i < n; i++) s_sug[i] = tmp[i];
  s_sugN = n;
  s_sugSeq = s_sugSeq + 1;
  s_sugLaeuft = false;
  vTaskDelete(nullptr);
}

static void sugDebCb(lv_timer_t* t) {
  lv_timer_pause(t);
  if (!s_ta) return;
  if (s_sugLaeuft || routeStatus == RS_BESCHAEFTIGT) { lv_timer_reset(t); lv_timer_resume(t); return; }
  const char* text = lv_textarea_get_text(s_ta);
  if (strlen(text) < 3) return;
  strlcpy(s_sugFrage, text, sizeof(s_sugFrage));
  s_sugFeld = feldIndex(s_ta);
  s_sugLaeuft = true;
  if (xTaskCreatePinnedToCore(sugTask, "suche", 12 * 1024, nullptr, 1, nullptr, 0) != pdPASS) s_sugLaeuft = false;
}

static void sugAnstossen() {
  if (!s_sugDeb) return;
  if (!s_ta || strlen(lv_textarea_get_text(s_ta)) < 3) { sugVerbergen(); return; }
  lv_timer_reset(s_sugDeb);
  lv_timer_resume(s_sugDeb);
}

static void sugPollCb(lv_timer_t*) {
  if (s_sugSeq == s_sugGezeigt) return;
  s_sugGezeigt = s_sugSeq;
  if (!s_ta || !s_sugPanel || lv_obj_has_flag(s_kb, LV_OBJ_FLAG_HIDDEN)) return;
  if (strcmp(lv_textarea_get_text(s_ta), s_sugFrage) != 0) { sugAnstossen(); return; }   // inzwischen weitergetippt
  if (s_sugN == 0) { lv_obj_add_flag(s_sugPanel, LV_OBJ_FLAG_HIDDEN); return; }
  for (int i = 0; i < 5; i++) {
    if (i < s_sugN) { lv_label_set_text(s_sugLbl[i], s_sug[i].label); lv_obj_clear_flag(s_sugBtn[i], LV_OBJ_FLAG_HIDDEN); }
    else lv_obj_add_flag(s_sugBtn[i], LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_clear_flag(s_sugPanel, LV_OBJ_FLAG_HIDDEN);
}

static void sugWahlCb(lv_event_t* e) {
  const int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i >= s_sugN || !s_ta) return;
  const int feld = s_sugFeld;
  lv_textarea_set_text(s_ta, s_sug[i].label);
  strlcpy(routeOrtWahl[feld].text, s_sug[i].label, sizeof(routeOrtWahl[feld].text));
  routeOrtWahl[feld].lat = s_sug[i].lat; routeOrtWahl[feld].lon = s_sug[i].lon; routeOrtWahl[feld].gesetzt = true;
  sugVerbergen();
}

static void sugEinrichten() {
  lv_obj_t* scr = objects.screenrouteneu;
  if (!scr || s_sugPanel) return;
  s_sugPanel = lv_obj_create(scr);
  lv_obj_set_size(s_sugPanel, 344, 270);
  lv_obj_set_pos(s_sugPanel, 660, 52);
  lv_obj_set_style_bg_color(s_sugPanel, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(s_sugPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_sugPanel, 1, 0);
  lv_obj_set_style_border_color(s_sugPanel, lv_color_hex(0x94A3B8), 0);
  lv_obj_set_style_radius(s_sugPanel, 8, 0);
  lv_obj_set_style_pad_all(s_sugPanel, 4, 0);
  lv_obj_clear_flag(s_sugPanel, LV_OBJ_FLAG_SCROLLABLE);
  for (int i = 0; i < 5; i++) {
    s_sugBtn[i] = lv_btn_create(s_sugPanel);
    lv_obj_set_size(s_sugBtn[i], 334, 48);
    lv_obj_set_pos(s_sugBtn[i], 0, i * 52);
    lv_obj_set_style_bg_color(s_sugBtn[i], lv_color_hex(0xE8F1FB), 0);
    lv_obj_set_style_radius(s_sugBtn[i], 6, 0);
    lv_obj_set_style_shadow_width(s_sugBtn[i], 0, 0);
    lv_obj_add_event_cb(s_sugBtn[i], sugWahlCb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    s_sugLbl[i] = lab(s_sugBtn[i], F18(), 0x111111, -8, -2, 318, 44, "");
    lv_label_set_long_mode(s_sugLbl[i], LV_LABEL_LONG_WRAP);
  }
  lv_obj_add_flag(s_sugPanel, LV_OBJ_FLAG_HIDDEN);
  s_sugDeb = lv_timer_create(sugDebCb, 800, nullptr); lv_timer_pause(s_sugDeb);
  s_sugPoll = lv_timer_create(sugPollCb, 300, nullptr);
}

// ── Motorradgebiete ──────────────────────────────────────────────────────────
static int s_gebTag = 0;
static lv_obj_t* s_gebEbene = nullptr;
static lv_timer_t* s_gebPoll = nullptr;

static lv_obj_t* s_gebHinweis = nullptr;
static void gebieteAnzeigen() {
  if (!objects.containergebieteliste) return;
  if (!s_gebHinweis) {   // feste Fussnote: was ein Gebiet hier ist (nicht scrollbar, bleibt unten)
    s_gebHinweis = lab(objects.containergebieteliste, F18(), 0x5A6B7B, 14, 414, 960, 48, "Gebiet = ein Punkt (der Ort) mit etwa 10 km Umkreis. Score: Mittel 10 bis 18 Uhr. Orientierung, noch nicht präzise.");
    lv_label_set_long_mode(s_gebHinweis, LV_LABEL_LONG_WRAP);
  }
  if (!s_gebEbene) { s_gebEbene = ebene(objects.containergebieteliste); lv_obj_add_flag(s_gebEbene, LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(s_gebEbene, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_scroll_dir(s_gebEbene, LV_DIR_VER); }
  lv_obj_clean(s_gebEbene);
  // aktive Taste hervorheben
  lv_obj_t* tasten[3] = {objects.buttongebieteheute, objects.buttongebietemorgen, objects.buttongebietewochenende};
  for (int i = 0; i < 3; i++) if (tasten[i]) lv_obj_set_style_bg_color(tasten[i], lv_color_hex(i == s_gebTag ? 0x0B3F8F : 0x1D6FD6), 0);
  int reihe[GEBIETE_MAX];
  const int n = gebieteRang(s_gebTag, reihe, GEBIETE_MAX);
  if (objects.labelgebieteleer) {
    if (n) lv_obj_add_flag(objects.labelgebieteleer, LV_OBJ_FLAG_HIDDEN);
    else { lv_obj_clear_flag(objects.labelgebieteleer, LV_OBJ_FLAG_HIDDEN); lv_label_set_text(objects.labelgebieteleer, gebieteStatus == 1 ? gebieteText : gebieteStatus == 3 ? gebieteText : "Noch keine Daten. Die Liste legst du in der Web-Oberfläche an (Reiter Routen)."); }
  }
  static const char* TAG[3] = {"heute", "morgen", "am Wochenende"};
  for (int r = 0; r < n; r++) {
    const Gebiet& g = gebiete[reihe[r]];
    lv_obj_t* z = lv_obj_create(s_gebEbene);
    lv_obj_remove_style_all(z);
    lv_obj_set_size(z, 940, 60);
    lv_obj_set_pos(z, 6, 6 + r * 66);
    lv_obj_set_style_bg_color(z, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(z, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(z, 8, 0);
    lv_obj_set_style_border_width(z, 1, 0);
    lv_obj_set_style_border_color(z, lv_color_hex(0x94A3B8), 0);
    char b[96];
    snprintf(b, sizeof(b), "%d", r + 1);
    lab(z, F32(), 0x5A6B7B, 8, 10, 50, 40, b, LV_TEXT_ALIGN_CENTER);
    lab(z, F22(), 0x111111, 70, 6, 600, 28, g.name);
    snprintf(b, sizeof(b), "%s: bis %.0f °C, Regen %.1f mm", TAG[s_gebTag], g.tmax[s_gebTag], g.mm[s_gebTag]);
    kommaText(b);
    lab(z, F18(), 0x5A6B7B, 70, 34, 700, 22, b);
    lv_obj_t* pill = lv_obj_create(z);
    lv_obj_remove_style_all(pill);
    lv_obj_set_size(pill, 100, 46);
    lv_obj_set_pos(pill, 830, 7);
    lv_obj_set_style_bg_color(pill, lv_color_hex(bikerFarbe(g.score[s_gebTag])), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pill, 8, 0);
    snprintf(b, sizeof(b), "%d", g.score[s_gebTag]);
    lab(pill, F32(), 0xFFFFFF, 0, 5, 100, 40, b, LV_TEXT_ALIGN_CENTER);
  }
}

static void gebPollCb(lv_timer_t* t) {
  if (gebieteStatus == 1) { if (objects.labelgebieteleer && lv_obj_has_flag(objects.labelgebieteleer, LV_OBJ_FLAG_HIDDEN) == false) lv_label_set_text(objects.labelgebieteleer, gebieteText); return; }
  gebieteAnzeigen();
  lv_timer_pause(t);
}
static void gebieteLaden() {
  if (gebieteAnzahl == 0) gebieteListeLaden();
  const bool alt = gebieteStand == 0 || time(nullptr) - gebieteStand > 1800;
  gebieteAnzeigen();
  if (alt && gebieteAuftrag(false, "")) { if (!s_gebPoll) s_gebPoll = lv_timer_create(gebPollCb, 500, nullptr); lv_timer_resume(s_gebPoll); }
}
static void gebieteEinrichten() {
  if (!objects.screengebiete) return;
  lv_obj_add_event_cb(objects.screengebiete, [](lv_event_t*) { gebieteLaden(); }, LV_EVENT_SCREEN_LOAD_START, nullptr);
  auto k = [](lv_obj_t* o, int tag) { if (o) { lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(o, [](lv_event_t* e) { s_gebTag = (int)(intptr_t)lv_event_get_user_data(e); gebieteAnzeigen(); }, LV_EVENT_CLICKED, (void*)(intptr_t)tag); } };
  k(objects.buttongebieteheute, 0); k(objects.buttongebietemorgen, 1); k(objects.buttongebietewochenende, 2);
}

// ── Anmeldung ────────────────────────────────────────────────────────────────
void uiRoutenInit() {
  LvLock lock;
  lv_obj_t* scr = objects.screenrouteneu;
  if (!scr || s_kb) return;
  feldEinrichten(objects.textareastart, "Start, zum Beispiel Lübeck");
  feldEinrichten(objects.textareaziel,  "Ziel, zum Beispiel Flensburg");
  feldEinrichten(objects.textareavia,   "Zwischenziel (kann leer bleiben)");

  s_kb = lv_btnmatrix_create(scr);
  lv_obj_set_size(s_kb, 1004, 262);
  lv_obj_set_pos(s_kb, 10, 330);
  lv_obj_set_style_text_font(s_kb, F22(), 0);
  lv_obj_set_style_pad_all(s_kb, 6, 0);
  lv_obj_set_style_pad_gap(s_kb, 6, 0);
  lv_obj_set_style_bg_color(s_kb, lv_color_hex(0xC9D3DD), 0);
  lv_obj_set_style_border_width(s_kb, 0, 0);
  lv_obj_set_style_radius(s_kb, 8, 0);
  lv_obj_set_style_bg_color(s_kb, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
  lv_obj_set_style_text_color(s_kb, lv_color_hex(0x111111), LV_PART_ITEMS);
  lv_obj_set_style_radius(s_kb, 6, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(s_kb, lv_color_hex(0x1D6FD6), LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_text_color(s_kb, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_PRESSED);
  kartenSetzen(true);
  lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(s_kb, cbTaste, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(scr, [](lv_event_t*) { tastaturZu(); }, LV_EVENT_SCREEN_UNLOAD_START, nullptr);
  lv_obj_add_event_cb(scr, [](lv_event_t*) { eingabeAnzeigen(); }, LV_EVENT_SCREEN_LOAD_START, nullptr);

  auto klick = [](lv_obj_t* o, lv_event_cb_t cb) { if (o) { lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(o, cb, LV_EVENT_CLICKED, nullptr); } };
  klick(objects.buttonabfahrtminus, cbAbfMinus); klick(objects.buttonabfahrtplus, cbAbfPlus); klick(objects.buttonabfahrtjetzt, cbAbfJetzt);
  klick(objects.buttontempominus, cbTempoMinus); klick(objects.buttontempoplus, cbTempoPlus);
  klick(objects.buttonrouteberechnen, cbBerechnen);
  if (objects.switchautobahn) lv_obj_add_state(objects.switchautobahn, LV_STATE_CHECKED);   // Autobahn meiden ist vorgewaehlt
  if (objects.labelautobahn) lv_label_set_text(objects.labelautobahn, "Autobahn meiden");
  if (objects.labeltastaturhinweis) lv_label_set_text(objects.labeltastaturhinweis, "Start und Ziel antippen: die Tastatur erscheint.");

  if (objects.screenroutenliste) lv_obj_add_event_cb(objects.screenroutenliste, [](lv_event_t*) { listeAufbauen(); }, LV_EVENT_SCREEN_LOAD_START, nullptr);
  if (objects.screenroutewetter) lv_obj_add_event_cb(objects.screenroutewetter, [](lv_event_t*) { routeWetterAnzeigen(); }, LV_EVENT_SCREEN_LOAD_START, nullptr);
  if (objects.screenroutehoehe)  lv_obj_add_event_cb(objects.screenroutehoehe,  [](lv_event_t*) { hoeheAnzeigen(); },       LV_EVENT_SCREEN_LOAD_START, nullptr);
  if (objects.screenabfahrt)     lv_obj_add_event_cb(objects.screenabfahrt,     [](lv_event_t*) { planerAnzeigen(); },      LV_EVENT_SCREEN_LOAD_START, nullptr);
  if (objects.buttontempominus_label) { lv_label_set_text(objects.buttontempominus_label, "- 5"); lv_obj_set_style_text_font(objects.buttontempominus_label, F22(), 0); }
  if (objects.buttontempoplus_label)  { lv_label_set_text(objects.buttontempoplus_label,  "+ 5"); lv_obj_set_style_text_font(objects.buttontempoplus_label,  F22(), 0); }
  if (objects.labelquellerw) {
    lv_label_set_text(objects.labelquellerw, "Daten: Open-Meteo.com, Strecke: OpenStreetMap-Mitwirkende, Valhalla");
    lv_obj_set_pos(objects.labelquellerw, 628, 538);
    lv_obj_set_size(objects.labelquellerw, 376, 50);
    lv_label_set_long_mode(objects.labelquellerw, LV_LABEL_LONG_WRAP);
  }
  if (objects.containerkarte) {   // Tippen auf die Karte oeffnet die Kartenansicht (schieben, zoomen)
    lv_obj_add_flag(objects.containerkarte, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(objects.containerkarte, [](lv_event_t*) { uiKarteOeffnen(); }, LV_EVENT_CLICKED, nullptr);
  }
  lv_timer_create([](lv_timer_t*) {   // Kartenbild der Route ist fertig: anzeigen, wenn die Seite offen ist
    if (karteStatus() == 2 && lv_scr_act() == objects.screenroutewetter) { s_kartenId[0] = 0; routeWetterAnzeigen(); }
  }, 1000, nullptr);
  eingabeAnzeigen();
  sugEinrichten();
  gebieteEinrichten();
}

// Fuer das Bildschirmfoto (screenshot.cpp): Inhalt der Routenseite jetzt aufbauen, Aufrufer haelt den LVGL-Lock
void uiRoutenSeiteVorbereiten(const char* name) {
  if (!strcmp(name, "routen")) listeAufbauen();
  else if (!strcmp(name, "routenwetter")) routeWetterAnzeigen();
  else if (!strcmp(name, "hoehe")) hoeheAnzeigen();
  else if (!strcmp(name, "abfahrt")) planerAnzeigen();
  else if (!strcmp(name, "gebiete")) gebieteAnzeigen();
  else if (!strcmp(name, "routeneu")) eingabeAnzeigen();
}
