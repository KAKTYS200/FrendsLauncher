// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QWidget>

/*!
 * Shared scaling for the custom Frends chrome (sidebar, bottom bar, dialogs).
 *
 * The theme is authored against the window size the original launcher uses:
 * 1067x837 px (gui.frame.width / gui.frame.height in tlauncher-2.0.properties).
 * The final factor is the product of two things:
 *   - the user preference "FrendsUiScale" (percent, 80..150), and
 *   - the window size relative to the reference height, clamped so a maximized
 *     window does not turn the chrome into a billboard.
 *
 * Qt already maps CSS px to device independent pixels, so HiDPI displays are
 * handled by Qt itself; this factor only controls the layout proportions.
 */
namespace FrendsScale {

constexpr int BASE_WIDTH = 1067;
constexpr int BASE_HEIGHT = 837;

constexpr int MIN_USER_PERCENT = 80;
constexpr int MAX_USER_PERCENT = 150;
constexpr int DEFAULT_USER_PERCENT = 100;

/// User preference from settings, as a multiplier (0.8 .. 1.5).
qreal userFactor();

/// Window size relative to the reference size (0.85 .. 1.35).
qreal windowFactor(const QWidget* widget);

/// Combined factor used by the theme (0.7 .. 2.0).
qreal factor(const QWidget* widget);

/// Scales a base pixel metric.
int px(qreal base, const QWidget* widget);

}  // namespace FrendsScale
