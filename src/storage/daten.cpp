// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Datenlaufwerk im Flash (siehe daten.h)
#include "daten.h"
#include "../app/app_state.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_vfs_fat.h"
#include "esp_partition.h"
#include "wear_levelling.h"

static wl_handle_t s_wl = WL_INVALID_HANDLE;
static bool s_bereit = false;
static char s_status[80] = "nicht eingehaengt";

bool datenBereit() { return s_bereit; }
const char* datenStatusText() { return s_status; }

bool datenInfo(uint64_t* gesamt, uint64_t* frei) {
  if (!s_bereit) return false;
  uint64_t g = 0, f = 0;
  if (esp_vfs_fat_info(DATEN_MOUNT, &g, &f) != ESP_OK) return false;
  if (gesamt) *gesamt = g;
  if (frei) *frei = f;
  return true;
}

// Schreib-/Lesetest: 4 KB mit Muster schreiben, zuruecklesen, loeschen. Zeigt, ob der Flash oberhalb von 16 MB beschreibbar ist.
static bool selbsttest(char* fehler, size_t n) {
  static const char* DATEI = DATEN_MOUNT "/.test";
  uint8_t puffer[512];
  FILE* f = fopen(DATEI, "wb");
  if (!f) { snprintf(fehler, n, "Datei anlegen fehlgeschlagen"); return false; }
  for (int i = 0; i < 8; i++) {
    for (int k = 0; k < 512; k++) puffer[k] = (uint8_t)(k * 7 + i * 13);
    if (fwrite(puffer, 1, 512, f) != 512) { fclose(f); snprintf(fehler, n, "Schreiben fehlgeschlagen"); return false; }
  }
  fclose(f);
  f = fopen(DATEI, "rb");
  if (!f) { snprintf(fehler, n, "Datei lesen fehlgeschlagen"); return false; }
  for (int i = 0; i < 8; i++) {
    if (fread(puffer, 1, 512, f) != 512) { fclose(f); snprintf(fehler, n, "Lesen fehlgeschlagen"); return false; }
    for (int k = 0; k < 512; k++) if (puffer[k] != (uint8_t)(k * 7 + i * 13)) { fclose(f); snprintf(fehler, n, "Daten stimmen nicht"); return false; }
  }
  fclose(f);
  unlink(DATEI);
  return true;
}

bool datenInit() {
  const esp_partition_t* p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "daten");
  if (!p) {
    strlcpy(s_status, "Partition fehlt (Partitionstabelle per USB aufspielen)", sizeof(s_status));
    logDiag("[Daten] %s", s_status);
    return false;
  }
  esp_vfs_fat_mount_config_t cfg = {};
  cfg.format_if_mount_failed = true;   // beim ersten Start ist die Partition leer: formatieren
  cfg.max_files = 6;
  cfg.allocation_unit_size = 0;        // Standard
  esp_err_t e = esp_vfs_fat_spiflash_mount_rw_wl(DATEN_MOUNT, "daten", &cfg, &s_wl);
  if (e != ESP_OK) {
    snprintf(s_status, sizeof(s_status), "Fehler beim Einhaengen: %s", esp_err_to_name(e));
    logDiag("[Daten] %s (Partition bei 0x%x, %u KB)", s_status, (unsigned)p->address, (unsigned)(p->size / 1024));
    s_wl = WL_INVALID_HANDLE;
    return false;
  }
  char fehler[48] = "";
  if (!selbsttest(fehler, sizeof(fehler))) {
    snprintf(s_status, sizeof(s_status), "Fehler: %s", fehler);
    logDiag("[Daten] Selbsttest nicht bestanden: %s (Partition bei 0x%x)", fehler, (unsigned)p->address);
    esp_vfs_fat_spiflash_unmount_rw_wl(DATEN_MOUNT, s_wl); s_wl = WL_INVALID_HANDLE;
    return false;
  }
  s_bereit = true;
  uint64_t g = 0, f = 0;
  if (datenInfo(&g, &f)) snprintf(s_status, sizeof(s_status), "bereit, %.1f MB frei von %.1f MB", f / 1048576.0, g / 1048576.0);
  else strlcpy(s_status, "bereit", sizeof(s_status));
  logDiag("[Daten] Datenlaufwerk %s (Partition bei 0x%x, oberhalb 16 MB: %s)", s_status, (unsigned)p->address, p->address >= 0x1000000 ? "ja" : "nein");
  return true;
}
