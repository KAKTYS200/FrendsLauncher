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

#include <QDateTime>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QTimer>
#include <QtMath>

#include <algorithm>
#include <cmath>

#include "Application.h"
#include "FrendsPalette.h"
#include "settings/SettingsObject.h"

namespace {

struct Vec3 {
    qreal x;
    qreal y;
    qreal z;
};

/// One cube of the backdrop. Positions and sizes are fractions of the frame, so
/// the scene keeps its proportions at any window size.
struct CubeSpec {
    qreal x;
    qreal y;
    qreal size;
    qreal speed;
    qreal phase;
    qreal drift;
    qreal driftPhase;
    int color;
};

// A fixed field: the cubes keep their places, only the rotation and a slow bob
// move, which reads as a calm loop rather than a scrolling wallpaper.
const CubeSpec kCubes[] = {
    { 0.08, 0.14, 0.18, 0.16, 0.0, 0.020, 0.0, 0 },  { 0.26, 0.05, 0.13, 0.24, 1.1, 0.016, 2.1, 2 },
    { 0.44, 0.17, 0.22, 0.12, 2.4, 0.024, 4.0, 1 },  { 0.62, 0.07, 0.15, 0.20, 0.7, 0.018, 1.2, 3 },
    { 0.80, 0.20, 0.20, 0.14, 3.3, 0.022, 5.1, 0 },  { 0.96, 0.10, 0.14, 0.26, 2.0, 0.020, 3.4, 4 },
    { 0.15, 0.36, 0.16, 0.22, 4.2, 0.026, 0.6, 2 },  { 0.35, 0.30, 0.21, 0.10, 1.6, 0.014, 2.8, 1 },
    { 0.55, 0.38, 0.13, 0.30, 5.0, 0.022, 4.4, 3 },  { 0.73, 0.32, 0.19, 0.18, 0.4, 0.018, 1.8, 1 },
    { 0.90, 0.42, 0.15, 0.24, 3.7, 0.024, 3.9, 0, }, { 0.06, 0.58, 0.17, 0.14, 2.7, 0.016, 0.9, 4 },
    { 0.24, 0.52, 0.22, 0.20, 4.8, 0.022, 5.6, 2 },  { 0.44, 0.62, 0.14, 0.28, 1.9, 0.018, 2.5, 3 },
    { 0.63, 0.56, 0.20, 0.16, 0.9, 0.020, 3.1, 0 },  { 0.82, 0.66, 0.16, 0.26, 3.0, 0.018, 1.5, 1 },
    { 0.97, 0.60, 0.21, 0.12, 5.4, 0.024, 4.8, 4 },  { 0.12, 0.82, 0.19, 0.22, 2.2, 0.020, 0.3, 3 },
    { 0.34, 0.76, 0.15, 0.18, 4.4, 0.016, 2.7, 0 },  { 0.53, 0.88, 0.22, 0.14, 1.3, 0.024, 5.2, 2 },
    { 0.74, 0.84, 0.17, 0.24, 3.9, 0.018, 1.9, 1 },  { 0.93, 0.90, 0.20, 0.16, 0.6, 0.022, 4.1, 3 },
};

QColor shaded(const QColor& base, qreal factor)
{
    return QColor(qBound(0, int(base.red() * factor), 255), qBound(0, int(base.green() * factor), 255),
                  qBound(0, int(base.blue() * factor), 255));
}

constexpr qreal kTau = 6.28318530717958647692;

/// Length of one full cycle of the backdrop.
///
/// Every cube is locked to a whole number of turns and a whole number of bobs
/// inside this window, so the frame at the end of a cycle is the frame at its
/// start. Without that the cubes drift apart at their own speeds and the
/// moment the clock wraps reads as a jerk instead of a loop.
constexpr qreal kLoopSeconds = 60.0;

/// The rotation speed that makes a cube complete `speed` radians per second on
/// average, snapped to a whole number of turns per cycle (at least one).
qreal loopedSpeed(qreal speed)
{
    const qreal turns = qMax<qreal>(1.0, qRound(speed * kLoopSeconds / kTau));
    return turns * kTau / kLoopSeconds;
}

/// One cube of the falling animation. The cubes do not share the turning
/// field's layout: they start at different heights and fall at different
/// speeds, which is what gives the scene its depth.
struct RainSpec {
    qreal x;      ///< horizontal position, fraction of the width
    qreal size;   ///< edge length, fraction of the height
    qreal speed;  ///< fall speed, window heights per second
    qreal offset; ///< starting point inside the loop, 0..1
    qreal sway;   ///< horizontal drift, fraction of the width
    qreal swayPhase;
    qreal spin;   ///< rotation speed, radians per second
    int color;
};

// Small cubes fall slower than big ones: the eye reads that as distance, and it
// keeps the big ones from overtaking the whole scene.
const RainSpec kRain[] = {
    { 0.06, 0.15, 0.075, 0.02, 0.010, 0.0, 0.10, 0 },  { 0.19, 0.10, 0.105, 0.58, 0.014, 1.3, 0.14, 2 },
    { 0.31, 0.20, 0.062, 0.31, 0.008, 2.4, 0.08, 1 },  { 0.44, 0.12, 0.092, 0.79, 0.012, 0.7, 0.12, 3 },
    { 0.57, 0.17, 0.070, 0.14, 0.010, 3.2, 0.09, 0 },  { 0.69, 0.11, 0.115, 0.66, 0.015, 1.9, 0.15, 4 },
    { 0.81, 0.19, 0.058, 0.41, 0.009, 4.1, 0.07, 2 },  { 0.93, 0.13, 0.088, 0.88, 0.012, 2.8, 0.11, 1 },
    { 0.12, 0.22, 0.052, 0.25, 0.007, 5.0, 0.06, 1 },  { 0.26, 0.09, 0.125, 0.72, 0.016, 0.4, 0.16, 3 },
    { 0.38, 0.14, 0.082, 0.47, 0.011, 3.6, 0.10, 0 },  { 0.50, 0.18, 0.066, 0.09, 0.009, 1.6, 0.09, 4 },
    { 0.63, 0.10, 0.108, 0.55, 0.013, 4.6, 0.13, 1 },  { 0.75, 0.21, 0.055, 0.36, 0.008, 2.1, 0.07, 0 },
    { 0.87, 0.12, 0.098, 0.81, 0.012, 5.4, 0.12, 2 },  { 0.96, 0.16, 0.072, 0.19, 0.010, 3.0, 0.10, 3 },
};

/// Draws one cube turned around its vertical axis and tilted towards the
/// viewer, with the three visible faces lit from the upper left.
void drawCube(QPainter& painter, qreal cx, qreal cy, qreal half, qreal angle, const QColor& base, qreal shadeBase, qreal shadeGain)
{
    const qreal ca = qCos(angle);
    const qreal sa = qSin(angle);
    // A fixed tilt so the top face stays visible; without it the cubes read as
    // flat squares whenever they turn past the camera.
    const qreal tilt = qDegreesToRadians(-24.0);
    const qreal ct = qCos(tilt);
    const qreal st = qSin(tilt);

    auto transform = [&](qreal x, qreal y, qreal z) {
        const qreal x1 = x * ca + z * sa;
        const qreal z1 = -x * sa + z * ca;
        const qreal y2 = y * ct - z1 * st;
        const qreal z2 = y * st + z1 * ct;
        return Vec3{ x1, y2, z2 };
    };

    // Corner index bits: 1 = x, 2 = y, 4 = z.
    Vec3 corner[8];
    for (int i = 0; i < 8; i++) {
        corner[i] = transform((i & 1) ? 1 : -1, (i & 2) ? 1 : -1, (i & 4) ? 1 : -1);
    }

    struct Face {
        int a, b, c, d;
        Vec3 normal;
    };
    static const Face faces[] = {
        { 1, 3, 7, 5, { 1, 0, 0 } },   // +X
        { 0, 4, 6, 2, { -1, 0, 0 } },  // -X
        { 2, 6, 7, 3, { 0, 1, 0 } },   // +Y
        { 0, 1, 5, 4, { 0, -1, 0 } },  // -Y
        { 4, 5, 7, 6, { 0, 0, 1 } },   // +Z
        { 0, 2, 3, 1, { 0, 0, -1 } },  // -Z
    };

    // Light comes from the upper left, slightly in front of the scene.
    const Vec3 light = { -0.42, 0.62, 0.66 };

    painter.setPen(Qt::NoPen);
    for (const auto& face : faces) {
        const Vec3 n = transform(face.normal.x, face.normal.y, face.normal.z);
        if (n.z <= 0.01)
            continue;  // facing away from the viewer

        const qreal lit = qMax(0.0, n.x * light.x + n.y * light.y + n.z * light.z);
        painter.setBrush(shaded(base, shadeBase + shadeGain * lit));

        QPolygonF poly;
        poly << QPointF(cx + corner[face.a].x * half, cy - corner[face.a].y * half)
             << QPointF(cx + corner[face.b].x * half, cy - corner[face.b].y * half)
             << QPointF(cx + corner[face.c].x * half, cy - corner[face.c].y * half)
             << QPointF(cx + corner[face.d].x * half, cy - corner[face.d].y * half);
        painter.drawPolygon(poly);
    }
}

}  // namespace

