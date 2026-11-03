// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
// Schriften der Biker-Bildschirme: PicoPixel kann die erzeugten Schriften nicht an Labels binden (Status "registering"),
// deshalb werden sie hier gesetzt. Ohne das gaebe es Standardschrift ohne Umlaute. Erzeugt aus der Aufbau-Liste der Bildschirme.
#include "biker_fonts.h"
#include "app_state.h"
#include "../ui/screens.h"
#include "../ui/fonts/fonts.h"

void uiSchriftenSetzen() {
  auto f = [](lv_obj_t* o, const lv_font_t* font) { if (o) lv_obj_set_style_text_font(o, font, 0); };
  f(objects.labelort, &BIKER_F32);
  f(objects.labeldatum, &BIKER_F32);
  f(objects.labelscorezahl, &font_montserrat_92);
  f(objects.labelscorestufe, &BIKER_F22);
  f(objects.labelscorekopf, &BIKER_F18);
  f(objects.labelbestetitel, &BIKER_F18);
  f(objects.labelbestewert, &BIKER_F32);
  f(objects.labelbestedetail, &BIKER_F18);
  f(objects.labeltemptitel, &BIKER_F18);
  f(objects.labeltempwert, &BIKER_F48);
  f(objects.labeltempdetail, &BIKER_F18);
  f(objects.labelwindtitel, &BIKER_F18);
  f(objects.labelwindwert, &BIKER_F48);
  f(objects.labelwinddetail, &BIKER_F18);
  f(objects.labelregentitel, &BIKER_F18);
  f(objects.labelregenwert, &BIKER_F48);
  f(objects.labelregendetail, &BIKER_F18);
  f(objects.labelsichttitel, &BIKER_F18);
  f(objects.labelsichtwert, &BIKER_F48);
  f(objects.labelsichtdetail, &BIKER_F18);
  f(objects.labelstundentitel, &BIKER_F18);
  f(objects.labelempfehlungtitel, &BIKER_F18);
  f(objects.labelempfehlungtext, &BIKER_F22);
  f(objects.buttonheuterouten_label, &BIKER_F22);
  f(objects.buttonheutewarnung_label, &BIKER_F22);
  f(objects.labelquelleheute, &BIKER_F18);
  f(objects.labelroutentitel, &BIKER_F32);
  f(objects.labelroutenleer, &BIKER_F22);
  f(objects.buttonrouteneu_label, &BIKER_F32);
  f(objects.labelgpxtitel, &BIKER_F22);
  f(objects.labelgpxtext, &BIKER_F18);
  f(objects.labelspeichertitel, &BIKER_F18);
  f(objects.labelspeicherwert, &BIKER_F22);
  f(objects.labelrntitel, &BIKER_F32);
  f(objects.labelstarttitel, &BIKER_F18);
  f(objects.textareastart, &BIKER_F22);
  f(objects.labelzieltitel, &BIKER_F18);
  f(objects.textareaziel, &BIKER_F22);
  f(objects.labelviatitel, &BIKER_F18);
  f(objects.textareavia, &BIKER_F22);
  f(objects.labelabfahrttitel, &BIKER_F18);
  f(objects.labelabfahrtwert, &BIKER_F32);
  f(objects.buttonabfahrtminus_label, &BIKER_F18);
  f(objects.buttonabfahrtplus_label, &BIKER_F18);
  f(objects.buttonabfahrtjetzt_label, &BIKER_F18);
  f(objects.labeltempotitel, &BIKER_F18);
  f(objects.labeltempowert, &BIKER_F32);
  f(objects.buttontempominus_label, &BIKER_F18);
  f(objects.buttontempoplus_label, &BIKER_F18);
  f(objects.labelautobahn, &BIKER_F22);
  f(objects.buttonrouteberechnen_label, &BIKER_F32);
  f(objects.buttonrouteabbrechen_label, &BIKER_F22);
  f(objects.labeltastaturhinweis, &BIKER_F22);
  f(objects.labelrwtitel, &BIKER_F32);
  f(objects.labelrwinfo, &BIKER_F18);
  f(objects.labelkarteplatzhalter, &BIKER_F22);
  f(objects.labelrwscorezahl, &BIKER_F48);
  f(objects.labelrwscoretext, &BIKER_F22);
  f(objects.labelrwscorekopf, &BIKER_F18);
  f(objects.labelrwname1, &BIKER_F18);
  f(objects.labelrwwert1, &BIKER_F22);
  f(objects.labelrwname2, &BIKER_F18);
  f(objects.labelrwwert2, &BIKER_F22);
  f(objects.labelrwname3, &BIKER_F18);
  f(objects.labelrwwert3, &BIKER_F22);
  f(objects.labelrwname4, &BIKER_F18);
  f(objects.labelrwwert4, &BIKER_F22);
  f(objects.labelabschnittetitel, &BIKER_F18);
  f(objects.buttonrwhoehe_label, &BIKER_F22);
  f(objects.buttonrwplaner_label, &BIKER_F22);
  f(objects.labelquellerw, &BIKER_F18);
  f(objects.labelrhtitel, &BIKER_F32);
  f(objects.labelprofiltitel, &BIKER_F18);
  f(objects.labelhoehetitel, &BIKER_F18);
  f(objects.labelhoehewert, &BIKER_F32);
  f(objects.labelhoehedetail, &BIKER_F18);
  f(objects.labelfrosttitel, &BIKER_F18);
  f(objects.labelfrostwert, &BIKER_F32);
  f(objects.labelfrostdetail, &BIKER_F18);
  f(objects.labelwindtitel, &BIKER_F18);
  f(objects.labelwindwert, &BIKER_F32);
  f(objects.labelwinddetail, &BIKER_F18);
  f(objects.labelsonnetitel, &BIKER_F18);
  f(objects.labelsonnewert, &BIKER_F32);
  f(objects.labelsonnedetail, &BIKER_F18);
  f(objects.labelblendtitel, &BIKER_F18);
  f(objects.labelblendwert, &BIKER_F32);
  f(objects.labelblenddetail, &BIKER_F18);
  f(objects.labelabtitel, &BIKER_F32);
  f(objects.labelabinfo, &BIKER_F18);
  f(objects.labelbestzeittitel, &BIKER_F18);
  f(objects.labelbestzeitwert, &BIKER_F48);
  f(objects.labelbestzeitdetail, &BIKER_F18);
  f(objects.labeljetzttitel, &BIKER_F18);
  f(objects.labeljetztwert, &BIKER_F48);
  f(objects.labeljetztdetail, &BIKER_F18);
  f(objects.labelgebietetitel, &BIKER_F32);
  f(objects.buttongebieteheute_label, &BIKER_F22);
  f(objects.buttongebietemorgen_label, &BIKER_F22);
  f(objects.buttongebietewochenende_label, &BIKER_F22);
  f(objects.labelgebieteleer, &BIKER_F22);
  if (objects.labelcube) lv_label_set_text(objects.labelcube, "Biker");   // Bootscreen: "WetterCube Biker"
}

