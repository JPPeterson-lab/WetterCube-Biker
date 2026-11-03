// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Routen: Speicher (Datenlaufwerk), Routing (Valhalla, Kostenmodell Motorrad), GPX-Parser, Wetter je Abschnitt, Score und Abfahrtszeit-Planer.
#include "route.h"
#include "kacheln.h"
#include "app_state.h"
#include "../storage/daten.h"
#include <ArduinoJson.h>
#include <math.h>
#include <new>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

bool punktAuswerten(JsonVariantConst o, BikerPunkt* p);   // data_stunden.cpp

RAnalyse* routeAkt = nullptr;
RErgebnis routeErg;
int       routePlaner[PLANER_STUNDEN];
time_t    routePlanerStart = 0;
int       routeBesteIdx = -1, routeJetztScore = -1;
int       routeTempo = 60;
time_t    routeAbfahrt = 0;
volatile int routeStatus = RS_LEER;
char      routeStatusText[96] = "";
RouteOrtWahl routeOrtWahl[3];

static const char* ROUTEN_DIR = DATEN_MOUNT "/routen";
static const char* VALHALLA = "https://valhalla1.openstreetmap.de/route";

// ── Geometrie ────────────────────────────────────────────────────────────────
static const double RAD = M_PI / 180.0;
static float abstandKm(float la1, float lo1, float la2, float lo2) {
  double dla = (la2 - la1) * RAD, dlo = (lo2 - lo1) * RAD;
  double a = sin(dla / 2) * sin(dla / 2) + cos(la1 * RAD) * cos(la2 * RAD) * sin(dlo / 2) * sin(dlo / 2);
  return (float)(6371.0 * 2 * atan2(sqrt(a), sqrt(1 - a)));
}
static int kursGrad(float la1, float lo1, float la2, float lo2) {
  double dlo = (lo2 - lo1) * RAD;
  double y = sin(dlo) * cos(la2 * RAD), x = cos(la1 * RAD) * sin(la2 * RAD) - sin(la1 * RAD) * cos(la2 * RAD) * cos(dlo);
  int k = (int)lround(atan2(y, x) / RAD);
  return (k + 360) % 360;
}

void sonnenStand(double lat, double lon, time_t t, double* hoehe, double* azimut) {
  const double jd = (double)t / 86400.0 + 2440587.5, n = jd - 2451545.0;
  double L = fmod(280.460 + 0.9856474 * n, 360.0), g = fmod(357.528 + 0.9856003 * n, 360.0) * RAD;
  double lam = (L + 1.915 * sin(g) + 0.020 * sin(2 * g)) * RAD, eps = (23.439 - 0.0000004 * n) * RAD;
  double alpha = atan2(cos(eps) * sin(lam), cos(lam)), delta = asin(sin(eps) * sin(lam));
  double gmst = fmod(280.46061837 + 360.98564736629 * n, 360.0);
  double H = (gmst + lon) * RAD - alpha;
  double la = lat * RAD;
  *hoehe = asin(sin(la) * sin(delta) + cos(la) * cos(delta) * cos(H)) / RAD;
  double az = atan2(sin(H), cos(H) * sin(la) - tan(delta) * cos(la)) / RAD + 180.0;
  *azimut = fmod(az + 360.0, 360.0);
}

