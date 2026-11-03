// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Grosse Karte: Tippen auf das Kartenbild im Routenwetter oeffnet ein festes, grosses Kartenbild (Zoomstufe 10, bei langen Strecken kleiner)
// mit der Strecke in Score-Farben. Kein Schieben und kein Zoom: das war auf dem Display zu zaeh (siehe CHANGELOG 0.0.10).
// Das Bild entsteht im Hintergrund zusammen mit dem kleinen Kartenbild (kacheln.cpp).
#include "biker_fonts.h"
#include "app_state.h"
#include "route.h"
#include "kacheln.h"
#include "navigation.h"
#include "../ui/screens.h"
#include <math.h>

static lv_obj_t* s_scr = nullptr;
static lv_obj_t* s_img = nullptr;
static lv_obj_t* s_ebene = nullptr;
static lv_obj_t* s_text = nullptr;
static lv_obj_t* s_quelle = nullptr;
static lv_timer_t* s_poll = nullptr;
static lv_img_dsc_t s_dsc;
static uint8_t* s_buf = nullptr;
static char s_id[12] = "";
static lv_point_t* s_pts[ROUTE_MAX_ABSCHNITTE] = {};

static void linienFrei() { for (int i = 0; i < ROUTE_MAX_ABSCHNITTE; i++) if (s_pts[i]) { heap_caps_free(s_pts[i]); s_pts[i] = nullptr; } }

static void zeichnen() {
  lv_obj_clean(s_ebene);
  linienFrei();
  const RAnalyse* a = routeAkt;
  if (!a || !a->route) { lv_label_set_text(s_text, "Keine Route gewählt."); lv_obj_clear_flag(s_text, LV_OBJ_FLAG_HIDDEN); return; }
  const Route* r = a->route;
  if (strcmp(s_id, r->id) != 0 || !s_buf || karteBildLaden(r->id, true, &s_dsc) != s_buf) {
    if (s_buf) { lv_img_set_src(s_img, nullptr); s_buf = nullptr; }
    s_buf = karteGrossVorhanden(r->id) ? karteBildLaden(r->id, true, &s_dsc) : nullptr;
    strlcpy(s_id, r->id, sizeof(s_id));
  }
  if (!s_buf && !karteStatusLaeuft() && !karteGrossVorhanden(r->id)) karteErzeugenStarten(r->id);   // aeltere Route ohne grosse Karte
  if (!s_buf) {
    lv_label_set_text(s_text, karteStatusLaeuft() ? "Die große Karte wird noch geladen ..." : "Für diese Route gibt es noch keine Karte.");
    lv_obj_clear_flag(s_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_img, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_add_flag(s_text, LV_OBJ_FLAG_HIDDEN);
  lv_img_set_src(s_img, &s_dsc);
  lv_obj_set_pos(s_img, 0, 0);
  lv_obj_clear_flag(s_img, LV_OBJ_FLAG_HIDDEN);
  const int z = karteZoom(r->id);
  float laMin = 90, laMax = -90, loMin = 180, loMax = -180;
  for (int i = 0; i < r->n; i++) { if (r->p[i].lat < laMin) laMin = r->p[i].lat; if (r->p[i].lat > laMax) laMax = r->p[i].lat; if (r->p[i].lon < loMin) loMin = r->p[i].lon; if (r->p[i].lon > loMax) loMax = r->p[i].lon; }
  double x0, y0, x1, y1;
  mercatorPx(laMax, loMin, z, &x0, &y0); mercatorPx(laMin, loMax, z, &x1, &y1);
  const double cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
  auto px = [&](const RoutePunkt& p) { double x, y; mercatorPx(p.lat, p.lon, z, &x, &y); lv_point_t q; q.x = (lv_coord_t)lround(x - cx + KARTE_GB / 2.0); q.y = (lv_coord_t)lround(y - cy + KARTE_GH / 2.0); return q; };
  for (int s = 0; s < a->n; s++) {
    const int i0 = a->a[s].i0, i1 = a->a[s].i1 < r->n ? a->a[s].i1 : r->n - 1;
    const int cnt = i1 - i0 + 1;
    if (cnt < 2) continue;
    s_pts[s] = (lv_point_t*)heap_caps_malloc(sizeof(lv_point_t) * cnt, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_pts[s]) continue;
    for (int k = 0; k < cnt; k++) s_pts[s][k] = px(r->p[i0 + k]);
    for (int durchgang = 0; durchgang < 2; durchgang++) {
      lv_obj_t* ln = lv_line_create(s_ebene);
      lv_line_set_points(ln, s_pts[s], cnt);
      lv_obj_set_style_line_width(ln, durchgang == 0 ? 12 : 8, 0);
      lv_obj_set_style_line_rounded(ln, true, 0);
      lv_obj_set_style_line_color(ln, lv_color_hex(durchgang == 0 ? 0xFFFFFF : (routeErg.gueltig && s < routeErg.n ? bikerFarbe(routeErg.score[s]) : 0x5A6B7B)), 0);
    }
  }
  auto marke = [&](const RoutePunkt& p, uint32_t farbe, const char* t) {
    lv_point_t q = px(p);
    lv_obj_t* o = lv_obj_create(s_ebene);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, 32, 32);
    lv_obj_set_pos(o, q.x - 16, q.y - 16);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(farbe), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(0xFFFFFF), 0);
    lv_obj_t* l = lv_label_create(o);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, &BIKER_F22, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(l);
  };
  marke(r->p[0], 0x1D6FD6, "S");
  marke(r->p[r->n - 1], 0x111111, "Z");
}

