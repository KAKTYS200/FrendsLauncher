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

#include <QString>
#include <QWidget>

class QPaintEvent;
class QPainter;
class QTimer;

/// The window backdrop: a field of slowly turning cubes.
///
/// The cubes are drawn by the launcher itself rather than played back from a
/// video, so the motion is continuous and never restarts with a visible jump.
/// Each frame is rendered into a small pixmap and scaled back up, which both
/// blurs it and keeps the cost of animating a whole window low.
///
/// Two animations are available; the player picks one on the Interface page and
/// the choice is stored in the "FrendsBackdrop" setting.
class FrendsBackdrop : public QWidget {
    Q_OBJECT

   public:
    enum Mode {
        /// Cubes turn in place and bob gently.
        Orbit,
        /// Cubes fall from above the window to below it and wrap around out of
        /// sight, so the motion is a loop with no seam.
        Rain,
    };

    explicit FrendsBackdrop(QWidget* parent = nullptr);

    /// Reads the animation named by the "FrendsBackdrop" setting.
    static Mode modeFromSettings();
    static QString settingValue(Mode mode);
    static QString settingLabel(Mode mode);

    Mode mode() const { return m_mode; }
    /// Switches the animation without restarting the clock.
    void setMode(Mode mode);

    /**
     * Pauses the animation while the launcher is busy.
     *
     * Every frame is a full-window repaint in the GUI thread, and that thread
     * is also the one that has to react to a download or an unzip reporting
     * progress. Ten thousand repaints in the middle of an install made the
     * whole launcher feel stuck, so the backdrop stops moving until the work is
     * done. The clock keeps running, so the cubes are exactly where they would
     * have been when it resumes.
     */
    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }

   protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

   private:
    void paintScene(QPainter& painter, const QRect& area, qreal seconds);
    void paintOrbit(QPainter& painter, const QRect& area, qreal seconds);
    void paintRain(QPainter& painter, const QRect& area, qreal seconds);
    void paintShade(QPainter& painter);

    QTimer* m_timer = nullptr;
    Mode m_mode = Orbit;
    bool m_paused = false;
    /// Seconds since the animation started. Every position is a function of it,
    /// so the motion has no seam and never restarts.
    qreal m_elapsed = 0.0;
    /// Wall clock the elapsed time is measured from, so a dropped frame slows
    /// nothing down.
    qint64 m_startedMs = 0;
};
