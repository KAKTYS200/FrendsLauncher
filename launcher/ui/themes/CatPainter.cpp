// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2025 Trial97 <alexandru.tripon97@gmail.com>
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

#include "ui/themes/CatPainter.h"
#include <QPixmap>
#include "Application.h"
#include "settings/SettingsObject.h"

CatPainter::CatPainter(const QString& path, QObject* parent) : QObject(parent)
{
    // Attempt to load as a movie
    m_movie = new QMovie(path, QByteArray(), this);
    if (m_movie->isValid()) {
        // Start the animation if it's a valid movie file
        connect(m_movie, &QMovie::frameChanged, this, &CatPainter::updateFrame);
        m_movie->start();
    } else {
        // Otherwise, load it as a static image
        delete m_movie;
        m_movie = nullptr;

        m_image = QPixmap(path);
    }
}

void CatPainter::paint(QPainter* painter, const QRect& viewport)
{
    QPixmap frame = m_image;
    if (m_movie && m_movie->isValid()) {
        frame = m_movie->currentPixmap();
    }

    // The picture is a backdrop for the whole instance list, so by default it
    // is stretched over the entire viewport instead of sitting in a corner. The
    // old behaviour kept the original size and pinned the result to the bottom
    // right, which left most of the white area empty.
    const auto fit = APPLICATION->settings()->get("CatFit").toString();
    painter->setOpacity(APPLICATION->settings()->get("CatOpacity").toFloat() / 100);

    QRect target = viewport;
    const bool keepAspect = fit == QLatin1String("fill") || fit == QLatin1String("fit");
    if (keepAspect) {
        // Still cover the whole area: scale to the larger ratio so no white
        // gaps remain, then centre the overflow instead of anchoring it to a
        // corner.
        const QSize scaled = frame.size().scaled(viewport.size(), Qt::KeepAspectRatioByExpanding);
        target = QRect(QPoint(0, 0), scaled);
        target.moveCenter(viewport.center());
    }

    const auto pixmap = frame.scaled(target.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    painter->drawPixmap(target.topLeft(), pixmap);
    painter->setOpacity(1.0);
};
