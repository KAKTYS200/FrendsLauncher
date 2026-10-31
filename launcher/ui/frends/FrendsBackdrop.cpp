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

#include "FrendsBackdrop.h"

#include <QColor>
#include <QLinearGradient>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QRadialGradient>
#include <QResizeEvent>

namespace {
const QString kBackdropAnimation = QStringLiteral(":/frends/background.gif");

struct Block {
    int x;
    int y;
    int size;
    QColor top;
    QColor left;
    QColor right;
};

void paintBlock(QPainter& painter, const Block& block, qreal depth)
{
    const QPointF origin(block.x, block.y);
    const qreal w = block.size;
    const qreal h = block.size;
    const qreal d = depth;

    QPolygonF top;
    top << QPointF(origin.x() + d, origin.y()) << QPointF(origin.x() + w + d, origin.y()) << QPointF(origin.x() + w, origin.y() + d)
        << QPointF(origin.x(), origin.y() + d);

    QPolygonF left;
    left << QPointF(origin.x(), origin.y() + d) << QPointF(origin.x() + w, origin.y() + d) << QPointF(origin.x() + w, origin.y() + d + h)
         << QPointF(origin.x(), origin.y() + d + h);

    QPolygonF right;
    right << QPointF(origin.x() + w, origin.y() + d) << QPointF(origin.x() + w + d, origin.y())
          << QPointF(origin.x() + w + d, origin.y() + h) << QPointF(origin.x() + w, origin.y() + d + h);

    painter.setPen(Qt::NoPen);
    painter.setBrush(block.top);
    painter.drawPolygon(top);
    painter.setBrush(block.left);
    painter.drawPolygon(left);
    painter.setBrush(block.right);
    painter.drawPolygon(right);
}
}  // namespace

FrendsBackdrop::FrendsBackdrop(QWidget* parent) : QWidget(parent)
{
    setAutoFillBackground(false);

    // The looping cube animation is the backdrop. Frames are decoded on the fly
    // instead of being cached, which would cost tens of megabytes for a three
    // second loop.
    m_movie = new QMovie(kBackdropAnimation, QByteArray(), this);
    if (m_movie->isValid()) {
        m_movie->setCacheMode(QMovie::CacheNone);
        connect(m_movie, &QMovie::frameChanged, this, [this] { update(); });
        m_movie->start();
    }
}

void FrendsBackdrop::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_scene = QPixmap();
}

void FrendsBackdrop::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    bool painted = false;
    if (m_movie && m_movie->isValid() && m_movie->state() == QMovie::Running) {
        const QPixmap frame = m_movie->currentPixmap();
        if (!frame.isNull()) {
            // Cover the whole widget: the frame keeps its aspect ratio and the
            // overflow is cropped evenly on both sides.
            const QSize cover = frame.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
            QRect target(QPoint(0, 0), cover);
            target.moveCenter(rect().center());
            painter.drawPixmap(target, frame);
            painted = true;
        }
    }

    if (!painted) {
        if (m_scene.size() != size()) {
            // Fallback artwork: drawn small and blown back up, which turns the
            // cubes into a soft out-of-focus backdrop.
            const qreal shrink = 0.22;
            const int w = qMax(24, int(width() * shrink));
            const int h = qMax(24, int(height() * shrink));

            QPixmap small(w, h);
            small.fill(Qt::transparent);
            {
                QPainter scenePainter(&small);
                scenePainter.setRenderHint(QPainter::Antialiasing, true);
                paintScene(scenePainter, QRect(0, 0, w, h));
            }

            m_scene = small.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        painter.drawPixmap(0, 0, m_scene);
    }

    paintShade(painter);
}

void FrendsBackdrop::paintShade(QPainter& painter)
{
    const QRect area = rect();

    // A light hand: the animation is meant to be seen. The shade only pulls it
    // down far enough for white text to stay readable on top of it.
    painter.fillRect(area, QColor(6, 9, 14, 55));

    QRadialGradient vignette(QPointF(area.width() * 0.5, area.height() * 0.42), area.width() * 0.80);
    vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
    vignette.setColorAt(0.62, QColor(0, 0, 0, 30));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 120));
    painter.fillRect(area, vignette);
}