// ── Speicher im PSRAM ────────────────────────────────────────────────────────
template <typename T> static T* psAlloc(size_t n) { return (T*)heap_caps_calloc(n, sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }

void routeFrei(Route* r) {
  if (!r) return;
  if (r->p) heap_caps_free(r->p);
  r->p = nullptr; r->n = 0;
}

// Sammelt Punkte (im PSRAM, waechst bei Bedarf) und laesst zu dichte Punkte weg
struct Sammler {
  RoutePunkt* p = nullptr; int n = 0, kap = 0; float minKm; bool ele = false;
  explicit Sammler(float minAbstandKm = 0.03f) : minKm(minAbstandKm) {}
  ~Sammler() { if (p) heap_caps_free(p); }
  bool add(float la, float lo, int16_t e) {
    if (n > 0 && abstandKm(p[n - 1].lat, p[n - 1].lon, la, lo) < minKm) return true;
    if (n >= kap) {
      int nk = kap ? kap * 2 : 2048; if (nk > 60000) return false;
      RoutePunkt* np = (RoutePunkt*)heap_caps_realloc(p, nk * sizeof(RoutePunkt), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!np) return false;
      p = np; kap = nk;
    }
    p[n].lat = la; p[n].lon = lo; p[n].ele = e; p[n].pad = 0;
    if (e != -32768) ele = true;
    n++;
    return true;
  }
};

// Auf hoechstens ROUTE_MAX_PUNKTE gleichmaessig (nach Strecke) ausduennen und in r uebernehmen; setzt r->km
static bool inRoute(Sammler& s, Route* r) {
  if (s.n < 2) return false;
  float gesamt = 0;
  for (int i = 1; i < s.n; i++) gesamt += abstandKm(s.p[i - 1].lat, s.p[i - 1].lon, s.p[i].lat, s.p[i].lon);
  const float abstand = gesamt / (ROUTE_MAX_PUNKTE - 1);
  RoutePunkt* out = psAlloc<RoutePunkt>(ROUTE_MAX_PUNKTE + 1);
  if (!out) return false;
  int m = 0; float seit = 0;
  out[m++] = s.p[0];
  for (int i = 1; i < s.n && m < ROUTE_MAX_PUNKTE; i++) {
    seit += abstandKm(s.p[i - 1].lat, s.p[i - 1].lon, s.p[i].lat, s.p[i].lon);
    if (seit >= abstand || i == s.n - 1) { out[m++] = s.p[i]; seit = 0; }
  }
  if (out[m - 1].lat != s.p[s.n - 1].lat || out[m - 1].lon != s.p[s.n - 1].lon) { if (m > ROUTE_MAX_PUNKTE) m = ROUTE_MAX_PUNKTE; out[m - 1] = s.p[s.n - 1]; }
  routeFrei(r);
  r->p = out; r->n = m; r->km = gesamt;
  return true;
}

// ── Speicherung ──────────────────────────────────────────────────────────────
struct Kopf { uint32_t magic; char name[40]; char start[32]; char ziel[32]; float km; uint32_t sek; uint16_t n; uint8_t gpx, res; };
static const uint32_t MAGIC = 0x31524357;   // "WCR1"

static void verzeichnisAnlegen() { mkdir(DATEN_MOUNT, 0777); mkdir(ROUTEN_DIR, 0777); }

static int idListe(char ids[][12], int max) {   // sortiert aufsteigend
  int n = 0;
  DIR* d = opendir(ROUTEN_DIR);
  if (!d) return 0;
  struct dirent* e;
  while ((e = readdir(d)) && n < max) {
    size_t l = strlen(e->d_name);
    if (l > 3 && l < 16 && !strcmp(e->d_name + l - 3, ".rt")) { memset(ids[n], 0, 12); strncpy(ids[n], e->d_name, l - 3 > 11 ? 11 : l - 3); n++; }
  }
  closedir(d);
  for (int i = 1; i < n; i++) for (int j = i; j > 0 && strcmp(ids[j - 1], ids[j]) > 0; j--) { char t[12]; memcpy(t, ids[j], 12); memcpy(ids[j], ids[j - 1], 12); memcpy(ids[j - 1], t, 12); }
  return n;
}

int routenAnzahl() { char ids[ROUTE_MAX_GESPEICHERT][12]; return idListe(ids, ROUTE_MAX_GESPEICHERT); }

static bool kopfLesen(const char* id, Kopf* k) {
  char pfad[64]; snprintf(pfad, sizeof(pfad), "%s/%s.rt", ROUTEN_DIR, id);
  FILE* f = fopen(pfad, "rb");
  if (!f) return false;
  bool ok = fread(k, sizeof(Kopf), 1, f) == 1 && k->magic == MAGIC;
  fclose(f);
  return ok;
}

bool routenInfo(int i, char* id, size_t idN, char* name, size_t nameN, float* km, uint32_t* sek, bool* gpx) {   // i = 0: neueste
  char ids[ROUTE_MAX_GESPEICHERT][12];
  int n = idListe(ids, ROUTE_MAX_GESPEICHERT);
  if (i < 0 || i >= n) return false;
  const char* wahl = ids[n - 1 - i];
  Kopf k;
  if (!kopfLesen(wahl, &k)) return false;
  if (id) strlcpy(id, wahl, idN);
  if (name) strlcpy(name, k.name, nameN);
  if (km) *km = k.km;
  if (sek) *sek = k.sek;
  if (gpx) *gpx = k.gpx != 0;
  return true;
}

bool routeLaden(const char* id, Route* r) {
  Kopf k;
  char pfad[64]; snprintf(pfad, sizeof(pfad), "%s/%s.rt", ROUTEN_DIR, id);
  FILE* f = fopen(pfad, "rb");
  if (!f) return false;
  bool ok = fread(&k, sizeof(Kopf), 1, f) == 1 && k.magic == MAGIC && k.n >= 2 && k.n <= ROUTE_MAX_PUNKTE + 1;
  if (ok) {
    routeFrei(r);
    r->p = psAlloc<RoutePunkt>(k.n);
    ok = r->p && fread(r->p, sizeof(RoutePunkt), k.n, f) == (size_t)k.n;
    if (ok) {
      r->n = k.n; r->km = k.km; r->sekundenDienst = k.sek; r->gpx = k.gpx != 0;
      strlcpy(r->name, k.name, sizeof(r->name)); strlcpy(r->start, k.start, sizeof(r->start)); strlcpy(r->ziel, k.ziel, sizeof(r->ziel));
      strlcpy(r->id, id, sizeof(r->id));
    }
  }
  fclose(f);
  if (!ok) routeFrei(r);
  return ok;
}

bool routeSpeichern(Route& r) {
  if (!datenBereit() || r.n < 2) return false;
  verzeichnisAnlegen();
  char ids[ROUTE_MAX_GESPEICHERT][12];
  int n = idListe(ids, ROUTE_MAX_GESPEICHERT);
  if (n >= ROUTE_MAX_GESPEICHERT) return false;
  if (!r.id[0]) {
    unsigned v = (unsigned)(time(nullptr) & 0x0fffffff);
    for (;; v++) {
      snprintf(r.id, sizeof(r.id), "r%07x", v);
      char pfad[64]; snprintf(pfad, sizeof(pfad), "%s/%s.rt", ROUTEN_DIR, r.id);
      struct stat st; if (stat(pfad, &st) != 0) break;
    }
  }
  Kopf k = {}; k.magic = MAGIC;
  strlcpy(k.name, r.name, sizeof(k.name)); strlcpy(k.start, r.start, sizeof(k.start)); strlcpy(k.ziel, r.ziel, sizeof(k.ziel));
  k.km = r.km; k.sek = r.sekundenDienst; k.n = (uint16_t)r.n; k.gpx = r.gpx ? 1 : 0;
  char pfad[64]; snprintf(pfad, sizeof(pfad), "%s/%s.rt", ROUTEN_DIR, r.id);
  FILE* f = fopen(pfad, "wb");
  if (!f) return false;
  bool ok = fwrite(&k, sizeof(k), 1, f) == 1 && fwrite(r.p, sizeof(RoutePunkt), r.n, f) == (size_t)r.n;
  fclose(f);
  if (!ok) unlink(pfad);
  return ok;
}

bool routeLoeschen(const char* id) {
  karteLoeschen(id);
  char pfad[64]; snprintf(pfad, sizeof(pfad), "%s/%s.rt", ROUTEN_DIR, id);
  return unlink(pfad) == 0;
}

// ── Routing (Valhalla) ───────────────────────────────────────────────────────
static void polylineDekodieren(const char* s, Sammler& out) {
  int idx = 0, lat = 0, lon = 0; const int len = (int)strlen(s);
  while (idx < len) {
    for (int k = 0; k < 2; k++) {
      int shift = 0, res = 0, b;
      do { b = s[idx++] - 63; res |= (b & 0x1f) << shift; shift += 5; } while (b >= 0x20 && idx < len);
      int d = (res & 1) ? ~(res >> 1) : (res >> 1);
      if (k == 0) lat += d; else lon += d;
    }
    out.add((float)(lat / 1e6), (float)(lon / 1e6), -32768);
  }
}

// Text in ein char-Feld kopieren, ohne ein UTF-8-Zeichen (Umlaut) in der Mitte abzuschneiden
static void kuerzen(char* ziel, size_t n, const String& s) {
  String t = s; t.trim();
  size_t l = t.length() < n - 1 ? t.length() : n - 1;
  if (l < t.length()) while (l > 0 && ((uint8_t)t[l] & 0xC0) == 0x80) l--;   // l zeigt auf ein Folgebyte: zurueck zum Zeichenanfang
  memcpy(ziel, t.c_str(), l); ziel[l] = 0;
}
// Kurzname eines Orts fuer den Routennamen: der Teil vor dem ersten Komma ("Fackenburger Allee, 23554 Luebeck" -> "Fackenburger Allee")
static String kurzOrt(const String& s) {
  String t = s; t.trim();
  int k = t.indexOf(',');
  if (k > 0) t = t.substring(0, k);
  // hoechstens 18 Zeichen (UTF-8: Folgebytes nicht mitzaehlen), damit "Start - Ziel" in den 39 Bytes des Routennamens passt
  String o; int z = 0;
  for (size_t i = 0; i < t.length(); i++) {
    const uint8_t c = (uint8_t)t[i];
    if ((c & 0xC0) != 0x80) { if (z >= 18) break; z++; }
    o += (char)c;
  }
  o.trim();
  return o;
}

bool routeAusOrten(const String& name, const String& start, const String& ziel, const String& via, bool autobahnMeiden, Route* r, String& fehler) {
  float la[3], lo[3]; int k = 0;
  struct { const String* s; const char* wer; } orte[3] = {{&start, "Start"}, {&via, "Zwischenziel"}, {&ziel, "Ziel"}};
  for (int i = 0; i < 3; i++) {
    String t = *orte[i].s; t.trim();
    if (i == 1 && t.isEmpty()) continue;
    if (t.isEmpty()) { fehler = String(orte[i].wer) + " fehlt"; return false; }
    const RouteOrtWahl& w = routeOrtWahl[i];
    if (w.gesetzt && t == w.text) { la[k] = w.lat; lo[k] = w.lon; k++; continue; }   // aus den Vorschlaegen gewaehlt
    snprintf(routeStatusText, sizeof(routeStatusText), "Suche %s: %s ...", orte[i].wer, t.c_str());
    if (!geocodeAdresse(t, la[k], lo[k])) { fehler = String(orte[i].wer) + " '" + t + "' nicht gefunden. Tipp: Ort oder Straße mit Ort eingeben."; return false; }
    k++;
  }
  String body = "{\"locations\":[";
  for (int i = 0; i < k; i++) { if (i) body += ","; body += "{\"lat\":" + String(la[i], 5) + ",\"lon\":" + String(lo[i], 5) + "}"; }
  body += "],\"costing\":\"motorcycle\",";
  if (autobahnMeiden) body += "\"costing_options\":{\"motorcycle\":{\"use_highways\":0.0}},";
  body += "\"directions_type\":\"none\",\"shape_format\":\"polyline6\",\"units\":\"kilometers\"}";

  snprintf(routeStatusText, sizeof(routeStatusText), "Berechne Strecke (Motorrad) ...");
  WiFiClientSecure sc; HTTPClient http;
  http.begin(sc, VALHALLA);
  http.setTimeout(40000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", String("WetterCube-Biker/") + FIRMWARE_VERSION + " (info@wettercube.de)");
  int code = http.POST(body);
  if (code != 200) {
    logHttpFehler("Route", code, sc);
    http.end();
    fehler = code == 400 ? "Keine Strecke gefunden (Start oder Ziel nicht befahrbar)" : "Routing-Dienst nicht erreichbar (HTTP " + String(code) + ")";
    return false;
  }
  String antwort = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, antwort) != DeserializationError::Ok) { fehler = "Antwort des Routing-Dienstes nicht lesbar"; return false; }
  antwort = String();
  JsonArrayConst legs = doc["trip"]["legs"];
  if (legs.isNull() || legs.size() == 0) { fehler = "Keine Strecke gefunden"; return false; }
  Sammler s(0.05f);
  for (JsonObjectConst leg : legs) polylineDekodieren(leg["shape"] | "", s);
  float km = doc["trip"]["summary"]["length"] | 0.0f;
  uint32_t sek = (uint32_t)(doc["trip"]["summary"]["time"] | 0.0f);
  if (!inRoute(s, r)) { fehler = "Strecke zu kurz oder kein Speicher"; return false; }
  if (km > 0) r->km = km;
  r->sekundenDienst = sek; r->gpx = false;
  kuerzen(r->name, sizeof(r->name), name.isEmpty() ? kurzOrt(start) + " - " + kurzOrt(ziel) : name);
  kuerzen(r->start, sizeof(r->start), start); kuerzen(r->ziel, sizeof(r->ziel), ziel);
  r->id[0] = 0;
  return true;
}

