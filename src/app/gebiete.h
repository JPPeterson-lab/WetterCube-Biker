// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
#pragma once
// Motorradgebiete: Liste (Namen mit Koordinaten) und Rangfolge nach dem Biker-Score fuer heute, morgen und das Wochenende.
#include <stdint.h>
#include <time.h>
#include "../platform/Arduino.h"

#define GEBIETE_MAX 20

struct Gebiet {
  char  name[32] = "";
  float lat = 0, lon = 0;
  int   score[3] = {-1, -1, -1};     // heute, morgen, Wochenende (Mittel der Tagesstunden 10 bis 18 Uhr)
  float tmax[3] = {};
  float mm[3] = {};                  // Regen in diesen Stunden (Summe je Tag, beim Wochenende Mittel je Tag)
};

extern Gebiet gebiete[GEBIETE_MAX];
extern int    gebieteAnzahl;
extern time_t gebieteStand;                  // wann die Wetterdaten zuletzt geholt wurden (0 = nie)
extern volatile int gebieteStatus;           // 0 = Ruhe, 1 = laeuft, 2 = fertig, 3 = Fehler
extern char   gebieteText[96];

void gebieteListeLaden();                    // aus /daten/gebiete.txt, beim ersten Mal mit Beispielen
bool gebieteTextSpeichern(const String& zeilen);   // eine Zeile je Gebiet (Name oder "Name; lat; lon"), sucht fehlende Koordinaten (blockierend)
String gebieteTextHolen();                   // Liste als Text (eine Zeile je Gebiet)
bool gebieteAuftrag(bool neueListe, const String& zeilen);   // Hintergrundauftrag: (Liste speichern und) Wetter holen
int  gebieteRang(int tag, int* reihenfolge, int max);        // Indizes nach Score absteigend, nur Gebiete mit Wert
