// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Biker-Score: eigene Regeln, Stundenwert 0 bis 100 (Abzuege je Faktor, Deckel bei Gewitter, Glaette, Frost, Dunkelheit)
#include "biker.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

BikerEinstellung bikerCfg;

float bikerFahrtwindTemp(float t, float v)
{
  if (t > 10.0f || v < 5.0f) return t;                       // Windchill gilt nur bei Kaelte und etwas Fahrtwind
  float p = powf(v, 0.16f);
  float wc = 13.12f + 0.6215f * t - 11.37f * p + 0.3965f * t * p;
  return wc < t ? wc : t;
}

int bikerScore(const BikerStunde& h, float fahrtKmh)
{
  float s = 100;
  const float tf = bikerFahrtwindTemp(h.temp, fahrtKmh);

  // Temperatur (am Fahrtwind-Wert): ideal 15 bis 27 Grad
  if (tf < 15) s -= fminf(70.0f, (15 - tf) * 3.5f + (tf < 3 ? (3 - tf) * 3.0f : 0));
  else if (tf > 27) s -= fminf(60.0f, (tf - 27) * 5.0f);

  // Niederschlag und Wahrscheinlichkeit
  if (h.mm >= 3.0f) s -= 65;
  else if (h.mm >= 1.0f) s -= 45;
  else if (h.mm >= 0.1f) s -= 20;
  else if (h.pop >= 30) s -= h.pop * 0.15f;

  // Wind und Boeen
  if (h.gust > 40) s -= fminf(40.0f, (h.gust - 40) * 1.0f);
  if (h.wind > 30) s -= fminf(15.0f, (h.wind - 30) * 0.5f);

  // Sicht und Feuchte
  if (h.vis < 1.0f) s -= 50;
  else if (h.vis < 5.0f) s -= 20;
  if (h.hum > 90) s -= 5;

  // Sonne: kleiner Bonus
  if (h.tag && h.cloud < 30 && h.mm < 0.1f) s += 5;

  // Deckel
  if (h.wmo >= 95) s = fminf(s, 20);
  if (h.temp <= 3.0f && (h.mm >= 0.1f || h.hum > 90)) s = fminf(s, 30);
  if (h.temp <= 0.0f) s = fminf(s, 15);
  if (!h.tag) s = fminf(s, 40);

  if (s < 0) s = 0;
  if (s > 100) s = 100;
  return (int)(s + 0.5f);
}

const char* bikerStufe(int s)
{
  if (s >= 75) return "Gute Bedingungen";
  if (s >= 55) return "Okay";
  if (s >= 35) return "Mäßig";
  return "Schlecht";
}

uint32_t bikerFarbe(int s)
{
  if (s >= 75) return 0x00A838;   // gruen
  if (s >= 55) return 0x8DB600;   // gelbgruen
  if (s >= 35) return 0xE8A000;   // orange
  return 0xD01818;                // rot
}

int bikerIndexJetzt(const BikerPunkt& p, time_t jetzt)
{
  if (!p.geladen || p.n <= 0) return -1;
  for (int i = p.n - 1; i >= 0; i--) if (p.h[i].t <= jetzt) return i;
  return 0;
}

int bikerTagesScore(const BikerPunkt& p, int tag)
{
  if (!p.geladen || tag < 0 || tag > 6) return -1;
  int summe = 0, n = 0;
  for (int h = 10; h < 18; h++) { const int i = tag * 24 + h; if (i >= p.n) break; summe += bikerScore(p.h[i], bikerCfg.fahrtKmh); n++; }
  return n ? summe / n : -1;
}