// ── GPX (Stueck fuer Stueck) ─────────────────────────────────────────────────
static struct GpxZustand {
  Sammler* trk = nullptr; Sammler* rte = nullptr;
  char tag[240]; int tagN = 0; bool inTag = false;
  char txt[24]; int txtN = 0; bool inEle = false;
  bool inPt = false; int kind = 0; float lat = 0, lon = 0; int16_t ele = -32768;
  char name[40]; bool inName = false;
} G;

static float attrWert(const char* tag, const char* key, bool* ok) {
  const char* p = strstr(tag, key);
  *ok = false;
  if (!p) return 0;
  p += strlen(key);
  while (*p == ' ' || *p == '=') p++;
  if (*p == '"' || *p == '\'') p++;
  char* ende; float v = strtof(p, &ende);
  *ok = ende != p;
  return v;
}

static void gpxPunktFertig() {
  Sammler* s = G.kind == 1 ? G.trk : G.rte;
  if (s) s->add(G.lat, G.lon, G.ele);
  G.inPt = false;
}

static void gpxTag(const char* tag) {
  while (*tag == ' ') tag++;
  const bool trk = !strncmp(tag, "trkpt", 5), rte = !strncmp(tag, "rtept", 5);
  if (trk || rte) {
    bool o1, o2;
    float la = attrWert(tag, "lat", &o1), lo = attrWert(tag, "lon", &o2);
    if (o1 && o2) {
      G.lat = la; G.lon = lo; G.ele = -32768; G.kind = trk ? 1 : 2; G.inPt = true;
      if (tag[strlen(tag) - 1] == '/') gpxPunktFertig();
    }
  } else if (!strncmp(tag, "/trkpt", 6) || !strncmp(tag, "/rtept", 6)) {
    if (G.inPt) gpxPunktFertig();
  } else if (!strcmp(tag, "ele") && G.inPt) { G.inEle = true; G.txtN = 0; }
  else if (!strcmp(tag, "name") && !G.name[0]) { G.inName = true; G.txtN = 0; }
}

