// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Web-Oberflaeche der Routen: Liste, neue Route (Start/Ziel), GPX-Upload, Wetter je Abschnitt, Abfahrtszeit-Planer.
#include "app_state.h"
#include "route.h"
#include "gebiete.h"
#include "../platform/WebServer.h"
#include <ArduinoJson.h>
#include "esp_heap_caps.h"

static String idPruefen(const String& id) {   // nur Buchstaben und Ziffern
  String o;
  for (size_t i = 0; i < id.length() && i < 11; i++) { char c = id[i]; if (isalnum((unsigned char)c)) o += c; }
  return o;
}

static void antwortJson(WebServer& s, const String& j) { s.send(200, "application/json; charset=utf-8", j); }

void routenWebuiRegistrieren(WebServer& server) {
  WebServer* S = &server;

  S->on("/api/routen", HTTP_GET, [S]() {
    JsonDocument d;
    JsonArray a = d["routen"].to<JsonArray>();
    const int n = routenAnzahl();
    for (int i = 0; i < n; i++) {
      char id[12], name[40]; float km; uint32_t sek; bool gpx;
      if (!routenInfo(i, id, sizeof(id), name, sizeof(name), &km, &sek, &gpx)) continue;
      JsonObject o = a.add<JsonObject>();
      o["id"] = id; o["name"] = name; o["km"] = (int)lroundf(km); o["gpx"] = gpx;
    }
    { LvLock l; d["aktiv"] = (routeAkt && routeAkt->route) ? routeAkt->route->id : ""; }
    d["status"] = (int)routeStatus; d["text"] = routeStatusText; d["tempo"] = routeTempo;
    String out; serializeJson(d, out);
    antwortJson(*S, out);
  });

  S->on("/api/route_status", HTTP_GET, [S]() {
    JsonDocument d; d["status"] = (int)routeStatus; d["text"] = routeStatusText;
    String out; serializeJson(d, out);
    antwortJson(*S, out);
    if (routeStatus == RS_FERTIG || routeStatus == RS_FEHLER) { /* Status bleibt stehen, bis ein neuer Auftrag startet */ }
  });

  S->on("/api/route_neu", HTTP_POST, [S]() {
    String start = S->arg("start"), ziel = S->arg("ziel"), via = S->arg("via"), name = S->arg("name");
    if (start.isEmpty() || ziel.isEmpty()) { S->send(400, "text/plain; charset=utf-8", "Start und Ziel eingeben"); return; }
    if (!routeAuftragNeu(name, start, ziel, via, S->arg("autobahn") == "1")) { S->send(409, "text/plain; charset=utf-8", "Es läuft schon eine Berechnung"); return; }
    S->send(200, "text/plain", "ok");
  });

  S->on("/api/route_waehlen", HTTP_POST, [S]() {
    String id = idPruefen(S->arg("id"));
    if (id.isEmpty() || !routeAuftragWaehlen(id.c_str())) { S->send(409, "text/plain; charset=utf-8", "Es läuft schon eine Berechnung"); return; }
    S->send(200, "text/plain", "ok");
  });

  S->on("/api/route_aktualisieren", HTTP_POST, [S]() {
    if (!routeAuftragAktualisieren()) { S->send(409, "text/plain; charset=utf-8", "Keine Route gewählt oder es läuft schon eine Berechnung"); return; }
    S->send(200, "text/plain", "ok");
  });

  S->on("/api/route_loeschen", HTTP_POST, [S]() {
    String id = idPruefen(S->arg("id"));
    if (id.isEmpty()) { S->send(400, "text/plain", "id fehlt"); return; }
    if (routeAkt && routeAkt->route && id == routeAkt->route->id) routeAktivSetzen(nullptr);
    S->send(routeLoeschen(id.c_str()) ? 200 : 404, "text/plain", "");
  });

  // Abfahrt (Unix-Zeit, 0 = jetzt) und Tempo setzen; rechnet das Ergebnis neu (die Wetterdaten bleiben)
  S->on("/api/route_abfahrt", HTTP_POST, [S]() {
    LvLock l;
    if (S->hasArg("t")) routeAbfahrt = (time_t)S->arg("t").toInt();
    if (S->hasArg("tempo")) routeTempo = constrain(S->arg("tempo").toInt(), 30, 120);
    if (routeAkt) {
      routeBewerten(routeAkt, routeAbfahrt ? routeAbfahrt : time(nullptr), routeTempo, &routeErg);
      routePlanerRechnen(routeAkt, routeTempo);
    }
    S->send(200, "text/plain", "ok");
  });

  // GPX: der Rumpf der Anfrage ist die Datei selbst (Stueck fuer Stueck gelesen, nie ganz im Speicher)
  S->on("/api/route_gpx", HTTP_POST, [S]() {
    const size_t gesamt = S->clientContentLength();
    if (gesamt == 0 || gesamt > 12u * 1024u * 1024u) { S->send(413, "text/plain; charset=utf-8", "Datei leer oder größer als 12 MB"); return; }
    uint8_t* buf = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buf) { S->send(500, "text/plain; charset=utf-8", "Zu wenig Speicher"); return; }
    gpxBegin();
    size_t rest = gesamt;
    bool abgebrochen = false;
    while (rest > 0) {
      int n = S->bodyLesen(buf, rest < 4096 ? rest : 4096);
      if (n <= 0) { abgebrochen = true; break; }
      gpxFeed((const char*)buf, (size_t)n);
      rest -= (size_t)n;
    }
    heap_caps_free(buf);
    if (abgebrochen) { Route* t = new Route(); String f; gpxEnde("", t, f); routeFrei(t); delete t; S->send(400, "text/plain; charset=utf-8", "Upload abgebrochen"); return; }
    Route* r = new Route();
    String fehler;
    String name = S->arg("name"); name.trim();
    if (!gpxEnde(name, r, fehler)) { delete r; S->send(400, "text/plain; charset=utf-8", fehler); return; }
    if (!routeSpeichern(*r)) { routeFrei(r); delete r; S->send(507, "text/plain; charset=utf-8", "Speichern nicht möglich (Datenlaufwerk voll oder mehr als 30 Routen)"); return; }
    logDiag("[Route] GPX gespeichert: %s, %.0f km, %d Punkte", r->name, r->km, r->n);
    String id = r->id;
    routeFrei(r); delete r;
    S->send(200, "text/plain", id);
  });

  S->on("/api/route_ergebnis", HTTP_GET, [S]() {
    JsonDocument d;
    {
      LvLock l;
      if (!routeAkt || !routeAkt->route || !routeErg.gueltig) { d["leer"] = true; }
      else {
        const RAnalyse* a = routeAkt; const RErgebnis& e = routeErg;
        JsonObject ro = d["route"].to<JsonObject>();
        ro["id"] = a->route->id; ro["name"] = a->route->name; ro["km"] = (int)lroundf(a->route->km);
        ro["hmin"] = a->hMin; ro["hmax"] = a->hMax; ro["anstieg"] = a->anstieg; ro["gpx"] = a->route->gpx;
        JsonObject er = d["ergebnis"].to<JsonObject>();
        er["abfahrt"] = (long)e.abfahrt; er["ankunft"] = (long)e.ankunft; er["tempo"] = e.tempo; er["gesamt"] = e.gesamt;
        er["tmin"] = e.tmin; er["tmax"] = e.tmax; er["boemax"] = e.boeMax; er["regen"] = e.regenAbschnitte; er["regenkm"] = e.erstesRegenKm;
        er["frostkm"] = e.frostKm; er["sonne"] = (long)e.sonnenuntergang; er["hell"] = e.imHellenAn; er["blendkm"] = e.blendKm; er["langs"] = e.langsMittel; er["quer"] = e.querMax;
        JsonArray ab = d["abschnitte"].to<JsonArray>();
        for (int i = 0; i < e.n; i++) {
          JsonObject o = ab.add<JsonObject>();
          o["km0"] = (int)a->a[i].km0; o["km1"] = (int)a->a[i].km1; o["score"] = e.score[i]; o["t"] = (long)e.zeit[i];
          o["temp"] = e.temp[i]; o["mm"] = e.mm[i]; o["boe"] = e.boe[i]; o["langs"] = e.langs[i]; o["pop"] = e.pop[i]; o["wmo"] = e.wmo[i]; o["h"] = a->a[i].hoehe;
        }
        JsonObject pl = d["planer"].to<JsonObject>();
        pl["start"] = (long)routePlanerStart; pl["beste"] = routeBesteIdx; pl["jetzt"] = routeJetztScore;
        JsonArray sc = pl["scores"].to<JsonArray>();
        for (int k = 0; k < PLANER_STUNDEN; k++) sc.add(routePlaner[k]);
      }
    }
    String out; serializeJson(d, out);
    antwortJson(*S, out);
  });

  // Motorradgebiete: Liste (ein Name je Zeile, Koordinaten werden gesucht) und Rangfolge
  S->on("/api/gebiete", HTTP_GET, [S]() {
    if (gebieteAnzahl == 0) gebieteListeLaden();
    JsonDocument d;
    d["liste"] = gebieteTextHolen().c_str();
    d["status"] = (int)gebieteStatus; d["text"] = gebieteText; d["stand"] = (long)gebieteStand;
    JsonArray a = d["gebiete"].to<JsonArray>();
    { LvLock l;
      for (int i = 0; i < gebieteAnzahl; i++) {
        JsonObject o = a.add<JsonObject>();
        o["name"] = gebiete[i].name;
        for (int k = 0; k < 3; k++) { o["s"].add(gebiete[i].score[k]); o["t"].add(gebiete[i].tmax[k]); o["mm"].add(gebiete[i].mm[k]); }
      } }
    String out; serializeJson(d, out);
    antwortJson(*S, out);
  });
  S->on("/api/gebiete_save", HTTP_POST, [S]() {
    String liste = S->arg("liste");
    if (!gebieteAuftrag(true, liste)) { S->send(409, "text/plain; charset=utf-8", "Es läuft schon eine Abfrage"); return; }
    S->send(200, "text/plain", "ok");
  });
  S->on("/api/gebiete_aktualisieren", HTTP_POST, [S]() {
    if (!gebieteAuftrag(false, "")) { S->send(409, "text/plain; charset=utf-8", "Es läuft schon eine Abfrage"); return; }
    S->send(200, "text/plain", "ok");
  });
}
