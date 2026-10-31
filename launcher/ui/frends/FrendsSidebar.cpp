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

#include "FrendsSidebar.h"

#include <QApplication>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>

#include "BuildConfig.h"
#include "FrendsScale.h"

namespace {

enum class NavIcon { News, Cube, Create, Servers, Accounts, Settings, Logs, About };

NavIcon iconFor(const QString& key)
{
    if (key == QLatin1String("news"))
        return NavIcon::News;
    if (key == QLatin1String("instances"))
        return NavIcon::Cube;
    if (key == QLatin1String("add"))
        return NavIcon::Create;
    if (key == QLatin1String("servers"))
        return NavIcon::Servers;
    if (key == QLatin1String("accounts"))
        return NavIcon::Accounts;
    if (key == QLatin1String("settings"))
        return NavIcon::Settings;
    if (key == QLatin1String("logs"))
        return NavIcon::Logs;
    return NavIcon::About;
}

/// Draws one navigation glyph on a 24x24 grid and scales it to `size`.
/// Everything is vector, so the icons stay sharp at any UI scale.
QPixmap drawNavIcon(NavIcon icon, int size, const QColor& color)
{
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(size / 24.0, size / 24.0);

    QPen pen(color, 1.9);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (icon) {
        case NavIcon::News: {
            p.drawRoundedRect(QRectF(2.5, 4.5, 19, 15), 2, 2);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRoundedRect(QRectF(5, 7, 6.5, 6.5), 1, 1);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawLine(QPointF(14, 8.5), QPointF(19, 8.5));
            p.drawLine(QPointF(14, 12), QPointF(19, 12));
            p.drawLine(QPointF(5, 16.5), QPointF(19, 16.5));
            break;
        }
        case NavIcon::Cube: {
            QPolygonF top;
            top << QPointF(12, 3.5) << QPointF(20, 8) << QPointF(12, 12.5) << QPointF(4, 8);
            QPolygonF left;
            left << QPointF(4, 8) << QPointF(12, 12.5) << QPointF(12, 20.5) << QPointF(4, 16);
            QPolygonF right;
            right << QPointF(20, 8) << QPointF(12, 12.5) << QPointF(12, 20.5) << QPointF(20, 16);

            QColor fill = color;
            fill.setAlphaF(0.35);
            p.setBrush(fill);
            p.drawPolygon(top);
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(left);
            p.drawPolygon(right);
            break;
        }
        case NavIcon::Create: {
            p.drawRoundedRect(QRectF(3, 3, 8, 8), 1.5, 1.5);
            p.drawRoundedRect(QRectF(13, 3, 8, 8), 1.5, 1.5);
            p.drawRoundedRect(QRectF(3, 13, 8, 8), 1.5, 1.5);
            p.drawEllipse(QPointF(17, 17), 4, 4);
            break;
        }
        case NavIcon::Servers: {
            p.drawEllipse(QPointF(12, 12), 9, 9);
            p.drawEllipse(QPointF(12, 12), 4.2, 9);
            p.drawLine(QPointF(3.4, 9), QPointF(20.6, 9));
            p.drawLine(QPointF(3.4, 15), QPointF(20.6, 15));
            break;
        }
        case NavIcon::Accounts: {
            p.drawEllipse(QPointF(12, 8), 4.2, 4.2);
            QPainterPath shoulders;
            shoulders.moveTo(4.2, 21);
            shoulders.arcTo(QRectF(4.2, 13.2, 15.6, 15.6), 180, -180);
            p.drawPath(shoulders);
            break;
        }
        case NavIcon::Settings: {
            p.drawEllipse(QPointF(12, 12), 6.6, 6.6);
            p.drawEllipse(QPointF(12, 12), 2.6, 2.6);
            QPen teeth = pen;
            teeth.setWidthF(2.6);
            teeth.setCapStyle(Qt::FlatCap);
            p.setPen(teeth);
            for (int i = 0; i < 8; i++) {
                p.save();
                p.translate(12, 12);
                p.rotate(i * 45.0);
                p.drawLine(QPointF(0, -8.4), QPointF(0, -11));
                p.restore();
            }
            break;
        }
        case NavIcon::Logs: {
            QPainterPath page;
            page.moveTo(5, 2.5);
            page.lineTo(14, 2.5);
            page.lineTo(19, 7.5);
            page.lineTo(19, 21.5);
            page.lineTo(5, 21.5);
            page.closeSubpath();
            p.drawPath(page);
            p.drawLine(QPointF(14, 2.5), QPointF(14, 7.5));
            p.drawLine(QPointF(14, 7.5), QPointF(19, 7.5));
            p.drawLine(QPointF(8, 12), QPointF(16, 12));
            p.drawLine(QPointF(8, 15.5), QPointF(16, 15.5));
            p.drawLine(QPointF(8, 19), QPointF(13, 19));
            break;
        }
        case NavIcon::About: {
            p.drawEllipse(QPointF(12, 12), 9.2, 9.2);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawEllipse(QPointF(12, 7.6), 1.35, 1.35);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawLine(QPointF(12, 11.2), QPointF(12, 17));
            break;
        }
    }

    return pm;
}

}  // namespace

