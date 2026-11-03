// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// OpenStreetMap-Kacheln holen, dekodieren, zwischenspeichern; Kartenbild der Route zusammensetzen.
#include "kacheln.h"
#include "app_state.h"
#include "../storage/daten.h"
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

extern "C" unsigned lodepng_decode24(unsigned char** out, unsigned* w, unsigned* h, const unsigned char* in, size_t insize);

static const int KC_N = 60;
struct KC { int16_t z; int32_t x, y; uint16_t* px; uint8_t zustand; uint32_t zeit; };   // zustand: 0 frei, 1 angefordert, 2 laeuft, 3 fertig, 4 Fehler
static KC kc[KC_N];
static SemaphoreHandle_t s_mtx = nullptr;
static bool s_arbeiter = false;

static uint32_t jetztMs() { return (uint32_t)(esp_timer_get_time() / 1000); }

// Pro Arbeiter eine dauerhafte HTTPS-Verbindung (keep-alive): der TLS-Handshake kostet auf dem Geraet mehrere Sekunden, die Kachel selbst nur Bruchteile davon.
struct KBuf { uint8_t* d = nullptr; size_t n = 0, kap = 0; };
static esp_err_t kEreignis(esp_http_client_event_t* e) {
  if (e->event_id == HTTP_EVENT_ON_DATA && e->user_data && e->data_len > 0) {
    KBuf* b = (KBuf*)e->user_data;
    if (b->n + e->data_len > 300000) return ESP_FAIL;
    if (b->n + e->data_len > b->kap) {
      size_t nk = b->kap ? b->kap * 2 : 32768; while (nk < b->n + e->data_len) nk *= 2;
      uint8_t* nd = (uint8_t*)heap_caps_realloc(b->d, nk, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!nd) return ESP_FAIL;
      b->d = nd; b->kap = nk;
    }
    memcpy(b->d + b->n, e->data, e->data_len); b->n += e->data_len;
  }
  return ESP_OK;
}

struct KVerb { esp_http_client_handle_t h = nullptr; KBuf buf; };

static bool holen(KVerb& v, int z, int x, int y, uint16_t** out) {
  *out = nullptr;
  const int n = 1 << z;
  if (y < 0 || y >= n) return false;
  x = ((x % n) + n) % n;
  char url[96]; snprintf(url, sizeof(url), "https://tile.openstreetmap.org/%d/%d/%d.png", z, x, y);
  if (!v.h) {
    esp_http_client_config_t c = {};
    c.url = url;
    c.timeout_ms = 15000;
    c.crt_bundle_attach = esp_crt_bundle_attach;
    c.event_handler = kEreignis;
    c.user_data = &v.buf;
    c.keep_alive_enable = true;
    c.buffer_size = 2048; c.buffer_size_tx = 1024;
    static char ua[80]; snprintf(ua, sizeof(ua), "WetterCube-Biker/%s (info@wettercube.de)", FIRMWARE_VERSION);
    c.user_agent = ua;
    v.h = esp_http_client_init(&c);
    if (!v.h) return false;
  } else esp_http_client_set_url(v.h, url);
  v.buf.n = 0;
  const esp_err_t e = esp_http_client_perform(v.h);
  const int code = e == ESP_OK ? esp_http_client_get_status_code(v.h) : -1;
  if (code != 200) {
    logDiag("[Kachel] %d/%d/%d: HTTP %d (%s)", z, x, y, code, esp_err_to_name(e));
    esp_http_client_cleanup(v.h); v.h = nullptr;   // naechste Kachel mit frischer Verbindung
    return false;
  }
  unsigned char* rgb = nullptr; unsigned w = 0, h = 0;
  const unsigned err = lodepng_decode24(&rgb, &w, &h, v.buf.d, v.buf.n);
  if (err || !rgb || w != KACHEL_PX || h != KACHEL_PX) { if (rgb) free(rgb); return false; }
  uint16_t* px = (uint16_t*)heap_caps_malloc(KACHEL_PX * KACHEL_PX * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!px) { free(rgb); return false; }
  for (int i = 0; i < KACHEL_PX * KACHEL_PX; i++) px[i] = ((uint16_t)(rgb[i * 3] & 0xF8) << 8) | ((uint16_t)(rgb[i * 3 + 1] & 0xFC) << 3) | (rgb[i * 3 + 2] >> 3);
  free(rgb);
  *out = px;
  return true;
}