// Die in PicoPixel an Labels gebundenen Fusion-Schriften (18/22/32/48) haben keine Umlaute. Hier werden sie an allen Objekten der
// Oberflaeche durch die Biker-Schriften ersetzt (gleiche Groesse, dazu Umlaute, ß und ° aus Latin-1).
void uiSchriftenTauschen() {
  const lv_font_t* alt[3] = {&font_montserrat_18, &font_montserrat_22, &font_montserrat_32};
  const lv_font_t* neu[3] = {&BIKER_F18, &BIKER_F22, &BIKER_F32};
  lv_obj_t** p = (lv_obj_t**)&objects;
  const int n = (int)(sizeof(objects) / sizeof(lv_obj_t*));
  int getauscht = 0;
  for (int i = 0; i < n; i++) {
    if (!p[i]) continue;
    const lv_font_t* f = lv_obj_get_style_text_font(p[i], LV_PART_MAIN);
    bool getan = false;
    for (int k = 0; k < 3; k++) {
      if (f == alt[k] && alt[k] != neu[k]) { lv_obj_set_style_text_font(p[i], neu[k], 0); getauscht++; getan = true; break; }
    }
    // Labels ohne eigene Schrift (PicoPixel liess sie auf der Standardschrift ohne Umlaute): Biker 18
    if (!getan && f == LV_FONT_DEFAULT && lv_obj_check_type(p[i], &lv_label_class)) { lv_obj_set_style_text_font(p[i], &BIKER_F18, 0); getauscht++; }
  }
  logDiag("[UI] %d Schriften durch Biker-Schriften ersetzt", getauscht);
}
