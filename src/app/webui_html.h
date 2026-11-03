// SPDX-License-Identifier: AGPL-3.0-only
// Copyright 2026 Jan Althaus (JPPeterson-lab)
#pragma once
#include "webui_theme.h"
// HTML-Strings in .h ausgelagert, da der Arduino-Präprozessor
// Raw-String-Literals in .ino falsch parst (beendet String bei " inmitten von R"...").

const char PORTAL_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WetterCube Biker – Einrichtung</title>
<style>
  body{font-family:Arial,sans-serif;background:#0d1117;color:#e6edf3;margin:0;padding:20px}
  h1{color:#58a6ff;font-size:1.4em}
  .card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:20px;max-width:420px;margin:auto}
  label{display:block;margin-top:12px;color:#8b949e;font-size:.9em}
  input,select{width:100%;padding:8px;border-radius:4px;border:1px solid #30363d;
    background:#21262d;color:#e6edf3;box-sizing:border-box;margin-top:4px}
  button{margin-top:20px;width:100%;padding:10px;background:#238636;color:#fff;
    border:none;border-radius:4px;font-size:1em;cursor:pointer}
  button:hover{background:#2ea043}
  .hint{font-size:.8em;color:#8b949e;margin-top:4px}
.rwt{border-collapse:collapse;font-size:.85em;width:100%}.rwt td,.rwt th{border:1px solid var(--bd);padding:3px 6px;text-align:center}
</style></head><body>
<div class="card">
  <h1>WetterCube Biker Einrichtung</h1>
  <form action="/speichern" method="post">
    <label>WLAN-Netzwerk</label>
    <select name="ssid">%NETZWERKE%</select>
    <label>WLAN-Passwort</label>
    <input type="password" name="pass" placeholder="Passwort eingeben">
    <label>Standort (Stadt)</label>
    <input type="text" name="location" placeholder="z.B. Berlin" value="%LOCATION%">
    <p class="hint">Keine Umlaute: Munchen statt München, Koln statt Koeln</p>
    <label>DWD-Region (Warnungen und Karte)</label>
    <select name="dwd_region">%DWD_REGIONEN%</select>
    <button type="submit">Speichern &amp; Neustart</button>
  </form>
</div></body></html>
)rawhtml";

const char PORTAL_OK_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8"><title>Gespeichert</title>
<meta http-equiv="refresh" content="5;url=/">
<style>
  body{font-family:Arial;background:#0d1117;color:#e6edf3;text-align:center;padding-top:60px}
  .ok{color:#2ea043;font-size:2em}
</style></head><body>
<div class="ok">Gespeichert</div>
<p>Geraet wird neu gestartet...</p>
</body></html>
)rawhtml";

const char WEBUI_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WetterCube Biker</title>
)rawhtml" WEBUI_THEME_HEAD WEBUI_THEME_CSS R"rawhtml(
<style>
  *{box-sizing:border-box}
  body{font-family:Arial,sans-serif;background:var(--bg);color:var(--txt);margin:0;padding:16px;max-width:860px;margin:auto}
  h1{color:var(--acc);margin:0 0 4px}
  .top{display:flex;justify-content:space-between;align-items:flex-start;gap:10px;margin-bottom:14px}
  .ver{color:var(--mut);font-size:.85em}
  .tabs{display:flex;gap:6px;overflow-x:auto;margin:0 0 16px;padding-bottom:4px;-webkit-overflow-scrolling:touch}
  .tabs a{flex:0 0 auto;padding:9px 16px;border-radius:8px;background:var(--tab);color:var(--txt);text-decoration:none;font-size:.95em;border:1px solid var(--bd);white-space:nowrap}
  .tabs a.aktiv{background:#1f6feb;border-color:#1f6feb;color:#fff}
  .tab{display:none}
  .card{background:var(--card);border:1px solid var(--bd);border-radius:8px;padding:16px;margin-bottom:16px}
  h2{color:var(--txt);font-size:1em;margin:0 0 12px}
  label{display:block;color:var(--mut);font-size:.85em;margin-top:10px}
  input[type=text],input[type=number],select{width:100%;padding:7px;border-radius:4px;
    border:1px solid var(--bd);background:var(--in);color:var(--txt);margin-top:3px}
  input[type=range]{width:100%;margin-top:6px}
  .row{display:flex;align-items:center;justify-content:space-between;margin-top:10px}
  .toggle{position:relative;width:44px;height:24px;flex-shrink:0}
  .toggle input{opacity:0;width:0;height:0}
  .slider{position:absolute;inset:0;background:var(--slider);border-radius:12px;cursor:pointer;transition:.3s}
  .slider::before{content:'';position:absolute;width:18px;height:18px;left:3px;bottom:3px;
    background:#fff;border-radius:50%;transition:.3s}
  input:checked+.slider{background:#238636}
  input:checked+.slider::before{transform:translateX(20px)}
  .btn{display:inline-block;padding:9px 16px;border:none;border-radius:4px;cursor:pointer;font-size:.9em;margin-top:6px;text-decoration:none}
  .btn-gruen{background:#238636;color:#fff}.btn-gruen:hover{background:#2ea043}
  .btn-blau{background:#1f6feb;color:#fff}.btn-blau:hover{background:#388bfd}
  .btn-rot{background:#b91c1c;color:#fff}.btn-rot:hover{background:#dc2626}
  .status{font-size:.85em;color:var(--mut);margin-top:6px}
  .hint{font-size:.78em;color:var(--mut);margin-top:3px}
  .row{display:flex;gap:10px;margin-top:8px}
  .col{flex:1}
  .savebar{position:sticky;bottom:0;background:var(--bg);border-top:1px solid var(--bd);padding:10px 0;margin-top:4px;display:flex;align-items:center;gap:12px;z-index:5}
  .savebar .btn{margin:0}
  #dirty{font-size:.85em;color:#d29922;display:none}
</style></head><body>
<div class="top">
  <div><h1>WetterCube Biker</h1>
  <div class="ver">Firmware: %VERSION% | IP: %IP% | wettercube-biker.local</div></div>
  <button id="wcThemeBtn" class="themebtn" onclick="wcTheme()" title="Hell oder dunkel"></button>
</div>

<div class="tabs" id="tabs">
  <a href="#routen" data-t="routen">Routen</a>
  <a href="#anzeige" data-t="anzeige">Anzeige</a>
  <a href="#wetter" data-t="wetter">Wetter &amp; Daten</a>
  <a href="#system" data-t="system">System</a>
</div>

<div class="tab" id="t-routen">
<div class="card">
  <h2>Neue Route</h2>
  <p class="hint">Start und Ziel eingeben, der Dienst berechnet eine Strecke f&uuml;r Motorr&auml;der (Landstra&szlig;en bevorzugt). Das Wetter wird f&uuml;r jeden Abschnitt zur Durchfahrtszeit geholt.</p>
  <label>Start</label><input type="text" id="rn_start" placeholder="z. B. L&uuml;beck">
  <label>Zwischenziel (optional)</label><input type="text" id="rn_via" placeholder="z. B. Plön">
  <label>Ziel</label><input type="text" id="rn_ziel" placeholder="z. B. Flensburg">
  <label>Name (optional)</label><input type="text" id="rn_name" maxlength="38">
  <div class="row"><span>Autobahn meiden</span>
    <label class="toggle"><input type="checkbox" id="rn_auto" checked><span class="slider"></span></label></div>
  <button type="button" class="btn btn-gruen" onclick="routeNeu()">Route berechnen</button>
  <p class="hint" id="rn_msg" style="margin-top:8px"></p>
</div>

<div class="card">
  <h2>GPX-Datei laden</h2>
  <p class="hint">Eine Route aus einer GPX-Datei (Track oder Route, zum Beispiel aus Kurviger, Calimoto oder Komoot). Die Datei wird auf dem Ger&auml;t in etwa 1500 Punkte ausged&uuml;nnt gespeichert.</p>
  <input type="file" id="gpx_datei" accept=".gpx,application/gpx+xml,text/xml">
  <label>Name (optional)</label><input type="text" id="gpx_name" maxlength="38">
  <button type="button" class="btn btn-gruen" onclick="gpxHoch()">GPX hochladen</button>
  <p class="hint" id="gpx_msg" style="margin-top:8px"></p>
</div>

<div class="card">
  <h2>Meine Routen</h2>
  <div id="rt_liste"><p class="hint">Wird geladen ...</p></div>
</div>

<div class="card">
  <h2>Motorradgebiete</h2>
  <p class="hint"><b>Frühe Beta, noch nicht ganz präzise.</b> Eine Liste von Gebieten (ein Name je Zeile, zum Beispiel Harz oder Plön). Das Gerät sucht die Koordinaten und bewertet jedes Gebiet nach dem Biker-Score für heute, morgen und das Wochenende. Die Rangfolge steht auch am Gerät auf der Seite Gebiete.</p>
  <p class="hint"><b>Was ist ein Gebiet?</b> Für den WetterCube ist ein Gebiet <b>ein Punkt</b>: die Koordinate des eingetragenen Ortes, kein Umriss. Die Vorhersage stammt vom Wettermodell (Gitter von rund 2 bis 7 km) an diesem Punkt und gilt grob für einen Umkreis von etwa 10 km. Der Score eines Tages ist das <b>Mittel der Stunden 10 bis 18 Uhr</b>, das Wochenende das Mittel aus Samstag und Sonntag. Im Mittelgebirge ändert sich das Wetter auf kurzer Strecke (Tal und Gipfel), und ein Name wie &bdquo;Harz&ldquo; ist ein Gebiet, nicht ein Punkt: Wer genau einen Punkt meint, schreibt <code>Name; Breite; Länge</code> (zum Beispiel <code>Brocken; 51.80; 10.62</code>).</p>
  <textarea id="gb_liste" rows="6" style="width:100%"></textarea>
  <button type="button" class="btn btn-gruen" onclick="gebieteSpeichern()">Speichern und bewerten</button>
  <button type="button" class="btn" onclick="gebieteAktual()">Wetter neu holen</button>
  <p class="hint" id="gb_msg" style="margin-top:8px"></p>
  <div style="overflow-x:auto"><table class="rwt" id="gb_tab"></table></div>
</div>

<div class="card">
  <h2>Routenwetter und beste Abfahrt</h2>
  <div id="rw_leer" class="hint">Noch keine Route gew&auml;hlt: unter &bdquo;Meine Routen&ldquo; auf &bdquo;Wetter&ldquo; tippen.</div>
  <div id="rw_inhalt" style="display:none">
    <h3 id="rw_titel" style="margin:0 0 4px"></h3>
    <div class="status" id="rw_info"></div>

    <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:10px">
      <div><label>Abfahrt</label><input type="datetime-local" id="rw_abf"></div>
      <div><label>Tempo (km/h)</label><input type="number" id="rw_tempo" min="30" max="120" step="5" value="60"></div>
    </div>
    <button type="button" class="btn btn-blau" onclick="abfahrtSetzen(false)">Neu berechnen</button>
    <button type="button" class="btn" onclick="abfahrtSetzen(true)">Jetzt losfahren</button>
    <button type="button" class="btn" onclick="routeAktual()">Wetter neu laden</button>
    <h3 style="margin:14px 0 4px;font-size:1em">Abschnitte</h3>
    <div style="overflow-x:auto"><table class="rwt" id="rw_tab"></table></div>
    <h3 style="margin:14px 0 4px;font-size:1em">Score je Abfahrtszeit (n&auml;chste 48 Stunden, Balken antippen)</h3>
    <div id="rw_planer" style="display:flex;align-items:flex-end;gap:2px;height:120px;border-bottom:1px solid var(--bd)"></div>
    <div id="rw_planer_t" style="display:flex;gap:2px;font-size:.7em;margin-top:2px"></div>
    <p class="hint" id="rw_best" style="margin-top:8px"></p>
  </div>
</div>

<div class="card">
  <h2>Fahrtenbuch</h2>
  <p class="hint">Nach der Ausfahrt eintragen: Route und Abfahrt, wie oben gew&auml;hlt, mit dem Wetter dieser Abfahrt (Score, Temperatur, Regen, B&ouml;en). Dazu die Route unter &bdquo;Wetter&ldquo; w&auml;hlen und bei Bedarf die Abfahrt anpassen. Liegt die Abfahrt vor dem Zeitraum der Vorhersage, wird sie ohne Wetter eingetragen.</p>
  <button type="button" class="btn btn-blau" onclick="fahrtEintragen()">Diese Fahrt eintragen</button>
  <p class="hint" id="fb_msg" style="margin-top:8px"></p>
  <div class="row"><span>Jahr</span><select id="fb_jahr" onchange="fahrtenZeigen()"></select></div>
  <p id="fb_summe" style="margin:8px 0"></p>
  <div id="fb_monate" style="display:flex;align-items:flex-end;gap:3px;height:70px;border-bottom:1px solid var(--bd)"></div>
  <div id="fb_monate_t" style="display:flex;gap:3px;font-size:.7em;margin:2px 0 10px"></div>
  <div style="overflow-x:auto"><table class="rwt" id="fb_tab"></table></div>
</div>
</div>

<form action="/speichern" method="post" id="einst">

<div class="tab" id="t-anzeige">
<div class="card">
  <h2>Display</h2>
  <label>Helligkeit: <span id="bv">%BRIGHT%</span>%</label>
  <input type="range" name="brightness" min="10" max="100" value="%BRIGHT%"
    oninput="document.getElementById('bv').textContent=this.value">
  <label>Dimm-Timeout</label>
  <select name="dim_time">
    <option value="0" %DT0%>Aus</option>
    <option value="-1" %DTT%>30 Sekunden (Test)</option>
    <option value="1" %DT1%>1 Minute</option>
    <option value="3" %DT3%>3 Minuten (Standard)</option>
    <option value="5" %DT5%>5 Minuten</option>
    <option value="10" %DT10%>10 Minuten</option>
  </select>
  <hr style="margin:14px 0;border-color:var(--bd)">
  <h3 style="margin:0 0 4px;font-size:1em">Tag- und Nachtmodus</h3>
  <label>Nachtmodus</label>
  <select name="night_modus" id="nm_sel" onchange="nachtUi()">
    <option value="0" %NMO0%>Aus</option>
    <option value="1" %NMO1%>Feste Uhrzeiten</option>
    <option value="2" %NMO2%>Nach Sonnenuntergang und Sonnenaufgang</option>
  </select>
  <div id="nm_zeit" style="display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:8px">
    <div>
      <label>Von</label>
      <select name="night_from">%NIGHT_FROM_OPTIONS%</select>
    </div>
    <div>
      <label>Bis</label>
      <select name="night_to">%NIGHT_TO_OPTIONS%</select>
    </div>
  </div>
  <div id="nm_sonne" style="display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:8px">
    <div>
      <label>Nacht beginnt (Minuten nach Sonnenuntergang)</label>
      <input type="number" name="night_sonne_ab" min="-180" max="180" step="5" value="%NSA%">
    </div>
    <div>
      <label>Nacht endet (Minuten nach Sonnenaufgang)</label>
      <input type="number" name="night_sonne_bis" min="-180" max="180" step="5" value="%NSB%">
    </div>
  </div>
  <p class="hint" style="margin:6px 0 0">%NIGHT_INFO%</p>
  <label style="margin-top:8px;display:block">Helligkeit nachts: <span id="nbv">%NBR%</span>%</label>
  <input type="range" name="night_bright" min="0" max="50" value="%NBR%"
    oninput="document.getElementById('nbv').textContent=this.value">
  <div class="row" style="margin-top:10px"><span>Nacht-Uhr: nachts nur die gro&szlig;e Uhr mit Datum und Au&szlig;entemperatur zeigen (Antippen zeigt den Hauptscreen)</span>
    <label class="toggle"><input type="checkbox" name="night_uhr" %NU%><span class="slider"></span></label></div>
  <h3 style="margin:14px 0 4px;font-size:1em">Bildschirm Wetter: zwei w&auml;hlbare K&auml;sten</h3>
  <p class="hint" style="margin:0 0 6px">Auf dem Bildschirm Wetter stehen oben rechts zwei K&auml;sten. Ein Tipp auf einen Kasten schaltet am Ger&auml;t zur n&auml;chsten Anzeige weiter.</p>
  <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
    <div><label>Kasten 1</label><select name="wetter_modul1"><option value="0" %WA0%>Aus</option><option value="1" %WA1%>Fahrtwind-Gef&uuml;hl</option><option value="2" %WA2%>Regen in K&uuml;rze</option><option value="3" %WA3%>Gl&auml;tte und Frost</option><option value="4" %WA4%>Sonne und Blendung</option><option value="5" %WA5%>Aktive Route</option><option value="6" %WA6%>Bestes Gebiet (Wochenende)</option></select></div>
    <div><label>Kasten 2</label><select name="wetter_modul2"><option value="0" %WB0%>Aus</option><option value="1" %WB1%>Fahrtwind-Gef&uuml;hl</option><option value="2" %WB2%>Regen in K&uuml;rze</option><option value="3" %WB3%>Gl&auml;tte und Frost</option><option value="4" %WB4%>Sonne und Blendung</option><option value="5" %WB5%>Aktive Route</option><option value="6" %WB6%>Bestes Gebiet (Wochenende)</option></select></div>
  </div>
  <div class="row" style="margin-top:10px"><span>Nachts dunkles Design (danach wieder das vorherige)</span>
    <label class="toggle"><input type="checkbox" name="night_theme" %NT%><span class="slider"></span></label></div>
  <hr style="margin:14px 0;border-color:var(--bd)">
  <h3 style="margin:0 0 4px;font-size:1em">Bilderrahmen</h3>
  <p class="hint" style="margin:0 0 6px">Nach einer Weile ohne Ber&uuml;hrung zeigt das Display Fotos von der Speicherkarte, eines nach dem anderen. Antippen zeigt den Bildschirm Heute. Stelle das Dimmen (oben) auf &bdquo;Aus&ldquo; oder l&auml;nger als die Startzeit, sonst sind die Fotos gedimmt.</p>
  <input type="hidden" name="rahmen_form" value="1">
  <div class="row"><span>Bilderrahmen einschalten</span>
    <label class="toggle"><input type="checkbox" name="rahmen_an" %RAH%><span class="slider"></span></label></div>
  <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:8px">
    <div><label>Startet nach (ohne Ber&uuml;hrung)</label>
      <select name="rahmen_min">
        <option value="1" %RMN1%>1 Minute</option><option value="3" %RMN3%>3 Minuten</option><option value="5" %RMN5%>5 Minuten</option>
        <option value="10" %RMN10%>10 Minuten</option><option value="15" %RMN15%>15 Minuten</option><option value="30" %RMN30%>30 Minuten</option><option value="60" %RMN60%>1 Stunde</option>
      </select></div>
    <div><label>Bild wechselt alle</label>
      <select name="rahmen_sek">
        <option value="10" %RSK10%>10 Sekunden</option><option value="30" %RSK30%>30 Sekunden</option><option value="60" %RSK60%>1 Minute</option>
        <option value="120" %RSK120%>2 Minuten</option><option value="300" %RSK300%>5 Minuten</option>
      </select></div>
  </div>
  <div class="row" style="margin-top:10px"><span>Auch nachts (gedimmt) statt der Nacht-Uhr</span>
    <label class="toggle"><input type="checkbox" name="rahmen_nacht" %RAN%><span class="slider"></span></label></div>
  <label style="margin-top:10px">Fotos hinzuf&uuml;gen</label>
  <input type="file" id="rf_files" accept="image/*" multiple>
  <button type="button" class="btn btn-blau" onclick="rahmenHochladen()">Fotos auf die Karte laden</button>
  <div id="rf_status" class="status"></div>
  <p class="hint" style="margin:6px 0 0">Die Fotos werden im Browser auf Bildschirmgr&ouml;&szlig;e verkleinert (h&ouml;chstens 1024 &times; 600), gedreht wie auf dem Foto und als JPEG auf der Karte im Ordner <code>bilder</code> gespeichert. Du kannst auch eigene Baseline-JPEG-Dateien (keine progressiven) dorthin kopieren. Die Karte muss eingesteckt sein (Seite Speicherkarte).</p>
  <div id="rf_liste" class="status"></div>
  <script>
  async function rahmenHochladen(){
    var fs=document.getElementById('rf_files').files, st=document.getElementById('rf_status'), ok=0;
    if(!fs.length){st.textContent='Bitte zuerst Fotos ausw\u00e4hlen.';return;}
    for(var i=0;i<fs.length;i++){
      st.textContent='Bild '+(i+1)+' von '+fs.length+' ...';
      try{
        var bmp;
        try{bmp=await createImageBitmap(fs[i],{imageOrientation:'from-image'});}
        catch(e1){   // Ersatzweg: Bild ueber ein img-Element laden (Browser drehen es dabei meist selbst richtig)
          bmp=await new Promise(function(ok2,err2){var im=new Image();im.onload=function(){ok2(im);};im.onerror=err2;im.src=URL.createObjectURL(fs[i]);});
          bmp.width=bmp.naturalWidth; bmp.height=bmp.naturalHeight;
        }
        var s=Math.min(1,1024/bmp.width,600/bmp.height), w=Math.max(1,Math.round(bmp.width*s)), h=Math.max(1,Math.round(bmp.height*s));
        var c=document.createElement('canvas'); c.width=w; c.height=h; c.getContext('2d').drawImage(bmp,0,0,w,h);
        var blob=await new Promise(function(r){c.toBlob(r,'image/jpeg',0.88);});
        var name='foto-'+Date.now().toString(36)+'-'+i+'.jpg';
        var r=await fetch('/api/bild',{method:'POST',headers:{'X-Name':name,'Content-Type':'image/jpeg'},body:blob});
        if(!r.ok){st.textContent=await r.text();return;}
        ok++;
      }catch(e){st.textContent='Fehler bei '+fs[i].name+' (Format nicht lesbar?)';return;}
    }
    st.textContent=ok+' Bild(er) gespeichert.'; document.getElementById('rf_files').value=''; rahmenListe();
  }
  function rahmenListe(){
    fetch('/api/bilder').then(function(r){return r.json();}).then(function(d){
      var e=document.getElementById('rf_liste');
      if(!d.karte){e.textContent='Keine Speicherkarte gefunden.';return;}
      var t=d.anzahl+' Bild(er) auf der Karte. ';
      if(d.anzahl){t+='<a href="#" onclick="rahmenAlle();return false">Alle l\u00f6schen</a><br>';
        d.namen.forEach(function(n){t+=n+' <a href="#" onclick="rahmenWeg(\''+n+'\');return false">l\u00f6schen</a><br>';});}
      e.innerHTML=t;
    }).catch(function(){});
  }
  function rahmenWeg(n){fetch('/api/bild_loeschen',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'name='+encodeURIComponent(n)}).then(rahmenListe);}
  function rahmenAlle(){if(confirm('Wirklich alle Bilder von der Karte l\u00f6schen?'))fetch('/api/bild_loeschen',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'alle=1'}).then(rahmenListe);}
  rahmenListe();
  </script>
  <hr style="margin:14px 0;border-color:var(--bd)">
  <h3 style="margin:0 0 4px;font-size:1em">Ruhezeit (keine Warnungen auf dem Display)</h3>
  <p class="hint" style="margin:0 0 6px">In der Ruhezeit blinkt nichts und es erscheinen keine Warnbildschirme, Hinweis-Kacheln oder DWD-Warnungen. Danach werden noch bestehende Warnungen wie gewohnt gezeigt. Handy-Meldungen (ntfy) laufen weiter.</p>
  <select name="ruhe_modus" id="rm_sel" onchange="ruheUi()">
    <option value="0" %RMO0%>Aus</option>
    <option value="1" %RMO1%>Feste Uhrzeiten</option>
    <option value="2" %RMO2%>Wie der Nachtmodus</option>
  </select>
  <div id="rm_zeit" style="display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:8px">
    <div><label>Von</label><select name="ruhe_von">%RUHE_VON_OPTIONS%</select></div>
    <div><label>Bis</label><select name="ruhe_bis">%RUHE_BIS_OPTIONS%</select></div>
  </div>
  <p class="hint" style="margin:6px 0 0">%RUHE_INFO%</p>
  <script>function ruheUi(){document.getElementById('rm_zeit').style.display=document.getElementById('rm_sel').value=='1'?'grid':'none';}ruheUi();</script>
  <script>function nachtUi(){var m=document.getElementById('nm_sel').value;document.getElementById('nm_zeit').style.display=m=='1'?'grid':'none';document.getElementById('nm_sonne').style.display=m=='2'?'grid':'none';}nachtUi();</script>
</div>

<div class="card">
  <h2>Hinweise auf dem Hauptscreen</h2>
  <p class="hint">Im Feld &bdquo;Empfehlung&ldquo; auf dem Bildschirm Heute erscheinen Hinweise zu Regen, Frost und starker UV-Strahlung.</p>
  <div class="row"><span>Regen (in den n&auml;chsten Stunden, mit Minutenangabe)</span>
    <label class="toggle"><input type="checkbox" name="hw_regen" %HR%><span class="slider"></span></label></div>
  <div class="row"><span>Frost (0 &deg;C oder darunter in den n&auml;chsten 12 Stunden)</span>
    <label class="toggle"><input type="checkbox" name="hw_frost" %HF%><span class="slider"></span></label></div>
  <div class="row"><span>UV-Index 6 oder h&ouml;her</span>
    <label class="toggle"><input type="checkbox" name="hw_uv" %HU%><span class="slider"></span></label></div>
</div>

<div class="card">
  <h2>Temperaturverlauf</h2>
  <p class="hint">Der Screen nach dem Hauptscreen zeigt die Temperatur der n&auml;chsten 24 Stunden als Linie. Darunter zeigen Balken, wie viel es in jeder Stunde regnet.</p>
  <div class="row"><span>Regenbalken anzeigen (aus: das Temperaturdiagramm wird h&ouml;her)</span>
    <label class="toggle"><input type="checkbox" name="vl_regen" %VR%><span class="slider"></span></label></div>
</div>
<div class="card">
  <h2>T&ouml;ne (Lautsprecher)</h2>
  <h3 style="margin:0 0 4px;font-size:1em">Wetterwarnung</h3>
  <div class="row"><span>Warnton bei neuer Warnung (nicht in der Ruhezeit)</span>
    <label class="toggle"><input type="checkbox" name="warn_ton_an" %WTA%><span class="slider"></span></label></div>
  <label>Warnton</label>
  <select name="warn_ton" id="tw_ton">%WARN_TON_OPTIONS%</select>
  <label>Lautst&auml;rke: <span id="wl_v">%WARN_LAUT%</span>%</label>
  <input type="range" name="warn_laut" id="tw_laut" min="0" max="100" value="%WARN_LAUT%" oninput="document.getElementById('wl_v').textContent=this.value">
  <div style="margin-top:10px" id="tw_test_zeile">
    <button type="button" class="btn" onclick="tonTest(true,'w')">Ton probeh&ouml;ren</button>
    <button type="button" class="btn" onclick="tonTest(false,'w')">Stopp</button>
  </div>
</div>
</div>

<div class="tab" id="t-wetter">
<div class="card">
  <h2>Standort &amp; Region</h2>
  <label>Stadt (ohne Umlaute: Munchen, Koeln)</label>
  <input type="text" name="location" value="%LOCATION%">
  <label>DWD-Region (Warnungen und Karte)</label>
  <select name="dwd_region">%DWD_REGIONEN%</select>
</div>

<!--ST2--><div class="card">
  <h2>Zweiter Standort</h2>
  <p class="hint">Optional, zum Beispiel Wohnort und Ferienhaus. Auf dem Bildschirm Heute schaltest du mit einem Tipp auf den Ortsnamen zwischen beiden um. Wetter, Score, 7-Tage-Vorschau und Warnungen folgen dem gew&auml;hlten Ort. Nach einem Neustart gilt wieder der erste Standort. Feld leeren = zweiten Standort entfernen.</p>
  <label>Stadt (ohne Umlaute: Munchen, Koeln)</label>
  <input type="text" name="location2" value="%LOCATION2%" placeholder="leer = kein zweiter Standort">
  <label>DWD-Region (Warnungen und Karte)</label>
  <select name="dwd_region2">%DWD_REGIONEN2%</select>
  <p class="hint" id="so_status">%STANDORT_STATUS%</p>
  <button type="button" class="btn btn-blau" onclick="standortWechseln(0)">Standort 1 anzeigen</button>
  <button type="button" class="btn btn-blau" onclick="standortWechseln(1)">Standort 2 anzeigen</button>
</div>
<!--/ST2-->
<div class="card">
  <h2>Warnungen</h2>
  <div class="row"><span>DWD-Wetterwarnungen</span>
    <label class="toggle"><input type="checkbox" name="warn_region" %WR%><span class="slider"></span></label></div>
</div>

</div>

<div class="savebar" id="savebar">
  <button type="submit" class="btn btn-gruen">Einstellungen speichern</button>
  <span id="dirty">Ungespeicherte &Auml;nderungen</span>
</div>
</form>

<div class="tab" id="t-system">
<div class="card">
  <h2>Benachrichtigung aufs Handy (ntfy)</h2>
  <p class="hint"><b>ntfy</b> ist ein kostenloser Dienst, der Nachrichten aufs Handy schickt. Die Meldungen laufen &uuml;ber den Server von ntfy.sh (ein Dienst eines Dritten); wer das nicht m&ouml;chte, tr&auml;gt unten einen eigenen Server ein. So richtest du es ein: Installiere die App <b>ntfy</b> (Android oder iPhone), tippe auf &bdquo;+&ldquo; und trage ein <b>Topic</b> ein, am besten einen langen, nicht erratbaren Namen wie <i>wettercube-k7x2m9q4</i>. Dasselbe Topic tr&auml;gst du hier ein. Das Topic wirkt wie ein Passwort: Wer es kennt, kann mitlesen. Es steht deshalb nicht in der Einstellungssicherung und nicht im Log.</p>
  <form action="/api/ntfy_save" method="post">
  <div class="row"><span>Benachrichtigungen aktiv</span>
    <label class="toggle"><input type="checkbox" name="ntfy_aktiv" %NA%><span class="slider"></span></label></div>
  <label>Server</label>
  <input type="text" name="ntfy_server" value="%NSRV%" autocapitalize="off" autocorrect="off">
  <label>Topic</label>
  <input type="text" name="ntfy_topic" value="%NTOP%" autocapitalize="off" autocorrect="off" autocomplete="off">
  <details><summary>Gesch&uuml;tztes Topic oder eigener Server mit Anmeldung</summary>
    <label>Zugangstoken</label>
    <input type="password" name="ntfy_token" autocomplete="new-password" placeholder="%NTOKPH%">
    <label style="display:flex;gap:8px;align-items:center"><input type="checkbox" name="ntfy_token_weg" style="width:auto"> gespeicherten Token l&ouml;schen</label>
  </details>
  <label style="margin-top:12px">Das wird gemeldet</label>
  <div class="row"><span>Guter Fahrtag: abends der Hinweis auf das beste Fahrfenster von morgen (ab Score 65)</span>
    <label class="toggle"><input type="checkbox" name="ntfy_q_tag" %NQT%><span class="slider"></span></label></div>
  <div class="row"><span>Regen in K&uuml;rze am Heimatort (&bdquo;Regen in 25 Minuten&ldquo;)</span>
    <label class="toggle"><input type="checkbox" name="ntfy_q_regen" %NQR%><span class="slider"></span></label></div>
  <div class="row"><span>Neue DWD-Warnung f&uuml;r den Standort</span>
    <label class="toggle"><input type="checkbox" name="ntfy_q_warn" %NQW%><span class="slider"></span></label></div>
  <div class="row"><span>Saisonstart: einmal im Jahr, wenn es an einem Tag ab Februar &uuml;ber 15 &deg;C und gut f&uuml;rs Fahren wird</span>
    <label class="toggle"><input type="checkbox" name="ntfy_q_saison" %NQS%><span class="slider"></span></label></div>
  <div class="row"><span>Termine (T&Uuml;V, Inspektion, Reifen ...): Erinnerung 14 Tage vorher, 3 Tage vorher und am Tag selbst</span>
    <label class="toggle"><input type="checkbox" name="ntfy_q_termin" %NQM%><span class="slider"></span></label></div>
  <details style="margin:6px 0"><summary>Termine eintragen (bis zu vier)</summary>
    <div class="row"><input type="text" name="tm_name0" value="%TN0%" maxlength="24" placeholder="z. B. T&Uuml;V"><input type="date" name="tm_datum0" value="%TD0%"></div>
    <div class="row"><input type="text" name="tm_name1" value="%TN1%" maxlength="24" placeholder="z. B. Inspektion"><input type="date" name="tm_datum1" value="%TD1%"></div>
    <div class="row"><input type="text" name="tm_name2" value="%TN2%" maxlength="24" placeholder="z. B. Reifenwechsel"><input type="date" name="tm_datum2" value="%TD2%"></div>
    <div class="row"><input type="text" name="tm_name3" value="%TN3%" maxlength="24" placeholder="z. B. Versicherung"><input type="date" name="tm_datum3" value="%TD3%"></div>
  </details>
  <div class="row"><span>Entwarnung senden, wenn ein Alarm endet</span>
    <label class="toggle"><input type="checkbox" name="ntfy_ent" %NEN%><span class="slider"></span></label></div>
  <button type="submit" class="btn btn-gruen">Speichern</button>
  <button type="button" class="btn" onclick="ntfyTest()">Testnachricht senden</button>
  <p class="hint" id="ntfy_msg" style="margin-top:8px"></p>
  </form>
  <script>
  function ntfyTest(){var m=document.getElementById('ntfy_msg');m.textContent='Wird gesendet ...';
    fetch('/api/ntfy_test',{method:'POST'}).then(function(){var n=0;var t=setInterval(function(){
      fetch('/api/ntfy_status').then(function(r){return r.text();}).then(function(s){m.textContent=s;});if(++n>4)clearInterval(t);},1500);});}
  </script>
</div>

<div class="card">
  <h2>Speicherkarte</h2>
  <p class="hint">Eine microSD-Karte (FAT32) im Einschub speichert das Log und die Notfall-Kopie der Firmware. Status: <b>%SD%</b></p>
  <a href="/karte" class="btn btn-blau">Speicherkarte auslesen</a>
</div>

<div class="card">
  <h2>WLAN-Zugangsdaten aendern</h2>
  <p class="hint">SSID/Passwort aendern ohne neu zu flashen.</p>
  <a href="/wlan" class="btn btn-blau">WLAN aendern</a>
</div>

<div class="card">
  <h2>Passwort f&uuml;r die Web-Oberfl&auml;che</h2>
  <p class="hint">%WPSTATUS%</p>
  <p class="hint">Mit Passwort fragt das Ger&auml;t beim &Ouml;ffnen dieser Seite nach Benutzername (<b>admin</b>) und Passwort. Das Passwort wird im Heimnetz unverschl&uuml;sselt (HTTP) &uuml;bertragen: Es sch&uuml;tzt vor Zugriffen anderer Ger&auml;te und fremder Webseiten, nicht vor jemandem, der den Netzverkehr mitliest. <b>Vergessen?</b> Die Notfall-App kann es entfernen (&bdquo;Web-Passwort entfernen&ldquo;).</p>
  <form action="/api/web_pw_save" method="post" onsubmit="return wpBestaetigen()">
    <label>Neues Passwort (mindestens 6 Zeichen)</label>
    <input type="password" name="pw1" autocomplete="new-password">
    <label>Wiederholen</label>
    <input type="password" name="pw2" autocomplete="new-password">
    <button type="submit" class="btn btn-blau" style="margin-top:8px">Passwort speichern</button>
    <label style="display:flex;gap:8px;align-items:center;margin-top:10px"><input type="checkbox" name="pw_weg" style="width:auto"> Passwort entfernen</label>
  </form>
</div>

<div class="card">
  <h2>Firmware-Update</h2>
  <div class="status">Aktuelle Version: %VERSION%</div>
  <form action="/api/update_modus_save" method="post" onsubmit="return umBestaetigen()">
    <label>Updates</label>
    <label style="display:flex;gap:8px;align-items:flex-start"><input type="radio" name="update_modus" value="2" style="width:auto;margin-top:4px" %UMS2% onchange="umWarnung()"><span><b>Hinweis am Display und in der Web-Oberfl&auml;che</b> (empfohlen). Eine neue Version erscheint als Update-Symbol auf dem Hauptscreen.</span></label>
    <label style="display:flex;gap:8px;align-items:flex-start"><input type="radio" name="update_modus" value="1" style="width:auto;margin-top:4px" %UMS1% onchange="umWarnung()"><span><b>Nur in der Web-Oberfl&auml;che.</b> Updates sind &uuml;ber diese Seite m&ouml;glich, aber das Display zeigt keinen Hinweis.</span></label>
    <label style="display:flex;gap:8px;align-items:flex-start"><input type="radio" name="update_modus" value="0" style="width:auto;margin-top:4px" %UMS0% onchange="umWarnung()"><span><b>Ganz aus.</b> Das Ger&auml;t fragt nicht nach neuen Versionen.</span></label>
    <label style="margin-top:8px">Nach neuen Versionen fragen alle</label>
    <select name="update_std"><option value="1" %UST1%>1 Stunde</option><option value="6" %UST6%>6 Stunden (Standard)</option><option value="24" %UST24%>24 Stunden</option></select>
    <div id="um_warn" class="status" style="display:none;margin-top:8px"><b>Achtung:</b> Ohne Updates kommen auch <b>keine Sicherheitsupdates</b> mehr. Du musst die Firmware dann selbst aktuell halten (neue Datei von GitHub laden und unten installieren).</div>
    <button type="submit" class="btn btn-blau" style="margin-top:8px">Speichern</button>
  </form>
  <div class="status">%UINFO%</div>
  <div %UMHIDE%>
  <button type="button" class="btn btn-blau" onclick="checkOta()">Auf Updates pruefen</button>
  <div id="ota_status" class="status"></div>
  </div>
  <hr style="margin:14px 0;border-color:var(--bd)">
  <label>Firmware aus einer Datei installieren (z. B. firmware.bin)</label>
  <input type="file" id="fw_file" accept=".bin,application/octet-stream">
  <button type="button" class="btn btn-rot" onclick="firmwareDatei()">Datei installieren</button>
  <div id="fw_status" class="status"></div>
  <div class="status" style="margin-top:10px">Hinweis: %UPDATE_HINWEIS% Das Geraet dabei <b>nicht vom Strom trennen</b> &ndash; es startet danach selbst neu (bis zu 5 Minuten).</div>
</div>

<div class="card">
  <h2>Einstellungen sichern</h2>
  <p class="hint">Speichert alle Einstellungen (ohne WLAN-Passwort) als Datei und spielt sie bei Bedarf wieder ein, zum Beispiel nach einer Neuinstallation mit gel&ouml;schtem Speicher. Auch Sicherungen des WetterCubePlus werden akzeptiert.</p>
  <a href="/backup" class="btn btn-blau">Einstellungen herunterladen</a>
  <label>Sicherung wiederherstellen</label>
  <input type="file" id="restore_file" accept=".json,application/json">
  <button type="button" class="btn btn-rot" onclick="restoreSettings()">Wiederherstellen</button>
  <div id="restore_status" class="status"></div>
</div>

<div class="card">
  <h2>Wiederherstellung</h2>
  <p class="hint">Falls das Ger&auml;t nicht mehr richtig l&auml;uft oder ein Update schiefgegangen ist: Die Notfall-App erneuert die Firmware (von der Speicherkarte oder aus dem Ger&auml;t) oder setzt das Ger&auml;t in den Werkszustand zur&uuml;ck. Gew&auml;hlt wird <b>am Display</b>, nichts passiert ohne deine Best&auml;tigung. %NOTFALL%.</p>
  <button type="button" class="btn btn-rot" %NOTFALL_KNOPF% onclick="wiederherstellung()">Wiederherstellung starten</button>
  <div id="wh_status" class="status"></div>
  <hr style="margin:14px 0;border-color:var(--bd)">
  <p class="hint">Die Einstellungen werden bei jeder &Auml;nderung ohne Passw&ouml;rter auf die Speicherkarte gesichert (notfall/einstellungen.json). %SICHERUNG%.</p>
  <button type="button" class="btn btn-blau" %SICHERUNG_KNOPF% onclick="sicherungKarte()">Einstellungen von der Karte wiederherstellen</button>
  <div id="sk_status" class="status"></div>
</div>

<div class="card">
  <h2>Diagnose</h2>
  <p class="hint">Das Log zeigt Start, WLAN, Datenabrufe und Fehler. Bei Problemen hilft es bei der Suche.</p>
  <a href="/log" target="_blank" class="btn btn-blau">Log ansehen</a>
  <p class="hint" style="margin-top:14px">Das Bildschirmfoto zeigt, was das Display gerade anzeigt. Es wird als PNG gespeichert (dauert einige Sekunden).</p>
  <a href="/screenshot.png" download="wettercube-bildschirm.png" class="btn btn-blau">Bildschirmfoto speichern</a>
</div>

<div class="card">
  <h2>Datenquellen</h2>
  <p class="hint">Quellcode (AGPL-3.0): <a href="https://github.com/JPPeterson-lab/WetterCube-Biker" target="_blank" rel="noopener">github.com/JPPeterson-lab/WetterCube-Biker</a></p>
  <p class="hint">Wetterdaten: <a href="https://open-meteo.com" target="_blank" rel="noopener">Weather data by Open-Meteo.com</a> (<a href="https://open-meteo.com/en/licence" target="_blank" rel="noopener">CC BY 4.0</a>; die kostenlose Schnittstelle ist nur f&uuml;r nicht-kommerzielle Nutzung). Die Werte werden im WetterCube teils umgerechnet und zusammengefasst, zum Beispiel zum Biker-Score und zu Tagesh&ouml;chstwerten. Ortssuche: Location data by <a href="https://www.geonames.org" target="_blank" rel="noopener">GeoNames</a> via Open-Meteo. Warnungen und Radar: Quelle: <a href="https://opendata.dwd.de" target="_blank" rel="noopener">Deutscher Wetterdienst</a> (<a href="https://www.dwd.de/DE/service/rechtliche_hinweise/rechtliche_hinweise_node.html" target="_blank" rel="noopener">CC BY 4.0</a>).</p>
</div>
</div>

<script>

function rwFarbe(s){return s>=75?'#00A838':s>=55?'#8DB600':s>=35?'#E8A000':'#D01818';}
function zT(t,tag){var d=new Date(t*1000),W=['So','Mo','Di','Mi','Do','Fr','Sa'];function z(n){return (n<10?'0':'')+n;}return (tag?W[d.getDay()]+' ':'')+z(d.getHours())+':'+z(d.getMinutes());}
function esc(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;');}
function warteAuftrag(msgId,fertig){
  var m=document.getElementById(msgId);
  var t=setInterval(function(){fetch('/api/route_status').then(function(r){return r.json();}).then(function(d){
    m.textContent=d.text||'';
    if(d.status===2){clearInterval(t);if(fertig)fertig();}
    else if(d.status===3){clearInterval(t);m.textContent='Fehler: '+d.text;}
  }).catch(function(){});},900);
}
function routenLaden(){
  fetch('/api/routen').then(function(r){return r.json();}).then(function(d){
    var h='';
    if(!d.routen.length)h='<p class="hint">Noch keine Route gespeichert.</p>';
    for(var i=0;i<d.routen.length;i++){var r=d.routen[i];
      h+='<div class="row"><span><b>'+esc(r.name)+'</b><br><span class="hint">'+r.km+' km'+(r.gpx?', GPX-Datei':'')+(r.id===d.aktiv?', aktiv':'')+'</span></span>'+
        '<span><button type="button" class="btn btn-blau" onclick="routeWaehlen(\''+r.id+'\')">Wetter</button> '+
        '<a class="btn" href="/api/route_export?id='+r.id+'" download>GPX</a> '+
        '<button type="button" class="btn" onclick="routeLoesch(\''+r.id+'\')">L&ouml;schen</button></span></div>';}
    document.getElementById('rt_liste').innerHTML=h;
    if(d.tempo)document.getElementById('rw_tempo').value=d.tempo;
    if(d.aktiv)ergebnisLaden();
  }).catch(function(){});
}
function post(url,body,ctype){return fetch(url,{method:'POST',headers:ctype?{'Content-Type':ctype}:{},body:body});}
function routeNeu(){
  var m=document.getElementById('rn_msg');
  var b='start='+encodeURIComponent(document.getElementById('rn_start').value)+'&ziel='+encodeURIComponent(document.getElementById('rn_ziel').value)+
    '&via='+encodeURIComponent(document.getElementById('rn_via').value)+'&name='+encodeURIComponent(document.getElementById('rn_name').value)+
    '&autobahn='+(document.getElementById('rn_auto').checked?'1':'0');
  m.textContent='Wird berechnet ...';
  post('/api/route_neu',b,'application/x-www-form-urlencoded').then(function(r){return r.text().then(function(t){
    if(!r.ok){m.textContent='Fehler: '+t;return;}
    warteAuftrag('rn_msg',function(){m.textContent='Fertig.';routenLaden();});});});
}
function gpxHoch(){
  var f=document.getElementById('gpx_datei').files[0],m=document.getElementById('gpx_msg');
  if(!f){m.textContent='Bitte eine GPX-Datei w\u00e4hlen.';return;}
  m.textContent='Wird hochgeladen ('+Math.round(f.size/1024)+' KB) ...';
  var n=document.getElementById('gpx_name').value||f.name.replace(/\.gpx$/i,'');
  fetch('/api/route_gpx?name='+encodeURIComponent(n),{method:'POST',body:f}).then(function(r){return r.text().then(function(t){
    if(!r.ok){m.textContent='Fehler: '+t;return;}
    m.textContent='Gespeichert. Wetter wird geholt ...';
    post('/api/route_waehlen?id='+t,'').then(function(){warteAuftrag('gpx_msg',function(){m.textContent='Fertig.';routenLaden();});});});});
}
function routeWaehlen(id){
  var m=document.getElementById('rn_msg');
  post('/api/route_waehlen?id='+id,'').then(function(r){if(!r.ok){m.textContent='Es l\u00e4uft schon eine Berechnung.';return;}
    document.getElementById('rw_leer').textContent='Wetter wird geholt ...';
    warteAuftrag('rn_msg',function(){routenLaden();});});
}
function routeLoesch(id){if(!confirm('Route l\u00f6schen?'))return;post('/api/route_loeschen?id='+id,'').then(function(){routenLaden();});}
function routeAktual(){post('/api/route_aktualisieren','').then(function(r){if(r.ok)warteAuftrag('rn_msg',function(){ergebnisLaden();});});}
function abfahrtSetzen(jetzt){
  var t=0,v=document.getElementById('rw_abf').value;
  if(!jetzt&&v)t=Math.floor(new Date(v).getTime()/1000);
  post('/api/route_abfahrt?t='+t+'&tempo='+document.getElementById('rw_tempo').value,'').then(function(){ergebnisLaden();});
}
function ergebnisLaden(){
  fetch('/api/route_ergebnis').then(function(r){return r.json();}).then(function(d){
    if(d.leer){document.getElementById('rw_leer').style.display='block';document.getElementById('rw_inhalt').style.display='none';return;}
    document.getElementById('rw_leer').style.display='none';document.getElementById('rw_inhalt').style.display='block';
    var e=d.ergebnis,r=d.route;
    document.getElementById('rw_titel').textContent=r.name;
    var min=Math.round(r.km/e.tempo*60);
    document.getElementById('rw_info').innerHTML='<b>Score '+e.gesamt+'</b> &middot; '+r.km+' km, '+Math.floor(min/60)+' Std. '+(min%60)+' Min. bei '+e.tempo+' km/h &middot; Abfahrt '+zT(e.abfahrt,true)+', Ankunft '+zT(e.ankunft,false)+
      '<br>Temperatur '+Math.round(e.tmin)+' bis '+Math.round(e.tmax)+' &deg;C &middot; B&ouml;en bis '+Math.round(e.boemax)+' km/h &middot; '+(e.regen?'Regen ab km '+e.regenkm:'kein Regen')+
      ' &middot; H&ouml;he '+r.hmin+' bis '+r.hmax+' m, Anstieg '+r.anstieg+' m &middot; '+(e.frostkm>=0?'Frost ab km '+e.frostkm:'kein Frost')+' &middot; '+(e.hell?'Ankunft im Hellen':'Ankunft nach Sonnenuntergang')+(e.blendkm>=0?' &middot; Blendung ab km '+e.blendkm:'')+(Math.abs(e.langs)>=8?' &middot; R&uuml;ckweg auf derselben Strecke bei gleichem Wind: '+(e.langs<0?'R&uuml;ckenwind':'Gegenwind'):'')+(e.pause?'<br><b style="color:#C8501E">'+e.pause+'</b>':'');
    var abf=new Date((e.abfahrt)*1000);abf.setMinutes(abf.getMinutes()-abf.getTimezoneOffset());document.getElementById('rw_abf').value=abf.toISOString().slice(0,16);
    document.getElementById('rw_tempo').value=e.tempo;
    var h='<tr><th>km</th><th>Zeit</th><th>Score</th><th>&deg;C</th><th>Regen mm</th><th>B&ouml;en</th><th>Wind l&auml;ngs</th><th>H&ouml;he</th></tr>';
    for(var i=0;i<d.abschnitte.length;i++){var a=d.abschnitte[i];
      h+='<tr><td>'+a.km0+'-'+a.km1+'</td><td>'+zT(a.t,false)+'</td><td style="background:'+rwFarbe(a.score)+';color:#fff;font-weight:bold">'+a.score+'</td><td>'+Math.round(a.temp)+'</td><td>'+a.mm.toFixed(1)+'</td><td>'+Math.round(a.boe)+'</td><td>'+(a.langs<0?'Gegen ':'R&uuml;cken ')+Math.round(Math.abs(a.langs))+'</td><td>'+a.h+' m</td></tr>';}
    document.getElementById('rw_tab').innerHTML=h;
    var p=d.planer,ph='',pt='';
    for(var k=0;k<p.scores.length;k++){var s=p.scores[k],t=p.start+k*3600;
      ph+='<div title="'+zT(t,true)+': '+(s<0?'-':s)+'" onclick="abfSetzenZeit('+t+')" style="flex:1;cursor:pointer;height:'+(s<0?4:Math.max(4,s*1.15))+'px;background:'+(s<0?'#C9D3DD':rwFarbe(s))+(k===p.beste?';outline:2px solid #111':'')+';border-radius:2px"></div>';
      var hh=new Date(t*1000).getHours();pt+='<div style="flex:1;text-align:center">'+(hh%6===0?hh:'')+'</div>';}
    document.getElementById('rw_planer').innerHTML=ph;document.getElementById('rw_planer_t').innerHTML=pt;
    document.getElementById('rw_best').innerHTML=p.beste>=0?'Beste Abfahrt: <b>'+zT(p.start+p.beste*3600,true)+'</b> (Score '+p.scores[p.beste]+'). Wenn du jetzt losf&auml;hrst: Score '+p.jetzt+'. <a href="/api/route_export?id='+r.id+'" download>GPX mit bester Abfahrt laden</a>':'Der Zeitraum der Vorhersage reicht f&uuml;r diese Strecke nicht aus.';
  }).catch(function(){});
}

function abfSetzenZeit(t){post('/api/route_abfahrt?t='+t+'&tempo='+document.getElementById('rw_tempo').value,'').then(function(){ergebnisLaden();});}
function fahrtenLaden(){
  fetch('/api/fahrten').then(function(r){return r.json();}).then(function(d){
    window.FB=d.fahrten||[];
    var jahre={};for(var i=0;i<FB.length;i++)jahre[new Date(FB[i].t*1000).getFullYear()]=1;
    var j=Object.keys(jahre).sort().reverse(),s=document.getElementById('fb_jahr'),alt=s.value;
    if(!j.length)j=[String(new Date().getFullYear())];
    s.innerHTML=j.map(function(y){return '<option>'+y+'</option>';}).join('');
    if(alt&&j.indexOf(alt)>=0)s.value=alt;
    fahrtenZeigen();
  }).catch(function(){});
}
function fahrtenZeigen(){
  var jahr=+document.getElementById('fb_jahr').value,L=(window.FB||[]).filter(function(f){return new Date(f.t*1000).getFullYear()===jahr;});
  var km=0,sc=0,ns=0,regen=0,kalt=99,warm=-99,mon=[0,0,0,0,0,0,0,0,0,0,0,0];
  L.forEach(function(f){km+=f.km;mon[new Date(f.t*1000).getMonth()]+=f.km;if(f.score>=0){sc+=f.score;ns++;if(f.regen>0)regen++;if(f.tmin<kalt)kalt=f.tmin;if(f.tmax>warm)warm=f.tmax;}});
  document.getElementById('fb_summe').innerHTML=L.length?'<b>'+L.length+' Fahrten, '+km+' km</b>'+(ns?' &middot; &Oslash; Score '+Math.round(sc/ns)+' &middot; '+regen+' mit Regen auf der Strecke &middot; k&auml;lteste Fahrt '+Math.round(kalt)+' &deg;C, w&auml;rmste '+Math.round(warm)+' &deg;C':''):'Noch keine Fahrt in '+jahr+'.';
  var mx=Math.max.apply(null,mon.concat([1])),M=['J','F','M','A','M','J','J','A','S','O','N','D'],b='',t='';
  for(var m=0;m<12;m++){b+='<div title="'+mon[m]+' km" style="flex:1;height:'+(mon[m]?Math.max(4,mon[m]/mx*66):2)+'px;background:'+(mon[m]?'#1D6FD6':'#C9D3DD')+';border-radius:2px"></div>';t+='<div style="flex:1;text-align:center">'+M[m]+'</div>';}
  document.getElementById('fb_monate').innerHTML=b;document.getElementById('fb_monate_t').innerHTML=t;
  var h='<tr><th>Abfahrt</th><th>Route</th><th>km</th><th>Score</th><th>&deg;C</th><th>Regen</th><th>B&ouml;en</th><th></th></tr>';
  L.forEach(function(f){var w=f.score>=0;
    h+='<tr><td>'+zT(f.t,true)+' '+new Date(f.t*1000).toLocaleDateString('de-DE',{day:'2-digit',month:'2-digit'})+'</td><td>'+esc(f.name)+'</td><td>'+f.km+'</td>'+
      (w?'<td style="background:'+rwFarbe(f.score)+';color:#fff;font-weight:bold">'+f.score+'</td><td>'+Math.round(f.tmin)+(Math.round(f.tmin)===Math.round(f.tmax)?'':' bis '+Math.round(f.tmax))+'</td><td>'+(f.regen?f.regen+' von '+f.n+' Abschn.':'kein')+'</td><td>'+f.boe+'</td>':'<td colspan="4">kein Wetter gespeichert</td>')+
      '<td><button type="button" class="btn" onclick="fahrtLoesch('+f.t+')">&times;</button></td></tr>';});
  document.getElementById('fb_tab').innerHTML=L.length?h:'';
}
function fahrtEintragen(){var m=document.getElementById('fb_msg');
  post('/api/fahrt_eintragen','').then(function(r){return r.text().then(function(t){m.textContent=r.ok?'Eingetragen.':t;if(r.ok)fahrtenLaden();});});}
function fahrtLoesch(t){if(!confirm('Eintrag l\u00f6schen?'))return;post('/api/fahrt_loeschen?t='+t,'').then(function(){fahrtenLaden();});}
routenLaden();
fahrtenLaden();
function gebieteLaden(){
  fetch('/api/gebiete').then(function(r){return r.json();}).then(function(d){
    var l=document.getElementById('gb_liste');if(document.activeElement!==l)l.value=d.liste;
    var h='<tr><th>Gebiet</th><th>Heute</th><th>Morgen</th><th>Wochenende</th></tr>';
    var g=d.gebiete.slice();
    for(var i=0;i<g.length;i++){h+='<tr><td style="text-align:left">'+esc(g[i].name)+'</td>';
      for(var k=0;k<3;k++){var s=g[i].s[k];h+='<td style="'+(s>=0?'background:'+rwFarbe(s)+';color:#fff;font-weight:bold':'')+'">'+(s>=0?s+' <small>('+Math.round(g[i].t[k])+'&deg;, '+g[i].mm[k].toFixed(1)+' mm)</small>':'-')+'</td>';}
      h+='</tr>';}
    document.getElementById('gb_tab').innerHTML=h;
    if(d.status===1)setTimeout(gebieteLaden,1500);
    document.getElementById('gb_msg').textContent=d.status===1?d.text:(d.status===3?'Fehler: '+d.text:(d.stand?'Stand '+zT(d.stand,true):''));
  }).catch(function(){});
}
function gebieteSpeichern(){post('/api/gebiete_save','liste='+encodeURIComponent(document.getElementById('gb_liste').value),'application/x-www-form-urlencoded').then(function(r){return r.text().then(function(t){document.getElementById('gb_msg').textContent=r.ok?'Wird bewertet ...':'Fehler: '+t;setTimeout(gebieteLaden,1200);});});}
function gebieteAktual(){post('/api/gebiete_aktualisieren','').then(function(){setTimeout(gebieteLaden,1200);});}
gebieteLaden();
function standortWechseln(n){
  var s=document.getElementById('so_status');
  fetch('/api/standort?nr='+n,{method:'POST'}).then(function(r){return r.text();}).then(function(t){if(s)s.textContent=t;}).catch(function(){if(s)s.textContent='Fehler beim Umschalten';});
}
function wpBestaetigen(){
  var f=document.querySelector('input[name=pw_weg]');
  if(f&&f.checked) return confirm('Passwort entfernen? Dann kann jeder im Heimnetz die Einstellungen aendern.');
  return true;
}
function umWarnung(){
  var r=document.querySelector('input[name=update_modus]:checked');
  document.getElementById('um_warn').style.display=(r&&r.value==='0')?'block':'none';
}
function umBestaetigen(){
  var r=document.querySelector('input[name=update_modus]:checked');
  if(r&&r.value==='0') return confirm('Updates ganz ausschalten?\n\nDann kommen auch keine Sicherheitsupdates mehr. Du musst die Firmware selbst aktuell halten.');
  return true;
}
document.addEventListener('DOMContentLoaded',umWarnung);
function checkOta(){
  document.getElementById('ota_status').textContent='Pruefe...';
  fetch('/ota_check').then(function(r){return r.json();}).then(function(d){
    if(d.aus){ document.getElementById('ota_status').textContent='Updates sind ausgeschaltet.'; return; }
    if(d.update_available){
      document.getElementById('ota_status').innerHTML=
        'Neue Version: '+d.latest+' &nbsp;'+
        '<button class="btn btn-rot" onclick="doUpdate()">Jetzt installieren</button>';
    } else {
      document.getElementById('ota_status').textContent='Bereits aktuell ('+d.latest+')';
    }
  }).catch(function(){document.getElementById('ota_status').textContent='Fehler beim Pruefen';});
}
function doUpdate(){
  if(!confirm('Update jetzt installieren?\n\n%UPDATE_CONFIRM% Das Geraet NICHT vom Strom trennen, es startet danach selbst neu.')) return;
  document.getElementById('ota_status').textContent='Update laeuft... %UPDATE_LAEUFT% Geraet NICHT ausschalten (bis zu 5 Minuten).';
  fetch('/ota_update',{method:'POST'}).then(function(r){return r.text();}).then(function(t){
    document.getElementById('ota_status').textContent=(t==='ok')?'Update installiert. Das Geraet startet neu ...':t;
  }).catch(function(){
    document.getElementById('ota_status').textContent='Verbindung beendet. Wenn das Geraet gerade neu startet, ist das normal.';
  });
}
function firmwareDatei(){
  var f=document.getElementById('fw_file').files[0], st=document.getElementById('fw_status');
  if(!f){st.textContent='Bitte zuerst eine Datei waehlen.';return;}
  if(!confirm('Firmware aus "'+f.name+'" jetzt installieren?\n\nDas Geraet NICHT vom Strom trennen, es startet danach selbst neu.')) return;
  var x=new XMLHttpRequest();
  x.upload.onprogress=function(e){if(e.lengthComputable)st.textContent='Wird hochgeladen und installiert: '+Math.round(e.loaded*100/e.total)+' %';};
  x.onload=function(){st.textContent=(x.status===200)?'Installiert. Das Geraet startet neu (Seite in ca. 1 Minute neu laden).':'Fehler: '+x.responseText;};
  x.onerror=function(){st.textContent='Verbindung beendet. Wenn das Geraet gerade neu startet, ist das normal.';};
  st.textContent='Wird hochgeladen ...';
  x.open('POST','/update_datei'); x.setRequestHeader('Content-Type','application/octet-stream'); x.send(f);
}
function wiederherstellung(){
  if(!confirm('Wiederherstellung starten?\n\nDas Geraet startet in die Notfall-App. Dort waehlst du am Display, was passieren soll. Bis dahin wird nichts veraendert.')) return;
  var st=document.getElementById('wh_status'); st.textContent='Starte die Wiederherstellung ... Bitte jetzt auf das Display schauen.';
  fetch('/wiederherstellung',{method:'POST'}).then(function(r){return r.text();}).then(function(t){if(t!=='ok')st.textContent='Fehler: '+t;}).catch(function(){});
}
function sicherungKarte(){
  if(!confirm('Die Einstellungen von der Speicherkarte jetzt einspielen? Das Geraet startet danach neu.')) return;
  var st=document.getElementById('sk_status'); st.textContent='Wird eingespielt ...';
  fetch('/restore_karte',{method:'POST'}).then(function(r){return r.text().then(function(t){return {ok:r.ok,t:t};});})
    .then(function(o){st.textContent=o.ok?'Eingespielt. Das Geraet startet neu (Seite in ca. 30 Sekunden neu laden).':'Fehler: '+o.t;})
    .catch(function(){st.textContent='Verbindung beendet. Wenn das Geraet gerade neu startet, ist das normal.';});
}
function restoreSettings(){
  var f=document.getElementById('restore_file').files[0];
  var st=document.getElementById('restore_status');
  if(!f){st.textContent='Bitte zuerst eine Datei waehlen.';return;}
  if(!confirm('Die Einstellungen aus der Datei jetzt einspielen? Das Geraet startet danach neu.')) return;
  var r=new FileReader();
  r.onload=function(){
    st.textContent='Wird eingespielt ...';
    fetch('/restore',{method:'POST',headers:{'Content-Type':'application/json'},body:r.result})
      .then(function(x){return x.text().then(function(tx){return {ok:x.ok,t:tx};});})
      .then(function(o){st.textContent=o.ok?'Eingespielt. Das Geraet startet neu (Seite in ca. 30 Sekunden neu laden).':'Fehler: '+o.t;})
      .catch(function(){st.textContent='Verbindung beendet. Wenn das Geraet gerade neu startet, ist das normal.';});
  };
  r.readAsText(f);
}

var TABS=['routen','anzeige','wetter','system'];
function zeige(t){
  if(TABS.indexOf(t)<0)t='anzeige';
  for(var i=0;i<TABS.length;i++){
    var s=document.getElementById('t-'+TABS[i]);if(s)s.style.display=(TABS[i]===t)?'block':'none';
    var a=document.querySelector('.tabs a[data-t="'+TABS[i]+'"]');if(a)a.className=(TABS[i]===t)?'aktiv':'';
  }
  var sb=document.getElementById('savebar');if(sb)sb.style.display=(t==='anzeige'||t==='wetter')?'flex':'none';
  try{localStorage.setItem('wcf-tab',t);}catch(e){}
  if(location.hash!=='#'+t&&history.replaceState)history.replaceState(null,'','#'+t);
}
function startTab(){
  var t=location.hash.replace('#','');
  if(!t){try{t=localStorage.getItem('wcf-tab')||'';}catch(e){}}
  zeige(t);
}
window.addEventListener('hashchange',function(){zeige(location.hash.replace('#',''));});
document.getElementById('tabs').addEventListener('click',function(e){var a=e.target.closest?e.target.closest('a[data-t]'):null;if(a){e.preventDefault();zeige(a.getAttribute('data-t'));}});
document.getElementById('einst').addEventListener('input',function(){var d=document.getElementById('dirty');if(d)d.style.display='inline';});
wcThemeLabel();startTab();
function tonTest(an,w){
  var m=document.getElementById('tt_msg');
  if(!an){fetch('/api/tontest?aus=1',{method:'POST'});m.textContent='';return;}
  var ton=document.getElementById('t'+w+'_ton').value,laut=document.getElementById('t'+w+'_laut').value;
  fetch('/api/tontest?ton='+ton+'&laut='+laut,{method:'POST'}).then(function(r){return r.text();}).then(function(t){m.textContent=t==='ok'?'Ton l\u00e4uft (Stopp beendet ihn)':t;}).catch(function(){m.textContent='Fehler';});
}
</script>
</body></html>
)rawhtml";

const char WLAN_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>WLAN aendern</title>
)rawhtml" WEBUI_THEME_HEAD WEBUI_THEME_CSS R"rawhtml(
<style>
  body{font-family:Arial;background:var(--bg);color:var(--txt);padding:20px}
  .card{background:var(--card);border:1px solid var(--bd);border-radius:8px;padding:20px;max-width:420px;margin:auto}
  h1{color:var(--acc);font-size:1.3em}
  label{display:block;color:var(--mut);font-size:.85em;margin-top:10px}
  input,select{width:100%;padding:8px;border-radius:4px;border:1px solid var(--bd);
    background:var(--in);color:var(--txt);box-sizing:border-box;margin-top:3px}
  .btn{width:100%;padding:10px;background:#238636;color:#fff;border:none;border-radius:4px;font-size:1em;cursor:pointer;margin-top:16px}
  .btn:hover{background:#2ea043}
  a{color:var(--acc)}
</style></head><body>
<div class="wcnav"><a href="/">Einstellungen</a><span class="rechts"><button id="wcThemeBtn" class="themebtn" onclick="wcTheme()"></button></span></div>
<script>wcThemeLabel();</script>
<div class="card">
  <h1>WLAN-Zugangsdaten aendern</h1>
  <form action="/wlan_speichern" method="post">
    <label>Netzwerk</label>
    <select name="ssid">%NETZWERKE%</select>
    <label>Passwort</label>
    <input type="password" name="pass">
    <button type="submit" class="btn">Speichern &amp; Neustart</button>
  </form>
  <br><a href="/">Zurueck</a>
</div></body></html>
)rawhtml";