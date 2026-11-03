// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Ortssuche ueber Photon (komoot, Daten von OpenStreetMap): findet Strassen, Hausnummern und Orte, auch halb eingegeben (Vorschlaege beim Tippen).
// Nutzung nach den Regeln des Dienstes: eigener User-Agent, kein Massenabruf (hier hoechstens ein Abruf je Tipppause).
#include "route.h"
#include "app_state.h"
#include <ArduinoJson.h>

static void anhaengen(String& s, const char* teil, const char* trenner) {
  if (!teil || !teil[0]) return;
  if (s.length()) s += trenner;
  s += teil;
}

int photonSuche(const String& anfrage, OrtVorschlag* out, int max) {
  if (WiFi.status() != WL_CONNECTED || anfrage.length() < 2 || !out || max < 1) return 0;
  String url = "https://photon.komoot.io/api/?q=" + urlKodieren(anfrage) + "&limit=" + String(max) + "&lang=de";
  if (stdLat() != 0.0f) url += "&lat=" + String(stdLat(), 3) + "&lon=" + String(stdLon(), 3);   // Treffer in der Naehe bevorzugen
  WiFiClientSecure sc; HTTPClient http;
  http.nurTls12();   // der Dienst beherrscht nur TLS 1.2; mit dem TLS-1.3-Angebot des Geraets bricht er den Handshake ab
  http.begin(sc, url);
  http.setTimeout(10000);
  http.addHeader("User-Agent", String("WetterCube-Biker/") + FIRMWARE_VERSION + " (info@wettercube.de)");
  int code = http.GET();
  if (code != 200) { logHttpFehler("Photon", code, sc); http.end(); return 0; }
  String body = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) return 0;
  int n = 0;
  for (JsonObjectConst f : doc["features"].as<JsonArrayConst>()) {
    if (n >= max) break;
    JsonObjectConst p = f["properties"];
    JsonArrayConst c = f["geometry"]["coordinates"];
    if (c.size() < 2) continue;
    const char* name = p["name"] | ""; const char* strasse = p["street"] | ""; const char* nr = p["housenumber"] | "";
    const char* plz = p["postcode"] | ""; const char* ort = p["city"] | "";
    if (!ort[0]) ort = p["town"] | "";
    if (!ort[0]) ort = p["village"] | "";
    if (!ort[0]) ort = p["county"] | "";
    if (!ort[0]) ort = p["state"] | "";
    String l;
    if (strasse[0]) {
      String st = strasse; if (nr[0]) { st += " "; st += nr; }
      if (name[0] && strcmp(name, strasse) != 0) { l = name; l += ", "; }
      l += st;
    } else anhaengen(l, name, ", ");
    String o; if (plz[0]) { o = plz; if (ort[0]) { o += " "; o += ort; } } else o = ort;
    anhaengen(l, o.c_str(), ", ");
    if (l.isEmpty()) continue;
    bool doppelt = false;
    for (int i = 0; i < n; i++) if (l == out[i].label) doppelt = true;
    if (doppelt) continue;
    strlcpy(out[n].label, l.c_str(), sizeof(out[n].label));
    out[n].lon = c[0].as<float>(); out[n].lat = c[1].as<float>();
    n++;
  }
  return n;
}

bool geocodeAdresse(const String& text, float& lat, float& lon) {
  OrtVorschlag v[1];
  String q = text; q.trim();
  // Erst der volle Text, dann jeweils ein Wort weniger vom Ende ("Eutin Marktplatz" -> "Eutin"), solange noch Treffer fehlen
  for (int runde = 0; runde < 4 && q.length() >= 2; runde++) {
    if (photonSuche(q, v, 1) > 0) { lat = v[0].lat; lon = v[0].lon; logDiag("[Geo] Photon: '%s' -> %s", q.c_str(), v[0].label); return true; }
    int sp = q.lastIndexOf(' ');
    if (sp < 2) break;
    q = q.substring(0, sp); q.trim();
  }
  return geocodeOrt(text, lat, lon);
}
