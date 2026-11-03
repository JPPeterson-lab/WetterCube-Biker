#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright 2026 Jan Althaus (JPPeterson-lab)
# Baut WetterCube Biker (ESP-IDF 5.5.5 muss unter ~/esp/esp-idf-v5.5.5 installiert sein).
#   bash build.sh               bauen (Ziel esp32p4)
#   bash build.sh flash PORT    bauen und nur die Firmware nach app0 flashen (z. B. /dev/cu.usbmodem1234)
#   bash build.sh monitor PORT  seriellen Monitor oeffnen
#   bash build.sh recovery      Notfall-App bauen (recovery/build/wettercube_biker_notfall.bin, kommt in die Partition factory)
#   bash build.sh flashall PORT alles per USB: Bootloader, Partitionstabelle, Notfall-App, otadata und Firmware (noetig bei geaenderter Tabelle)
#   bash build.sh clean         build/ und sdkconfig loeschen (nach Aenderungen an sdkconfig.defaults)
source ~/esp/esp-idf-v5.5.5/export.sh > /dev/null 2>&1 || { echo "ESP-IDF 5.5.5 nicht gefunden"; exit 1; }
cd "$(dirname "$0")" || exit 1
case "$1" in
  clean) rm -rf build sdkconfig sdkconfig.old; echo "aufgeraeumt"; exit 0 ;;
  monitor) idf.py -p "$2" monitor; exit $? ;;
esac
if [ "$1" = "recovery" ]; then
  (cd recovery && { [ -f sdkconfig ] || idf.py set-target esp32p4; } && idf.py -DNOTFALL_SNAPSHOT=OFF -DNOTFALL_DEMO=OFF build) || exit 1
  ls -la recovery/build/wettercube_biker_notfall.bin; exit 0
fi
if [ "$1" = "flashall" ]; then
  idf.py build || exit 1
  python3 tools/flash_usb.py "$2" $3; exit $?
fi
[ -f sdkconfig ] || idf.py set-target esp32p4 || exit 1
if [ "$1" = "flash" ]; then idf.py build || exit 1; python3 tools/flash_usb.py "$2" --nur-firmware; else idf.py build; fi   # flash: nur Firmware nach app0 (nie nach factory)