void gpxBegin() {
  if (G.trk) { delete G.trk; } if (G.rte) { delete G.rte; }
  G.trk = new Sammler(0.03f); G.rte = new Sammler(0.03f);
  G.tagN = 0; G.inTag = false; G.txtN = 0; G.inEle = false; G.inPt = false; G.name[0] = 0; G.inName = false;
}

void gpxFeed(const char* d, size_t n) {
  for (size_t i = 0; i < n; i++) {
    char c = d[i];
    if (c == '<') {
      if (G.inEle) { G.txt[G.txtN] = 0; G.ele = (int16_t)lroundf(strtof(G.txt, nullptr)); G.inEle = false; }
      if (G.inName) { G.txt[G.txtN] = 0; strlcpy(G.name, G.txt, sizeof(G.name)); G.inName = false; }
      G.inTag = true; G.tagN = 0;
    } else if (G.inTag) {
      if (c == '>') { G.tag[G.tagN] = 0; G.inTag = false; gpxTag(G.tag); }
      else if (G.tagN < (int)sizeof(G.tag) - 1) G.tag[G.tagN++] = c;
    } else if ((G.inEle || G.inName) && G.txtN < (int)sizeof(G.txt) - 1) G.txt[G.txtN++] = c;
  }
}

bool gpxEnde(const String& name, Route* r, String& fehler) {
  Sammler* s = nullptr;
  if (G.trk && G.trk->n >= 2) s = G.trk; else if (G.rte && G.rte->n >= 2) s = G.rte;
  if (!s) { fehler = "Keine Streckenpunkte in der Datei (erwartet: Track oder Route in GPX)"; return false; }
  bool ok = inRoute(*s, r);
  if (!ok) fehler = "Zu wenig Speicher";
  else {
    r->gpx = true; r->sekundenDienst = 0; r->id[0] = 0;
    kuerzen(r->name, sizeof(r->name), !name.isEmpty() ? name : (G.name[0] ? String(G.name) : String("GPX-Route")));
    strlcpy(r->start, "GPX", sizeof(r->start)); strlcpy(r->ziel, "", sizeof(r->ziel));
  }
  delete G.trk; delete G.rte; G.trk = G.rte = nullptr;
  return ok;
}