/// One row of the sidebar. It paints itself so the icon, the label and the
/// active plate line up exactly as in the design, which a stock QPushButton
/// with a stylesheet cannot do reliably.
class FrendsNavButton : public QPushButton {
   public:
    FrendsNavButton(NavIcon icon, QWidget* parent) : QPushButton(parent), m_icon(icon)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover, true);
        setFlat(true);
        setText(QString());
    }

    void setActive(bool active)
    {
        if (m_active == active)
            return;
        m_active = active;
        refreshIcon();
        update();
    }

    void applyMetrics(qreal scale)
    {
        Q_UNUSED(scale);
        m_iconSize = FrendsScale::px(18, this);
        m_radius = FrendsScale::px(8, this);
        m_iconLeft = FrendsScale::px(12, this);
        m_textLeft = FrendsScale::px(40, this);

        QFont rowFont = font();
        rowFont.setPixelSize(FrendsScale::px(12, this));
        rowFont.setBold(false);
        rowFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.2);
        setFont(rowFont);

        setFixedHeight(FrendsScale::px(34, this));
        refreshIcon();
        update();
    }

   protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF plate = QRectF(rect()).adjusted(0, 0, -0.5, -0.5);

        if (m_active) {
            // Warm gradient plate with a soft halo, the same shape the design
            // uses to mark the page you are on.
            QLinearGradient glow(plate.topLeft(), plate.bottomLeft());
            glow.setColorAt(0.0, QColor(255, 219, 80, 70));
            glow.setColorAt(1.0, QColor(255, 170, 20, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(glow);
            p.drawRoundedRect(plate.adjusted(-1, -2, 1, 2), m_radius + 2, m_radius + 2);

            QLinearGradient fill(plate.topLeft(), plate.bottomLeft());
            fill.setColorAt(0.0, QColor(0xFF, 0xD9, 0x4A));
            fill.setColorAt(1.0, QColor(0xF2, 0xAF, 0x17));
            p.setBrush(fill);
            p.drawRoundedRect(plate, m_radius, m_radius);

            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(255, 255, 255, 110), 1));
            p.drawRoundedRect(plate.adjusted(0.5, 0.5, -0.5, -0.5), m_radius, m_radius);
        } else if (m_hover) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 20));
            p.drawRoundedRect(plate, m_radius, m_radius);
        }

        if (!m_pixmap.isNull()) {
            const QRect iconRect(m_iconLeft, (height() - m_iconSize) / 2, m_iconSize, m_iconSize);
            p.drawPixmap(iconRect, m_pixmap);
        }

        p.setFont(font());
        p.setPen(m_active ? QColor(0x3B, 0x2C, 0x00) : QColor(0xE7, 0xE8, 0xEA));
        const QRect textRect(m_textLeft, 0, width() - m_textLeft - FrendsScale::px(6, this), height());
        const QString label = fontMetrics().elidedText(text(), Qt::ElideRight, textRect.width());
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    void enterEvent(QEnterEvent* event) override
    {
        m_hover = true;
        update();
        QPushButton::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        m_hover = false;
        update();
        QPushButton::leaveEvent(event);
    }

   private:
    void refreshIcon()
    {
        // The plate is light, so the glyph on it has to flip to a dark tone.
        const QColor tone = m_active ? QColor(0x3B, 0x2C, 0x00) : QColor(0xEC, 0xED, 0xEF);
        m_pixmap = drawNavIcon(m_icon, qMax(10, m_iconSize), tone);
    }

    NavIcon m_icon;
    QPixmap m_pixmap;
    bool m_active = false;
    bool m_hover = false;
    int m_iconSize = 18;
    int m_radius = 8;
    int m_iconLeft = 12;
    int m_textLeft = 40;
};