static void cbZurueck(lv_event_t*) { nav_goto(SCREEN_ID_SCREENROUTEWETTER); }

static void bauen() {
  s_scr = lv_obj_create(nullptr);
  lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(s_scr, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
  s_img = lv_img_create(s_scr);
  lv_obj_set_pos(s_img, 0, 0);
  s_ebene = lv_obj_create(s_scr);
  lv_obj_remove_style_all(s_ebene);
  lv_obj_set_size(s_ebene, KARTE_GB, KARTE_GH);
  lv_obj_set_pos(s_ebene, 0, 0);
  lv_obj_clear_flag(s_ebene, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(s_ebene, LV_OBJ_FLAG_CLICKABLE);
  s_text = lv_label_create(s_scr);
  lv_obj_set_style_text_font(s_text, &BIKER_F32, 0);
  lv_obj_set_style_text_color(s_text, lv_color_hex(0x5A6B7B), 0);
  lv_obj_align(s_text, LV_ALIGN_CENTER, 0, -30);
  s_quelle = lv_label_create(s_scr);
  lv_obj_set_style_text_font(s_quelle, &BIKER_F18, 0);
  lv_obj_set_style_text_color(s_quelle, lv_color_hex(0x333333), 0);
  lv_obj_set_style_bg_color(s_quelle, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(s_quelle, LV_OPA_80, 0);
  lv_obj_set_style_pad_all(s_quelle, 4, 0);
  lv_label_set_text(s_quelle, "© OpenStreetMap-Mitwirkende");
  lv_obj_align(s_quelle, LV_ALIGN_TOP_RIGHT, -8, KARTE_GH - 34);
  lv_obj_t* b = lv_btn_create(s_scr);
  lv_obj_set_size(b, 140, 44);
  lv_obj_set_pos(b, 18, 552);
  lv_obj_set_style_bg_color(b, lv_color_hex(0x5A6B7B), 0);
  lv_obj_set_style_radius(b, 10, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_add_event_cb(b, cbZurueck, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, "Zurück");
  lv_obj_set_style_text_font(l, &BIKER_F32, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
  lv_obj_center(l);
  // Bis das grosse Bild fertig ist: alle 1,5 Sekunden nachsehen
  s_poll = lv_timer_create([](lv_timer_t*) { if (lv_scr_act() == s_scr && !s_buf) zeichnen(); }, 1500, nullptr);
}

void uiKarteOeffnen() {
  LvLock lock;
  if (!routeAkt || !routeAkt->route) return;
  const uint32_t t0 = millis();
  if (!s_scr) bauen();
  zeichnen();
  lv_scr_load(s_scr);
  logDiag("[Karte] Kartenseite geoeffnet in %u ms", (unsigned)(millis() - t0));
}
