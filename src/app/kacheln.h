// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
#pragma once
// OpenStreetMap-Kacheln (256 x 256, RGB565) fuer die Kartenbilder der Route (klein im Routenwetter, gross auf der Kartenseite).
// Das Geraet holt nur, was angezeigt wird (eine Route: etwa 6 bis 20 Kacheln), merkt sich die letzten 60 im PSRAM und meldet sich mit
// eigenem User-Agent. Hinweis "© OpenStreetMap-Mitwirkende" steht in jeder Kartenansicht.
#include <stdint.h>
#include <stddef.h>
#include "route.h"

#define KACHEL_PX 256

// Kachel-Pixel (RGB565, 256 x 256), wenn fertig geladen; sonst nullptr. anfordern = true stellt sie in die Warteschlange.
const uint16_t* kachelPixel(int z, int x, int y, bool anfordern);
bool kachelFehler(int z, int x, int y);              // Abruf fehlgeschlagen (wird nach einiger Zeit erneut versucht)
int  kachelnOffen();                                 // Anzahl angeforderter, noch nicht fertiger Kacheln

// Zoomstufe, bei der die Strecke in einen Bereich w x h (Pixel) mit Rand passt; Mittelpunkt der Strecke in Pixeln dieser Stufe
int  karteZoomFuerRoute(const Route* r, int w, int h, int rand, double* cx, double* cy);

// Kartenbild der Route (640 x 302) im Hintergrund erzeugen und im Datenlaufwerk ablegen (karten/<id>.map); fertig = karteStatus() == 2
void karteErzeugenStarten(const char* routeId);
bool karteStatusLaeuft();                            // Kartenbild wird gerade erzeugt
int  karteStatus();                                  // 0 = nichts, 1 = laeuft, 2 = fertig (nach Abruf wieder 0), 3 = Fehler
