// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "FrendsScale.h"

#include <QtMath>

#include "Application.h"
#include "settings/SettingsObject.h"

namespace FrendsScale {

qreal userFactor()
{
    int percent = DEFAULT_USER_PERCENT;
    if (APPLICATION) {
        // Registers the setting with its default the first time it is touched.
        APPLICATION->settings()->getOrRegisterSetting("FrendsUiScale", DEFAULT_USER_PERCENT);
        percent = APPLICATION->settings()->get("FrendsUiScale").toInt();
    }

    percent = qBound(MIN_USER_PERCENT, percent, MAX_USER_PERCENT);
    return percent / 100.0;
}

qreal windowFactor(const QWidget* widget)
{
    if (!widget)
        return 1.0;

    // Use the top level window: the widgets themselves report their own height,
    // which would make the factor depend on the factor (feedback loop).
    const QWidget* top = widget->window();
    const int height = top ? top->height() : BASE_HEIGHT;
    if (height <= 0)
        return 1.0;

    const qreal ratio = height / qreal(BASE_HEIGHT);
    return qBound(0.85, ratio, 1.35);
}

qreal factor(const QWidget* widget)
{
    return qBound(0.7, userFactor() * windowFactor(widget), 2.0);
}

int px(qreal base, const QWidget* widget)
{
    return qMax(1, int(qRound(base * factor(widget))));
}

}  // namespace FrendsScale
