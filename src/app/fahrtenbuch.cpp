// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Fahrtenbuch: Nach einer Ausfahrt traegt man die Fahrt in der Web-Oberflaeche ein. Gespeichert werden Abfahrt, Route, Laenge und das Wetter dieser
// Abfahrt (Score, Temperatur, Regen, Boeen), wie es der Biker fuer die gewaehlte Abfahrt berechnet hat. Datei /daten/fahrten.csv im Datenlaufwerk,
// eine Zeile je Fahrt: "Abfahrt;Routen-Id;Name;km;Score;tmin;tmax;Regenabschnitte;Abschnitte;Boeen" (Score -1: kein Wetter, weil die Abfahrt
// vor dem Zeitraum der Vorhersage lag). Die Jahresuebersicht rechnet die Web-Oberflaeche aus der Liste.
#include "app_state.h"
#include "route.h"
#include "../storage/daten.h"
#include "../platform/WebServer.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

static const char* DATEI = DATEN_MOUNT "/fahrten.csv";
static const int MAX_ZEILEN = 3000;

struct Fahrt { long t; char id[12]; char name[48]; int km; int score; float tmin, tmax; int regen, n, boe; };

static bool zeileLesen(const char* z, Fahrt* f) {
  float km, tmin, tmax, boe;
  if (sscanf(z, "%ld;%11[^;];%47[^;];%f;%d;%f;%f;%d;%d;%f", &f->t, f->id, f->name, &km, &f->score, &tmin, &tmax, &f->regen, &f->n, &boe) != 10) return false;
  f->km = (int)(km + 0.5f); f->tmin = tmin; f->tmax = tmax; f->boe = (int)(boe + 0.5f);
  return true;
}

static void saeubern(char* s) { for (; *s; s++) if (*s == ';' || *s == '\n' || *s == '\r') *s = ','; }

static bool eintragen(String& meld) {
  Fahrt f = {};
  {
    LvLock l;
    if (!routeAkt || !routeAkt->route || !routeErg.gueltig) { meld = "Keine Route gewählt"; return false; }
    const RAnalyse* a = routeAkt; const RErgebnis& e = routeErg;
    const bool wetter = a->wx[0] && e.abfahrt >= a->wx[0]->h[0].t;   // vor dem Zeitraum der Vorhersage gibt es kein Wetter
    f.t = (long)e.abfahrt;
    strlcpy(f.id, a->route->id, sizeof(f.id)); strlcpy(f.name, a->route->name, sizeof(f.name)); saeubern(f.name);
    f.km = (int)(a->route->km + 0.5f);
    f.score = wetter ? e.gesamt : -1; f.tmin = wetter ? e.tmin : 0; f.tmax = wetter ? e.tmax : 0;
    f.regen = wetter ? e.regenAbschnitte : 0; f.n = e.n; f.boe = wetter ? (int)(e.boeMax + 0.5f) : 0;
  }
  if (FILE* r = fopen(DATEI, "r")) {   // dieselbe Abfahrt nicht doppelt eintragen
    char z[200]; int n = 0; Fahrt g;
    while (fgets(z, sizeof(z), r) && n++ < MAX_ZEILEN) if (zeileLesen(z, &g) && g.t == f.t && !strcmp(g.id, f.id)) { fclose(r); meld = "Diese Fahrt ist schon eingetragen"; return false; }
    fclose(r);
  }
  FILE* w = fopen(DATEI, "a");
  if (!w) { meld = "Datenlaufwerk nicht beschreibbar"; return false; }
  fprintf(w, "%ld;%s;%s;%d;%d;%.1f;%.1f;%d;%d;%d\n", f.t, f.id, f.name, f.km, f.score, f.tmin, f.tmax, f.regen, f.n, f.boe);
  fclose(w);
  logDiag("[Fahrtenbuch] Fahrt eingetragen: %s, %d km, Score %d", f.name, f.km, f.score);
  return true;
}

static void listeSenden(WebServer& S) {
  JsonDocument d;
  JsonArray a = d["fahrten"].to<JsonArray>();
  // neueste zuerst: erst alle Zeilen merken (die Datei ist klein), dann rueckwaerts ausgeben
  static Fahrt* alle = nullptr;
  if (!alle) alle = (Fahrt*)heap_caps_malloc(sizeof(Fahrt) * MAX_ZEILEN, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  int n = 0;
  if (alle) {
    if (FILE* r = fopen(DATEI, "r")) {
      char z[200];
      while (fgets(z, sizeof(z), r) && n < MAX_ZEILEN) if (zeileLesen(z, &alle[n])) n++;
      fclose(r);
    }
    for (int i = n - 1; i >= 0; i--) {
      JsonObject o = a.add<JsonObject>();
      o["t"] = alle[i].t; o["id"] = alle[i].id; o["name"] = alle[i].name; o["km"] = alle[i].km; o["score"] = alle[i].score;
      o["tmin"] = alle[i].tmin; o["tmax"] = alle[i].tmax; o["regen"] = alle[i].regen; o["n"] = alle[i].n; o["boe"] = alle[i].boe;
    }
  }
  String out; serializeJson(d, out);
  S.send(200, "application/json; charset=utf-8", out);
}

static bool loeschen(long t) {
  String neu; bool gefunden = false;
  FILE* r = fopen(DATEI, "r");
  if (!r) return false;
  char z[200]; Fahrt g; int n = 0;
  while (fgets(z, sizeof(z), r) && n++ < MAX_ZEILEN) { if (zeileLesen(z, &g) && g.t == t) { gefunden = true; continue; } neu += z; }
  fclose(r);
  if (!gefunden) return false;
  FILE* w = fopen(DATEI, "w");
  if (!w) return false;
  fwrite(neu.c_str(), 1, neu.length(), w);
  fclose(w);
  return true;
}

void fahrtenbuchWebuiRegistrieren(WebServer& server) {
  WebServer* S = &server;
  S->on("/api/fahrten", HTTP_GET, [S]() { listeSenden(*S); });
  S->on("/api/fahrt_eintragen", HTTP_POST, [S]() {
    String meld;
    if (!eintragen(meld)) { S->send(409, "text/plain; charset=utf-8", meld); return; }
    S->send(200, "text/plain", "ok");
  });
  S->on("/api/fahrt_loeschen", HTTP_POST, [S]() {
    if (!loeschen(S->arg("t").toInt())) { S->send(404, "text/plain; charset=utf-8", "Eintrag nicht gefunden"); return; }
    S->send(200, "text/plain", "ok");
  });
}