FrendsBackdrop::FrendsBackdrop(QWidget* parent) : QWidget(parent)
{
    setAutoFillBackground(false);

    m_mode = modeFromSettings();

    m_startedMs = QDateTime::currentMSecsSinceEpoch();

    // 30 frames per second is plenty for a slow turn and leaves the UI thread
    // free; each frame is rendered small and scaled up.
    m_timer = new QTimer(this);
    m_timer->setInterval(33);
    connect(m_timer, &QTimer::timeout, this, [this] {
        m_elapsed = (QDateTime::currentMSecsSinceEpoch() - m_startedMs) / 1000.0;
        update();
    });
    m_timer->start();
}

void FrendsBackdrop::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // Keep the clock running while hidden, otherwise the cubes would jump
    // forward by the time spent in the background.
    m_startedMs = QDateTime::currentMSecsSinceEpoch() - qint64(m_elapsed * 1000);
    m_timer->start();
}

void FrendsBackdrop::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    m_timer->stop();
}

void FrendsBackdrop::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_frame = QPixmap();
}

void FrendsBackdrop::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    // A fifth of the window is enough: the smooth upscale is what softens the
    // artwork, and it keeps the per-frame cost down.
    const QSize small(qMax(48, width() / 5), qMax(48, height() / 5));
    if (m_frame.size() != small)
        m_frame = QPixmap(small);

    m_frame.fill(Qt::transparent);
    {
        QPainter scenePainter(&m_frame);
        scenePainter.setRenderHint(QPainter::Antialiasing, true);
        paintScene(scenePainter, QRect(QPoint(0, 0), small), m_elapsed);
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawPixmap(rect(), m_frame);

    paintShade(painter);
}