// ── Wetter je Abschnitt ──────────────────────────────────────────────────────
static void freiAnalyse(RAnalyse* a) {
  if (!a) return;
  for (int i = 0; i < ROUTE_MAX_ABSCHNITTE; i++) if (a->wx[i]) heap_caps_free(a->wx[i]);
  if (a->route) { routeFrei(a->route); delete a->route; }
  heap_caps_free(a);
}

bool routeWetterLaden(Route* r, RAnalyse** out, String& fehler) {
  if (!r || r->n < 2) { fehler = "Keine Route"; return false; }
  RAnalyse* a = (RAnalyse*)heap_caps_calloc(1, sizeof(RAnalyse), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!a) { fehler = "Zu wenig Speicher"; return false; }
  new (a) RAnalyse();
  a->route = r;

  // Abschnitte nach Strecke
  float* cum = psAlloc<float>(r->n);
  if (!cum) { fehler = "Zu wenig Speicher"; a->route = nullptr; freiAnalyse(a); return false; }
  for (int i = 1; i < r->n; i++) cum[i] = cum[i - 1] + abstandKm(r->p[i - 1].lat, r->p[i - 1].lon, r->p[i].lat, r->p[i].lon);
  const float gesamt = cum[r->n - 1];
  int nA = (int)ceilf(gesamt / 20.0f);
  if (nA < 1) nA = 1;
  if (nA > ROUTE_MAX_ABSCHNITTE) nA = ROUTE_MAX_ABSCHNITTE;
  a->n = nA;
  const float laenge = gesamt / nA;
  auto punktBei = [&](float km, float* la, float* lo) {
    int i = 1; while (i < r->n - 1 && cum[i] < km) i++;
    float d = cum[i] - cum[i - 1], f = d > 0 ? (km - cum[i - 1]) / d : 0;
    *la = r->p[i - 1].lat + (r->p[i].lat - r->p[i - 1].lat) * f; *lo = r->p[i - 1].lon + (r->p[i].lon - r->p[i - 1].lon) * f;
  };
  for (int s = 0; s < nA; s++) {
    RAbschnitt& b = a->a[s];
    b.km0 = s * laenge; b.km1 = (s + 1) * laenge; b.kmMitte = (b.km0 + b.km1) * 0.5f;
    punktBei(b.kmMitte, &b.lat, &b.lon);
    float la0, lo0, la1, lo1; punktBei(b.km0 + laenge * 0.25f, &la0, &lo0); punktBei(b.km1 - laenge * 0.25f, &la1, &lo1);
    b.kurs = (int16_t)kursGrad(la0, lo0, la1, lo1);
    int i0 = 0; while (i0 < r->n - 1 && cum[i0] < b.km0) i0++;
    int i1 = i0; while (i1 < r->n - 1 && cum[i1] < b.km1) i1++;
    b.i0 = i0; b.i1 = i1;
  }

  // ein Abruf fuer alle Abschnitte
  snprintf(routeStatusText, sizeof(routeStatusText), "Hole Wetter fuer %d Abschnitte ...", nA);
  String la, lo;
  for (int s = 0; s < nA; s++) { if (s) { la += ","; lo += ","; } la += String(a->a[s].lat, 3); lo += String(a->a[s].lon, 3); }
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + la + "&longitude=" + lo;
  url += "&hourly=temperature_2m,apparent_temperature,precipitation,precipitation_probability,weather_code,wind_speed_10m,wind_gusts_10m,"
         "wind_direction_10m,visibility,relative_humidity_2m,cloud_cover,is_day";
  url += "&daily=sunrise,sunset&timezone=auto&forecast_days=3";
  WiFiClientSecure sc; HTTPClient http;
  http.begin(sc, url);
  http.setTimeout(30000);
  int code = http.GET();
  if (code != 200) {
    logHttpFehler("RouteWetter", code, sc);
    http.end();
    fehler = "Wetterdienst nicht erreichbar (HTTP " + String(code) + ")";
    heap_caps_free(cum); a->route = nullptr; freiAnalyse(a);
    return false;
  }
  String body = http.getString();
  http.end();
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  body = String();
  if (err != DeserializationError::Ok) { fehler = String("Wetterdaten nicht lesbar: ") + err.c_str(); heap_caps_free(cum); a->route = nullptr; freiAnalyse(a); return false; }
  for (int s = 0; s < nA; s++) {
    JsonVariantConst o = doc.as<JsonVariantConst>();
    if (nA > 1) o = doc[s];
    a->wx[s] = (BikerPunkt*)heap_caps_calloc(1, sizeof(BikerPunkt), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!a->wx[s]) { fehler = "Zu wenig Speicher"; heap_caps_free(cum); a->route = nullptr; freiAnalyse(a); return false; }
    new (a->wx[s]) BikerPunkt();
    if (!punktAuswerten(o, a->wx[s])) { fehler = "Wetterdaten unvollstaendig"; heap_caps_free(cum); a->route = nullptr; freiAnalyse(a); return false; }
    a->a[s].hoehe = (int16_t)lroundf(a->wx[s]->hoehe);
  }

  // Hoehenprofil: aus der GPX-Datei, sonst aus den Abschnitten
  bool gpxEle = false; int eleN = 0;
  for (int i = 0; i < r->n; i++) if (r->p[i].ele != -32768) eleN++;
  gpxEle = eleN > r->n / 2;
  int hMin = 32000, hMax = -32000, anstieg = 0, letzte = 0; bool erste = true;
  if (gpxEle) {
    float geglaettet = 0;
    for (int i = 0; i < r->n; i++) {
      if (r->p[i].ele == -32768) continue;
      geglaettet = erste ? r->p[i].ele : geglaettet * 0.8f + r->p[i].ele * 0.2f;
      int h = (int)lroundf(geglaettet);
      if (h < hMin) hMin = h;
      if (h > hMax) hMax = h;
      if (!erste && h > letzte + 2) anstieg += h - letzte;
      if (erste || h > letzte + 2 || h < letzte - 2) letzte = h;
      erste = false;
    }
  } else {
    for (int s = 0; s < nA; s++) {
      int h = a->a[s].hoehe;
      if (h < hMin) hMin = h;
      if (h > hMax) hMax = h;
      if (s > 0 && h > letzte) anstieg += h - letzte;
      letzte = h;
    }
  }
  a->hMin = hMin == 32000 ? 0 : hMin; a->hMax = hMax == -32000 ? 0 : hMax; a->anstieg = anstieg; a->hoeheAusGpx = gpxEle;
  heap_caps_free(cum);
  *out = a;
  return true;
}

