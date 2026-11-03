// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Warnungen: DWD-Warnkarte mit Standort-Marker und Windpfeil, Warnliste, Warn-Button,
// Detail-Popup der Warnliste
#include "biker_fonts.h"
#include "app_state.h"
#include "ntfy.h"
#include "../board/board.h"
#include <math.h>
#include "navigation.h"
#include "../ui/screens.h"
#include "../ui/fonts/fonts.h"

static const int SCREEN_W = 1024;
static const int KARTE_Y  = 4;       // Oberkante der Karte; Button-Leiste beginnt bei y=548

static lv_obj_t* imgWarnkarte     = nullptr;
static lv_obj_t* lblWarnkarteHint = nullptr;
static lv_obj_t* radarMarker      = nullptr;
static lv_obj_t* windBg = nullptr, *windShaft = nullptr, *windHead1 = nullptr, *windHead2 = nullptr;
static lv_point_t wPts0[2], wPts1[2], wPts2[2];          // Linienpunkte müssen bestehen bleiben

static lv_obj_t*   warnListeContainer = nullptr;
static lv_obj_t*   warnPopup = nullptr;
static bool        warnPopupNurSchliessen = false;
static lv_obj_t*   dwdWarnBtn = nullptr;
static lv_timer_t* dwdBlinker = nullptr;

