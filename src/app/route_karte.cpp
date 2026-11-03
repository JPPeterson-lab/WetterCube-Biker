// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Kartenbilder der Route (klein im Routenwetter, gross auf der Kartenseite) als RGB565 im PSRAM.
// Bewusst NICHT im Flash-Dateisystem: Das Schreiben von rund 1,5 MB in die FAT-Partition sperrt waehrenddessen den Flash-Zugriff
// (Oberflaeche und Web-Server stehen dann sekundenlang) und dauerte zusammen mit den Kacheln fast zwei Minuten. Im PSRAM ist das Bild nach
// wenigen Sekunden fertig; nach einem Neustart wird es beim Oeffnen der Route neu geholt.
#include "route.h"
#include "app_state.h"
#include <math.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include "../storage/daten.h"
#include "esp_heap_caps.h"

void mercatorPx(double lat, double lon, int z, double* x, double* y) {
  const double n = 256.0 * (double)(1 << z);
  const double la = lat * M_PI / 180.0;
  *x = (lon + 180.0) / 360.0 * n;
  *y = (1.0 - log(tan(la) + 1.0 / cos(la)) / M_PI) / 2.0 * n;
}

struct Bild { char id[12]; int z, w, h; uint16_t* px; };
static Bild s_aktuell[2];   // [0] klein, [1] gross
static Bild s_vorher[2];    // das vorherige Bild bleibt noch eine Runde bestehen, falls die Oberflaeche es gerade zeichnet

static Bild* suchen(const char* id, bool gross) {
  Bild& b = s_aktuell[gross ? 1 : 0];
  return (b.px && !strcmp(b.id, id)) ? &b : nullptr;
}

int karteZoom(const char* id) { Bild* b = suchen(id, false); return b ? b->z : -1; }
bool karteGrossVorhanden(const char* id) { return suchen(id, true) != nullptr; }

bool karteBildSpeichern(const char* id, bool gross, int z, int w, int h, const uint16_t* px) {
  uint16_t* kopie = (uint16_t*)heap_caps_malloc((size_t)w * h * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!kopie) return false;
  memcpy(kopie, px, (size_t)w * h * 2);
  LvLock lock;
  const int i = gross ? 1 : 0;
  if (s_vorher[i].px) heap_caps_free(s_vorher[i].px);
  s_vorher[i] = s_aktuell[i];
  strlcpy(s_aktuell[i].id, id, sizeof(s_aktuell[i].id));
  s_aktuell[i].z = z; s_aktuell[i].w = w; s_aktuell[i].h = h; s_aktuell[i].px = kopie;
  return true;
}

void karteLoeschen(const char* id) {
  LvLock lock;
  for (int i = 0; i < 2; i++) if (s_aktuell[i].px && !strcmp(s_aktuell[i].id, id)) { s_aktuell[i].id[0] = 0; }   // Speicher wird beim naechsten Bild frei
}

// Gibt den Zeiger auf den internen Puffer zurueck (gehoert diesem Modul, nicht freigeben)
uint8_t* karteBildLaden(const char* id, bool gross, lv_img_dsc_t* dsc) {
  Bild* b = suchen(id, gross);
  if (!b) return nullptr;
  memset(dsc, 0, sizeof(lv_img_dsc_t));
  dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
  dsc->header.w = b->w; dsc->header.h = b->h;
  dsc->data_size = (uint32_t)b->w * b->h * 2;
  dsc->data = (const uint8_t*)b->px;
  return (uint8_t*)b->px;
}

// Aeltere Versionen (0.0.4 bis 0.0.15) legten Kartenbilder im Datenlaufwerk ab (karten/*.png, *.zoom, *.map, *.big, zusammen ueber 8 MB): wegraeumen
void karteAltlastenLoeschen() {
  const char* dir = DATEN_MOUNT "/karten";
  DIR* d = opendir(dir);
  if (!d) return;
  struct dirent* e; int n = 0;
  while ((e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    char p[96]; snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
    if (unlink(p) == 0) n++;
  }
  closedir(d);
  rmdir(dir);
  if (n) logDiag("[Daten] %d alte Kartendateien geloescht", n);
}
