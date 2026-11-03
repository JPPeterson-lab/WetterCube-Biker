// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Open-Meteo: 7 Tage stuendlich fuer den Biker-Score (Heimatort)
#include "app_state.h"
#include "biker.h"
#include "esp_heap_caps.h"
#include <ArduinoJson.h>
#include <new>

BikerPunkt* bikerHeim = nullptr;

static time_t zeitAusIso(const char* s)   // "2026-11-01T09:00" oder "2026-11-01" in lokaler Zeit
{
  int y = 0, mo = 0, d = 0, h = 0, mi = 0;
  if (!s || sscanf(s, "%d-%d-%dT%d:%d", &y, &mo, &d, &h, &mi) < 3) return 0;
  struct tm t = {};
  t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d; t.tm_hour = h; t.tm_min = mi; t.tm_isdst = -1;
  return mktime(&t);
}

static float zahl(JsonArrayConst a, size_t i, float vorgabe) { if (i >= a.size() || a[i].isNull()) return vorgabe; return a[i].as<float>(); }

// Wertet die Antwort fuer EINEN Ort aus (Objekt mit "hourly" und "daily")
bool punktAuswerten(JsonVariantConst o, BikerPunkt* p)
{
  JsonObjectConst hr = o["hourly"];
  JsonArrayConst zt = hr["time"];
  if (zt.isNull() || zt.size() == 0) return false;
  JsonArrayConst temp = hr["temperature_2m"], feels = hr["apparent_temperature"], mm = hr["precipitation"], pop = hr["precipitation_probability"],
                 wmo = hr["weather_code"], wind = hr["wind_speed_10m"], gust = hr["wind_gusts_10m"], wdir = hr["wind_direction_10m"],
                 vis = hr["visibility"], hum = hr["relative_humidity_2m"], cloud = hr["cloud_cover"], tag = hr["is_day"], uv = hr["uv_index"], druck = hr["pressure_msl"];
  int n = 0;
  for (size_t i = 0; i < zt.size() && n < BK_STUNDEN; i++) {
    BikerStunde& h = p->h[n];
    h.t     = zeitAusIso(zt[i] | "");
    h.temp  = zahl(temp, i, 0);
    h.feels = zahl(feels, i, h.temp);
    h.mm    = zahl(mm, i, 0);
    h.pop   = (uint8_t)zahl(pop, i, 0);
    h.wmo   = (uint8_t)zahl(wmo, i, 0);
    h.wind  = zahl(wind, i, 0);
    h.gust  = zahl(gust, i, h.wind);
    h.wdir  = (int16_t)zahl(wdir, i, 0);
    h.vis   = zahl(vis, i, 20000) / 1000.0f;
    h.hum   = (uint8_t)zahl(hum, i, 50);
    h.cloud = (uint8_t)zahl(cloud, i, 50);
    h.tag   = zahl(tag, i, 1) != 0;
    h.uv    = zahl(uv, i, 0);
    h.druck = zahl(druck, i, 0);
    n++;
  }
  p->n = n;
  p->hoehe = o["elevation"] | 0.0f;
  JsonArrayConst sr = o["daily"]["sunrise"], ss = o["daily"]["sunset"];
  for (int d = 0; d < 7; d++) {
    p->aufgang[d]   = d < (int)sr.size() ? zeitAusIso(sr[d] | "") : 0;
    p->untergang[d] = d < (int)ss.size() ? zeitAusIso(ss[d] | "") : 0;
  }
  // Regen-Nowcast (Zeit in lokaler Zeit, TZ ist gesetzt)
  p->nc_n = 0;
  JsonArrayConst ncT = o["minutely_15"]["time"], ncP = o["minutely_15"]["precipitation"];
  for (size_t i = 0; i < ncT.size() && i < ncP.size() && p->nc_n < BikerPunkt::NC_MAX; i++) {
    p->nc_start[p->nc_n] = zeitAusIso(ncT[i] | "");
    p->nc_mm[p->nc_n] = ncP[i].isNull() ? 0.0f : ncP[i].as<float>();
    p->nc_n++;
  }
  p->geladen = n > 0;
  return p->geladen;
}


