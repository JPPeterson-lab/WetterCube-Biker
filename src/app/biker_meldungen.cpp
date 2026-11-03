// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Handy-Meldungen (ntfy) des Biker: abends das beste Fahrfenster von morgen, "Regen in 25 Minuten" am Heimatort.
// (Saisonstart und Termine kommen ebenfalls von hier.) (DWD-Warnungen meldet ui_warn.cpp, sobald eine neue erscheint.) Wird alle 30 Sekunden aus dem Netzwerk-Task aufgerufen.
#include "app_state.h"
#include "biker.h"
#include "ntfy.h"
#include <time.h>
#include <math.h>
#include <stdio.h>

// Tage seit 1970 (nach Howard Hinnant), fuer den Abstand zwischen zwei Daten
static long tagNr(int J, int M, int D) {
  J -= M <= 2;
  const long era = (J >= 0 ? J : J - 399) / 400, yoe = J - era * 400;
  const long doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
  return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

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

  // Saisonstart: einmal im Jahr, am ersten Tag ab Februar mit Hoechstwert ab 15 Grad und gutem Tagesscore
  static int saisonGeprueft = -1;
  if ((cfg.ntfy_quellen & NTFY_Q_SAISON) && tm.tm_hour >= 8 && tm.tm_mon >= 1 && cfg.saison_jahr != tm.tm_year + 1900 && saisonGeprueft != tm.tm_yday) {
    saisonGeprueft = tm.tm_yday;
    const int sc = bikerTagesScore(*bikerHeim, 0);
    float tmax = -99;
    for (int h = 10; h < 18 && h < bikerHeim->n; h++) if (bikerHeim->h[h].temp > tmax) tmax = bikerHeim->h[h].temp;
    if (tmax >= 15.0f && sc >= 60) {
      char t[160];
      snprintf(t, sizeof(t), "Heute bis %.0f °C und Score %d: die erste richtig warme Ausfahrt des Jahres. Saison eröffnen?", tmax, sc);
      ntfySenden("Saisonstart", t, 3, "motorcycle");
      cfg.saison_jahr = tm.tm_year + 1900;
      speichereCfg();
    }
  }

  // Termine (TUeV, Inspektion ...): 14 Tage vorher, 3 Tage vorher und am Tag selbst, je einmal, ab 8 Uhr
  static int terminGeprueft = -1;
  if ((cfg.ntfy_quellen & NTFY_Q_TERMIN) && tm.tm_hour >= 8 && terminGeprueft != tm.tm_yday) {
    terminGeprueft = tm.tm_yday;
    const long heute = tagNr(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    bool geaendert = false;
    for (int i = 0; i < 4; i++) {
      int J, M, D;
      if (cfg.termin_name[i].isEmpty() || sscanf(cfg.termin_datum[i].c_str(), "%d-%d-%d", &J, &M, &D) != 3) continue;
      const long tage = tagNr(J, M, D) - heute;
      if (tage < 0) continue;
      const int stufe = tage == 0 ? 3 : tage <= 3 ? 2 : tage <= 14 ? 1 : 0;
      if (stufe == 0 || stufe <= cfg.termin_stufe[i]) continue;
      char t[160];
      if (tage == 0) snprintf(t, sizeof(t), "%s ist heute fällig.", cfg.termin_name[i].c_str());
      else snprintf(t, sizeof(t), "%s ist in %ld %s fällig (%02d.%02d.%04d).", cfg.termin_name[i].c_str(), tage, tage == 1 ? "Tag" : "Tagen", D, M, J);
      ntfySenden("Termin", t, stufe == 3 ? 4 : 3, "calendar");
      cfg.termin_stufe[i] = (uint8_t)stufe; geaendert = true;
    }
    if (geaendert) speichereCfg();
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