static void arbeiterTask(void*) {
  KVerb verb;
  uint32_t zuletzt = jetztMs();
  for (;;) {
    int wahl = -1;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    for (int i = 0; i < KC_N; i++) if (kc[i].zustand == 1 && (wahl < 0 || kc[i].zeit > kc[wahl].zeit)) wahl = i;   // zuletzt angeforderte zuerst
    int z = 0, x = 0, y = 0;
    if (wahl >= 0) { kc[wahl].zustand = 2; z = kc[wahl].z; x = kc[wahl].x; y = kc[wahl].y; }
    xSemaphoreGive(s_mtx);
    if (wahl < 0) {
      vTaskDelay(pdMS_TO_TICKS(80));
      if (verb.h && jetztMs() - zuletzt > 20000) { esp_http_client_cleanup(verb.h); verb.h = nullptr; }   // Verbindung nach 20 s Ruhe schliessen
      continue;
    }
    zuletzt = jetztMs();
    uint16_t* px = nullptr;
    const bool ok = WiFi.status() == WL_CONNECTED && holen(verb, z, x, y, &px);
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    kc[wahl].px = px; kc[wahl].zustand = ok ? 3 : 4; kc[wahl].zeit = jetztMs();
    xSemaphoreGive(s_mtx);
  }
}

static void arbeiterStarten() {
  if (s_arbeiter) return;
  s_arbeiter = true;
  s_mtx = xSemaphoreCreateMutex();
  for (int i = 0; i < 3; i++) xTaskCreatePinnedToCore(arbeiterTask, "kachel", 14 * 1024, nullptr, 1, nullptr, 0);
}

static int suchen(int z, int x, int y) { for (int i = 0; i < KC_N; i++) if (kc[i].zustand && kc[i].z == z && kc[i].x == x && kc[i].y == y) return i; return -1; }

const uint16_t* kachelPixel(int z, int x, int y, bool anfordern) {
  arbeiterStarten();
  const int n = 1 << z; x = ((x % n) + n) % n;
  const uint16_t* r = nullptr;
  xSemaphoreTake(s_mtx, portMAX_DELAY);
  int i = suchen(z, x, y);
  if (i >= 0) {
    if (kc[i].zustand == 3) { kc[i].zeit = jetztMs(); r = kc[i].px; }
    else if (kc[i].zustand == 4 && anfordern && jetztMs() - kc[i].zeit > 20000) { kc[i].zustand = 1; kc[i].zeit = jetztMs(); }
  } else if (anfordern) {
    int w = -1;
    for (int k = 0; k < KC_N; k++) if (kc[k].zustand == 0) { w = k; break; }
    if (w < 0) { for (int k = 0; k < KC_N; k++) if ((kc[k].zustand == 3 || kc[k].zustand == 4) && (w < 0 || kc[k].zeit < kc[w].zeit)) w = k; }
    if (w >= 0) {
      if (kc[w].px) { heap_caps_free(kc[w].px); kc[w].px = nullptr; }
      kc[w].z = (int16_t)z; kc[w].x = x; kc[w].y = y; kc[w].zustand = 1; kc[w].zeit = jetztMs();
    }
  }
  xSemaphoreGive(s_mtx);
  return r;
}

bool kachelFehler(int z, int x, int y) {
  if (!s_mtx) return false;
  const int n = 1 << z; x = ((x % n) + n) % n;
  xSemaphoreTake(s_mtx, portMAX_DELAY);
  const int i = suchen(z, x, y);
  const bool f = i >= 0 && kc[i].zustand == 4;
  xSemaphoreGive(s_mtx);
  return f;
}

int kachelnOffen() {
  if (!s_mtx) return 0;
  int n = 0;
  xSemaphoreTake(s_mtx, portMAX_DELAY);
  for (int i = 0; i < KC_N; i++) if (kc[i].zustand == 1 || kc[i].zustand == 2) n++;
  xSemaphoreGive(s_mtx);
  return n;
}

int karteZoomFuerRoute(const Route* r, int w, int h, int rand, double* cx, double* cy) {
  float laMin = 90, laMax = -90, loMin = 180, loMax = -180;
  for (int i = 0; i < r->n; i++) {
    if (r->p[i].lat < laMin) laMin = r->p[i].lat;
    if (r->p[i].lat > laMax) laMax = r->p[i].lat;
    if (r->p[i].lon < loMin) loMin = r->p[i].lon;
    if (r->p[i].lon > loMax) loMax = r->p[i].lon;
  }
  int z = 16;
  double x0, y0, x1, y1;
  for (; z > 3; z--) {
    mercatorPx(laMax, loMin, z, &x0, &y0); mercatorPx(laMin, loMax, z, &x1, &y1);
    if (x1 - x0 <= w - 2 * rand && y1 - y0 <= h - 2 * rand) break;
  }
  mercatorPx(laMax, loMin, z, &x0, &y0); mercatorPx(laMin, loMax, z, &x1, &y1);
  *cx = (x0 + x1) / 2; *cy = (y0 + y1) / 2;
  return z;
}

// ── Kartenbild der Route ─────────────────────────────────────────────────────
static volatile int s_status = 0;
static volatile bool s_laeuft = false;
static char s_id[12];