void FrendsBackdrop::paintShade(QPainter& painter)
{
    const auto& theme = FrendsTheme::colors();
    const QRect area = rect();

    painter.fillRect(area, theme.backdropShade);

    const QColor edge = theme.vignette;
    QRadialGradient vignette(QPointF(area.width() * 0.5, area.height() * 0.42), area.width() * 0.80);
    vignette.setColorAt(0.0, QColor(edge.red(), edge.green(), edge.blue(), 0));
    vignette.setColorAt(0.62, QColor(edge.red(), edge.green(), edge.blue(), edge.alpha() / 4));
    vignette.setColorAt(1.0, edge);
    painter.fillRect(area, vignette);
}

void FrendsBackdrop::paintScene(QPainter& painter, const QRect& area, qreal seconds)
{
    if (area.isEmpty())
        return;

    const auto& theme = FrendsTheme::colors();

    QLinearGradient sky(0, 0, 0, area.height());
    sky.setColorAt(0.0, theme.skyTop);
    sky.setColorAt(1.0, theme.skyBottom);
    painter.fillRect(area, sky);

    // Soft light behind the cubes so the middle of the window is not flat.
    QRadialGradient glow(QPointF(area.width() * 0.42, area.height() * 0.34), area.width() * 0.62);
    const QColor glowColor = theme.dark ? QColor(190, 150, 230, 70) : QColor(255, 255, 255, 130);
    glow.setColorAt(0.0, glowColor);
    glow.setColorAt(1.0, QColor(glowColor.red(), glowColor.green(), glowColor.blue(), 0));
    painter.fillRect(area, glow);

    if (m_mode == Rain)
        paintRain(painter, area, seconds);
    else
        paintOrbit(painter, area, seconds);
}

