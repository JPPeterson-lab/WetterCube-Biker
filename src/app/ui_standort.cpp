// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Standort-Umschalter auf dem Bildschirm "Heute": ein Tipp auf den Ortsnamen oben links wechselt zwischen Standort 1 und 2
// (nur wenn ein zweiter Standort eingerichtet ist). Hinter dem Namen steht dann "1/2" oder "2/2".
#include "app_state.h"
#include "../ui/screens.h"

static bool s_bereit = false;

static void beiTipp(lv_event_t*) { if (hatStandort2()) standortWechseln(standortAktiv == 0 ? 1 : 0); }

void uiStandortInit() {
  if (s_bereit || !objects.labelort) return;
  s_bereit = true;
  lv_obj_add_flag(objects.labelort, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(objects.labelort, beiTipp, LV_EVENT_CLICKED, nullptr);
}

void aktualisiereStandort() {
  LvLock lock;
  uiStandortInit();
  if (!objects.labelort) return;
  String t = stdName();
  if (hatStandort2()) t += standortAktiv == 1 ? "  2/2" : "  1/2";
  if (strcmp(lv_label_get_text(objects.labelort), t.c_str()) != 0) lv_label_set_text(objects.labelort, t.c_str());
}