// ── Bewertung ────────────────────────────────────────────────────────────────
static float winkelDiff(float a, float b) { float d = fabsf(fmodf(a - b + 540.0f, 360.0f) - 180.0f); return d; }

void routeBewerten(RAnalyse* a, time_t abfahrt, int tempo, RErgebnis* e, int pauseKm, int pauseMin) {
  *e = RErgebnis();
  if (!a || !a->route || a->n < 1 || !a->wx[0]) return;
  if (tempo < 30) tempo = 30;
  if (tempo > 140) tempo = 140;
  const Route* r = a->route;
  e->abfahrt = abfahrt; e->tempo = tempo; e->n = a->n;
  const float stunden = r->km / tempo;
  e->ankunft = abfahrt + (time_t)(stunden * 3600.0f) + (pauseKm >= 0 ? (time_t)pauseMin * 60 : 0);
  int summe = 0; float langsSumme = 0;
  for (int i = 0; i < a->n; i++) {
    const RAbschnitt& b = a->a[i];
    time_t t = abfahrt + (time_t)(b.kmMitte / tempo * 3600.0f);
    if (pauseKm >= 0 && b.km0 >= pauseKm) t += (time_t)pauseMin * 60;   // nach der Pause
    e->zeit[i] = t;
    const BikerPunkt* w = a->wx[i];
    int idx = (int)((t - w->h[0].t) / 3600); if (idx < 0) idx = 0; if (idx >= w->n) idx = w->n - 1;
    BikerStunde h = w->h[idx];
    float langs, quer; bikerWindKomponenten(h.wind, h.wdir, b.kurs, &langs, &quer);
    const float boeFaktor = h.wind > 1 ? h.gust / h.wind : 1.0f;
    int sc = bikerScore(h, (float)tempo);
    const float gegen = -langs;
    if (gegen > 25) sc -= (int)((gegen - 25) * 0.4f);
    if (quer * boeFaktor > 40) sc -= (int)((quer * boeFaktor - 40) * 0.5f);
    if (sc < 0) sc = 0;
    e->score[i] = sc; e->temp[i] = h.temp; e->mm[i] = h.mm; e->boe[i] = h.gust; e->langs[i] = langs; e->quer[i] = quer * boeFaktor;
    e->pop[i] = h.pop; e->wmo[i] = h.wmo;
    summe += sc; if (sc < e->schlechtester) e->schlechtester = sc;
    if (h.temp < e->tmin) e->tmin = h.temp;
    if (h.temp > e->tmax) e->tmax = h.temp;
    e->mmSumme += h.mm;
    if (h.gust > e->boeMax) e->boeMax = h.gust;
    if (quer * boeFaktor > e->querMax) e->querMax = quer * boeFaktor;
    langsSumme += langs;
    if (h.mm >= 0.1f) { e->regenAbschnitte++; if (e->erstesRegenKm < 0) e->erstesRegenKm = (int)b.km0; }
    if (h.temp < e->frostMinTemp) e->frostMinTemp = h.temp;
    if (h.temp <= 1.0f && e->frostKm < 0) e->frostKm = (int)b.km0;
    double sh, az; sonnenStand(b.lat, b.lon, t, &sh, &az);
    if (sh > 0 && sh < 15 && winkelDiff((float)az, b.kurs) < 35 && e->blendKm < 0) { e->blendKm = (int)b.km0; e->sonneHoeheBlend = (float)sh; }
  }
  e->langsMittel = langsSumme / a->n;
  e->gesamt = (int)lroundf(summe * 0.7f / a->n + e->schlechtester * 0.3f);
  const BikerPunkt* wl = a->wx[a->n - 1];
  int tag = (int)((e->ankunft - wl->h[0].t) / 86400); if (tag < 0) tag = 0; if (tag > 6) tag = 6;
  e->sonnenuntergang = wl->untergang[tag];
  e->imHellenAn = e->sonnenuntergang == 0 || e->ankunft <= e->sonnenuntergang;
  e->gueltig = true;
}

