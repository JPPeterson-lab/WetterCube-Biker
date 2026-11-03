// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// WLAN-Einrichtungsportal (Captive Portal) und Web-Konfiguration unter wettercube-biker.local
#include "app_state.h"
#include "bilderrahmen.h"
#include "../platform/WebServer.h"
#include "../platform/DnsPortal.h"
#include "mdns.h"
#include <ArduinoJson.h>
#include "webui_html.h"
#include "navigation.h"
#include "../storage/storage.h"
#include <nvs_flash.h>
#include "ntfy.h"
#include "../board/board.h"
#include "esp_random.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"

void routenWebuiRegistrieren(WebServer& server);   // webui_route.cpp
static void handleApiLog();   // unten definiert, auch im Einrichtungsmodus erreichbar

static WebServer server(80);

static String baueDwdRegionenOptions(int ausgewaehlt) {
  String html;
  for (int i = 0; DWD_REGIONS[i].name != nullptr; i++) {
    html += "<option value=\"" + String(DWD_REGIONS[i].id) + "\"";
    if (DWD_REGIONS[i].id == ausgewaehlt) html += " selected";
    html += ">" + String(DWD_REGIONS[i].name) + "</option>\n";
  }
  return html;
}

// ── Captive Portal (WLAN-Ersteinrichtung) ────────────────────────────────────
static void handlePortalRoot() {
  logDiag("[Portal] GET /");
  int n = WiFi.scanNetworks();
  String nets;
  for (int i = 0; i < n; i++)
    nets += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + " dBm)</option>\n";
  logDiag("[Portal] %d Netzwerke gefunden", n);
  String html = FPSTR(PORTAL_HTML);
  html.replace("%NETZWERKE%", nets);
  html.replace("%LOCATION%", cfg.location);
  html.replace("%DWD_REGIONEN%", baueDwdRegionenOptions(cfg.dwd_region));
  server.send(200, "text/html", html);
}

static void handlePortalSave() {
  if (server.hasArg("ssid"))       cfg.ssid       = server.arg("ssid");
  if (server.hasArg("pass"))       cfg.pass       = server.arg("pass");
  if (server.hasArg("location"))   cfg.location   = server.arg("location");
  if (server.hasArg("dwd_region")) cfg.dwd_region = server.arg("dwd_region").toInt();
  cfg.lat = 0.0f; cfg.lon = 0.0f;   // erzwingt Geocoding beim nächsten Start
  speichereCfg();
  server.send(200, "text/html", FPSTR(PORTAL_OK_HTML));
  delay(2000);
  ESP.restart();
}

// Passwort des Einrichtungs-WLANs: 8 Ziffern (WPA2 verlangt mindestens 8 Zeichen), bei jedem Einrichtungsmodus neu, steht am Display
char portalPasswort[12] = "";
void portalPasswortNeu() { snprintf(portalPasswort, sizeof(portalPasswort), "%08u", (unsigned)(esp_random() % 100000000UL)); }