// ── Warnkarte ────────────────────────────────────────────────────────────────
static void windpfeilAktualisieren() {
  if (!windShaft || !wetter.geladen) return;
  const int CX = SCREEN_W - 56, CY = KARTE_Y + 56;     // Mitte des Kreises oben rechts auf der Karte
  const int LEN = 22, HEAD = 10;
  // wind_dir = Richtung, aus der der Wind kommt → +180° = Zugrichtung
  float rad = fmodf(wetter.wind_dir + 180.0f, 360.0f) * (float)M_PI / 180.0f;
  float dx = sinf(rad), dy = -cosf(rad);
  int tx = CX + (int)(dx * LEN), ty = CY + (int)(dy * LEN);   // Spitze
  int bx = CX - (int)(dx * LEN), by = CY - (int)(dy * LEN);   // Ende
  wPts0[0] = {(lv_coord_t)bx, (lv_coord_t)by};
  wPts0[1] = {(lv_coord_t)tx, (lv_coord_t)ty};
  float h1 = rad + 2.44f, h2 = rad - 2.44f;                  // Pfeilspitzen ±140°
  wPts1[0] = {(lv_coord_t)tx, (lv_coord_t)ty};
  wPts1[1] = {(lv_coord_t)(tx + (int)(sinf(h1) * HEAD)), (lv_coord_t)(ty - (int)(cosf(h1) * HEAD))};
  wPts2[0] = {(lv_coord_t)tx, (lv_coord_t)ty};
  wPts2[1] = {(lv_coord_t)(tx + (int)(sinf(h2) * HEAD)), (lv_coord_t)(ty - (int)(cosf(h2) * HEAD))};
  lv_line_set_points(windShaft, wPts0, 2);
  lv_line_set_points(windHead1, wPts1, 2);
  lv_line_set_points(windHead2, wPts2, 2);
  lv_obj_clear_flag(windBg,    LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(windShaft, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(windHead1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(windHead2, LV_OBJ_FLAG_HIDDEN);
}

void warnkarteFreigeben() {
  LvLock lock;
  if (!warnkarteBuf) return;
  if (imgWarnkarte) lv_obj_add_flag(imgWarnkarte, LV_OBJ_FLAG_HIDDEN);
  if (radarMarker)  lv_obj_add_flag(radarMarker,  LV_OBJ_FLAG_HIDDEN);
  lv_img_cache_invalidate_src(&warnkarteDsc);
  free(warnkarteBuf);
  warnkarteBuf = nullptr;
  memset(&warnkarteDsc, 0, sizeof(warnkarteDsc));
}

void aktualisiereWarnkarte() {
  LvLock lock;
  if (!imgWarnkarte || !warnkarteBuf) return;
  lv_obj_clear_flag(imgWarnkarte, LV_OBJ_FLAG_HIDDEN);
  lv_img_cache_invalidate_src(&warnkarteDsc);
  lv_img_set_src(imgWarnkarte, &warnkarteDsc);
  lv_img_set_antialias(imgWarnkarte, false);
  lv_obj_align(imgWarnkarte, LV_ALIGN_TOP_MID, 0, KARTE_Y);
  if (lblWarnkarteHint) lv_obj_add_flag(lblWarnkarteHint, LV_OBJ_FLAG_HIDDEN);

  // Standort-Marker: Lage innerhalb des Kartenausschnitts
  if (radarMarker && stdLat() != 0.0f) {
    const DwdBbox* bb = getDwdBbox();
    float fx = (stdLon() - bb->minLon) / (bb->maxLon - bb->minLon);
    float fy = 1.0f - (stdLat() - bb->minLat) / (bb->maxLat - bb->minLat);
    int x0 = (SCREEN_W - warnkarteW) / 2;
    lv_obj_set_pos(radarMarker, x0 + (int)(fx * warnkarteW) - 8, KARTE_Y + (int)(fy * warnkarteH) - 8);
    lv_obj_clear_flag(radarMarker, LV_OBJ_FLAG_HIDDEN);
  }
  windpfeilAktualisieren();
  lv_obj_invalidate(objects.screenwarnkarte1);
}

static void erstelleWarnkarte() {
  if (!objects.screenwarnkarte1) return;
  // Hinweis zuerst, Bild danach → das Bild liegt oben
  lblWarnkarteHint = lv_label_create(objects.screenwarnkarte1);
  lv_label_set_text(lblWarnkarteHint, "Lade Warnkarte ...");
  lv_obj_set_style_text_font(lblWarnkarteHint, &BIKER_F32, 0);
  lv_obj_set_style_text_color(lblWarnkarteHint, lv_color_hex(0x888888), 0);
  lv_obj_align(lblWarnkarteHint, LV_ALIGN_CENTER, 0, -30);

  imgWarnkarte = lv_img_create(objects.screenwarnkarte1);
  lv_obj_align(imgWarnkarte, LV_ALIGN_TOP_MID, 0, KARTE_Y);

  radarMarker = lv_obj_create(objects.screenwarnkarte1);
  lv_obj_set_size(radarMarker, 16, 16);
  lv_obj_set_style_radius(radarMarker, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(radarMarker, lv_color_hex(0xff0000), 0);
  lv_obj_set_style_bg_opa(radarMarker, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(radarMarker, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_width(radarMarker, 2, 0);
  lv_obj_clear_flag(radarMarker, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(radarMarker, LV_OBJ_FLAG_HIDDEN);

  windBg = lv_obj_create(objects.screenwarnkarte1);
  lv_obj_set_size(windBg, 84, 84);
  lv_obj_set_pos(windBg, SCREEN_W - 56 - 42, KARTE_Y + 56 - 42);
  lv_obj_set_style_radius(windBg, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(windBg, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(windBg, 110, 0);
  lv_obj_set_style_border_width(windBg, 0, 0);
  lv_obj_clear_flag(windBg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(windBg, LV_OBJ_FLAG_HIDDEN);

  auto linie = [](lv_obj_t*& l) {
    l = lv_line_create(objects.screenwarnkarte1);
    lv_obj_set_style_line_color(l, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_line_width(l, 3, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
  };
  linie(windShaft); linie(windHead1); linie(windHead2);
}

// ── Warnliste (Warnkarte 2) ──────────────────────────────────────────────────
static lv_color_t dwdSevColor(int sev) {
  switch (sev) {
    case 4:  return lv_color_hex(0xC000C0);
    case 3:  return lv_color_hex(0xFF0000);
    case 2:  return lv_color_hex(0xFF8000);
    default: return lv_color_hex(0xFFCC00);
  }
}

static void schliesseWarnPopup() {
  if (warnPopup) { lv_obj_del(warnPopup); warnPopup = nullptr; }
}

static void zeigeWarnPopup(lv_obj_t* parent, const char* titel, const char* text, bool nurSchliessen);

static void cbWarnKarteCardTap(lv_event_t* e) {
  if (warnPopup) { schliesseWarnPopup(); return; }
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= anzahl_warnungen) return;
  static const char* levelName[] = {"", "Vorinformation", "Warnung", "Schwere Warnung", "Extreme Warnung"};
  int lv = constrain(warnungen[idx].level, 0, 4);
  String text = String(levelName[lv]) + "\n" + bereinigeText(warnungen[idx].beschreibung).substring(0, 400);
  zeigeWarnPopup(objects.screenwarnkarte2, bereinigeText(warnungen[idx].titel).c_str(), text.c_str(), true);
}

void aktualisiereWarnliste() {
  LvLock lock;
  if (!objects.screenwarnkarte2) return;
  if (warnListeContainer) { lv_obj_del(warnListeContainer); warnListeContainer = nullptr; }

  warnListeContainer = lv_obj_create(objects.screenwarnkarte2);
  lv_obj_set_size(warnListeContainer, 1000, 532);
  lv_obj_align(warnListeContainer, LV_ALIGN_TOP_MID, 0, 4);
  lv_obj_set_style_bg_color(warnListeContainer, lv_color_hex(0x111111), 0);
  lv_obj_set_style_bg_opa(warnListeContainer, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(warnListeContainer, 0, 0);
  lv_obj_set_style_pad_all(warnListeContainer, 8, 0);
  lv_obj_set_style_bg_color(warnListeContainer, lv_color_hex(0x444444), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(warnListeContainer, LV_OPA_COVER, LV_PART_SCROLLBAR);

  if (anzahl_warnungen == 0) {
    lv_obj_t* lbl = lv_label_create(warnListeContainer);
    lv_label_set_text(lbl, "Keine aktiven\nDWD-Wetterwarnungen");
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(lbl, &BIKER_F32, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(warnListeContainer, LV_OBJ_FLAG_SCROLLABLE);
    return;
  }

  const int CARD_H = 160;
  for (int i = 0; i < anzahl_warnungen && i < 5; i++) {
    int sev = warnungen[i].level;
    lv_color_t textCol = (sev <= 1) ? lv_color_hex(0x111111) : lv_color_hex(0xffffff);

    lv_obj_t* card = lv_obj_create(warnListeContainer);
    lv_obj_set_size(card, 970, CARD_H);
    lv_obj_set_pos(card, 0, i * (CARD_H + 8));
    lv_obj_set_style_bg_color(card, dwdSevColor(sev), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, cbWarnKarteCardTap, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    lv_obj_t* typ = lv_label_create(card);
    lv_label_set_text(typ, bereinigeText(warnungen[i].typ).c_str());
    lv_obj_set_style_text_font(typ, &BIKER_F18, 0);
    lv_obj_set_style_text_color(typ, textCol, 0);
    lv_obj_align(typ, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* titel = lv_label_create(card);
    lv_label_set_long_mode(titel, LV_LABEL_LONG_DOT);
    lv_obj_set_size(titel, 940, 30);
    lv_label_set_text(titel, bereinigeText(warnungen[i].titel).c_str());
    lv_obj_set_style_text_font(titel, &BIKER_F22, 0);
    lv_obj_set_style_text_color(titel, textCol, 0);
    lv_obj_align(titel, LV_ALIGN_TOP_LEFT, 0, 26);

    lv_obj_t* beschr = lv_label_create(card);
    lv_label_set_long_mode(beschr, LV_LABEL_LONG_DOT);
    lv_obj_set_size(beschr, 940, 52);
    lv_label_set_text(beschr, bereinigeText(warnungen[i].beschreibung).substring(0, 220).c_str());
    lv_obj_set_style_text_font(beschr, &BIKER_F18, 0);
    lv_obj_set_style_text_color(beschr, textCol, 0);
    lv_obj_align(beschr, LV_ALIGN_TOP_LEFT, 0, 62);

    lv_obj_t* hint = lv_label_create(card);
    lv_label_set_text(hint, "Tippen für Details");
    lv_obj_set_style_text_font(hint, &BIKER_F18, 0);
    lv_obj_set_style_text_color(hint, (sev <= 1) ? lv_color_hex(0x333333) : lv_color_hex(0xdddddd), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  }
  bool scroll = anzahl_warnungen * (CARD_H + 8) > 532;
  if (scroll) { lv_obj_add_flag(warnListeContainer, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_scroll_dir(warnListeContainer, LV_DIR_VER); }
  else          lv_obj_clear_flag(warnListeContainer, LV_OBJ_FLAG_SCROLLABLE);
}

// Warnton (nur Boards mit Lautsprecher): kurz, einmal je neuer Warnung; nicht in der Ruhezeit, ein laufender Timer-Alarm hat Vorrang
void warnTonSpielen() {
  if (!boardAudioVorhanden() || !cfg.warn_ton_an || warnRuheAktiv() || boardAudioSpielt()) return;
  boardAudioTon(cfg.warn_ton, cfg.warn_laut, 4000);
}

// ── Warn-Button auf dem Bildschirm "Heute" ───────────────────────────────────
// Der Button "Warnung" ist immer da und fuehrt zur Warnkarte. Gibt es DWD-Warnungen fuer den Standort, wird er rot und blinkt bis zum Antippen.
static bool dwdBlinkAn = false;
static void dwdBtnFarbe(bool rot) {
  if (!dwdWarnBtn) return;
  lv_obj_set_style_bg_color(dwdWarnBtn, lv_color_hex(rot ? 0xcc0000 : 0xE8A000), 0);
}
static void cbDwdBlinker(lv_timer_t*) {
  if (!dwdWarnBtn || dwdWarnBestaetigt || warnRuheAktiv()) return;
  dwdBlinkAn = !dwdBlinkAn;
  lv_obj_set_style_bg_color(dwdWarnBtn, lv_color_hex(dwdBlinkAn ? 0xcc0000 : 0x660000), 0);
}

static void cbDwdWarnBtnTap(lv_event_t*) {
  if (anzahl_warnungen > 0) {
    dwdWarnBestaetigt = true;
    if (dwdBlinker) { lv_timer_del(dwdBlinker); dwdBlinker = nullptr; }
    dwdBtnFarbe(true);
    nav_goto(SCREEN_ID_SCREENWARNKARTE2);
    aktualisiereWarnliste();
  }
}

static void erstelleDwdWarnBtn() {
  if (dwdWarnBtn) return;
  dwdWarnBtn = objects.buttonheutewarnung;
  if (!dwdWarnBtn) return;
  lv_obj_add_event_cb(dwdWarnBtn, cbDwdWarnBtnTap, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);   // erscheint nur bei Warnung oder Hinweis
}

bool hinweisFuerButton(char* buf, size_t n, uint32_t* farbe);   // ui_update.cpp

// Der Button zeigt (in dieser Reihenfolge) eine DWD-Warnung fuer den Standort, sonst den ersten Hinweis (Regen, Frost, UV); gibt es nichts, ist er versteckt.
void aktualisiereDwdWarnBtn() {
  LvLock lock;
  if (!dwdWarnBtn) return;
  static uint32_t zuletztGemeldet = 0;                           // Kennung der zuletzt gemeldeten Warnungen: Ton nur bei neuen
  uint32_t kennung = 0;
  if (cfg.warn_region) for (int i = 0; i < anzahl_warnungen && i < 5; i++) for (const char* c = warnungen[i].titel.c_str(); *c; c++) kennung = kennung * 31 + (uint8_t)*c;
  char hb[100]; uint32_t hf = 0;
  const bool ruhe = warnRuheAktiv();
  if (cfg.warn_region && anzahl_warnungen > 0 && !ruhe) {
    if (kennung != zuletztGemeldet && !dwdWarnBestaetigt) {
      warnTonSpielen();
      if (ntfyQuelleAktiv(NTFY_Q_WARNUNG) && kennung != zuletztGemeldet) {
        String t = warnungen[0].titel; if (anzahl_warnungen > 1) t += " (+" + String(anzahl_warnungen - 1) + ")";
        ntfySenden("DWD-Warnung für den Standort", t.c_str(), 4, "warning");
      }
    }
    zuletztGemeldet = kennung;
    lv_obj_clear_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);
    if (objects.buttonheutewarnung_label) lv_label_set_text(objects.buttonheutewarnung_label, anzahl_warnungen > 1 ? "DWD Warnungen" : "DWD Warnung");
    dwdBtnFarbe(true);
    if (!dwdWarnBestaetigt && !dwdBlinker) dwdBlinker = lv_timer_create(cbDwdBlinker, 600, nullptr);
  } else {
    if (!anzahl_warnungen || !cfg.warn_region) zuletztGemeldet = 0;
    dwdWarnBestaetigt = false;
    if (dwdBlinker) { lv_timer_del(dwdBlinker); dwdBlinker = nullptr; }
    if (!ruhe && hinweisFuerButton(hb, sizeof(hb), &hf)) {
      lv_obj_clear_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);
      if (objects.buttonheutewarnung_label) lv_label_set_text(objects.buttonheutewarnung_label, hb);
      lv_obj_set_style_bg_color(dwdWarnBtn, lv_color_hex(hf), 0);
    } else lv_obj_add_flag(dwdWarnBtn, LV_OBJ_FLAG_HIDDEN);
  }
}


static void cbWarnPopupOk(lv_event_t*) { schliesseWarnPopup(); }

static void zeigeWarnPopup(lv_obj_t* parent, const char* titel, const char* text, bool nurSchliessen) {
  (void)nurSchliessen;
  schliesseWarnPopup();
  warnPopup = lv_obj_create(parent);
  lv_obj_set_size(warnPopup, 800, 420);
  lv_obj_align(warnPopup, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(warnPopup, lv_color_hex(0x111111), 0);
  lv_obj_set_style_bg_opa(warnPopup, LV_OPA_90, 0);
  lv_obj_set_style_border_color(warnPopup, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_width(warnPopup, 2, 0);
  lv_obj_set_style_radius(warnPopup, 12, 0);
  lv_obj_clear_flag(warnPopup, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* lblTitel = lv_label_create(warnPopup);
  lv_label_set_long_mode(lblTitel, LV_LABEL_LONG_DOT);
  lv_obj_set_width(lblTitel, 740);
  lv_label_set_text(lblTitel, titel);
  lv_obj_set_style_text_font(lblTitel, &BIKER_F32, 0);
  lv_obj_set_style_text_color(lblTitel, lv_color_hex(0xffcc00), 0);
  lv_obj_align(lblTitel, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t* lblText = lv_label_create(warnPopup);
  lv_label_set_long_mode(lblText, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(lblText, 740, 230);
  lv_label_set_text(lblText, text);
  lv_obj_set_style_text_font(lblText, &BIKER_F22, 0);
  lv_obj_set_style_text_color(lblText, lv_color_hex(0xeeeeee), 0);
  lv_obj_align(lblText, LV_ALIGN_TOP_MID, 0, 60);

  lv_obj_t* btn = lv_btn_create(warnPopup);
  lv_obj_set_size(btn, 220, 56);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x333333), 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_t* lblBtn = lv_label_create(btn);
  lv_label_set_text(lblBtn, "OK");
  lv_obj_set_style_text_font(lblBtn, &BIKER_F22, 0);
  lv_obj_set_style_text_color(lblBtn, lv_color_hex(0xffffff), 0);
  lv_obj_center(lblBtn);
  lv_obj_add_event_cb(btn, cbWarnPopupOk, LV_EVENT_CLICKED, nullptr);
}

static void cbWarnkarte2Tap(lv_event_t*) { schliesseWarnPopup(); }

// Wird alle 500 ms aufgerufen: Ruhezeit-Wechsel erkennen und den Warn-Button anpassen
void uiWarnenPruefen() {
  LvLock lock;
  static bool ruheVorher = false;
  const bool ruhe = warnRuheAktiv();
  if (ruhe != ruheVorher) { ruheVorher = ruhe; aktualisiereDwdWarnBtn(); }
}

void uiWarnInit() {
  LvLock lock;
  erstelleWarnkarte();
  erstelleDwdWarnBtn();
  aktualisiereWarnliste();

  auto reg = [](lv_obj_t* o, lv_event_cb_t cb) { if (o) lv_obj_add_event_cb(o, cb, LV_EVENT_CLICKED, nullptr); };
  reg(objects.screenwarnkarte2,    cbWarnkarte2Tap);
  // Liste beim Öffnen von Warnkarte 2 frisch aufbauen
  reg(objects.labelbuttonscreenhub, [](lv_event_t*) { aktualisiereWarnliste(); });
}