void FrendsBackdrop::paintOrbit(QPainter& painter, const QRect& area, qreal seconds)
{
    const auto& theme = FrendsTheme::colors();

    // Far cubes first, so the near ones overlap them.
    QList<const CubeSpec*> order;
    order.reserve(int(sizeof(kCubes) / sizeof(kCubes[0])));
    for (const auto& cube : kCubes)
        order.append(&cube);
    std::sort(order.begin(), order.end(), [](const CubeSpec* a, const CubeSpec* b) { return a->size < b->size; });

    // One cycle, then start over from exactly the same frame.
    const qreal t = std::fmod(seconds, kLoopSeconds);

    // The bob is a sine, so it only repeats when it completes whole cycles per
    // loop; 0.30 rad/s snaps to three cycles a minute.
    const qreal bobSpeed = loopedSpeed(0.30);

    for (const CubeSpec* cube : order) {
        const qreal bob = qSin(t * bobSpeed + cube->driftPhase) * cube->drift;
        const qreal cx = area.width() * cube->x;
        const qreal cy = area.height() * (cube->y + bob);
        const qreal half = area.height() * cube->size * 0.5;
        const qreal angle = cube->phase + t * loopedSpeed(cube->speed);

        const QColor base = theme.cubeColors.at(cube->color % theme.cubeColors.size());
        drawCube(painter, cx, cy, half, angle, base, theme.cubeShadeBase, theme.cubeShadeGain);
    }
}

void FrendsBackdrop::paintRain(QPainter& painter, const QRect& area, qreal seconds)
{
    const auto& theme = FrendsTheme::colors();

    QList<const RainSpec*> order;
    order.reserve(int(sizeof(kRain) / sizeof(kRain[0])));
    for (const auto& cube : kRain)
        order.append(&cube);
    std::sort(order.begin(), order.end(), [](const RainSpec* a, const RainSpec* b) { return a->size < b->size; });

    const qreal height = area.height();

    for (const RainSpec* cube : order) {
        const qreal half = height * cube->size * 0.5;

        // The travel covers the window plus a cube above and below it, so a
        // cube leaves the frame before it is wrapped back to the top. The jump
        // happens entirely out of sight, which is what makes the fall loop
        // without a seam.
        const qreal span = height + 2.0 * half;
        const qreal fallen = std::fmod(cube->offset * span + seconds * cube->speed * height, span);
        const qreal cy = -half + fallen;

        const qreal cx = area.width() * (cube->x + qSin(seconds * 0.22 + cube->swayPhase) * cube->sway);
        const qreal angle = cube->swayPhase + seconds * cube->spin;

        const QColor base = theme.cubeColors.at(cube->color % theme.cubeColors.size());
        drawCube(painter, cx, cy, half, angle, base, theme.cubeShadeBase, theme.cubeShadeGain);
    }
}

FrendsBackdrop::Mode FrendsBackdrop::modeFromSettings()
{
    if (!APPLICATION)
        return Orbit;
    APPLICATION->settings()->getOrRegisterSetting("FrendsBackdrop", settingValue(Orbit));
    return APPLICATION->settings()->get("FrendsBackdrop").toString() == settingValue(Rain) ? Rain : Orbit;
}

QString FrendsBackdrop::settingValue(Mode mode)
{
    return mode == Rain ? QStringLiteral("rain") : QStringLiteral("orbit");
}

QString FrendsBackdrop::settingLabel(Mode mode)
{
    return mode == Rain ? tr("Кубы падают сверху") : tr("Кубы вращаются");
}

void FrendsBackdrop::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    m_frame = QPixmap();
    update();
}