void FrendsBackdrop::paintScene(QPainter& painter, const QRect& area)
{
    if (area.isEmpty())
        return;

    QLinearGradient sky(0, 0, 0, area.height());
    sky.setColorAt(0.0, QColor(58, 40, 92));
    sky.setColorAt(0.45, QColor(38, 26, 64));
    sky.setColorAt(1.0, QColor(18, 12, 32));
    painter.fillRect(area, sky);

    QRadialGradient glow(QPointF(area.width() * 0.26, area.height() * 0.10), area.width() * 0.60);
    glow.setColorAt(0.0, QColor(214, 150, 220, 70));
    glow.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.fillRect(area, glow);

    // The artwork is drawn on the reduced canvas, so the block size is measured
    // against that canvas - not against the window - or the cubes end up larger
    // than the whole picture once it is scaled back up.
    const qreal unit = qMax(4.0, area.height() / 6.5);
    const qreal depth = unit * 0.26;

    static const struct {
        qreal x;
        qreal y;
        qreal scale;
        int color;
    } blocks[] = {
        { 0.02, 0.02, 1.35, 0 }, { 0.17, -0.05, 1.10, 1 }, { 0.30, 0.05, 1.55, 2 }, { 0.46, -0.02, 1.05, 3 },
        { 0.58, 0.06, 1.30, 0 }, { 0.72, -0.04, 1.15, 2 }, { -0.04, 0.30, 1.05, 4 }, { 0.11, 0.38, 1.45, 0 },
        { 0.27, 0.31, 1.00, 3 }, { 0.39, 0.40, 1.25, 1 }, { 0.54, 0.33, 1.10, 4 }, { 0.66, 0.41, 1.50, 0 },
        { 0.82, 0.32, 1.05, 2 }, { 0.03, 0.62, 1.20, 1 }, { 0.19, 0.70, 1.50, 0 }, { 0.35, 0.63, 1.05, 4 },
        { 0.48, 0.72, 1.35, 2 }, { 0.63, 0.64, 1.10, 3 }, { 0.76, 0.73, 1.45, 0 }, { 0.90, 0.60, 1.05, 1 },
        { -0.03, 0.90, 1.40, 0 }, { 0.13, 0.96, 1.05, 2 }, { 0.29, 0.88, 1.30, 1 }, { 0.44, 0.98, 1.55, 0 },
        { 0.60, 0.90, 1.05, 4 }, { 0.73, 1.00, 1.30, 2 }, { 0.88, 0.92, 1.50, 0 },
    };

    static const QColor tops[] = {
        QColor(126, 78, 168), QColor(196, 84, 132), QColor(88, 74, 140), QColor(232, 146, 120), QColor(108, 62, 140),
    };
    static const QColor lefts[] = {
        QColor(96, 56, 132), QColor(156, 62, 104), QColor(66, 54, 110), QColor(190, 108, 88), QColor(82, 44, 112),
    };
    static const QColor rights[] = {
        QColor(74, 42, 104), QColor(126, 48, 82), QColor(50, 40, 88), QColor(158, 86, 68), QColor(64, 34, 92),
    };

    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const auto& spec : blocks) {
        const int color = spec.color % 5;
        Block block;
        block.size = static_cast<int>(unit * spec.scale);
        block.x = static_cast<int>(area.width() * spec.x);
        block.y = static_cast<int>(area.height() * spec.y);
        block.top = tops[color];
        block.left = lefts[color];
        block.right = rights[color];

        const int overflow = static_cast<int>(block.size + depth);
        if (block.x + overflow < -overflow || block.y + overflow < -overflow)
            continue;

        paintBlock(painter, block, depth);
    }
}
