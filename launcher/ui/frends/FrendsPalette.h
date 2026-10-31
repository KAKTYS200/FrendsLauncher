// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QApplication>
#include <QColor>
#include <QList>
#include <QPalette>
#include <QString>

/// Colours for the custom Frends chrome.
///
/// The launcher ships both a dark and a light application theme, so every part
/// of the chrome reads its colours from here instead of hard-coding them. The
/// palette is derived from the application palette, which means a theme the
/// user installs later is picked up as well.
namespace FrendsTheme {

/// True when the application palette is dark. Derived from the palette rather
/// than from the theme name, so custom themes work too.
inline bool isDark()
{
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

struct Colors {
    bool dark = true;

    // Window backdrop
    QColor skyTop;
    QColor skyBottom;
    QColor backdropShade;
    QColor vignette;

    // Content card
    QColor card;
    QColor cardBorder;

    // Sidebar and bottom bar
    QColor panelTop;
    QColor panelBottom;
    QColor panelEdge;

    // Text
    QColor text;
    QColor textMuted;
    QColor textOnAccent;

    // Navigation
    QColor navActiveTop;
    QColor navActiveBottom;
    QColor navHover;

    // Instance tiles
    QColor tile;
    QColor tileBorder;
    QColor tileSelectedTop;
    QColor tileSelectedBottom;
    QColor tileSelectedBorder;
    QColor chip;
    QColor chipText;

    // Inputs and buttons
    QColor field;
    QColor fieldBorder;
    QColor buttonPlate;
    QColor buttonPlateBorder;
    QColor playTop;
    QColor playBottom;

    // Backdrop cubes
    QList<QColor> cubeColors;
    /// Brightness of the darkest cube face and the spread between the darkest
    /// and the lit face. Pale cubes on a light sky need a tighter spread, or
    /// every face washes out into the same near-white.
    qreal cubeShadeBase = 0.66;
    qreal cubeShadeGain = 0.46;
};

inline void buildDark(Colors& c)
{
    c.dark = true;
    c.skyTop = QColor(58, 40, 92);
    c.skyBottom = QColor(16, 11, 28);
    c.backdropShade = QColor(6, 9, 14, 55);
    c.vignette = QColor(0, 0, 0, 120);

    c.card = QColor(8, 10, 15, 88);
    c.cardBorder = QColor(255, 255, 255, 85);

    c.panelTop = QColor(16, 19, 25, 232);
    c.panelBottom = QColor(8, 10, 14, 220);
    c.panelEdge = QColor(255, 255, 255, 26);

    c.text = QColor(0xE7, 0xE8, 0xEA);
    c.textMuted = QColor(255, 255, 255, 150);
    c.textOnAccent = QColor(0x3B, 0x2C, 0x00);

    c.navActiveTop = QColor(0xFF, 0xD9, 0x4A);
    c.navActiveBottom = QColor(0xF2, 0xAF, 0x17);
    c.navHover = QColor(255, 255, 255, 20);

    c.tile = QColor(26, 29, 36, 200);
    c.tileBorder = QColor(255, 255, 255, 18);
    c.tileSelectedTop = QColor(104, 152, 82, 240);
    c.tileSelectedBottom = QColor(62, 104, 50, 240);
    c.tileSelectedBorder = QColor(0x86, 0xD9, 0x62);
    c.chip = QColor(0x5F, 0xBF, 0x3F);
    c.chipText = QColor(Qt::white);

    c.field = QColor(0x1B, 0x1F, 0x27);
    c.fieldBorder = QColor(255, 255, 255, 32);
    c.buttonPlate = QColor(255, 255, 255, 16);
    c.buttonPlateBorder = QColor(255, 255, 255, 38);
    c.playTop = QColor(0xFF, 0xB3, 0x47);
    c.playBottom = QColor(0xF0, 0x7C, 0x1E);

    c.cubeColors = { QColor(126, 78, 168), QColor(196, 84, 132), QColor(88, 74, 140),
                     QColor(232, 146, 120), QColor(108, 62, 140) };
    c.cubeShadeBase = 0.66;
    c.cubeShadeGain = 0.46;
}

inline void buildLight(Colors& c)
{
    c.dark = false;
    c.skyTop = QColor(206, 220, 240);
    c.skyBottom = QColor(158, 178, 204);
    // A cool tint rather than white: white over a pale sky just washes the
    // cubes out instead of giving the scene depth.
    c.backdropShade = QColor(72, 92, 124, 30);
    c.vignette = QColor(60, 70, 90, 70);

    c.card = QColor(255, 255, 255, 192);
    c.cardBorder = QColor(255, 255, 255, 235);

    c.panelTop = QColor(250, 251, 253, 236);
    c.panelBottom = QColor(235, 238, 243, 232);
    c.panelEdge = QColor(0, 0, 0, 30);

    c.text = QColor(0x1E, 0x22, 0x28);
    c.textMuted = QColor(0, 0, 0, 140);
    c.textOnAccent = QColor(0x3B, 0x2C, 0x00);

    c.navActiveTop = QColor(0xFF, 0xD9, 0x4A);
    c.navActiveBottom = QColor(0xF2, 0xAF, 0x17);
    c.navHover = QColor(0, 0, 0, 18);

    c.tile = QColor(255, 255, 255, 225);
    c.tileBorder = QColor(0, 0, 0, 28);
    c.tileSelectedTop = QColor(150, 208, 118, 245);
    c.tileSelectedBottom = QColor(110, 176, 82, 245);
    c.tileSelectedBorder = QColor(0x3E, 0x8F, 0x28);
    c.chip = QColor(0x3E, 0x9A, 0x2C);
    c.chipText = QColor(Qt::white);

    c.field = QColor(255, 255, 255);
    c.fieldBorder = QColor(0, 0, 0, 45);
    c.buttonPlate = QColor(255, 255, 255, 170);
    c.buttonPlateBorder = QColor(0, 0, 0, 40);
    c.playTop = QColor(0xFF, 0xB3, 0x47);
    c.playBottom = QColor(0xF0, 0x7C, 0x1E);

    c.cubeColors = { QColor(122, 158, 202), QColor(176, 132, 180), QColor(126, 152, 192),
                     QColor(212, 152, 118), QColor(128, 166, 142) };
    c.cubeShadeBase = 0.76;
    c.cubeShadeGain = 0.36;
}

/// The current palette. Rebuilt when the application palette changes, so the
/// chrome follows a theme switch without any extra wiring.
inline const Colors& colors()
{
    static Colors cached;
    static int cachedLightness = -1;

    const int lightness = QApplication::palette().color(QPalette::Window).lightness();
    if (lightness != cachedLightness) {
        cachedLightness = lightness;
        if (lightness < 128)
            buildDark(cached);
        else
            buildLight(cached);
    }
    return cached;
}

/// QSS takes colours as strings; this keeps the stylesheets readable.
inline QString css(const QColor& color)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

}  // namespace FrendsTheme