// ── Pause-Tipp ────────────────────────────────────────────────────────────────
void routePauseRechnen(RAnalyse* a, time_t abfahrt, int tempo, RPause* p) {
  *p = RPause();
  if (!a || !a->route || a->n < 3 || !a->wx[0]) return;
  RErgebnis base;
  routeBewerten(a, abfahrt, tempo, &base);
  if (!base.gueltig) return;
  auto schlecht = [&](int i) { return base.wmo[i] >= 95 || base.mm[i] >= 0.3f; };   // Gewitter oder Regen ab 0,3 mm
  const BikerPunkt* w = a->wx[0];
  const time_t ende = w->h[0].t + (time_t)w->n * 3600;
  static const int LAENGEN[5] = {30, 60, 90, 120, 180};
  struct Opt { int stufe, km, min, score; };
  Opt opt[20]; int no = 0, best = base.gesamt;
  for (int i = 1, kand = 0; i < a->n && kand < 4; i++) {
    if (!schlecht(i) || schlecht(i - 1)) continue;   // nur der Anfang eines Regenstuecks, das nicht schon am Start liegt
    kand++;
    for (int L : LAENGEN) {
      RErgebnis e;
      routeBewerten(a, abfahrt, tempo, &e, (int)a->a[i].km0, L);
      if (!e.gueltig || e.ankunft > ende - 3600) continue;           // Vorhersage reicht nicht mehr
      if (base.imHellenAn && !e.imHellenAn) continue;                // die Pause darf die Ankunft nicht in die Dunkelheit schieben
      opt[no++] = {i, (int)a->a[i].km0, L, e.gesamt};
      if (e.gesamt > best) best = e.gesamt;
    }
  }
  if (best < base.gesamt + 8) return;                                // bringt zu wenig
  int w2 = -1;
  for (int k = 0; k < no; k++) if (opt[k].score >= best - 2 && (w2 < 0 || opt[k].min < opt[w2].min || (opt[k].min == opt[w2].min && opt[k].score > opt[w2].score))) w2 = k;   // kuerzeste Pause, die fast das Beste erreicht
  if (w2 < 0) return;
  p->da = true; p->abfahrt = abfahrt; p->km = opt[w2].km; p->minuten = opt[w2].min; p->scoreOhne = base.gesamt; p->scoreMit = opt[w2].score;
  p->anKm = abfahrt + (time_t)(opt[w2].km / (float)tempo * 3600.0f);   // so spaet erreichst du den Kilometer ohne Pause
  p->gewitter = base.wmo[opt[w2].stufe] >= 95;
}

void routePauseText(const RPause& p, char* out, size_t n) {
  out[0] = 0;
  if (!p.da) return;
  char dauer[24], z[8] = "";
  if (p.minuten < 60) snprintf(dauer, sizeof(dauer), "%d Min.", p.minuten);
  else if (p.minuten % 60 == 0) snprintf(dauer, sizeof(dauer), "%d Std.", p.minuten / 60);
  else snprintf(dauer, sizeof(dauer), "%d Std. %d Min.", p.minuten / 60, p.minuten % 60);
  char ab[8] = "";
  if (p.anKm) { struct tm tm; localtime_r(&p.anKm, &tm); snprintf(z, sizeof(z), "%02d:%02d", tm.tm_hour, tm.tm_min); }
  if (p.abfahrt) { struct tm tm; localtime_r(&p.abfahrt, &tm); snprintf(ab, sizeof(ab), "%02d:%02d", tm.tm_hour, tm.tm_min); }
  snprintf(out, n, "Pause-Tipp bei Abfahrt %s: %s ab km %d (ca. %s Uhr). %s Pause davor hebt den Score von %d auf %d.", ab, p.gewitter ? "Gewitter" : "Regen", p.km, z, dauer, p.scoreOhne, p.scoreMit);
}