void startePortal() {
  portal_modus = true;
  WiFi.disconnect(true); delay(100);
  WiFi.setAutoReconnect(false);   // kein Hintergrund-Scan, der das Portal-WLAN stört
  if (!portalPasswort[0]) portalPasswortNeu();
  if (!WiFi.mode(WIFI_AP) || !WiFi.softAP(PORTAL_SSID, portalPasswort)) {
    logDiag("[Portal] WLAN-Treiber startet nicht (freier SRAM %u, groesster Block %u)",
            (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    uiHinweis("WLAN nicht verfügbar", "Der WLAN-Treiber konnte nicht gestartet werden\n(zu wenig freier Arbeitsspeicher).\n\nBitte das Gerät neu starten.");
    return;
  }
  dnsPortalStart(WiFi.softAPIP().raw());   // alle Namen auf das Gerät umleiten (Captive Portal)
  server.on("/",          HTTP_GET,  handlePortalRoot);
  server.on("/speichern", HTTP_POST, handlePortalSave);
  server.on("/log",       HTTP_GET,  handleApiLog);   // Diagnose auch im Einrichtungsmodus: http://192.168.4.1/log
  // Handy-Betriebssysteme prüfen per HTTP, ob "Internet" da ist; mit einer Weiterleitung auf eine
  // absolute Adresse öffnen sie das Portal-Fenster
  server.onNotFound([]() {
    logDiag("[Portal] Anfrage %s%s", server.hostHeader().c_str(), server.uri().c_str());
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
  });
  server.begin();
  logDiag("[Portal] AP gestartet: %s auf %s", PORTAL_SSID, WiFi.softAPIP().toString().c_str());
}

// ── Zugriffsschutz (Normalbetrieb) ───────────────────────────────────────────
// 1) Host-Pruefung (gegen DNS-Rebinding): Anfragen muessen an die IP-Adresse oder den Namen des Geraets gehen
// 2) Herkunftspruefung (gegen fremde Webseiten): schreibende Anfragen aus dem Browser nur von dieser Seite selbst
// 3) optionales Passwort (HTTP Basic, Benutzer admin); die Lese-Schnittstellen fuer Next, WetterAmpel und Home Assistant bleiben offen
static bool hostErlaubt(String h) {
  h.toLowerCase();
  if (h.isEmpty()) return true;
  if (h[0] == '[') return true;                               // IPv6-Adresse
  int dp = h.indexOf(':'); if (dp >= 0) h = h.substring(0, dp);
  bool nurZiffern = true;
  for (size_t i = 0; i < h.length(); i++) if (!isdigit((unsigned char)h[i]) && h[i] != '.') { nurZiffern = false; break; }
  if (nurZiffern) return true;                                // IP-Adresse
  return h.startsWith(MDNS_NAME) || h.endsWith(".local") || h == "localhost";
}

static String sha256Hex(const String& s) {
  unsigned char d[32];
  mbedtls_sha256_context c;
  mbedtls_sha256_init(&c); mbedtls_sha256_starts(&c, 0);
  mbedtls_sha256_update(&c, (const unsigned char*)s.c_str(), s.length());
  mbedtls_sha256_finish(&c, d); mbedtls_sha256_free(&c);
  char hex[65];
  for (int i = 0; i < 32; i++) snprintf(hex + 2 * i, 3, "%02x", d[i]);
  return String(hex);
}

static void webPwSetzen(const String& pw) {
  char salz[17]; snprintf(salz, sizeof(salz), "%08x%08x", (unsigned)esp_random(), (unsigned)esp_random());
  cfg.web_pw = String(salz) + ":" + sha256Hex(String(salz) + pw);
}

static bool webPwPruefen(const String& pw) {
  int d = cfg.web_pw.indexOf(':');
  if (d != 16) return false;
  String soll = cfg.web_pw.substring(d + 1), ist = sha256Hex(cfg.web_pw.substring(0, d) + pw);
  unsigned char diff = (unsigned char)(soll.length() ^ ist.length());
  for (size_t i = 0; i < soll.length() && i < ist.length(); i++) diff |= (unsigned char)(soll[i] ^ ist[i]);   // gleich lange Pruefung
  return diff == 0;
}

static bool anmeldungOk() {
  String a = server.header("Authorization");
  if (!a.startsWith("Basic ")) return false;
  String b64 = a.substring(6); b64.trim();
  unsigned char out[160]; size_t n = 0;
  if (mbedtls_base64_decode(out, sizeof(out) - 1, &n, (const unsigned char*)b64.c_str(), b64.length()) != 0) return false;
  out[n] = 0;
  String up = String((const char*)out);
  int c = up.indexOf(':');
  if (c < 0 || up.substring(0, c) != "admin") return false;
  return webPwPruefen(up.substring(c + 1));
}

// Wird vor jedem Handler aufgerufen; false = die Anfrage wurde schon abgelehnt
static bool zugriffErlaubt() {
  if (portal_modus) return true;                              // Einrichtungs-WLAN: eigenes Passwort, und das Portal braucht beliebige Namen
  String host = server.hostHeader();
  if (!hostErlaubt(host)) { server.send(403, "text/plain", "Zugriff abgelehnt: Das ist nicht die Adresse des Geraets."); return false; }
  if (!server.istGet()) {                                     // schreibende Anfragen: Browser melden ihre Herkunft
    String o = server.header("Origin");
    if (o.length() && !o.equalsIgnoreCase("http://" + host)) {
      logDiag("[Sicherheit] Anfrage von fremder Herkunft abgelehnt: %s", o.c_str());
      server.send(403, "text/plain", "Zugriff abgelehnt: Anfrage von einer fremden Seite.");
      return false;
    }
  }
  if (cfg.web_pw.isEmpty()) return true;
  String u = server.uri();
  if (u == "/api/next" || u == "/api/ampel" || u == "/api/status") return true;   // Lese-Schnittstellen fuer andere Geraete
  static unsigned long sperreBis = 0; static uint8_t fehl = 0;
  if (sperreBis && (long)(millis() - sperreBis) < 0) { server.send(429, "text/plain", "Zu viele falsche Anmeldungen. Bitte eine Minute warten."); return false; }
  if (anmeldungOk()) { fehl = 0; return true; }
  if (server.header("Authorization").length()) {              // Anmeldedaten waren falsch (nicht nur die erste Anfrage ohne Daten)
    vTaskDelay(pdMS_TO_TICKS(500));
    if (++fehl >= 8) { fehl = 0; sperreBis = millis() + 60000UL; logDiag("[Sicherheit] Zu viele falsche Anmeldungen, 60 s gesperrt"); }
  }
  server.sendHeader("WWW-Authenticate", "Basic realm=\"WetterCube Biker\", charset=\"UTF-8\"");
  server.send(401, "text/plain", "Anmeldung erforderlich (Benutzer: admin)");
  return false;
}

// Passwort der Web-Oberflaeche setzen oder entfernen (nur erreichbar, wenn man schon angemeldet ist)
static void handleWebPwSave() {
  if (server.hasArg("pw_weg")) {
    cfg.web_pw = ""; speichereCfg();
    logDiag("[Sicherheit] Web-Passwort entfernt");
  } else {
    String p1 = server.arg("pw1"), p2 = server.arg("pw2");
    if (p1.length() < 6 || p1.length() > 64) { server.send(400, "text/plain; charset=utf-8", "Das Passwort muss 6 bis 64 Zeichen lang sein."); return; }
    if (p1 != p2) { server.send(400, "text/plain; charset=utf-8", "Die beiden Passwoerter sind nicht gleich."); return; }
    webPwSetzen(p1); speichereCfg();
    logDiag("[Sicherheit] Web-Passwort gesetzt");
  }
  server.sendHeader("Location", "/#system"); server.send(302);
}

// ── Web-Konfiguration (Normalbetrieb) ────────────────────────────────────────
static String htmlEsc(const String& t) {
  String o;
  for (size_t i = 0; i < t.length(); i++) { char c = t[i]; if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;"; else o += c; }
  return o;
}

static String updateInfoText();
static String standortStatusText();

static void handleWebRoot() {
  if (server.hostHeader().startsWith("wettercube-next")) { server.sendHeader("Location", "/next"); server.send(302); return; }   // Name der Next-Verwaltung
  String html = FPSTR(WEBUI_HTML);
  html.replace("%VERSION%",      FIRMWARE_VERSION);
#if UPDATE_DISPLAY_FLACKERT
  html.replace("%UPDATE_HINWEIS%", "Waehrend des Updates flackert das Display wild. Das ist normal.");
  html.replace("%UPDATE_CONFIRM%", "Das Display flackert waehrend des Updates. Das ist normal.");
  html.replace("%UPDATE_LAEUFT%",  "Display flackert, das ist normal.");
#else
  html.replace("%UPDATE_HINWEIS%", "Das Update dauert bis zu 5 Minuten, das Display zeigt dabei einen Fortschrittsbalken.");
  html.replace("%UPDATE_CONFIRM%", "Das Display zeigt waehrend des Updates einen Fortschrittsbalken.");
  html.replace("%UPDATE_LAEUFT%",  "Fortschritt auf dem Display.");
#endif
  html.replace("%IP%",           WiFi.localIP().toString());
  html.replace("%LOCATION%",     cfg.location);
  html.replace("%HR%",           cfg.hinweis_regen ? "checked" : "");
  html.replace("%HF%",           cfg.hinweis_frost ? "checked" : "");
  html.replace("%HU%",           cfg.hinweis_uv    ? "checked" : "");
  html.replace("%NEN%",          cfg.ntfy_entwarnung ? "checked" : "");
  html.replace("%NQT%",          (cfg.ntfy_quellen & NTFY_Q_TAG) ? "checked" : "");
  html.replace("%NQR%",          (cfg.ntfy_quellen & NTFY_Q_REGEN) ? "checked" : "");
  html.replace("%NQW%",          (cfg.ntfy_quellen & NTFY_Q_WARNUNG) ? "checked" : "");
  html.replace("%DWD_REGIONEN%", baueDwdRegionenOptions(cfg.dwd_region));
#if !FUSION_ZWEITER_STANDORT
  { int a = html.indexOf("<!--ST2-->"), b = html.indexOf("<!--/ST2-->"); if (a >= 0 && b > a) html.remove(a, b + 11 - a); }   // Karte "Zweiter Standort" nur wo die Funktion eingeschaltet ist
#endif
  html.replace("%LOCATION2%",    cfg.location2);
  html.replace("%DWD_REGIONEN2%", baueDwdRegionenOptions(cfg.dwd_region2));
  { String st = standortStatusText(); st.replace("&", "&amp;"); st.replace("<", "&lt;"); html.replace("%STANDORT_STATUS%", st); }
  html.replace("%WR%",           cfg.warn_region ? "checked" : "");
  html.replace("%VR%",           cfg.verlauf_regen ? "checked" : "");
  html.replace("%SD%",           speicherStatusText());
  { String nv; bool nb = notfallBereit(&nv);
    html.replace("%NOTFALL%", nb ? "Notfall-App Version " + nv + " ist installiert" : String("Notfall-App ist nicht installiert (nur per USB nachrüstbar)"));
    html.replace("%NOTFALL_KNOPF%", nb ? "" : "disabled");
    String sk = speicherSicherungLesen();
    html.replace("%SICHERUNG_KNOPF%", sk.isEmpty() ? "disabled" : "");
    html.replace("%SICHERUNG%", sk.isEmpty() ? String("Auf der Karte liegt keine Sicherung") : String("Sicherung auf der Karte vorhanden (ohne Passw&ouml;rter)")); }
  html.replace("%UST1%", cfg.update_intervall_h == 1  ? "selected" : "");
  html.replace("%UST6%", cfg.update_intervall_h == 6  ? "selected" : "");
  html.replace("%UST24%", cfg.update_intervall_h == 24 ? "selected" : "");
  html.replace("%NA%",           cfg.ntfy_aktiv ? "checked" : "");
  html.replace("%NSRV%",         htmlEsc(cfg.ntfy_server));
  html.replace("%NTOP%",         htmlEsc(cfg.ntfy_topic));
  html.replace("%NTOKPH%",       cfg.ntfy_token.length() ? "(gespeichert)" : "nur bei geschützten Topics");
  html.replace("%BRIGHT%",       String(cfg.brightness));
  html.replace("%DTT%", (cfg.dim_timeout == -1) ? "selected" : "");
  for (int i = 0; i <= 10; i++) html.replace("%DT" + String(i) + "%", (cfg.dim_timeout == i) ? "selected" : "");
  for (int i = 0; i < 3; i++) html.replace("%NMO" + String(i) + "%", cfg.night_modus == i ? "selected" : "");
  html.replace("%NSA%", String(cfg.night_sonne_ab));
  html.replace("%NSB%", String(cfg.night_sonne_bis));
  html.replace("%NT%",  cfg.night_theme ? "checked" : "");
  html.replace("%NU%",  cfg.night_uhr ? "checked" : "");
  html.replace("%RAH%", cfg.rahmen_an ? "checked" : "");
  html.replace("%RAN%", cfg.rahmen_nacht ? "checked" : "");
  for (int m : {1, 3, 5, 10, 15, 30, 60}) html.replace("%RMN" + String(m) + "%", cfg.rahmen_min == m ? "selected" : "");
  for (int m : {10, 30, 60, 120, 300}) html.replace("%RSK" + String(m) + "%", cfg.rahmen_sek == m ? "selected" : "");
  for (int i = 0; i < 7; i++) { html.replace("%WA" + String(i) + "%", cfg.wetter_modul1 == i ? "selected" : ""); html.replace("%WB" + String(i) + "%", cfg.wetter_modul2 == i ? "selected" : ""); }
  html.replace("%UMS0%", cfg.update_modus == 0 ? "checked" : "");
  html.replace("%UMS1%", cfg.update_modus == 1 ? "checked" : "");
  html.replace("%UMS2%", cfg.update_modus == 2 ? "checked" : "");
  html.replace("%UMHIDE%", cfg.update_modus == 0 ? "style=\"display:none\"" : "");
  html.replace("%UINFO%", updateInfoText());
  html.replace("%WPSTATUS%", cfg.web_pw.isEmpty() ? "Es ist kein Passwort gesetzt: Jeder im Heimnetz kann die Einstellungen &auml;ndern." : "Ein Passwort ist gesetzt (Benutzername: <b>admin</b>).");
  {
    int von, bis; bool sonne = nachtGrenzen(&von, &bis);
    char b1[8], b2[8]; snprintf(b1, sizeof(b1), "%02d:%02d", von / 60, von % 60); snprintf(b2, sizeof(b2), "%02d:%02d", bis / 60, bis % 60);
    String info;
    if (cfg.night_modus == 0) info = "Der Nachtmodus ist aus: die Helligkeit &auml;ndert sich nur durch den Dimm-Timeout.";
    else if (cfg.night_modus == 2 && sonne) info = "Heute: Sonnenaufgang " + wetter.sunrise + ", Sonnenuntergang " + wetter.sunset + ". Die Nacht dauert von " + b1 + " bis " + b2 + " Uhr.";
    else if (cfg.night_modus == 2) info = "Die Sonnenzeiten sind noch nicht geladen, bis dahin gelten die festen Uhrzeiten (" + String(b1) + " bis " + b2 + " Uhr).";
    else info = String("Die Nacht dauert von ") + b1 + " bis " + b2 + " Uhr.";
    html.replace("%NIGHT_INFO%", info);
  }
  html.replace("%NBR%", String(cfg.night_brightness));
  for (int i = 0; i < 3; i++) html.replace("%RMO" + String(i) + "%", cfg.ruhe_modus == i ? "selected" : "");
  {
    String optVon, optBis;
    for (int m = 0; m < 24 * 60; m += 15) {
      char buf[8]; snprintf(buf, sizeof(buf), "%02d:%02d", m / 60, m % 60);
      optVon += "<option value='" + String(m) + "'" + (m == cfg.ruhe_von ? " selected" : "") + ">" + buf + "</option>";
      optBis += "<option value='" + String(m) + "'" + (m == cfg.ruhe_bis ? " selected" : "") + ">" + buf + "</option>";
    }
    html.replace("%RUHE_VON_OPTIONS%", optVon);
    html.replace("%RUHE_BIS_OPTIONS%", optBis);
    String ri;
    if (cfg.ruhe_modus == 0) ri = "Die Ruhezeit ist aus: Warnungen erscheinen jederzeit auf dem Display.";
    else {
      int v, b;
      if (cfg.ruhe_modus == 2) { nachtGrenzen(&v, &b); } else { v = cfg.ruhe_von; b = cfg.ruhe_bis; }
      char c1[8], c2[8]; snprintf(c1, sizeof(c1), "%02d:%02d", v / 60, v % 60); snprintf(c2, sizeof(c2), "%02d:%02d", b / 60, b % 60);
      ri = String("Ruhezeit von ") + c1 + " bis " + c2 + " Uhr";
      if (cfg.ruhe_modus == 2 && cfg.night_modus == 0) ri += ", gilt aber nicht, solange der Nachtmodus aus ist";
      ri += warnRuheAktiv() ? ". Gerade ist Ruhezeit." : ".";
    }
    html.replace("%RUHE_INFO%", ri);
  }
  {
    String optFrom, optTo;   // 96 Einträge je, 15-Minuten-Schritte
    for (int m = 0; m < 24 * 60; m += 15) {
      char buf[8]; snprintf(buf, sizeof(buf), "%02d:%02d", m / 60, m % 60);
      optFrom += "<option value='" + String(m) + "'" + (m == cfg.night_from ? " selected" : "") + ">" + buf + "</option>";
      optTo   += "<option value='" + String(m) + "'" + (m == cfg.night_to   ? " selected" : "") + ">" + buf + "</option>";
    }
    html.replace("%NIGHT_FROM_OPTIONS%", optFrom);
    html.replace("%NIGHT_TO_OPTIONS%",   optTo);
  }
  { String o; for (int i = 0; i < BOARD_AUDIO_TOENE; i++) o += "<option value='" + String(i) + "'" + (i == cfg.warn_ton ? " selected" : "") + ">" + BOARD_AUDIO_TONNAMEN[i] + "</option>";
    html.replace("%WARN_TON_OPTIONS%", o); html.replace("%WARN_LAUT%", String(cfg.warn_laut)); html.replace("%WTA%", cfg.warn_ton_an ? "checked" : ""); }
  if (!boardAudioVorhanden()) { html.replace("id=\"tw_test_zeile\"", "id=\"tw_test_zeile\" style=\"display:none\""); html.replace("id=\"tt_test_zeile\"", "id=\"tt_test_zeile\" style=\"display:none\""); html.replace("id=\"tt_kein\" style=\"display:none\"", "id=\"tt_kein\""); }
  server.send(200, "text/html", html);
}

static String standortStatusText() {
  String t = "Angezeigt wird: " + (stdName().length() ? stdName() : String("-")) + (standortAktiv == 1 && hatStandort2() ? " (Standort 2)" : " (Standort 1)");
  if (!cfg.location2.isEmpty() && cfg.lat2 == 0.0f) t += ". Der zweite Ort wird beim nächsten Datenabruf gesucht.";
  return t;
}

static void handleWebSave() {
  String prevLocation = cfg.location;
  int prevRegion = cfg.dwd_region;
  String prevLocation2 = cfg.location2;
  int prevRegion2 = cfg.dwd_region2;
  if (server.hasArg("location2"))    { cfg.location2 = server.arg("location2"); cfg.location2.trim(); }
  if (server.hasArg("dwd_region2"))  cfg.dwd_region2     = server.arg("dwd_region2").toInt();
  if (server.hasArg("location"))     cfg.location        = server.arg("location");
  if (server.hasArg("dwd_region"))   cfg.dwd_region      = server.arg("dwd_region").toInt();
  cfg.warn_region = server.hasArg("warn_region");
  cfg.hinweis_regen = server.hasArg("hw_regen");
  cfg.hinweis_frost = server.hasArg("hw_frost");
  cfg.verlauf_regen = server.hasArg("vl_regen");
  cfg.hinweis_uv    = server.hasArg("hw_uv");
  if (server.hasArg("pol_a_form")) {   // Marker: nur auswerten, wenn die Allergen-Schalter wirklich mitgesendet wurden
    int m = 0;
  }
  if (server.hasArg("night_modus"))     cfg.night_modus     = constrain(server.arg("night_modus").toInt(), 0, 2);
  if (server.hasArg("night_sonne_ab"))  cfg.night_sonne_ab  = constrain(server.arg("night_sonne_ab").toInt(), -180, 180);
  if (server.hasArg("night_sonne_bis")) cfg.night_sonne_bis = constrain(server.arg("night_sonne_bis").toInt(), -180, 180);
  if (server.hasArg("night_modus"))     cfg.night_theme     = server.hasArg("night_theme");   // Schalter gehoert zum Formular (Haken fehlt, wenn aus)
  if (server.hasArg("night_modus"))     cfg.night_uhr       = server.hasArg("night_uhr");
  if (server.hasArg("rahmen_form"))     { cfg.rahmen_an = server.hasArg("rahmen_an"); cfg.rahmen_nacht = server.hasArg("rahmen_nacht"); }
  if (server.hasArg("rahmen_min"))      cfg.rahmen_min      = constrain(server.arg("rahmen_min").toInt(), 1, 60);
  if (server.hasArg("rahmen_sek"))      cfg.rahmen_sek      = constrain(server.arg("rahmen_sek").toInt(), 10, 600);
  if (server.hasArg("wetter_modul1"))   cfg.wetter_modul1   = constrain(server.arg("wetter_modul1").toInt(), 0, 6);
  if (server.hasArg("wetter_modul2"))   cfg.wetter_modul2   = constrain(server.arg("wetter_modul2").toInt(), 0, 6);
  if (server.hasArg("night_from"))   cfg.night_from       = server.arg("night_from").toInt();
  if (server.hasArg("night_to"))     cfg.night_to         = server.arg("night_to").toInt();
  if (server.hasArg("night_bright")) cfg.night_brightness = server.arg("night_bright").toInt();
  if (server.hasArg("ruhe_modus"))   cfg.ruhe_modus       = constrain(server.arg("ruhe_modus").toInt(), 0, 2);
  if (server.hasArg("ruhe_von"))     cfg.ruhe_von         = constrain(server.arg("ruhe_von").toInt(), 0, 1439);
  if (server.hasArg("ruhe_bis"))     cfg.ruhe_bis         = constrain(server.arg("ruhe_bis").toInt(), 0, 1439);
  if (server.hasArg("brightness"))   cfg.brightness       = server.arg("brightness").toInt();
  if (server.hasArg("dim_time"))     cfg.dim_timeout      = server.arg("dim_time").toInt();
  if (server.hasArg("warn_ton"))     { cfg.warn_ton_an = server.hasArg("warn_ton_an"); cfg.warn_ton = constrain(server.arg("warn_ton").toInt(), 0, BOARD_AUDIO_TOENE - 1); }
  if (server.hasArg("warn_laut"))    cfg.warn_laut        = constrain(server.arg("warn_laut").toInt(), 0, 100);
  if (cfg.location != prevLocation) {   // Geocoding neu anstoßen
    cfg.lat = 0.0f; cfg.lon = 0.0f;
    logDiag("[Config] Standort geaendert: '%s' -> '%s'", prevLocation.c_str(), cfg.location.c_str());
  }
  if (cfg.location2 != prevLocation2) {
    cfg.lat2 = 0.0f; cfg.lon2 = 0.0f;   // Geocoding im Netzwerk-Task
    if (cfg.location2.isEmpty() && standortAktiv == 1) standortAktiv = 0;
  }
  speichereCfg();
  logDiag("[Config] gespeichert");
  setBrightness(cfg.brightness);
  // Neuer Ort, Region oder Biowetter-Zone → Daten sofort neu abrufen (macht der Netzwerk-Task)
  if (cfg.location2 != prevLocation2) datenNeuLaden = true;   // neuen zweiten Ort aufloesen
  aktualisiereStandort();
  aktualisiereUI();
  server.sendHeader("Location", "/"); server.send(302);
}

// ── Einstellungen sichern und wiederherstellen (ohne WLAN-Passwort) ──────────
static const char* SICHERUNG_TYP    = "WetterCube-Biker-Einstellungen";
static const char* SICHERUNG_TYP_CUBE = "WetterCubePlus-Einstellungen";      // Sicherungen des WetterCubePlus werden ebenfalls akzeptiert
static const char* SICHERUNG_TYP_XL   = "WetterCubePlus-XL-Einstellungen";   // ... und die der XL

String webuiSicherungJson() {   // Einstellungen als JSON (ohne WLAN- und MQTT-Passwort), für den Download und die Sicherung auf der Karte
  JsonDocument doc;
  doc["typ"]              = SICHERUNG_TYP;
  doc["firmware"]         = FIRMWARE_VERSION;
  doc["location"]         = cfg.location;
  doc["lat"]              = cfg.lat;
  doc["lon"]              = cfg.lon;
  doc["dwd_region"]       = cfg.dwd_region;
  doc["location2"]        = cfg.location2;
  doc["lat2"]             = cfg.lat2;
  doc["lon2"]             = cfg.lon2;
  doc["dwd_region2"]      = cfg.dwd_region2;
  doc["hinweis_regen"]    = cfg.hinweis_regen;
  doc["hinweis_frost"]    = cfg.hinweis_frost;
  doc["verlauf_regen"]    = cfg.verlauf_regen;
  doc["hinweis_uv"]       = cfg.hinweis_uv;
  doc["update_intervall_h"] = cfg.update_intervall_h;
  doc["ntfy_aktiv"]       = cfg.ntfy_aktiv;   // Server, Topic und Token stehen bewusst nicht in der Sicherung
  doc["ntfy_quellen"]     = cfg.ntfy_quellen;
  doc["ntfy_entwarnung"]  = cfg.ntfy_entwarnung;
  doc["warn_region"]      = cfg.warn_region;
  doc["dim_timeout"]      = cfg.dim_timeout;
  doc["brightness"]       = cfg.brightness;
  doc["night_mode"]       = cfg.night_modus != 0;   // fuer aeltere Firmware
  doc["night_modus"]      = cfg.night_modus;
  doc["night_sonne_ab"]   = cfg.night_sonne_ab;
  doc["night_sonne_bis"]  = cfg.night_sonne_bis;
  doc["night_theme"]      = cfg.night_theme;
  doc["night_uhr"]        = cfg.night_uhr;
  doc["rahmen_an"]        = cfg.rahmen_an;
  doc["rahmen_min"]       = cfg.rahmen_min;
  doc["rahmen_sek"]       = cfg.rahmen_sek;
  doc["rahmen_nacht"]     = cfg.rahmen_nacht;
  doc["wetter_modul1"]    = cfg.wetter_modul1;
  doc["wetter_modul2"]    = cfg.wetter_modul2;
  doc["update_modus"]     = cfg.update_modus;
  doc["night_from"]       = cfg.night_from;
  doc["night_to"]         = cfg.night_to;
  doc["night_brightness"] = cfg.night_brightness;
  doc["ruhe_modus"]       = cfg.ruhe_modus;
  doc["ruhe_von"]         = cfg.ruhe_von;
  doc["ruhe_bis"]         = cfg.ruhe_bis;
  doc["warn_ton_an"]      = cfg.warn_ton_an;
  doc["warn_ton"]         = cfg.warn_ton;
  doc["warn_laut"]        = cfg.warn_laut;
  String out;
  serializeJsonPretty(doc, out);
  return out;
}

static void handleWebBackup() {
  server.sendHeader("Content-Disposition", "attachment; filename=wettercube-biker-einstellungen.json");
  server.send(200, "application/json; charset=utf-8", webuiSicherungJson());
}

static int rsKlemme(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Sicherung einspielen (Datei aus dem Browser oder von der Speicherkarte); Rückgabe: Anzahl übernommener Einstellungen, -1 = ungültig
static int wendeSicherungAn(const String& body) {
  JsonDocument doc;
  if (body.length() == 0 || deserializeJson(doc, body) != DeserializationError::Ok) return -1;
  String typ = String((const char*)(doc["typ"] | ""));
  if (typ != SICHERUNG_TYP && typ != SICHERUNG_TYP_CUBE && typ != SICHERUNG_TYP_XL) return -1;
  int n = 0;
  // Nur bekannte Schlüssel übernehmen, Zahlen auf sinnvolle Bereiche begrenzen
  #define RS_INT(key, feld, lo, hi)  if (!doc[key].isNull()) { cfg.feld = rsKlemme(doc[key].as<int>(), lo, hi); n++; }
  #define RS_BOOL(key, feld)         if (!doc[key].isNull()) { cfg.feld = doc[key].as<bool>(); n++; }
  RS_INT ("dwd_region",       dwd_region,       0, 255);
  RS_INT ("dwd_region2",      dwd_region2,      0, 255);
  RS_BOOL("hinweis_regen",    hinweis_regen);
  RS_BOOL("hinweis_frost",    hinweis_frost);
  RS_BOOL("verlauf_regen",    verlauf_regen);
  RS_BOOL("hinweis_uv",       hinweis_uv);
  { int h = doc["update_intervall_h"] | 0; if (h == 1 || h == 6 || h == 24) { cfg.update_intervall_h = h; n++; } }
  RS_BOOL("ntfy_aktiv",       ntfy_aktiv);
  RS_INT ("ntfy_quellen",     ntfy_quellen,     0, 255);
  RS_BOOL("ntfy_entwarnung",  ntfy_entwarnung);
  RS_BOOL("warn_region",      warn_region);
  RS_INT ("dim_timeout",      dim_timeout,      -1, 10);
  RS_INT ("brightness",       brightness,       3, 100);
  if (!doc["night_mode"].isNull() && doc["night_modus"].isNull()) { cfg.night_modus = doc["night_mode"].as<bool>() ? 1 : 0; n++; }   // aeltere Sicherung
  RS_INT ("night_modus",      night_modus,      0, 2);
  RS_INT ("night_sonne_ab",   night_sonne_ab,   -180, 180);
  RS_INT ("night_sonne_bis",  night_sonne_bis,  -180, 180);
  RS_BOOL("night_theme",      night_theme);
  RS_BOOL("night_uhr",        night_uhr);
  RS_BOOL("rahmen_an",        rahmen_an);
  RS_INT ("rahmen_min",       rahmen_min,       1, 60);
  RS_INT ("rahmen_sek",       rahmen_sek,       10, 600);
  RS_BOOL("rahmen_nacht",     rahmen_nacht);
  RS_INT ("wetter_modul1",    wetter_modul1,    0, 6);
  RS_INT ("wetter_modul2",    wetter_modul2,    0, 6);
  RS_INT ("update_modus",     update_modus,     0, 2);
  RS_INT ("night_from",       night_from,       0, 1439);
  RS_INT ("night_to",         night_to,         0, 1439);
  RS_INT ("night_brightness", night_brightness, 3, 50);
  RS_INT ("ruhe_modus",       ruhe_modus,       0, 2);
  RS_INT ("ruhe_von",         ruhe_von,         0, 1439);
  RS_INT ("ruhe_bis",         ruhe_bis,         0, 1439);
  RS_BOOL("warn_ton_an",      warn_ton_an);
  RS_INT ("warn_ton",         warn_ton,         0, BOARD_AUDIO_TOENE - 1);
  RS_INT ("warn_laut",        warn_laut,        0, 100);
  #undef RS_INT
  #undef RS_BOOL
  if (doc["location2"].is<const char*>()) {
    String ort2 = String((const char*)doc["location2"]);
    if (ort2.length() <= 60) {
      cfg.location2 = ort2; n++;
      float la = doc["lat2"] | 0.0f, lo = doc["lon2"] | 0.0f;
      bool koordOk = !ort2.isEmpty() && (la != 0.0f || lo != 0.0f) && la >= -90.0f && la <= 90.0f && lo >= -180.0f && lo <= 180.0f;
      cfg.lat2 = koordOk ? la : 0.0f;
      cfg.lon2 = koordOk ? lo : 0.0f;
    }
  }
  String ort = String((const char*)(doc["location"] | ""));
  if (ort.length() > 0 && ort.length() <= 60) {
    cfg.location = ort; n++;
    float la = doc["lat"] | 0.0f, lo = doc["lon"] | 0.0f;
    bool koordOk = (la != 0.0f || lo != 0.0f) && la >= -90.0f && la <= 90.0f && lo >= -180.0f && lo <= 180.0f;
    cfg.lat = koordOk ? la : 0.0f;   // 0 = beim nächsten Start per Geocoding neu bestimmen
    cfg.lon = koordOk ? lo : 0.0f;
  }
  speichereCfg();
  updateModusGeaendert();
  logDiag("[Sicherung] %d Einstellungen wiederhergestellt", n);
  return n;
}

static void handleWebRestore() {
  int n = wendeSicherungAn(server.arg("plain"));
  if (n < 0) { server.send(400, "text/plain", "Keine gueltige Sicherungsdatei"); return; }
  server.send(200, "text/plain", "ok " + String(n));
  delay(1500);
  ESP.restart();
}

// Einstellungen von der Speicherkarte (notfall/einstellungen.json) wiederherstellen
static void handleWebRestoreKarte() {
  String json = speicherSicherungLesen();
  if (json.isEmpty()) { server.send(404, "text/plain", "Keine Sicherung auf der Karte gefunden"); return; }
  int n = wendeSicherungAn(json);
  if (n < 0) { server.send(400, "text/plain", "Die Sicherung auf der Karte ist nicht lesbar"); return; }
  server.send(200, "text/plain", "ok " + String(n));
  delay(1500);
  ESP.restart();
}

static void handleUpdateModusSave() {
  cfg.update_modus = constrain(server.arg("update_modus").toInt(), 0, 2);
  { int h = server.arg("update_std").toInt(); cfg.update_intervall_h = (h == 1 || h == 24) ? h : 6; }
  speichereCfg();
  updateModusGeaendert();
  logDiag("[Update] Modus: %d (0 = aus, 1 = nur Web, 2 = Display und Web)", cfg.update_modus);
  server.sendHeader("Location", "/#system"); server.send(302);
}

static void handleNtfySave() {
  cfg.ntfy_aktiv = server.hasArg("ntfy_aktiv");
  String srv = server.arg("ntfy_server"); srv.trim();
  if (srv.length() > 8 && (srv.startsWith("https://") || srv.startsWith("http://"))) cfg.ntfy_server = srv;
  else if (srv.length() == 0) cfg.ntfy_server = "https://ntfy.sh";
  String top = server.arg("ntfy_topic"); top.trim();
  cfg.ntfy_topic = top;
  String tok = server.arg("ntfy_token"); tok.trim();
  if (tok.length()) cfg.ntfy_token = tok;
  if (server.hasArg("ntfy_token_weg")) cfg.ntfy_token = "";
  cfg.ntfy_entwarnung = server.hasArg("ntfy_ent");
  cfg.ntfy_quellen = (server.hasArg("ntfy_q_tag") ? NTFY_Q_TAG : 0) | (server.hasArg("ntfy_q_regen") ? NTFY_Q_REGEN : 0) | (server.hasArg("ntfy_q_warn") ? NTFY_Q_WARNUNG : 0);
  speichereCfg();
  server.sendHeader("Location", "/#system"); server.send(302);
}

// Diagnose-Log per Browser (http://<ip>/log), neuester Eintrag zuerst
static void handleApiLog() {
  String out = "WetterCube Biker Diagnose-Log (neueste zuerst)\n";
  out += "Version: " FIRMWARE_VERSION "\n";
  out += "Freier Heap: " + String(ESP.getFreeHeap()) + " Bytes (intern, tiefster Stand seit Start: " + String(ESP.getMinFreeHeap()) +
         ", groesster Block " + String(ESP.getMaxAllocHeap()) + ")\n";
  out += "PSRAM: frei " + String(ESP.getFreePsram()) + " von " + String(ESP.getPsramSize()) + " Bytes (tiefster Stand seit Start: " +
         String(ESP.getMinFreePsram()) + ", groesster Block " + String(ESP.getMaxAllocPsram()) + ")\n";
  { // Bildrate des Displays seit der letzten Abfrage (Soll bei 15 MHz Pixeltakt: 16.4 Hz); weniger = Treiber musste wegen PSRAM-Engpass neu starten
    static uint32_t letzteBilder = 0, letzteMs = 0;
    uint32_t b = boardBilderGezaehlt(), t = millis();
    float hz = (t > letzteMs) ? (b - letzteBilder) * 1000.0f / (t - letzteMs) : 0;
    char z[80]; snprintf(z, sizeof(z), "Bildrate: %.2f Hz seit der letzten Abfrage (%u Bilder gesamt)\n", hz, (unsigned)b);
    out += z; letzteBilder = b; letzteMs = t;
  }
  { extern uint32_t lvglStatRenders, lvglStatGross, lvglStatLetzteMs, lvglStatLetztePx, lvglStatMaxMs;
    char z[160]; snprintf(z, sizeof(z), "LVGL-Neuzeichnen: %u gesamt, %u grosse (>20000 px), letzte grosse %u px in %u ms, laengste %u ms\n",
                          (unsigned)lvglStatRenders, (unsigned)lvglStatGross, (unsigned)lvglStatLetztePx, (unsigned)lvglStatLetzteMs, (unsigned)lvglStatMaxMs);
    out += z; }
  out += "Uptime: " + String(millis() / 60000UL) + " Minuten\n";
  { LvLock lock; lv_mem_monitor_t m; lv_mem_monitor(&m);
    out += "LVGL-Speicher: frei " + String((unsigned)m.free_size) + " von " + String((unsigned)m.total_size) +
           " Bytes (" + String((unsigned)m.used_pct) + " % belegt), groesster Block " + String((unsigned)m.free_biggest_size) + "\n"; }
  out += String("Speicherkarte: ") + speicherStatusText() + (speicherBereit() ? " (Dateien: /karte)" : "") + "\n";
  out += otaSlotInfo() + "\n";
  { String nv; out += notfallBereit(&nv) ? "Notfall-App: Version " + nv + " installiert\n" : String("Notfall-App: nicht installiert\n"); }
  out += "Notfall-Kopie der Firmware auf der Karte: " + speicherNotfallStatus() + "\n";
  out += "Letzter Neustart: " + (resetGrundText.length() ? resetGrundText : String("-")) + "\n";
  if (spurVorAbsturz.length()) out += "Zuletzt aktiv vor dem Neustart: " + spurVorAbsturz + "\n";
  out += "WLAN beim Start: " + (wifiStartInfo.length() ? wifiStartInfo : String("-")) + "\n";
  if (WiFi.status() == WL_CONNECTED)
    out += "WLAN jetzt: '" + WiFi.SSID() + "', Signal " + String(WiFi.RSSI()) + " dBm, IP " + WiFi.localIP().toString() + "\n";
  else
    out += "WLAN jetzt: nicht verbunden\n";
  out += "Standort: " + (cfg.location.length() ? cfg.location : String("-")) + " (" + String(cfg.lat, 4) + ", " + String(cfg.lon, 4) +
         (cfg.lat == 0.0f ? " = noch nicht gefunden" : "") + ")\n";
  out += "Letzter erfolgreicher Abruf: " + abrufErfolgText() + "\n";
  { nvs_stats_t st;
    String nvs = (nvs_get_stats(NULL, &st) == ESP_OK) ? String((unsigned)st.used_entries) + " von " + String((unsigned)st.total_entries) + " Eintraegen belegt" : String("nicht lesbar");
    out += "Einstellungen: WLAN-Zugangsdaten " + String(cfg.ssid.length() ? "gespeichert" : "NICHT gespeichert") + ", Ort '" + cfg.location + "', NVS " + nvs + "\n"; }
  out += "Update: installiert " + String(FIRMWARE_VERSION) + ", neueste bekannte " +
         (updateNeueVersion.length() ? updateNeueVersion : String("-")) + (updateVerfuegbar ? " (Update verfuegbar)" : "") +
         ", letzte Pruefung " + (letzterUpdateCheck ? "vor " + String((millis() - letzterUpdateCheck) / 60000UL) + " Min." : String("noch nicht")) + "\n";
  out += "----------------------------------------\n";
  String eintraege = logHolen();
  out += eintraege.isEmpty() ? "Noch keine Diagnose-Einträge seit dem letzten Neustart.\n" : eintraege;
  server.send(200, "text/plain; charset=utf-8", out);
}

// ── OTA (Firmware von GitHub Pages): Prüfung und Installation liegen in ota.cpp ──
// Zeitangabe fuer die Update-Info: "12 Min." oder "5 Std. 10 Min."
static String dauerText(unsigned long ms) {
  unsigned long min = ms / 60000UL;
  if (min < 90) return String(min) + " Min.";
  return String(min / 60) + " Std. " + String(min % 60) + " Min.";
}

// Stand der Update-Pruefung fuer den Reiter System (Version, letzte und naechste Pruefung)
static String updateInfoText() {
  if (cfg.update_modus == 0) return String("Updates sind ausgeschaltet: Das Ger&auml;t fragt nicht nach neuen Versionen und meldet keine Updates.");
  String s = updateVerfuegbar ? String("Neue Version verf&uuml;gbar: ") + updateNeueVersion : String("Keine neuere Version bekannt.");
  s += "<br>Letzte Pr&uuml;fung: ";
  if (letzterUpdateCheck == 0 || updateLetztesErgebnis == 0) s += "noch nicht";
  else {
    unsigned long seit = millis() - letzterUpdateCheck;
    s += "vor " + dauerText(seit) + (updateLetztesErgebnis == 2 ? " (fehlgeschlagen)" : "");
    unsigned long bis = updateCheckIntervall > seit ? updateCheckIntervall - seit : 0;
    s += ", n&auml;chste in " + dauerText(bis);
  }
  return s;
}

static void handleWebOtaCheck() {
  if (cfg.update_modus == 0) {      // Updates ganz aus: keine Anfrage an GitHub
    server.send(200, "application/json", "{\"aus\":true,\"current\":\"" FIRMWARE_VERSION "\",\"latest\":\"" FIRMWARE_VERSION "\",\"update_available\":false}");
    return;
  }
  String latest = FIRMWARE_VERSION;
  bool ok = holeNeuesteVersion(latest);
  bool update_available = ok && versionNeuer(latest, FIRMWARE_VERSION);   // nur wenn die Version wirklich neuer ist
  if (ok) {
    updateVerfuegbar  = update_available;
    updateNeueVersion = latest;
    aktualisiereUpdateBadge();
  }
  String json = "{\"current\":\"" + String(FIRMWARE_VERSION) + "\",\"latest\":\"" + latest +
                "\",\"update_available\":" + (update_available ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

static void handleWebOtaDoUpdate() {
  if (cfg.update_modus == 0) { server.send(403, "text/plain", "Updates sind ausgeschaltet (Einstellung unter System). Eine Firmware-Datei laesst sich weiter installieren."); return; }
  String fehler = installiereUpdate();
  if (fehler.length() == 0) {
    server.send(200, "text/plain", "ok");
    updateErfolgNeustart();
  } else {
    server.send(500, "text/plain", fehler);
    updateFehlerAnzeigen(fehler);
  }
}

// Firmware aus einer Datei: der Rumpf der Anfrage ist die Datei selbst, Block für Block in den freien Slot schreiben, am Ende neu starten
static void handleWebUpdateDatei() {
  size_t gesamt = server.clientContentLength();
  logDiag("[OTA] Datei-Upload: %u Bytes", (unsigned)gesamt);
  String fehler = otaDateiStart(gesamt);
  uint8_t* buf = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!buf && fehler.isEmpty()) { fehler = "Zu wenig Speicher für den Upload"; otaDateiAbbrechen(); }
  size_t rest = gesamt;
  while (fehler.isEmpty() && rest > 0) {
    size_t fuell = 0;                                        // einen Block füllen (der erste braucht den Firmware-Kopf in einem Stück)
    while (fuell < 4096 && fuell < rest) {
      int n = server.bodyLesen(buf + fuell, (rest < 4096 ? rest : 4096) - fuell);
      if (n <= 0) break;
      fuell += (size_t)n;
    }
    if (fuell == 0) { fehler = "Der Upload ist abgebrochen"; otaDateiAbbrechen(); break; }
    fehler = otaDateiSchreiben(buf, fuell);
    rest -= fuell;
  }
  if (buf) heap_caps_free(buf);
  if (fehler.isEmpty()) fehler = otaDateiEnde();
  if (fehler.isEmpty()) {
    server.send(200, "text/plain", "ok");
    updateErfolgNeustart();
  } else {
    logDiag("[OTA] Datei-Update fehlgeschlagen: %s", fehler.c_str());
    server.send(500, "text/plain", fehler);
    updateFehlerAnzeigen(fehler);
  }
}

// ── WLAN ändern ──────────────────────────────────────────────────────────────
static void handleWebWlanAendern() {
  int n = WiFi.scanNetworks();
  String nets;
  for (int i = 0; i < n; i++) nets += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + "</option>\n";
  String html = FPSTR(WLAN_HTML);
  html.replace("%NETZWERKE%", nets);
  server.send(200, "text/html", html);
}

static void handleWebWlanSave() {
  if (server.hasArg("ssid")) cfg.ssid = server.arg("ssid");
  if (server.hasArg("pass")) cfg.pass = server.arg("pass");
  cfg.lat = 0.0f; cfg.lon = 0.0f;
  speichereCfg();
  logDiag("[WLAN] Netzwerk gewechselt zu '%s', Neustart", cfg.ssid.c_str());
  server.send(200, "text/html",
    "<!DOCTYPE html><html><body style='background:#0d1117;color:#e6edf3;font-family:Arial;padding:30px'>"
    "<h2 style='color:#2ea043'>WLAN gespeichert</h2><p>Neustart…</p></body></html>");
  delay(2000);
  ESP.restart();
}

void starteWebUI() {
  if (mdns_init() == ESP_OK && mdns_hostname_set(MDNS_NAME) == ESP_OK) {
    mdns_instance_name_set("WetterCube Biker");
    mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
    logDiag("[Start] mDNS: %s.local", MDNS_NAME);
  } else {
    logDiag("[Start] mDNS konnte nicht gestartet werden (Zugriff nur per IP-Adresse)");
  }
  server.on("/",               HTTP_GET,  handleWebRoot);
  server.on("/speichern",      HTTP_POST, handleWebSave);
  server.on("/ota_check",      HTTP_GET,  handleWebOtaCheck);
  server.on("/ota_update",     HTTP_POST, handleWebOtaDoUpdate);
  server.on("/update_datei",   HTTP_POST, handleWebUpdateDatei);
  server.on("/wlan",           HTTP_GET,  handleWebWlanAendern);
  server.on("/wlan_speichern", HTTP_POST, handleWebWlanSave);
  server.on("/api/tontest",    HTTP_POST, []() {   // Alarmton probehoeren (Einstellungen), ?aus=1 beendet ihn
    if (server.hasArg("aus")) { boardAudioStopp(); server.send(200, "text/plain", "ok"); return; }
    if (!boardAudioVorhanden()) { server.send(200, "text/plain", "Dieses Board hat keinen Lautsprecher"); return; }
    boardAudioTon(constrain(server.arg("ton").toInt(), 0, BOARD_AUDIO_TOENE - 1), constrain(server.arg("laut").toInt(), 0, 100));
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/ntfy_save",  HTTP_POST, handleNtfySave);
  server.on("/api/update_modus_save", HTTP_POST, handleUpdateModusSave);
  server.on("/api/web_pw_save", HTTP_POST, handleWebPwSave);
  server.setZugriffsPruefer(zugriffErlaubt);
  // Pruefung der Absturz-Aufzeichnung: loest absichtlich einen Absturz aus (nur mit ja=1), danach steht die Ursache im /log
  server.on("/api/absturztest", HTTP_POST, []() {
    if (server.arg("ja") != "1") { server.send(400, "text/plain", "Zum Ausloesen ja=1 mitsenden. Das Geraet startet danach neu."); return; }
    server.send(200, "text/plain", "Absturz wird ausgeloest ...");
    delay(300);
    logDiag("[Test] Absichtlicher Absturz");
    volatile int* p = nullptr; *p = 1;
  });
  server.on("/api/ntfy_test",  HTTP_POST, []() { ntfyTest(); server.send(200, "text/plain", "ok"); });
  server.on("/api/ntfy_status", HTTP_GET, []() { server.send(200, "text/plain; charset=utf-8", ntfyStatusText()); });
  server.on("/api/standort",   HTTP_POST, []() {   // ?nr=0|1: zwischen Standort 1 und 2 umschalten (wie der Tipp auf den Ortsnamen am Display)
    if (!hatStandort2() && server.arg("nr").toInt() == 1) { server.send(200, "text/plain; charset=utf-8", "Es ist noch kein zweiter Standort eingerichtet (erst speichern)."); return; }
    standortWechseln(server.arg("nr").toInt() == 1 ? 1 : 0);
    server.send(200, "text/plain; charset=utf-8", standortStatusText());
  });
  server.on("/backup",         HTTP_GET,  handleWebBackup);
  server.on("/restore",        HTTP_POST, handleWebRestore);
  server.on("/restore_karte",  HTTP_POST, handleWebRestoreKarte);
  server.on("/wiederherstellung", HTTP_POST, []() {
    if (!notfallBereit(nullptr)) { server.send(409, "text/plain", "Die Notfall-App ist nicht installiert."); return; }
    server.send(200, "text/plain", "ok");
    delay(500);
    notfallStarten(1);
  });
  server.on("/log",            HTTP_GET,  handleApiLog);
  routenWebuiRegistrieren(server);
  speicherWebuiRegistrieren(server);
  rahmenWebuiRegistrieren(server);
  screenshotRegistrieren(server);
  server.begin();
  logDiag("[WebUI] http://%s.local", MDNS_NAME);
}

void webuiBedienen() {}   // HTTP-Server und DNS-Antworter laufen in eigenen Tasks

