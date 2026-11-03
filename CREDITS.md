# Credits & third-party licences / Credits und Lizenzen Dritter

> **English summary.** This project uses third-party resources. The sections below are in German; licence texts are quoted in their original language. In short:
>
> - **Weather data / Wetterdaten:** "Weather data by [Open-Meteo.com](https://open-meteo.com)", licence [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) ([open-meteo.com/en/licence](https://open-meteo.com/en/licence)). Values are partly converted and summarised in the WetterCube (for example the Biker score and daily maxima). The free API is for **non-commercial use only**; commercial use needs a licence from Open-Meteo.
> - **Location search:** Location data by [GeoNames](https://www.geonames.org) via Open-Meteo (CC BY 4.0).
> - **Maps, routes, place search:** map data (c) [OpenStreetMap](https://www.openstreetmap.org/copyright) contributors (ODbL), tiles from the OpenStreetMap servers; routing by [Valhalla](https://github.com/valhalla/valhalla) at FOSSGIS e. V.; place search by [Photon](https://photon.komoot.io) (komoot). Used sparingly with an own User-Agent.
> - **German Weather Service (DWD):** Warnings and warning map (with precipitation radar) from [DWD Opendata](https://opendata.dwd.de), licence CC BY 4.0, credited as "Quelle: Deutscher Wetterdienst" next to the displayed data.
> - **Weather icons:** [Meteocons](https://github.com/basmilius/weather-icons) by Bas Milius (MIT).
> - **Font:** [Montserrat](https://github.com/JulietaUla/Montserrat) (SIL OFL 1.1).
> - **Libraries:** ESP-IDF (Apache 2.0), LVGL 8.4 incl. LodePNG (MIT / zlib), ArduinoJson (MIT), Espressif components (Apache 2.0).
>
> **Project licence:** source code GNU AGPL 3.0 ([LICENSE](LICENSE), [NOTICE](NOTICE)); commercial licence on request; texts, images and documentation CC BY-NC 4.0 ([LICENSE-Dokumentation.txt](LICENSE-Dokumentation.txt)). Third-party resources keep their own licences.

Dieses Projekt verwendet Ressourcen Dritter. Die jeweiligen Lizenzbedingungen sind nachfolgend aufgeführt.

| Bestandteil | Lizenz |
|---|---|
| [ESP-IDF](https://github.com/espressif/esp-idf) (Espressif Systems) | Apache-2.0 |
| [LVGL](https://lvgl.io) 8.4 (inkl. lodepng) | MIT / zlib |
| [Meteocons](https://github.com/basmilius/weather-icons) (Bas Milius): Wettersymbole (Platzhalter in `assets/icons/weather/`, die Firmware setzt sie zur Laufzeit) | MIT |
| [Montserrat](https://github.com/JulietaUla/Montserrat) (The Montserrat Project Authors): Schrift der Oberfläche (`assets/fonts/Montserrat-Regular.ttf`, unverändert, mit Lizenztext `assets/fonts/OFL.txt` im Repo und in der Firmware eingebettet) und Schriftzug des Logos | SIL Open Font License 1.1 |
| stb_truetype (in LVGL, `lv_tiny_ttf`): Zeichnen der Schrift zur Laufzeit | MIT / Public Domain |
| Espressif `esp_lcd_touch` (Touch-Treiber GT911), abgeleitet in `src/hardware/touch.*` und `gt911.*` (Dateien tragen weiter ihren Apache-2.0-Hinweis) | Apache-2.0 |
| [ArduinoJson](https://arduinojson.org) (Benoît Blanchon) | MIT |
| Espressif-Komponenten aus dem IDF-Komponentenregister: `mdns`, `esp_codec_dev`, `esp_lcd_ek79007`, `esp_wifi_remote`, `esp_hosted`, `eppp_link`, `esp_serial_slave_link`, `cmake_utilities` (Treiber und WLAN über den ESP32-C6) | Apache-2.0 |

Die Lizenztexte der Komponenten liegen beim Bauen im Ordner `managed_components/` (nicht im Repository).
Vor dem Einbau jeder Komponente die Lizenz prüfen (kein GPL und AGPL im Quelltext; siehe die Hinweise zum Touch-Treiber in [docs/HARDWARE.md](docs/HARDWARE.md)).

---

## Wetterdaten

**Quelle:** [Open-Meteo](https://open-meteo.com) – Wetter und Ortsnamen

„Weather data by [Open-Meteo.com](https://open-meteo.com)“. Lizenz und Quellen der Daten: [open-meteo.com/en/licence](https://open-meteo.com/en/licence). Die Werte werden im WetterCube umgerechnet und zusammengefasst (zum Beispiel zum Biker-Score und zu Tageshöchstwerten). Orte werden über die Geocoding-Schnittstelle gesucht, deren Daten von [GeoNames](https://www.geonames.org) stammen (CC BY 4.0).

**Lizenz der Daten:** [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (Namensnennung, Open-Meteo.com). Die kostenlose API ist nur für **nicht-kommerzielle** Nutzung vorgesehen ([Nutzungsbedingungen](https://open-meteo.com/en/terms)), ein API-Key ist nicht erforderlich. Die Vorhersagen beruhen auf Modellen nationaler Wetterdienste wie DWD, NOAA, ECMWF und Météo-France (Einzelheiten: [open-meteo.com/en/docs](https://open-meteo.com/en/docs)).


---

## DWD-Daten

**Quelle:** [Deutscher Wetterdienst (DWD) – Opendata](https://opendata.dwd.de)

Wetterwarnungen und die Warnkarte (mit Niederschlagsradar) stammen vom DWD (frei zugängliche Geodaten). Sie dürfen nach den
[Rechtlichen Hinweisen des DWD](https://www.dwd.de/DE/service/rechtliche_hinweise/rechtliche_hinweise_node.html) unter der Lizenz
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) mit Quellenvermerk weiterverwendet werden. Der Vermerk lautet
**„Quelle: Deutscher Wetterdienst“** und steht unmittelbar an den angezeigten DWD-Informationen; bei weitergehend bearbeiteten Daten
nennt der DWD Hinweise wie „Datenbasis: Deutscher Wetterdienst, …“ in zentralen Quellenverzeichnissen.

---

## Karten, Routen, Ortssuche

| Bestandteil | Lizenz / Hinweis |
|---|---|
| Kartendaten © [OpenStreetMap](https://www.openstreetmap.org/copyright)-Mitwirkende, Kacheln von `tile.openstreetmap.org` | ODbL 1.0; der Hinweis „© OpenStreetMap-Mitwirkende“ steht in jedem Kartenbild. Das Gerät holt je Route nur die Kacheln, die es anzeigt (etwa 6 bis 20), mit eigenem User-Agent (`WetterCube-Biker/Version (info@wettercube.de)`) und speichert das fertige Kartenbild im Gerät, keine Massenabrufe |
| Routenberechnung: [Valhalla](https://github.com/valhalla/valhalla) (Kostenmodell „motorcycle“), öffentlicher Dienst der FOSSGIS e. V. (`valhalla1.openstreetmap.de`, nur TLS 1.3) | Valhalla MIT, Daten OpenStreetMap (ODbL); der Dienst ist für leichte Nutzung gedacht: ein Abruf je Route, eigener User-Agent |
| Ortssuche mit Vorschlägen: [Photon](https://photon.komoot.io) (komoot, Daten von OpenStreetMap) | Daten ODbL; ein Abruf je Tipppause (mindestens 3 Zeichen), eigener User-Agent |
| Höhen der Strecke: aus der GPX-Datei, sonst Höhe des Wettermodells (Open-Meteo) | CC BY 4.0, nur nicht kommerzielle Nutzung wie die Wetterdaten |
| GPX 1.1 (Dateiformat) | offener Standard |
| Schrift im Gerät: Montserrat Regular als TrueType-Datei (`assets/fonts/Montserrat-Regular.ttf`), gezeichnet mit `lv_tiny_ttf` (stb_truetype, MIT/Public Domain) | SIL Open Font License 1.1 |

**Datenschutz:** Die Orte, die du in Start, Ziel und Zwischenziel eingibst, gehen an Photon (Ortssuche) und Valhalla (Strecke); die Koordinaten der Abschnitte gehen an Open-Meteo, die Kartenausschnitte werden bei OpenStreetMap abgerufen. Es gibt kein Konto und keine Weitergabe an Dritte durch den Betreiber des Geräts.
