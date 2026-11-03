// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Motorradgebiete: Liste im Datenlaufwerk, Wetter fuer alle Gebiete in einem Abruf, Rangfolge nach Biker-Score.
#include "gebiete.h"
#include "route.h"
#include "app_state.h"
#include "../storage/daten.h"
#include <ArduinoJson.h>
#include <new>
#include <stdio.h>
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

bool punktAuswerten(JsonVariantConst o, BikerPunkt* p);   // data_stunden.cpp

Gebiet gebiete[GEBIETE_MAX];
int    gebieteAnzahl = 0;
time_t gebieteStand = 0;
volatile int gebieteStatus = 0;
char   gebieteText[96] = "";
static const char* DATEI = DATEN_MOUNT "/gebiete.txt";

static void beispiele() {
  const struct { const char* n; float la, lo; } B[] = {{"Harz", 51.78f, 10.60f}, {"Ostholstein", 54.14f, 10.61f}, {"Plön", 54.16f, 10.42f}, {"Ratzeburg", 53.70f, 10.77f}};
  gebieteAnzahl = 0;
  for (auto& b : B) { Gebiet& g = gebiete[gebieteAnzahl++]; strlcpy(g.name, b.n, sizeof(g.name)); g.lat = b.la; g.lon = b.lo; }
}

static void listeSchreiben() {
  if (!datenBereit()) return;
  FILE* f = fopen(DATEI, "w");
  if (!f) return;
  for (int i = 0; i < gebieteAnzahl; i++) fprintf(f, "%s;%.4f;%.4f\n", gebiete[i].name, gebiete[i].lat, gebiete[i].lon);
  fclose(f);
}

void gebieteListeLaden() {
  gebieteAnzahl = 0;
  FILE* f = datenBereit() ? fopen(DATEI, "r") : nullptr;
  if (f) {
    char z[120];
    while (fgets(z, sizeof(z), f) && gebieteAnzahl < GEBIETE_MAX) {
      char* a = strchr(z, ';'); if (!a) continue;
      *a++ = 0; char* b = strchr(a, ';'); if (!b) continue; *b++ = 0;
      Gebiet& g = gebiete[gebieteAnzahl];
      g = Gebiet();
      strlcpy(g.name, z, sizeof(g.name)); g.lat = strtof(a, nullptr); g.lon = strtof(b, nullptr);
      if (g.name[0] && g.lat != 0) gebieteAnzahl++;
    }
    fclose(f);
  }
  if (gebieteAnzahl == 0) { beispiele(); listeSchreiben(); }
}

String gebieteTextHolen() {
  String s;
  for (int i = 0; i < gebieteAnzahl; i++) { s += gebiete[i].name; s += "\n"; }
  return s;
}

bool gebieteTextSpeichern(const String& zeilen) {
  Gebiet neu[GEBIETE_MAX]; int n = 0;
  int pos = 0;
  while (pos <= (int)zeilen.length() && n < GEBIETE_MAX) {
    int e = zeilen.indexOf('\n', pos); if (e < 0) e = zeilen.length();
    String z = zeilen.substring(pos, e); pos = e + 1; z.trim(); z.replace("\r", "");
    if (z.isEmpty()) continue;
    String name = z; float la = 0, lo = 0;
    int a = z.indexOf(';');
    if (a > 0) {   // "Name; lat; lon"
      name = z.substring(0, a); name.trim();
      int b = z.indexOf(';', a + 1);
      if (b > 0) { la = z.substring(a + 1, b).toFloat(); lo = z.substring(b + 1).toFloat(); }
    }
    for (int i = 0; i < gebieteAnzahl && la == 0; i++) if (name == gebiete[i].name) { la = gebiete[i].lat; lo = gebiete[i].lon; }   // schon bekannt
    if (la == 0) {
      snprintf(gebieteText, sizeof(gebieteText), "Suche %s ...", name.c_str());
      if (!geocodeAdresse(name, la, lo)) { snprintf(gebieteText, sizeof(gebieteText), "'%s' nicht gefunden", name.c_str()); return false; }
    }
    strlcpy(neu[n].name, name.c_str(), sizeof(neu[n].name)); neu[n].lat = la; neu[n].lon = lo; n++;
  }
  if (n == 0) { snprintf(gebieteText, sizeof(gebieteText), "Die Liste ist leer"); return false; }
  for (int i = 0; i < n; i++) gebiete[i] = neu[i];
  gebieteAnzahl = n;
  listeSchreiben();
  return true;
}

// Mittel der Stunden 10 bis 17 Uhr eines Tages (Tag-Nr. 0 = heute)
static void tagBewerten(const BikerPunkt& p, int tag, int* score, float* tmax, float* mm) {
  int summe = 0, n = 0; float tm = -99, regen = 0;
  for (int h = 10; h < 18; h++) {
    const int i = tag * 24 + h;
    if (i >= p.n) break;
    summe += bikerScore(p.h[i], bikerCfg.fahrtKmh); n++;
    if (p.h[i].temp > tm) tm = p.h[i].temp;
    regen += p.h[i].mm;
  }
  if (!n) { *score = -1; return; }
  *score = summe / n; *tmax = tm; *mm = regen;
}

