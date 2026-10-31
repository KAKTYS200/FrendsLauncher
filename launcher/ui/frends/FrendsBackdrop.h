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

#include <QPixmap>
#include <QWidget>

class QMovie;
class QPaintEvent;
class QPainter;

/// The window backdrop: a looping animation of drifting Minecraft cubes.
/// If the animation cannot be loaded the widget falls back to drawing the
/// cubes itself, so the launcher never ends up with a plain black background.
class FrendsBackdrop : public QWidget {
    Q_OBJECT

   public:
    explicit FrendsBackdrop(QWidget* parent = nullptr);

   protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

   private:
    void paintAnimation(QPainter& painter);
    /// Drawn block artwork, used when the animation is unavailable.
    void paintScene(QPainter& painter, const QRect& area);
    void paintShade(QPainter& painter);

    QMovie* m_movie = nullptr;
    QPixmap m_scene;
};
