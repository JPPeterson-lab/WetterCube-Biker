// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
#pragma once
// Routen: anlegen (Start/Ziel per Routing oder GPX-Datei), speichern (Datenlaufwerk), Wetter je Abschnitt, Score, Abfahrtszeit-Planer.
#include <stdint.h>
#include <time.h>
#include "biker.h"
#include "../platform/Arduino.h"
#include <lvgl.h>

#define ROUTE_MAX_PUNKTE 1500   // Stuetzpunkte einer gespeicherten Route
#define ROUTE_MAX_ABSCHNITTE 16
#define ROUTE_MAX_GESPEICHERT 30
#define PLANER_STUNDEN 48

struct RoutePunkt { float lat, lon; int16_t ele, pad; };   // ele = Hoehe in m, -32768 = unbekannt

struct Route {
  char     id[12] = "";      // Dateiname ohne Endung
  char     name[40] = "";
  char     start[32] = "", ziel[32] = "";
  float    km = 0;
  uint32_t sekundenDienst = 0;   // Fahrzeit laut Routing-Dienst (0 bei GPX)
  bool     gpx = false;
  int      n = 0;
  RoutePunkt* p = nullptr;       // im PSRAM
};

struct RAbschnitt {
  float   km0 = 0, km1 = 0, kmMitte = 0;
  float   lat = 0, lon = 0;
  int16_t hoehe = 0;
  int16_t kurs = 0;              // Fahrtrichtung in Grad
  int     i0 = 0, i1 = 0;        // Punktbereich der Route
};

// Ergebnis fuer eine Abfahrtszeit
struct RErgebnis {
  bool   gueltig = false;
  time_t abfahrt = 0, ankunft = 0;
  int    tempo = 60;
  int    n = 0;
  int    score[ROUTE_MAX_ABSCHNITTE] = {};
  float  temp[ROUTE_MAX_ABSCHNITTE] = {};
  float  mm[ROUTE_MAX_ABSCHNITTE] = {};
  float  boe[ROUTE_MAX_ABSCHNITTE] = {};
  float  langs[ROUTE_MAX_ABSCHNITTE] = {};   // Wind in Fahrtrichtung, negativ = Gegenwind
  float  quer[ROUTE_MAX_ABSCHNITTE] = {};
  uint8_t pop[ROUTE_MAX_ABSCHNITTE] = {};
  uint8_t wmo[ROUTE_MAX_ABSCHNITTE] = {};
  time_t zeit[ROUTE_MAX_ABSCHNITTE] = {};    // Durchfahrtszeit
  int    gesamt = 0, schlechtester = 100;
  float  tmin = 99, tmax = -99, mmSumme = 0, boeMax = 0, langsMittel = 0, querMax = 0;
  int    regenAbschnitte = 0;
  int    erstesRegenKm = -1;
  float  frostMinTemp = 99;                  // tiefste Temperatur unterwegs
  int    frostKm = -1;                       // erster Abschnitt mit Frost
  time_t sonnenuntergang = 0;
  bool   imHellenAn = true;                  // Ankunft vor Sonnenuntergang
  int    blendKm = -1;                       // erster Abschnitt mit tiefer Sonne in Fahrtrichtung
  float  sonneHoeheBlend = 0;
};

struct RAnalyse {
  Route*     route = nullptr;                // Eigentuemer ist die Analyse
  int        n = 0;
  RAbschnitt a[ROUTE_MAX_ABSCHNITTE];
  BikerPunkt* wx[ROUTE_MAX_ABSCHNITTE] = {}; // Stundenwerte je Abschnitt (PSRAM)
  int        hMin = 0, hMax = 0;             // Hoehe: tiefster und hoechster Punkt (m), Anstieg gesamt
  int        anstieg = 0;
  bool       hoeheAusGpx = false;
};

extern RAnalyse* routeAkt;                   // aktive Route mit Wetter (nullptr = keine)
extern RErgebnis routeErg;                   // Ergebnis fuer die gewaehlte Abfahrt
extern int       routePlaner[PLANER_STUNDEN];// Score je Abfahrtsstunde ab routePlanerStart (-1 = nicht berechenbar)
extern time_t    routePlanerStart;
extern int       routeBesteIdx, routeJetztScore;
extern int       routeTempo;                 // km/h
extern time_t    routeAbfahrt;               // gewaehlte Abfahrt (0 = jetzt)

// Gespeicherte Routen
int   routenAnzahl();
bool  routenInfo(int i, char* id, size_t idN, char* name, size_t nameN, float* km, uint32_t* sek, bool* gpx);
bool  routeLaden(const char* id, Route* r);          // r->p wird im PSRAM angelegt
bool  routeSpeichern(Route& r);                      // vergibt r.id
bool  routeLoeschen(const char* id);
void  routeFrei(Route* r);