static bool wetterHolen() {
  if (gebieteAnzahl == 0) return false;
  String la, lo;
  for (int i = 0; i < gebieteAnzahl; i++) { if (i) { la += ","; lo += ","; } la += String(gebiete[i].lat, 3); lo += String(gebiete[i].lon, 3); }
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + la + "&longitude=" + lo;
  url += "&hourly=temperature_2m,apparent_temperature,precipitation,precipitation_probability,weather_code,wind_speed_10m,wind_gusts_10m,"
         "wind_direction_10m,visibility,relative_humidity_2m,cloud_cover,is_day&daily=sunrise,sunset&timezone=auto&forecast_days=7";
  WiFiClientSecure sc; HTTPClient http;
  http.begin(sc, url); http.setTimeout(30000);
  int code = http.GET();
  if (code != 200) { logHttpFehler("Gebiete", code, sc); http.end(); snprintf(gebieteText, sizeof(gebieteText), "Wetterdienst nicht erreichbar (HTTP %d)", code); return false; }
  String body = http.getString(); http.end();
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) { snprintf(gebieteText, sizeof(gebieteText), "Wetterdaten nicht lesbar"); return false; }
  body = String();
  BikerPunkt* p = (BikerPunkt*)heap_caps_calloc(1, sizeof(BikerPunkt), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) return false;
  const time_t jetzt = time(nullptr);
  struct tm tm; localtime_r(&jetzt, &tm);
  // Wochenende: Samstag und Sonntag (am Samstag heute und morgen, am Sonntag nur heute)
  const int tagWe[2] = {tm.tm_wday == 0 ? 0 : 6 - tm.tm_wday, tm.tm_wday == 0 ? -1 : 7 - tm.tm_wday};
  Gebiet tmp[GEBIETE_MAX];
  for (int g = 0; g < gebieteAnzahl; g++) {
    new (p) BikerPunkt();
    JsonVariantConst o = doc.as<JsonVariantConst>();
    if (gebieteAnzahl > 1) o = doc[g];
    tmp[g] = gebiete[g];
    if (!punktAuswerten(o, p)) { tmp[g].score[0] = tmp[g].score[1] = tmp[g].score[2] = -1; continue; }
    tagBewerten(*p, 0, &tmp[g].score[0], &tmp[g].tmax[0], &tmp[g].mm[0]);
    tagBewerten(*p, 1, &tmp[g].score[1], &tmp[g].tmax[1], &tmp[g].mm[1]);
    int cnt = 0, sum = 0; float tmax = -99, regen = 0;
    for (int k = 0; k < 2; k++) {
      const int tag = tagWe[k];
      if (tag < 0 || tag > 6) continue;
      int sc = -1; float tx = 0, mm = 0;
      tagBewerten(*p, tag, &sc, &tx, &mm);
      if (sc >= 0) { sum += sc; cnt++; if (tx > tmax) tmax = tx; regen += mm; }
    }
    if (cnt) { tmp[g].score[2] = sum / cnt; tmp[g].tmax[2] = tmax; tmp[g].mm[2] = regen / cnt; } else tmp[g].score[2] = -1;
  }
  heap_caps_free(p);
  {
    LvLock lock;
    for (int g = 0; g < gebieteAnzahl; g++) gebiete[g] = tmp[g];
    gebieteStand = jetzt;
  }
  return true;
}

int gebieteRang(int tag, int* reihe, int max) {
  int n = 0;
  for (int i = 0; i < gebieteAnzahl && n < max; i++) if (gebiete[i].score[tag] >= 0) reihe[n++] = i;
  for (int a = 1; a < n; a++) for (int b = a; b > 0 && gebiete[reihe[b]].score[tag] > gebiete[reihe[b - 1]].score[tag]; b--) { int t = reihe[b]; reihe[b] = reihe[b - 1]; reihe[b - 1] = t; }
  return n;
}

static struct { bool neu; String zeilen; } GA;
static volatile bool s_laeuft = false;

static void gebieteTask(void*) {
  bool ok = true;
  if (GA.neu) { snprintf(gebieteText, sizeof(gebieteText), "Speichere Liste ..."); ok = gebieteTextSpeichern(GA.zeilen); }
  if (ok) { snprintf(gebieteText, sizeof(gebieteText), "Hole Wetter für %d Gebiete ...", gebieteAnzahl); ok = wetterHolen(); }
  if (ok) snprintf(gebieteText, sizeof(gebieteText), "%d Gebiete bewertet", gebieteAnzahl);
  gebieteStatus = ok ? 2 : 3;
  s_laeuft = false;
  vTaskDelete(nullptr);
}

bool gebieteAuftrag(bool neueListe, const String& zeilen) {
  if (s_laeuft) return false;
  s_laeuft = true; gebieteStatus = 1;
  GA.neu = neueListe; GA.zeilen = zeilen;
  if (gebieteAnzahl == 0) gebieteListeLaden();
  if (xTaskCreatePinnedToCore(gebieteTask, "gebiete", 16 * 1024, nullptr, 1, nullptr, 0) != pdPASS) { s_laeuft = false; gebieteStatus = 3; return false; }
  return true;
}
