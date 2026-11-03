// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
#include "navigation.h"
#include "app_state.h"

// ── Navigationsplan ───────────────────────────────────────────────────────────
//  Hauptkette (Kreis):  Heute ─> Wetter ─> 7 Tage ─> Verlauf ─> Warnkarte ─> Heute
//  Heute: "Routen" -> Routenliste, "Warnung" -> Warnkarte, Zahnrad -> Menue
//  Routen:  Liste ─> Neue Route ─> Routenwetter ─> Hoehe / Abfahrtszeit-Planer;  Liste ─> Gebiete
//  Warnkarte1 <-> Warnkarte2 ueber den Hub-Button; Home-Tasten fuehren auf Heute
// ─────────────────────────────────────────────────────────────────────────────

void nav_goto(enum ScreensEnum id)
{
    lv_obj_t *scr = ((lv_obj_t **)&objects)[id - 1];
    if (scr) lv_scr_load(scr);
}

// Ein Callback für alle Ziele: das Ziel steckt im user_data
static void on_nav(lv_event_t *e)
{
    nav_goto((enum ScreensEnum)(intptr_t)lv_event_get_user_data(e));
}

static void link(lv_obj_t *btn, enum ScreensEnum target)
{
    if (!btn) return;
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, on_nav, LV_EVENT_CLICKED, (void *)(intptr_t)target);
}

// Absturz-Spur: welcher Screen war zuletzt aktiv (steht nach einem Absturz im /log)
static void screenSpur(lv_timer_t*)
{
    static lv_obj_t* letzter = nullptr;
    lv_obj_t* a = lv_scr_act();
    if (a == letzter) return;
    letzter = a;
    struct { lv_obj_t* o; const char* n; } t[] = {
        {objects.screenheute, "Heute"}, {objects.screenwetter, "Wetter"}, {objects.screenwoche, "7-Tage-Vorschau"}, {objects.screenverlauf, "Verlauf"},
        {objects.screenwarnkarte1, "Warnkarte"}, {objects.screenwarnkarte2, "Warnliste"}, {objects.screenmenu, "Menue"},
        {objects.screenroutenliste, "Routen"}, {objects.screenrouteneu, "Neue Route"}, {objects.screenroutewetter, "Routenwetter"},
        {objects.screenroutehoehe, "Routenhoehe"}, {objects.screenabfahrt, "Abfahrtszeit"}, {objects.screengebiete, "Gebiete"},
        {objects.screennachtuhr, "Nacht-Uhr"},
    };
    char buf[48] = "Screen: anderer";
    for (auto &e : t) if (e.o && e.o == a) { snprintf(buf, sizeof(buf), "Screen: %s", e.n); break; }
    spur(buf);
}

void nav_init()
{
    lv_timer_create(screenSpur, 250, nullptr);

    // Heute
    link(objects.buttonheutezurueck,       SCREEN_ID_SCREENWARNKARTE1);
    link(objects.buttonheuteweiter,        SCREEN_ID_SCREENWETTER);
    link(objects.buttonheuterouten,        SCREEN_ID_SCREENROUTENLISTE);
    link(objects.buttonheutewarnung,       SCREEN_ID_SCREENWARNKARTE1);
    link(objects.imageheutesettings,       SCREEN_ID_SCREENMENU);

    // Wetter (zwischen Heute und 7 Tage)
    link(objects.buttonwetterzurueck,      SCREEN_ID_SCREENHEUTE);
    link(objects.buttonwetterweiter,       SCREEN_ID_SCREENWOCHE);

    // 7-Tage-Vorschau
    link(objects.buttonwochezurueck,       SCREEN_ID_SCREENWETTER);
    link(objects.buttonwocheweiter,        SCREEN_ID_SCREENVERLAUF);
    link(objects.buttonwochehome,          SCREEN_ID_SCREENHEUTE);

    // Verlauf
    link(objects.buttonverlaufzurueck,     SCREEN_ID_SCREENWOCHE);
    link(objects.buttonverlaufweiter,      SCREEN_ID_SCREENWARNKARTE1);
    link(objects.buttonverlaufhome,        SCREEN_ID_SCREENHEUTE);

    // Warnkarte 1 und 2
    link(objects.labelbuttonbackward,      SCREEN_ID_SCREENVERLAUF);
    link(objects.labelbuttonforward,       SCREEN_ID_SCREENHEUTE);
    link(objects.labelbuttonhome,          SCREEN_ID_SCREENHEUTE);
    link(objects.labelbuttonscreenhub,     SCREEN_ID_SCREENWARNKARTE2);
    link(objects.labelbuttonscreenhubback, SCREEN_ID_SCREENWARNKARTE1);
    link(objects.labelbuttonhome_1,        SCREEN_ID_SCREENHEUTE);

    // Routen
    link(objects.buttonroutenzurueck,      SCREEN_ID_SCREENHEUTE);
    link(objects.buttonroutenhome,         SCREEN_ID_SCREENHEUTE);
    link(objects.buttonroutenweiter,       SCREEN_ID_SCREENGEBIETE);
    link(objects.buttonrouteneu,           SCREEN_ID_SCREENROUTENEU);
    link(objects.buttonrnzurueck,          SCREEN_ID_SCREENROUTENLISTE);
    link(objects.buttonrouteabbrechen,     SCREEN_ID_SCREENROUTENLISTE);
    link(objects.buttonrwzurueck,          SCREEN_ID_SCREENROUTENLISTE);
    link(objects.buttonrwhoehe,            SCREEN_ID_SCREENROUTEHOEHE);
    link(objects.buttonrwplaner,           SCREEN_ID_SCREENABFAHRT);
    link(objects.buttonrhzurueck,          SCREEN_ID_SCREENROUTEWETTER);
    link(objects.buttonabzurueck,          SCREEN_ID_SCREENROUTEWETTER);

    // Gebiete
    link(objects.buttongebietezurueck,     SCREEN_ID_SCREENROUTENLISTE);
    link(objects.buttongebieteweiter,      SCREEN_ID_SCREENHEUTE);
    link(objects.buttongebietehome,        SCREEN_ID_SCREENHEUTE);

    // Einstellungen
    link(objects.labelbuttonmenu_2,        SCREEN_ID_SCREENHEUTE);
}
