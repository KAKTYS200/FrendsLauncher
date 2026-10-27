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
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QRadialGradient>
#include <QResizeEvent>

namespace {
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
}

void FrendsBackdrop::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_scene = QPixmap();
}

void FrendsBackdrop::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    if (m_scene.size() != size()) {
        m_scene = QPixmap(size());
        m_scene.fill(Qt::transparent);
        QPainter scenePainter(&m_scene);
        scenePainter.setRenderHint(QPainter::Antialiasing, true);
        paintScene(scenePainter);
    }

    QPainter painter(this);
    painter.drawPixmap(0, 0, m_scene);
}

void FrendsBackdrop::paintScene(QPainter& painter)
{
    const QRect area = rect();
    if (area.isEmpty())
        return;

    // Dark sky backdrop
    QLinearGradient sky(0, 0, 0, area.height());
    sky.setColorAt(0.0, QColor(63, 106, 143));
    sky.setColorAt(0.45, QColor(40, 77, 107));
    sky.setColorAt(1.0, QColor(20, 42, 58));
    painter.fillRect(area, sky);

    // Soft light spot in the upper-left corner
    QRadialGradient glow(QPointF(area.width() * 0.28, area.height() * 0.12), area.width() * 0.55);
    glow.setColorAt(0.0, QColor(255, 255, 255, 60));
    glow.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.fillRect(area, glow);

    const qreal unit = qMax(56.0, area.height() / 7.0);
    const qreal depth = unit * 0.26;

    // A fixed pseudo-random layout so the artwork stays stable between repaints
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
        QColor(104, 170, 63), QColor(122, 84, 46), QColor(52, 62, 78), QColor(146, 140, 62), QColor(70, 120, 52),
    };
    static const QColor lefts[] = {
        QColor(72, 124, 44), QColor(86, 58, 32), QColor(34, 42, 55), QColor(104, 100, 44), QColor(48, 88, 36),
    };
    static const QColor rights[] = {
        QColor(54, 98, 34), QColor(66, 44, 24), QColor(25, 32, 43), QColor(82, 78, 34), QColor(38, 70, 28),
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

        // Clip blocks that start outside the widget so the artwork fills the edges
        const int overflow = static_cast<int>(block.size + depth);
        if (block.x + overflow < -overflow || block.y + overflow < -overflow)
            continue;

        paintBlock(painter, block, depth);
    }

    // Subtle darkening towards the bottom keeps the white content card readable
    QLinearGradient shade(0, area.height() * 0.55, 0, area.height());
    shade.setColorAt(0.0, QColor(0, 0, 0, 0));
    shade.setColorAt(1.0, QColor(0, 0, 0, 110));
    painter.fillRect(area, shade);

    // Faint diagonal streaks
    painter.setPen(QPen(QColor(255, 255, 255, 12), 2));
    for (int i = -area.height(); i < area.width(); i += 120) {
        painter.drawLine(i, area.height(), i + area.height(), 0);
    }
}