// Bruecke zu den uebernommenen Anzeigen (Temperaturverlauf, Nacht-Uhr, Windpfeil der Warnkarte): die Struktur `wetter` aus den Stundenwerten fuellen.
// Aufrufer haelt den LVGL-Lock.
static void wetterAusStunden(const BikerPunkt& p)
{
  const int idx = bikerIndexJetzt(p, time(nullptr));
  if (idx < 0) return;
  const BikerStunde& h = p.h[idx];
  wetter.temp = h.temp; wetter.feels_like = h.feels; wetter.humidity = h.hum;
  wetter.wind_speed = h.wind; wetter.wind_dir = h.wdir; wetter.wmo_code = h.wmo; wetter.is_day = h.tag;
  const int tag0 = idx / 24 * 24;
  float tmin = 99, tmax = -99;
  for (int i = tag0; i < tag0 + 24 && i < p.n; i++) { if (p.h[i].temp < tmin) tmin = p.h[i].temp; if (p.h[i].temp > tmax) tmax = p.h[i].temp; }
  wetter.temp_min = tmin; wetter.temp_max = tmax;
  char b[8]; struct tm t;
  if (p.aufgang[idx / 24])   { localtime_r(&p.aufgang[idx / 24], &t);   snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min); wetter.sunrise = b; }
  if (p.untergang[idx / 24]) { localtime_r(&p.untergang[idx / 24], &t); snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min); wetter.sunset = b; }
  int n = 0;
  for (int i = 0; i <= 24 && idx + i < p.n; i++) { wetter.vl_temp[n] = p.h[idx + i].temp; wetter.vl_regen[n] = p.h[idx + i].mm; n++; }
  wetter.vl_n = n;
  struct tm th; localtime_r(&h.t, &th); wetter.vl_h0 = th.tm_hour;
  wetter.geladen = true;
}

bool fetchStunden(float lat, float lon, BikerPunkt* out)
{
  spur("Stunden: Abruf");
  if (WiFi.status() != WL_CONNECTED || lat == 0.0f || !out) return false;
  WiFiClientSecure sc;
  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast";
  url += "?latitude=" + String(lat, 4) + "&longitude=" + String(lon, 4);
  url += "&hourly=temperature_2m,apparent_temperature,precipitation,precipitation_probability,weather_code,wind_speed_10m,wind_gusts_10m,"
         "wind_direction_10m,visibility,relative_humidity_2m,cloud_cover,is_day,uv_index,pressure_msl";
  url += "&minutely_15=precipitation&forecast_minutely_15=12";   // Regen-Nowcast: naechste 3 Stunden
  url += "&daily=sunrise,sunset&timezone=auto&forecast_days=7";
  http.begin(sc, url);
  int code = http.GET();
  if (code != 200) {
    logHttpFehler("Stunden", code, sc);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  body = String();
  if (err != DeserializationError::Ok) { logDiag("[Stunden] JSON-Fehler: %s (frei %u)", err.c_str(), (unsigned)ESP.getFreeHeap()); return false; }

  LvLock lock;   // Daten werden von der UI gelesen
  return punktAuswerten(doc.as<JsonVariantConst>(), out);
}

bool fetchStundenHeim()
{
  if (!bikerHeim) {
    bikerHeim = (BikerPunkt*)heap_caps_calloc(1, sizeof(BikerPunkt), MALLOC_CAP_SPIRAM);
    if (!bikerHeim) { logDiag("[Stunden] kein PSRAM fuer %u Bytes", (unsigned)sizeof(BikerPunkt)); return false; }
    new (bikerHeim) BikerPunkt();   // Standardwerte der Struktur setzen (calloc allein setzt vis auf 0)
  }
  bool ok = fetchStunden(stdLat(), stdLon(), bikerHeim);
  if (ok) { LvLock lock; wetterAusStunden(*bikerHeim); abrufOk(AB_WETTER); letzterWetterFetchErfolg = millis(); }
  return ok;
}