// Anlegen
bool  routeAusOrten(const String& name, const String& start, const String& ziel, const String& via, bool autobahnMeiden, Route* r, String& fehler);
// GPX Stueck fuer Stueck lesen: gpxBegin, gpxFeed (beliebig oft), gpxEnde -> fertige Route (nicht gespeichert)
void  gpxBegin();
void  gpxFeed(const char* d, size_t n);
bool  gpxEnde(const String& name, Route* r, String& fehler);

// Wetter und Bewertung
bool  routeWetterLaden(Route* r, RAnalyse** out, String& fehler);   // Abschnitte bilden, Wetter holen (blockierend)
void  routeBewerten(RAnalyse* a, time_t abfahrt, int tempo, RErgebnis* e, int pauseKm = -1, int pauseMin = 0);   // fuer eine Abfahrt; optional mit Pause (vor Kilometer pauseKm, pauseMin Minuten)

// Pause-Tipp: Liegt mitten auf der Strecke ein Regen- oder Gewitterstueck, prueft der Biker Pausen von 30 bis 180 Minuten vor diesem Stueck
// und nennt die kuerzeste, die den Score der Route deutlich hebt. da = false: kein Regenstueck mitten auf der Strecke oder eine Pause bringt nichts.
struct RPause { bool da = false; int km = 0, minuten = 0, scoreOhne = 0, scoreMit = 0; time_t anKm = 0, abfahrt = 0; bool gewitter = false; };
void  routePauseRechnen(RAnalyse* a, time_t abfahrt, int tempo, RPause* p);
void  routePauseText(const RPause& p, char* out, size_t n);          // "Pause-Tipp: ..." (leer, wenn p.da false)
void  routePlanerRechnen(RAnalyse* a, int tempo);                   // Score je Abfahrtsstunde, setzt routePlaner, routeBesteIdx
void  routeAktivSetzen(RAnalyse* a);                                // ersetzt routeAkt (alte freigeben), rechnet Ergebnis und Planer neu

// Hintergrundauftraege (eigener Task), Fortschritt in routeStatus
enum RouteStatus { RS_LEER = 0, RS_BESCHAEFTIGT = 1, RS_FERTIG = 2, RS_FEHLER = 3 };
extern volatile int routeStatus;
extern char routeStatusText[96];
bool  routeAuftragNeu(const String& name, const String& start, const String& ziel, const String& via, bool autobahnMeiden);
bool  routeAuftragWaehlen(const char* id);    // gespeicherte Route laden und Wetter holen
bool  routeAuftragAktualisieren();            // Wetter der aktiven Route neu holen

// Ortssuche (Photon, auf OpenStreetMap: auch Strassen und Hausnummern) mit Vorschlaegen
struct OrtVorschlag { char label[80]; float lat, lon; };
int   photonSuche(const String& anfrage, OrtVorschlag* out, int max);        // Anzahl Treffer (blockierend); 0 = nichts oder Fehler
bool  geocodeAdresse(const String& text, float& lat, float& lon);            // Photon, sonst Open-Meteo (nur Ortsnamen)
// Aus der Vorschlagsliste gewaehlte Orte (Start, Zwischenziel, Ziel): werden ohne erneute Suche benutzt, solange der Text unveraendert ist
struct RouteOrtWahl { char text[80]; float lat, lon; bool gesetzt; };
extern RouteOrtWahl routeOrtWahl[3];

// Hilfen
void  sonnenStand(double lat, double lon, time_t t, double* hoeheGrad, double* azimutGrad);
String urlKodieren(const String& s);

// Kartenbild der Route: das Geraet setzt OpenStreetMap-Kacheln (kacheln.cpp) zu einem Bild (640 x 302) zusammen und legt es im Datenlaufwerk ab
// (karten/<id>.map); die Strecke wird mit derselben Web-Mercator-Abbildung darauf gezeichnet.
#define KARTE_B 640          // Kartenbild im Routenwetter
#define KARTE_H 302
#define KARTE_GB 1024        // grosse Karte (Tippen auf die Karte)
#define KARTE_GH 548
void  mercatorPx(double lat, double lon, int z, double* x, double* y);
int   karteZoom(const char* id);                                  // Zoomstufe des Kartenbilds, -1 = keines vorhanden
bool  karteGrossVorhanden(const char* id);
bool  karteBildSpeichern(const char* id, bool gross, int z, int w, int h, const uint16_t* px);   // karten/<id>.map bzw. .big (RGB565)
void  karteLoeschen(const char* id);
void  karteAltlastenLoeschen();                                   // alte Kartendateien aus dem Datenlaufwerk entfernen (Start)
uint8_t* karteBildLaden(const char* id, bool gross, lv_img_dsc_t* dsc);   // direkt aus RGB565 im PSRAM (Aufrufer gibt den Puffer frei), nullptr bei Fehler
