// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
#include "app_state.h"
#include "../storage/storage.h"
#include <ArduinoJson.h>

static Preferences prefs;

void ladeCfg() {
  prefs.begin(PREFS_NAMESPACE, true);
  cfg.ssid             = prefs.getString("ssid",       "");
  cfg.pass             = prefs.getString("pass",       "");
  cfg.location         = prefs.getString("location",   "");
  cfg.lat              = prefs.getFloat ("lat",         0.0f);
  cfg.lon              = prefs.getFloat ("lon",         0.0f);
  cfg.dwd_region       = prefs.getInt   ("dwd_region",  31);
  cfg.location2        = prefs.getString("location2",   "");
  cfg.lat2             = prefs.getFloat ("lat2",        0.0f);
  cfg.lon2             = prefs.getFloat ("lon2",        0.0f);
  cfg.dwd_region2      = prefs.getInt   ("dwd_region2", 31);
  cfg.hinweis_regen    = prefs.getBool  ("hw_regen",    true);
  cfg.hinweis_frost    = prefs.getBool  ("hw_frost",    true);
  cfg.verlauf_regen    = prefs.getBool  ("vl_regen",    true);
  cfg.hinweis_uv       = prefs.getBool  ("hw_uv",       true);
  cfg.night_uhr        = prefs.getBool  ("night_uhr",   false);
  cfg.rahmen_an        = prefs.getBool  ("rahmen",      false);
  cfg.rahmen_min       = constrain(prefs.getInt("rahmen_min", 5), 1, 60);
  cfg.rahmen_sek       = constrain(prefs.getInt("rahmen_sek", 30), 10, 600);
  cfg.rahmen_nacht     = prefs.getBool  ("rahmen_nacht", false);
  cfg.wetter_modul1    = constrain(prefs.getInt("wm1", 1), 0, 6);
  cfg.wetter_modul2    = constrain(prefs.getInt("wm2", 2), 0, 6);
  cfg.update_modus     = constrain(prefs.getInt("upd_modus", 2), 0, 2);
  { int h = prefs.getInt("upd_std", 6); cfg.update_intervall_h = (h == 1 || h == 24) ? h : 6; }
  cfg.dim_timeout      = prefs.getInt   ("dim_time",    3);
  cfg.brightness       = prefs.getInt   ("bright",      80);
  cfg.warn_region      = prefs.getBool  ("warn_reg",    true);
  cfg.night_modus      = prefs.getInt   ("night_modus", prefs.getBool("night_mode", false) ? 1 : 0);   // aelterer Schalter: an = feste Uhrzeiten
  cfg.night_sonne_ab   = prefs.getInt   ("night_s_ab",  30);
  cfg.night_sonne_bis  = prefs.getInt   ("night_s_bis", 0);
  cfg.night_theme      = prefs.getBool  ("night_theme", false);
  cfg.ntfy_aktiv       = prefs.getBool  ("ntfy_on",     false);
  cfg.ntfy_server      = prefs.getString("ntfy_srv",    "https://ntfy.sh");
  cfg.ntfy_topic       = prefs.getString("ntfy_top",    "");
  cfg.ntfy_token       = prefs.getString("ntfy_tok",    "");
  cfg.web_pw           = prefs.getString("web_pw",      "");
  cfg.ntfy_quellen     = prefs.getInt   ("ntfy_src",    7);
  cfg.ntfy_entwarnung  = prefs.getBool  ("ntfy_ent",    true);
  cfg.saison_jahr      = prefs.getInt   ("sais_j",      0);
  cfg.ntfy_regen_t     = (uint32_t)prefs.getInt("ntfy_rgt", 0);
  for (int i = 0; i < 4; i++) {
    char k[8];
    snprintf(k, sizeof(k), "tm%dn", i); cfg.termin_name[i]  = prefs.getString(k, "");
    snprintf(k, sizeof(k), "tm%dd", i); cfg.termin_datum[i] = prefs.getString(k, "");
    snprintf(k, sizeof(k), "tm%ds", i); cfg.termin_stufe[i] = (uint8_t)prefs.getInt(k, 0);
  }
  cfg.night_from       = prefs.getInt   ("night_from",  22 * 60);
  cfg.night_to         = prefs.getInt   ("night_to",     6 * 60);
  cfg.night_brightness = prefs.getInt   ("night_bright", 10);
  cfg.ruhe_modus       = prefs.getInt   ("ruhe_modus",  0);
  cfg.ruhe_von         = prefs.getInt   ("ruhe_von",    22 * 60);
  cfg.ruhe_bis         = prefs.getInt   ("ruhe_bis",     7 * 60);
  cfg.warn_ton_an      = prefs.getBool  ("warn_ton_an", true);
  cfg.warn_ton         = constrain(prefs.getInt("warn_ton", 1), 0, 4);
  cfg.warn_laut        = constrain(prefs.getInt("warn_laut", 50), 0, 100);
  prefs.end();
}

