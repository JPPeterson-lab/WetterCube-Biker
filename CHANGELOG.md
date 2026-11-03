# Changelog

## v0.1.0-beta6 (2026-11-02)

### Behoben
- **Wetter-Bildschirm, „Bestes Gebiet, Wochenende“:** Der Kasten zeigte immer das erste Gebiet der Liste (zum Beispiel den Harz), auch wenn es den schlechtesten Score hatte. Die Rangliste sortierte erst nach dem Kürzen auf ein Gebiet. Jetzt steht dort das Gebiet mit dem besten Score.
- **Wettersymbole in den Stundenkacheln und im Kasten Fahrtwind-Gefühl** saßen links oben versetzt und waren rechts abgeschnitten (nicht zentriert). Sie sind jetzt vollständig und mittig.
- **Handy-Meldung „Regen im Anmarsch“:** Sie kam nach jedem Neustart (zum Beispiel nach einem Update) erneut und schon bei Regen in zwei Stunden oder bei Nieselmengen. Jetzt kommt sie nur noch, wenn der Regen innerhalb der nächsten 45 Minuten beginnt und in der Stunde danach mindestens 0,3 mm bringt, höchstens alle 3 Stunden. Der Zeitpunkt der letzten Meldung bleibt über Neustarts erhalten.