void routePlanerRechnen(RAnalyse* a, int tempo) {
  for (int k = 0; k < PLANER_STUNDEN; k++) routePlaner[k] = -1;
  routeBesteIdx = -1; routeJetztScore = -1;
  if (!a || !a->route || !a->wx[0]) return;
  const time_t jetzt = time(nullptr);
  routePlanerStart = (jetzt / 3600 + 1) * 3600;
  const BikerPunkt* w = a->wx[0];
  const time_t ende = w->h[0].t + (time_t)w->n * 3600;
  const time_t dauer = (time_t)(a->route->km / tempo * 3600.0f);
  RErgebnis e;
  int best = -1;
  for (int k = 0; k < PLANER_STUNDEN; k++) {
    const time_t dep = routePlanerStart + (time_t)k * 3600;
    if (dep + dauer > ende - 3600) break;
    routeBewerten(a, dep, tempo, &e);
    if (!e.gueltig) continue;
    routePlaner[k] = e.gesamt;
    if (e.gesamt > best) { best = e.gesamt; routeBesteIdx = k; }
  }
  routeBewerten(a, jetzt, tempo, &e);
  routeJetztScore = e.gueltig ? e.gesamt : -1;
}

void routeAktivSetzen(RAnalyse* a) {
  RAnalyse* alt;
  {
    LvLock lock;
    alt = routeAkt;
    routeAkt = a;
    if (a) {
      const time_t abf = routeAbfahrt ? routeAbfahrt : time(nullptr);
      routeBewerten(a, abf, routeTempo, &routeErg);
      routePlanerRechnen(a, routeTempo);
    } else { routeErg = RErgebnis(); }
  }
  if (alt) freiAnalyse(alt);
}

// ── Hintergrundauftraege ─────────────────────────────────────────────────────
static struct { int typ; String name, start, ziel, via; bool autobahn; char id[12]; } AUF;
static volatile bool s_laeuft = false;

static void fertigMelden(bool ok, const String& text) {
  strlcpy(routeStatusText, text.c_str(), sizeof(routeStatusText));
  routeStatus = ok ? RS_FERTIG : RS_FEHLER;
  s_laeuft = false;
}

static void auftragTask(void*) {
  String fehler;
  Route* r = new Route();
  bool ok = false;
  if (AUF.typ == 1) {
    ok = routeAusOrten(AUF.name, AUF.start, AUF.ziel, AUF.via, AUF.autobahn, r, fehler);
    if (ok) {
      snprintf(routeStatusText, sizeof(routeStatusText), "Speichere Route ...");
      if (!routeSpeichern(*r)) { logDiag("[Route] Speichern fehlgeschlagen"); }   // Anzeige geht trotzdem
    }
  } else if (AUF.typ == 2) {
    ok = routeLaden(AUF.id, r);
    if (!ok) fehler = "Route nicht lesbar";
  } else if (AUF.typ == 3) {
    if (routeAkt && routeAkt->route && routeAkt->route->p) {
      *r = *routeAkt->route;   // Kopie der Kopfdaten; Punkte neu anlegen
      RoutePunkt* np = psAlloc<RoutePunkt>(r->n);
      if (np) { memcpy(np, routeAkt->route->p, sizeof(RoutePunkt) * r->n); r->p = np; ok = true; } else { r->p = nullptr; fehler = "Zu wenig Speicher"; }
    } else fehler = "Keine aktive Route";
  }
  RAnalyse* a = nullptr;
  if (ok) {
    ok = routeWetterLaden(r, &a, fehler);   // uebernimmt r bei Erfolg
  }
  if (ok && a) {
    routeAktivSetzen(a);
    if (a->route->id[0] && !karteGrossVorhanden(a->route->id)) karteErzeugenStarten(a->route->id);   // Kartenbild im Hintergrund (OSM-Kacheln)
    char t[96]; snprintf(t, sizeof(t), "%s: %.0f km, Score %d", a->route->name, a->route->km, routeErg.gesamt);
    logDiag("[Route] %s", t);
    fertigMelden(true, t);
  } else {
    if (!a) { routeFrei(r); delete r; }
    logDiag("[Route] Fehler: %s", fehler.c_str());
    fertigMelden(false, fehler.isEmpty() ? String("Unbekannter Fehler") : fehler);
  }
  vTaskDelete(nullptr);
}

static bool auftragStarten() {
  if (s_laeuft) return false;
  s_laeuft = true; routeStatus = RS_BESCHAEFTIGT;
  if (xTaskCreatePinnedToCore(auftragTask, "route", 20 * 1024, nullptr, 1, nullptr, 0) != pdPASS) { s_laeuft = false; routeStatus = RS_FEHLER; snprintf(routeStatusText, sizeof(routeStatusText), "Kein Speicher fuer den Auftrag"); return false; }
  return true;
}

bool routeAuftragNeu(const String& name, const String& start, const String& ziel, const String& via, bool autobahnMeiden) {
  if (s_laeuft) return false;
  AUF.typ = 1; AUF.name = name; AUF.start = start; AUF.ziel = ziel; AUF.via = via; AUF.autobahn = autobahnMeiden;
  snprintf(routeStatusText, sizeof(routeStatusText), "Starte ...");
  return auftragStarten();
}
bool routeAuftragWaehlen(const char* id) {
  if (s_laeuft) return false;
  AUF.typ = 2; strlcpy(AUF.id, id, sizeof(AUF.id));
  snprintf(routeStatusText, sizeof(routeStatusText), "Lade Route ...");
  return auftragStarten();
}
bool routeAuftragAktualisieren() {
  if (s_laeuft) return false;
  AUF.typ = 3;
  snprintf(routeStatusText, sizeof(routeStatusText), "Aktualisiere Wetter ...");
  return auftragStarten();
}