// Alle Kacheln eines Bereichs anfordern, abwarten und in einen RGB565-Puffer w x h setzen (ox, oy = Welt-Pixel der linken oberen Ecke)
static uint16_t* komponieren(int z, double ox, double oy, int w, int h, uint32_t frist) {
  const int tx0 = (int)floor(ox / KACHEL_PX), tx1 = (int)floor((ox + w) / KACHEL_PX);
  const int ty0 = (int)floor(oy / KACHEL_PX), ty1 = (int)floor((oy + h) / KACHEL_PX);
  const uint32_t t0 = jetztMs();
  bool alle = false;
  while (!alle && jetztMs() - t0 < frist) {
    alle = true;
    for (int tx = tx0; tx <= tx1; tx++) for (int ty = ty0; ty <= ty1; ty++) {
      if (!kachelPixel(z, tx, ty, true) && !kachelFehler(z, tx, ty)) alle = false;
    }
    if (!alle) vTaskDelay(pdMS_TO_TICKS(150));
  }
  uint16_t* bild = (uint16_t*)heap_caps_malloc((size_t)w * h * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!bild) return nullptr;
  for (size_t i = 0; i < (size_t)w * h; i++) bild[i] = 0xE73D;   // hellgrau, wo eine Kachel fehlt
  int geladen = 0;
  for (int tx = tx0; tx <= tx1; tx++) for (int ty = ty0; ty <= ty1; ty++) {
    const uint16_t* k = kachelPixel(z, tx, ty, false);
    if (!k) continue;
    geladen++;
    const int px0 = (int)lround(tx * (double)KACHEL_PX - ox), py0 = (int)lround(ty * (double)KACHEL_PX - oy);
    for (int yy = 0; yy < KACHEL_PX; yy++) {
      const int ya = py0 + yy; if (ya < 0 || ya >= h) continue;
      for (int xx = 0; xx < KACHEL_PX; xx++) { const int xa = px0 + xx; if (xa < 0 || xa >= w) continue; bild[(size_t)ya * w + xa] = k[yy * KACHEL_PX + xx]; }
    }
  }
  if (geladen == 0) { heap_caps_free(bild); return nullptr; }
  return bild;
}

// Beide Karten haben dieselbe Zoomstufe: hoechstens 10, kleiner, wenn die Strecke sonst nicht ins Kartenbild passt. Zuerst das kleine Bild
// (Routenwetter, wenige Kacheln, ist schnell fertig), danach das grosse (Kartenseite).
static void erzeugenTask(void*) {
  const uint32_t tStart = jetztMs();
  Route* r = new Route();
  bool ok = routeLaden(s_id, r);
  int z = 0;
  if (ok) {
    double cx, cy;
    z = karteZoomFuerRoute(r, KARTE_B, KARTE_H, 30, &cx, &cy);
    if (z > 10) { z = 10; double x0, y0, x1, y1; float laMin = 90, laMax = -90, loMin = 180, loMax = -180;
      for (int i = 0; i < r->n; i++) { if (r->p[i].lat < laMin) laMin = r->p[i].lat; if (r->p[i].lat > laMax) laMax = r->p[i].lat; if (r->p[i].lon < loMin) loMin = r->p[i].lon; if (r->p[i].lon > loMax) loMax = r->p[i].lon; }
      mercatorPx(laMax, loMin, z, &x0, &y0); mercatorPx(laMin, loMax, z, &x1, &y1); cx = (x0 + x1) / 2; cy = (y0 + y1) / 2; }
    uint16_t* klein = komponieren(z, cx - KARTE_B / 2.0, cy - KARTE_H / 2.0, KARTE_B, KARTE_H, 45000);
    ok = klein && karteBildSpeichern(s_id, false, z, KARTE_B, KARTE_H, klein);
    if (klein) heap_caps_free(klein);
    if (ok) {
      logDiag("[Karte] kleines Bild nach %u ms (Zoom %d)", (unsigned)(jetztMs() - tStart), z);
      s_status = 2;   // das kleine Bild ist fertig: Routenwetter zeigt es sofort
      uint16_t* gross = komponieren(z, cx - KARTE_GB / 2.0, cy - KARTE_GH / 2.0, KARTE_GB, KARTE_GH, 60000);
      if (gross) { karteBildSpeichern(s_id, true, z, KARTE_GB, KARTE_GH, gross); heap_caps_free(gross); }
      logDiag("[Karte] grosses Bild nach %u ms", (unsigned)(jetztMs() - tStart));
    }
  }
  routeFrei(r); delete r;
  logDiag("[Karte] Kartenbild der Route: %s (Zoom %d)", ok ? "fertig" : "fehlgeschlagen", z);
  if (!ok) s_status = 3;
  s_laeuft = false;
  vTaskDelete(nullptr);
}

void karteErzeugenStarten(const char* routeId) {
  if (s_laeuft || !routeId) return;
  strlcpy(s_id, routeId, sizeof(s_id));
  s_status = 1; s_laeuft = true;
  if (xTaskCreatePinnedToCore(erzeugenTask, "kartenbild", 12 * 1024, nullptr, 1, nullptr, 0) != pdPASS) { s_status = 3; s_laeuft = false; }
}

bool karteStatusLaeuft() { return s_laeuft; }

int karteStatus() {
  const int s = s_status;
  if (s == 2 || s == 3) s_status = 0;
  return s;
}