void speichereCfg() {
  speicherSicherungAnfordern();   // Sicherung auf der Speicherkarte nachziehen (falls eine steckt)
  prefs.begin(PREFS_NAMESPACE, false);
  prefs.putString("ssid",        cfg.ssid);
  prefs.putString("pass",        cfg.pass);
  prefs.putString("location",    cfg.location);
  prefs.putFloat ("lat",         cfg.lat);
  prefs.putFloat ("lon",         cfg.lon);
  prefs.putInt   ("dwd_region",  cfg.dwd_region);
  prefs.putString("location2",   cfg.location2);
  prefs.putFloat ("lat2",        cfg.lat2);
  prefs.putFloat ("lon2",        cfg.lon2);
  prefs.putInt   ("dwd_region2", cfg.dwd_region2);
  prefs.putBool  ("hw_regen",    cfg.hinweis_regen);
  prefs.putBool  ("hw_frost",    cfg.hinweis_frost);
  prefs.putBool  ("vl_regen",    cfg.verlauf_regen);
  prefs.putBool  ("hw_uv",       cfg.hinweis_uv);
  prefs.putBool  ("night_uhr",   cfg.night_uhr);
  prefs.putBool  ("rahmen",      cfg.rahmen_an);
  prefs.putInt   ("rahmen_min",  cfg.rahmen_min);
  prefs.putInt   ("rahmen_sek",  cfg.rahmen_sek);
  prefs.putBool  ("rahmen_nacht", cfg.rahmen_nacht);
  prefs.putInt   ("wm1",         cfg.wetter_modul1);
  prefs.putInt   ("wm2",         cfg.wetter_modul2);
  prefs.putInt   ("upd_modus",   cfg.update_modus);
  prefs.putInt   ("upd_std",     cfg.update_intervall_h);
  prefs.putInt   ("dim_time",    cfg.dim_timeout);
  prefs.putInt   ("bright",      cfg.brightness);
  prefs.putBool  ("warn_reg",    cfg.warn_region);
  prefs.putInt   ("night_modus", cfg.night_modus);
  prefs.putInt   ("night_s_ab",  cfg.night_sonne_ab);
  prefs.putInt   ("night_s_bis", cfg.night_sonne_bis);
  prefs.putBool  ("night_theme", cfg.night_theme);
  prefs.putBool  ("ntfy_on",     cfg.ntfy_aktiv);
  prefs.putString("ntfy_srv",    cfg.ntfy_server);
  prefs.putString("ntfy_top",    cfg.ntfy_topic);
  prefs.putString("ntfy_tok",    cfg.ntfy_token);
  prefs.putString("web_pw",      cfg.web_pw);
  prefs.putInt   ("ntfy_src",    cfg.ntfy_quellen);
  prefs.putBool  ("ntfy_ent",    cfg.ntfy_entwarnung);
  prefs.putInt   ("sais_j",      cfg.saison_jahr);
  prefs.putInt   ("ntfy_rgt",    (int)cfg.ntfy_regen_t);
  for (int i = 0; i < 4; i++) {
    char k[8];
    snprintf(k, sizeof(k), "tm%dn", i); prefs.putString(k, cfg.termin_name[i]);
    snprintf(k, sizeof(k), "tm%dd", i); prefs.putString(k, cfg.termin_datum[i]);
    snprintf(k, sizeof(k), "tm%ds", i); prefs.putInt(k, cfg.termin_stufe[i]);
  }
  prefs.putInt   ("night_from",  cfg.night_from);
  prefs.putInt   ("night_to",    cfg.night_to);
  prefs.putInt   ("night_bright",cfg.night_brightness);
  prefs.putInt   ("ruhe_modus",  cfg.ruhe_modus);
  prefs.putInt   ("ruhe_von",    cfg.ruhe_von);
  prefs.putInt   ("ruhe_bis",    cfg.ruhe_bis);
  prefs.putBool  ("warn_ton_an", cfg.warn_ton_an);
  prefs.putInt   ("warn_ton",    cfg.warn_ton);
  prefs.putInt   ("warn_laut",   cfg.warn_laut);
  prefs.end();
}

// Stadtname URL-tauglich machen (Umlaute und Leerzeichen)
String urlKodieren(const String& s) {
  String out;
  const char* hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
  }
  return out;
}

bool geocodeLocation(const String& city) { return geocodeOrt(city, cfg.lat, cfg.lon); }

bool geocodeOrt(const String& city, float& lat, float& lon) {
  WiFiClientSecure sc;
  HTTPClient http;
  String url = "https://geocoding-api.open-meteo.com/v1/search?name=" + urlKodieren(city) +
               "&count=1&language=de&format=json";
  logDiag("[Geo] Suche '%s'", city.c_str());
  http.begin(sc, url);
  bool ok = false;
  int code = http.GET();
  if (code != 200) {
    logHttpFehler("Geo", code, sc);
    logDiag("[Geo] Fehlgeschlagen fuer '%s': HTTP %d", city.c_str(), code);
  } else {
    JsonDocument doc;
    if (deserializeJson(doc, http.getString()) != DeserializationError::Ok) {
      logDiag("[Geo] Fehlgeschlagen fuer '%s': JSON-Fehler", city.c_str());
    } else if (doc["results"].size() == 0) {
      logDiag("[Geo] Fehlgeschlagen fuer '%s': kein Treffer", city.c_str());
    } else {
      lat = doc["results"][0]["latitude"].as<float>();
      lon = doc["results"][0]["longitude"].as<float>();
      logDiag("[Geo] Gefunden: %.4f, %.4f", lat, lon);
      ok = true;
    }
  }
  http.end();
  return ok;
}