bool bikerBesteFahrzeit(const BikerPunkt& p, int tagNr, time_t ab, int* vonIdx, int* bisIdx, int* schnitt)
{
  if (!p.geladen || tagNr < 0 || tagNr > 6) return false;
  const time_t auf = p.aufgang[tagNr], unter = p.untergang[tagNr];
  int bestVon = -1, bestBis = -1, bestSumme = 0, bestLen = 0;
  int von = -1, summe = 0, len = 0;
  auto abschluss = [&](int bis) {
    if (len > 0 && (len > bestLen || (len == bestLen && summe > bestSumme))) { bestVon = von; bestBis = bis; bestSumme = summe; bestLen = len; }
    von = -1; summe = 0; len = 0;
  };
  for (int i = tagNr * 24; i < (tagNr + 1) * 24 && i < p.n; i++) {
    const BikerStunde& h = p.h[i];
    const bool imTag = (auf == 0 || unter == 0) ? h.tag : (h.t + 3600 > auf && h.t < unter);
    const bool zukunft = h.t + 3600 > ab;
    const int sc = bikerScore(h, bikerCfg.fahrtKmh);
    if (imTag && zukunft && sc >= 60) { if (von < 0) von = i; summe += sc; len++; }
    else abschluss(i);
  }
  abschluss(tagNr * 24 + 24 < p.n ? tagNr * 24 + 24 : p.n);
  if (bestVon < 0) {   // kein Fenster mit 60 Punkten: bestmoegliche Einzelstunde am Tag
    int bi = -1, bs = -1;
    for (int i = tagNr * 24; i < (tagNr + 1) * 24 && i < p.n; i++) {
      const BikerStunde& h = p.h[i];
      if (h.t + 3600 <= ab || !h.tag) continue;
      int sc = bikerScore(h, bikerCfg.fahrtKmh);
      if (sc > bs) { bs = sc; bi = i; }
    }
    if (bi < 0) return false;
    *vonIdx = bi; *bisIdx = bi + 1; *schnitt = bs;
    return true;
  }
  *vonIdx = bestVon; *bisIdx = bestBis; *schnitt = bestSumme / bestLen;
  return true;
}

void bikerEmpfehlung(const BikerPunkt& p, int von, int bis, char* out, size_t n)
{
  out[0] = 0;
  if (!p.geladen || von < 0 || von >= p.n) return;
  if (bis > p.n) bis = p.n;
  if (bis <= von) bis = von + 1;
  float tmin = 99, tmax = -99, gmax = 0, mmSum = 0, vmin = 99; int popMax = 0; bool gewitter = false;
  for (int i = von; i < bis; i++) {
    const BikerStunde& h = p.h[i];
    float tf = bikerFahrtwindTemp(h.temp, bikerCfg.fahrtKmh);
    if (tf < tmin) tmin = tf;
    if (h.temp > tmax) tmax = h.temp;
    if (h.gust > gmax) gmax = h.gust;
    if (h.vis < vmin) vmin = h.vis;
    if (h.pop > popMax) popMax = h.pop;
    if (h.wmo >= 95) gewitter = true;
    mmSum += h.mm;
  }
  char* w = out; size_t rest = n;
  auto add = [&](const char* s) { int k = snprintf(w, rest, "%s%s", w == out ? "" : " ", s); if (k > 0 && (size_t)k < rest) { w += k; rest -= k; } };
  if (gewitter) add("Gewitter möglich: besser nicht fahren.");
  if (mmSum >= 0.5f || popMax >= 50) add("Regenkombi einpacken.");
  if (tmin <= 2) add("Glättegefahr, Thermo-Unterwäsche und warme Handschuhe.");
  else if (tmin < 10) add("Warme Handschuhe, Thermo-Unterwäsche.");
  else if (tmin < 17) add("Winterfutter oder Zwiebellook.");
  if (tmax >= 30) add("Belüftete Jacke, viel trinken.");
  if (gmax >= 50) add("Böen: Seitenwind beachten.");
  if (vmin < 5) add("Sicht schlecht, Visier mit Pinlock.");
  if (!out[0]) add("Normale Ausrüstung reicht.");
}

void bikerWindKomponenten(float windKmh, int windAusGrad, int fahrtGrad, float* langs, float* quer)
{
  const float d = (float)(windAusGrad - fahrtGrad) * (float)M_PI / 180.0f;
  *langs = -windKmh * cosf(d);          // negativ = Gegenwind
  *quer = fabsf(windKmh * sinf(d));
}
