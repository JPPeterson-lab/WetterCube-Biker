# Wettersymbole animiert rendern (Meteocons, MIT)

Die animierten SVGs (SMIL: nur `transform` rotate/translate und `opacity`, Schleifen von 1 bis 6 s) werden Bild fuer Bild gerendert:
`render.html` wertet die Animationen selbst aus (Dauerschleife, keyTimes, keySplines), setzt die Werte in die SVG, rastert sie
auf 128x128 (Canvas, mit Transparenz) und schickt jedes Bild an `server.py`, der es unter `frames/<name>/<nr>.png` speichert.
Danach packt `tools/anim_pack.py` alles in `assets/icons/anim/wetteranim.bin`.

1. `npm`-Paket `@meteocons/svg` entpacken, die gewuenschten SVGs aus `fill/` nach `svg/` neben `render.html` kopieren
2. `python3 server.py` (Port 8768) im Ordner mit `render.html`, `svg/`
3. Im Browser `http://127.0.0.1:8768/render.html?namen=clear-day,clear-night,...` oeffnen (alle Namen mit Komma, ohne `.svg`)
4. `python3 tools/anim_pack.py <Ordner mit frames/>`
