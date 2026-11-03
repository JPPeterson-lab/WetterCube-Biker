#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright 2026 Jan Althaus (JPPeterson-lab)
"""Erzeugt die Logos aus der Schrift Montserrat (Regular, aus der Variablen Schrift): "WetterCube" dunkel bzw. hell, das zweite Wort in Hellblau.

Aufruf:  python3 tools/logo_erzeugen.py <Montserrat-VariableFont_wght.ttf> <Wort> <Ausgabe ohne Endung>
Beispiel: python3 tools/logo_erzeugen.py Montserrat-VariableFont_wght.ttf Apex assets/logo/wettercube-apex-logo
Braucht fonttools (pip install fonttools). Erzeugt <Ausgabe>.svg (dunkle Schrift) und <Ausgabe>-hell.svg (helle Schrift fuer dunkle Hintergruende).
"""
import sys, re
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools.pens.svgPathPen import SVGPathPen

if len(sys.argv) != 4: sys.exit(__doc__)
schrift, wort2, ausgabe = sys.argv[1:4]
f = instancer.instantiateVariableFont(TTFont(schrift), {'wght': 400})
gs = f.getGlyphSet(); cmap = f.getBestCmap(); hm = f['hmtx']

def zahl(m):
    v = round(float(m.group(0)) * 2) / 2
    return ('%d' % v) if v == int(v) else ('%.1f' % v)

def pfad(ch):
    p = SVGPathPen(gs); gs[cmap[ord(ch)]].draw(p)
    return re.sub(r'-?\d+\.\d+|-?\d+', zahl, p.getCommands())

def logo(dunkel):
    x = 0; t1 = []; t2 = []
    for ch in 'WetterCube':
        t1.append('<path transform="translate(%d 0)" d="%s"/>' % (round(x), pfad(ch))); x += hm[cmap[ord(ch)]][0]
    x += hm[cmap[ord(' ')]][0]
    for ch in wort2:
        t2.append('<path transform="translate(%d 0)" d="%s"/>' % (round(x), pfad(ch))); x += hm[cmap[ord(ch)]][0]
    b = round(x) + 360
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d 1360" width="%d" height="680"><g transform="translate(180 900) scale(1 -1)">'
            '<g fill="%s">%s</g><g fill="#87CEEB">%s</g></g></svg>') % (b, b // 2, dunkel, ''.join(t1), ''.join(t2))

open(ausgabe + '.svg', 'w').write(logo('#111111'))
open(ausgabe + '-hell.svg', 'w').write(logo('#f2f2f2'))
print('Geschrieben:', ausgabe + '.svg', ausgabe + '-hell.svg')