FrendsSidebar::FrendsSidebar(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setAutoFillBackground(false);

    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(2);

    m_title = new QLabel(QStringLiteral("FRENDS LAUNCHER"), this);
    m_layout->addWidget(m_title);

    m_subtitle = new QLabel(this);
    m_layout->addWidget(m_subtitle);

    m_separator = new QWidget(this);
    m_separator->setFixedHeight(1);
    m_separator->setStyleSheet(QStringLiteral("background: rgba(255, 255, 255, 45);"));
    m_spacerTop = new QSpacerItem(0, 12, QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_spacerBottom = new QSpacerItem(0, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_layout->addItem(m_spacerTop);
    m_layout->addWidget(m_separator);
    m_layout->addItem(m_spacerBottom);

    m_newsButton = addNavigationButton(QStringLiteral("news"), QStringLiteral("news"));
    m_instancesButton = addNavigationButton(QStringLiteral("instances"), QStringLiteral("instances"));
    m_addButton = addNavigationButton(QStringLiteral("add"), QStringLiteral("add"));
    m_serversButton = addNavigationButton(QStringLiteral("servers"), QStringLiteral("servers"));
    m_accountsButton = addNavigationButton(QStringLiteral("accounts"), QStringLiteral("accounts"));
    m_settingsButton = addNavigationButton(QStringLiteral("settings"), QStringLiteral("settings"));
    m_logsButton = addNavigationButton(QStringLiteral("logs"), QStringLiteral("logs"));
    m_aboutButton = addNavigationButton(QStringLiteral("about"), QStringLiteral("about"));

    m_layout->addStretch(1);

    m_version = new QLabel(this);
    m_version->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_layout->addWidget(m_version);

    setNavigationTexts();
    applyScale();
}

void FrendsSidebar::applyScale()
{
    setFixedWidth(FrendsScale::px(190, this));
    m_layout->setContentsMargins(FrendsScale::px(12, this), FrendsScale::px(16, this), FrendsScale::px(12, this),
                                 FrendsScale::px(12, this));
    m_layout->setSpacing(FrendsScale::px(3, this));

    if (m_title) {
        m_title->setStyleSheet(QStringLiteral(
                                   "color: #ffffff; font-size: %1px; font-weight: 800; letter-spacing: 0.6px; background: transparent;")
                                   .arg(FrendsScale::px(15, this)));
    }
    if (m_subtitle) {
        m_subtitle->setStyleSheet(QStringLiteral(
                                      "color: rgba(255, 255, 255, 150); font-size: %1px; font-weight: 600; letter-spacing: 1.8px; "
                                      "background: transparent;")
                                      .arg(FrendsScale::px(8, this)));
    }
    if (m_version) {
        m_version->setStyleSheet(QStringLiteral("color: rgba(255, 255, 255, 135); font-size: %1px; background: transparent;")
                                     .arg(FrendsScale::px(9, this)));
    }

    for (auto* button : m_navButtons)
        button->applyMetrics(FrendsScale::factor(this));

    const int spacerHeight = FrendsScale::px(10, this);
    if (m_spacerTop)
        m_spacerTop->changeSize(0, spacerHeight, QSizePolicy::Minimum, QSizePolicy::Fixed);
    if (m_spacerBottom)
        m_spacerBottom->changeSize(0, spacerHeight, QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_layout->invalidate();
}

FrendsNavButton* FrendsSidebar::addNavigationButton(const QString& page, const QString& icon)
{
    auto* button = new FrendsNavButton(iconFor(icon), this);
    connect(button, &QPushButton::clicked, this, [this, page] { emit navigationRequested(page); });
    m_layout->addWidget(button);
    m_navButtons.append(button);
    m_navPages.append(page);
    return button;
}

void FrendsSidebar::setActivePage(const QString& page)
{
    if (m_activePage == page)
        return;

    m_activePage = page;
    updateActivePageStyles();
}

void FrendsSidebar::updateActivePageStyles()
{
    for (int i = 0; i < m_navButtons.size(); i++)
        m_navButtons.at(i)->setActive(m_navPages.at(i) == m_activePage);
}

void FrendsSidebar::retranslate()
{
    setNavigationTexts();
}

void FrendsSidebar::setNavigationTexts()
{
    if (m_subtitle)
        m_subtitle->setText(tr("ЛАУНЧЕР ДЛЯ MINECRAFT"));
    if (m_newsButton)
        m_newsButton->setText(tr("Новости"));
    if (m_instancesButton)
        m_instancesButton->setText(tr("Сборки"));
    if (m_addButton)
        m_addButton->setText(tr("Создать сборку"));
    if (m_serversButton)
        m_serversButton->setText(tr("Серверы"));
    if (m_accountsButton)
        m_accountsButton->setText(tr("Аккаунты"));
    if (m_settingsButton)
        m_settingsButton->setText(tr("Настройки"));
    if (m_logsButton)
        m_logsButton->setText(tr("Логи"));
    if (m_aboutButton)
        m_aboutButton->setText(tr("О лаунчере"));
    if (m_version)
        m_version->setText(QStringLiteral("v") + BuildConfig.versionString());
}

void FrendsSidebar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Near-black plate with a barely-there lift at the top, so the panel reads
    // as a surface rather than a flat block of colour.
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0.0, QColor(16, 19, 25, 232));
    gradient.setColorAt(0.55, QColor(11, 13, 18, 226));
    gradient.setColorAt(1.0, QColor(8, 10, 14, 220));
    painter.fillRect(rect(), gradient);

    // Hairline on the inner edge separates it from the content card.
    painter.fillRect(QRect(0, 0, 1, height()), QColor(255, 255, 255, 26));
}
