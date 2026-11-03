// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
#pragma once
// Datenlaufwerk im Flash: FAT mit Wear-Levelling in der Partition "daten" (obere Flash-Haelfte, ab 16 MB), eingehaengt unter /daten.
// Fuer Routen (GPX), Kartenbilder und Verlauf, damit das Geraet auch ohne Speicherkarte Platz hat. Programme laufen nur unterhalb
// von 16 MB; die Datenpartition wird ohne Einblendung gelesen und geschrieben.
#include <stdint.h>
#include <stdbool.h>

#define DATEN_MOUNT "/daten"

bool datenInit();                                   // einmal nach dem Start: einhaengen (beim ersten Mal formatieren) und kurz pruefen
bool datenBereit();                                 // eingehaengt und Schreib-/Lesetest bestanden
bool datenInfo(uint64_t* gesamt, uint64_t* frei);   // Groesse und freier Platz in Bytes
const char* datenStatusText();                      // "bereit, 14.6 MB frei von 14.9 MB", "Fehler: ..." (fuer Log und Web-Oberflaeche)
