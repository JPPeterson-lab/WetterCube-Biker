// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
#pragma once
// Biker-Bewertung: Stundenwerte (Open-Meteo, 7 Tage), Score 0 bis 100, beste Fahrzeit, Empfehlung.
// Alle Regeln sind eigene, einfache Regeln (siehe docs/KONZEPT.md), keine Fahrempfehlung.
#include <stdint.h>
#include <time.h>
#include <stddef.h>

#define BK_STUNDEN 168   // 7 Tage stuendlich ab 00:00 des heutigen Tages

struct BikerStunde {
  time_t  t = 0;           // Beginn der Stunde (lokale Zeit als time_t)
  float   temp = 0, feels = 0;
  float   mm = 0;          // Niederschlag in mm
  float   wind = 0, gust = 0;   // km/h
  float   vis = 20;        // Sicht in km
  int16_t wdir = 0;
  float   uv = 0;          // UV-Index
  float   druck = 0;       // Luftdruck auf Meereshoehe in hPa (0 = unbekannt)
  uint8_t pop = 0, hum = 0, cloud = 0, wmo = 0;
  bool    tag = true;
};

// Ein Ort (Heimatort oder Punkt einer Route) mit allen Stunden und den Sonnenzeiten der 7 Tage
struct BikerPunkt {
  BikerStunde h[BK_STUNDEN];
  int      n = 0;
  float    hoehe = 0;                 // Hoehe des Modellgitters in m
  time_t   aufgang[7] = {}, untergang[7] = {};
  // Regen-Nowcast (Open-Meteo minutely_15): Startzeit und Niederschlag je 15-Minuten-Slot, nur beim Heimatort
  static const int NC_MAX = 12;
  time_t   nc_start[NC_MAX] = {};
  float    nc_mm[NC_MAX] = {};
  int      nc_n = 0;
  bool     geladen = false;
};

extern BikerPunkt* bikerHeim;   // Heimatort (im PSRAM), nullptr bis zum ersten Abruf

// ── Abruf (blockierend, nur aus dem Netzwerk-Task) ───────────────────────────
bool fetchStunden(float lat, float lon, BikerPunkt* out);       // 1 Ort: true = erfolgreich
bool fetchStundenHeim();                                        // Standort 1 oder 2 (stdLat/stdLon) nach bikerHeim

// ── Bewertung ────────────────────────────────────────────────────────────────
struct BikerEinstellung {
  float fahrtKmh = 70;      // Reisegeschwindigkeit fuer Fahrtwind-Gefuehl und Durchfahrtszeit
};
extern BikerEinstellung bikerCfg;

float bikerFahrtwindTemp(float tempC, float fahrtKmh);          // gefuehlte Temperatur bei Fahrtgeschwindigkeit (Windchill unter 10 Grad)
int   bikerScore(const BikerStunde& h, float fahrtKmh);         // 0 bis 100
const char* bikerStufe(int score);                              // Gut, Okay, Maessig, Schlecht
uint32_t bikerFarbe(int score);                                 // 0xRRGGBB
int   bikerIndexJetzt(const BikerPunkt& p, time_t jetzt);       // Stunde, in der "jetzt" liegt (-1 = keine Daten)
int   bikerTagesScore(const BikerPunkt& p, int tagNr);          // Mittel der Stunden 10 bis 18 Uhr am Tag (0 = heute), -1 = keine Daten
// Beste zusammenhaengende Fahrzeit am Tag `tagNr` (0 = heute): Start- und Endstunde (Ende exklusiv), Durchschnitts-Score; false = keine Stunde ab ab dem Zeitpunkt
bool  bikerBesteFahrzeit(const BikerPunkt& p, int tagNr, time_t ab, int* vonIdx, int* bisIdx, int* schnitt);
// Empfehlung (Kleidung und Ausruestung) fuer die Stunden von..bis; schreibt einen deutschen Satz
void  bikerEmpfehlung(const BikerPunkt& p, int von, int bis, char* out, size_t n);
// Himmelsrichtung des Winds relativ zur Fahrtrichtung: Komponente in Fahrtrichtung (negativ = Gegenwind) und quer (immer positiv), in km/h
void  bikerWindKomponenten(float windKmh, int windAusGrad, int fahrtrichtungGrad, float* langs, float* quer);
