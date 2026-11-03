# SPDX-License-Identifier: AGPL-3.0-only
# Copyright 2026 Jan Althaus (JPPeterson-lab)
# Alle Quellen liegen unter src/ (nicht in main/): so bleibt main/ nur der Einstieg.
get_filename_component(FUSION_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
file(GLOB_RECURSE FUSION_SRCS "${FUSION_ROOT}/src/*.c" "${FUSION_ROOT}/src/*.cpp")
# Von macOS/iCloud angelegte Duplikate ("datei 2.cpp") nie mitbauen
list(FILTER FUSION_SRCS EXCLUDE REGEX " [0-9]+\\.(c|cpp)$")
# Board-Schicht: nur der P4 (src/board/p4); es gibt keine S3-Variante

idf_component_register(
    SRCS "main.cpp" ${FUSION_SRCS}
    INCLUDE_DIRS "." "${FUSION_ROOT}" "${FUSION_ROOT}/src" "${FUSION_ROOT}/src/platform"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error)
# Schrift fuer die ganze Oberflaeche: Montserrat (SIL Open Font License) als TrueType-Datei im Programm, LVGL zeichnet daraus zur Laufzeit
# Schriften in jeder Groesse mit Umlauten und Latin-1 (src/app/biker_fonts.cpp); die PicoPixel-Schriften haben keine Umlaute.
target_add_binary_data(${COMPONENT_LIB} "${FUSION_ROOT}/assets/fonts/Montserrat-Regular.ttf" BINARY)
# Absturz-Aufzeichnung (src/app/crashinfo.cpp): eigener Handler vor dem IDF-Panic-Handler
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")
# ArduinoJson soll unsere String-Klasse (src/platform/Arduino.h) als "Arduino-String" erkennen
target_compile_definitions(${COMPONENT_LIB} PRIVATE ARDUINOJSON_ENABLE_ARDUINO_STRING=1)
# Pixeltakt in MHz (Bildrate = Takt / (1386 x 661)): idf.py -DFUSION_PIXEL_CLOCK_MHZ=40 build   (30 MHz = 33 Hz, 40 = 44 Hz, 51 = 56 Hz)
if(NOT DEFINED FUSION_PIXEL_CLOCK_MHZ)
    set(FUSION_PIXEL_CLOCK_MHZ 30)   # 30 MHz = 33 Bilder/s: bei 15 MHz (16 Hz) flimmert das Panel sichtbar (Karte, dunkler Hintergrund); stabil, solange WLAN-Puffer im internen RAM liegen und PSRAM/Flash mit 80 MHz laufen
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE FUSION_PIXEL_CLOCK_HZ=${FUSION_PIXEL_CLOCK_MHZ}000000)
