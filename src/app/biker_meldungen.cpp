// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Handy-Meldungen (ntfy) des Biker: abends das beste Fahrfenster von morgen, "Regen in 25 Minuten" am Heimatort.
// (DWD-Warnungen meldet ui_warn.cpp, sobald eine neue erscheint.) Wird alle 30 Sekunden aus dem Netzwerk-Task aufgerufen.
#include "app_state.h"
#include "biker.h"
#include "ntfy.h"
#include <time.h>

void bikerMeldungenPruefen() {
  if (!ntfyBereit() || !bikerHeim || !bikerHeim->geladen) return;
  const time_t jetzt = time(nullptr);
  struct tm tm; localtime_r(&jetzt, &tm);
  if (tm.tm_year < 120) return;   // Uhr noch nicht gestellt

  // Guter Fahrtag: einmal je Tag nach 18 Uhr das beste Fenster von morgen, wenn es sich lohnt
  static int tagGemeldet = -1;
  if ((cfg.ntfy_quellen & NTFY_Q_TAG) && tm.tm_hour >= 18 && tagGemeldet != tm.tm_yday) {
    tagGemeldet = tm.tm_yday;
    int von, bis, schnitt;
    if (bikerBesteFahrzeit(*bikerHeim, 1, 0, &von, &bis, &schnitt) && schnitt >= 65) {
      struct tm a, e; time_t t0 = bikerHeim->h[von].t, t1 = bikerHeim->h[bis - 1].t + 3600;
      localtime_r(&t0, &a); localtime_r(&t1, &e);
      const BikerStunde& h = bikerHeim->h[von];
      char t[160];
      snprintf(t, sizeof(t), "Morgen %02d bis %02d Uhr gute Bedingungen (Score %d, %.0f °C). Viel Spaß!", a.tm_hour, e.tm_hour == 0 ? 24 : e.tm_hour, schnitt, h.temp);
      ntfySenden("Morgen lohnt sich eine Ausfahrt", t, 3, "motorcycle");
    }
  }

  // Regen in Kuerze: einmal je Regenereignis, erst wieder nach 2 Stunden ohne Regen
  static time_t letzterRegen = 0;
  static bool gemeldet = false;
  char hinweis[64];
  if ((cfg.ntfy_quellen & NTFY_Q_REGEN) && cfg.hinweis_regen) {
    if (bikerRegenHinweis(hinweis, sizeof(hinweis))) {
      letzterRegen = jetzt;
      if (!gemeldet && strstr(hinweis, " in ")) {   // "Regen in 20 Min.", nicht "Es regnet" oder "Regen noch ..."
        gemeldet = true;
        String t = String(hinweis) + " (" + stdName() + ")";
        ntfySenden("Regen im Anmarsch", t.c_str(), 3, "cloud_with_rain");
      }
    } else if (gemeldet && jetzt - letzterRegen > 2 * 3600) gemeldet = false;
  }
}
