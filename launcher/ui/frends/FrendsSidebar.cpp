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

#include <QFrame>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPolygonF>
#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>

#include "FrendsScale.h"

namespace {
QFrame* makeSeparator(QWidget* parent)
{
    auto* separator = new QFrame(parent);
    separator->setFrameShape(QFrame::HLine);
    separator->setFixedHeight(1);
    separator->setStyleSheet(QStringLiteral("background: rgba(255, 255, 255, 120); border: none;"));
    return separator;
}
}  // namespace

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

    auto* separator = makeSeparator(this);
    m_spacerTop = new QSpacerItem(0, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_spacerBottom = new QSpacerItem(0, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_layout->addItem(m_spacerTop);
    m_layout->addWidget(separator);
    m_layout->addItem(m_spacerBottom);

    m_newsButton = addNavigationButton(QStringLiteral("news"));
    m_instancesButton = addNavigationButton(QStringLiteral("instances"));
    m_addButton = addNavigationButton(QStringLiteral("add"));
    m_serversButton = addNavigationButton(QStringLiteral("servers"));
    m_modsButton = addNavigationButton(QStringLiteral("mods"));
    m_accountsButton = addNavigationButton(QStringLiteral("accounts"));
    m_settingsButton = addNavigationButton(QStringLiteral("settings"));
    m_logsButton = addNavigationButton(QStringLiteral("logs"));
    m_aboutButton = addNavigationButton(QStringLiteral("about"));

    m_layout->addWidget(m_newsButton);
    m_layout->addWidget(m_instancesButton);
    m_layout->addWidget(m_addButton);
    m_layout->addWidget(m_serversButton);
    m_layout->addWidget(m_modsButton);
    m_layout->addWidget(m_accountsButton);
    m_layout->addWidget(m_settingsButton);
    m_layout->addWidget(m_logsButton);
    m_layout->addWidget(m_aboutButton);

    m_layout->addStretch(1);

    m_version = new QLabel(QStringLiteral("v1.0"), this);
    m_layout->addWidget(m_version);

    setNavigationTexts();
    applyScale();
}

QString FrendsSidebar::navigationButtonStyle(qreal scale) const
{
    const int font = qMax(9, int(qRound(12 * scale)));
    const int padding = qMax(4, int(qRound(7 * scale)));
    return QStringLiteral(
               "QPushButton {"
               "  color: rgba(255, 255, 255, 225);"
               "  background: transparent;"
               "  border: none;"
               "  border-radius: 4px;"
               "  padding: %1px %2px;"
               "  text-align: left;"
               "  font-size: %3px;"
               "  font-weight: 600;"
               "}"
               "QPushButton:hover { background: rgba(255, 255, 255, 30); color: #ffffff; }"
               "QPushButton:pressed { background: rgba(255, 255, 255, 50); }")
        .arg(padding)
        .arg(padding + 3)
        .arg(font);
}

void FrendsSidebar::applyScale()
{
    setFixedWidth(FrendsScale::px(190, this));
    m_layout->setContentsMargins(FrendsScale::px(18, this), FrendsScale::px(22, this), FrendsScale::px(18, this),
                                 FrendsScale::px(16, this));
    m_layout->setSpacing(FrendsScale::px(2, this));

    if (m_title) {
        m_title->setStyleSheet(QStringLiteral(
                                   "color: #ffffff; font-size: %1px; font-weight: 800; letter-spacing: 0.5px; background: transparent;")
                                   .arg(FrendsScale::px(16, this)));
    }
    if (m_subtitle) {
        m_subtitle->setStyleSheet(QStringLiteral(
                                      "color: rgba(255, 255, 255, 190); font-size: %1px; font-weight: 600; letter-spacing: 2px; "
                                      "background: transparent;")
                                      .arg(FrendsScale::px(8, this)));
    }
    if (m_version) {
        m_version->setStyleSheet(QStringLiteral("color: rgba(255, 255, 255, 150); font-size: %1px; background: transparent;")
                                     .arg(FrendsScale::px(10, this)));
    }

    const int buttonHeight = FrendsScale::px(28, this);
    for (auto* button : m_navButtons) {
        button->setFixedHeight(buttonHeight);
    }
    // The per-button colours depend on the active page, so they are applied
    // after the shared metrics rather than inside the loop above.
    updateActivePageStyles();

    // The two spacers around the separator follow the scale as well.
    const int spacerHeight = FrendsScale::px(10, this);
    if (m_spacerTop)
        m_spacerTop->changeSize(0, spacerHeight, QSizePolicy::Minimum, QSizePolicy::Fixed);
    if (m_spacerBottom)
        m_spacerBottom->changeSize(0, spacerHeight, QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_layout->invalidate();
}

QPushButton* FrendsSidebar::addNavigationButton(const QString& page)
{
    auto* button = new QPushButton(this);
    button->setCursor(Qt::PointingHandCursor);
    button->setFlat(true);
    button->setFocusPolicy(Qt::NoFocus);
    connect(button, &QPushButton::clicked, this, [this, page] { emit navigationRequested(page); });
    m_navButtons.append(button);
    m_navPages.append(page);
    return button;
}

QString FrendsSidebar::activeNavigationButtonStyle(qreal scale) const
{
    const int font = qMax(9, int(qRound(12 * scale)));
    const int padding = qMax(4, int(qRound(7 * scale)));

    // The active page wears the brand accent of the original launcher
    // (#F9D900) as a filled plate, so the current position is readable at a
    // glance on the dark sidebar.
    return QStringLiteral(
               "QPushButton {"
               "  color: #4a3b00;"
               "  background: #F9D900;"
               "  border: 1px solid #d8bb00;"
               "  border-radius: 4px;"
               "  padding: %1px %2px;"
               "  text-align: left;"
               "  font-size: %3px;"
               "  font-weight: 700;"
               "}"
               "QPushButton:hover { background: #ffe733; }"
               "QPushButton:pressed { background: #e3c500; }")
        .arg(padding)
        .arg(padding + 3)
        .arg(font);
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
    const qreal s = FrendsScale::factor(this);
    const auto normal = navigationButtonStyle(s);
    const auto active = activeNavigationButtonStyle(s);

    for (int i = 0; i < m_navButtons.size(); i++)
        m_navButtons.at(i)->setStyleSheet(m_navPages.at(i) == m_activePage ? active : normal);
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
    if (m_modsButton)
        m_modsButton->setText(tr("Моды"));
    if (m_accountsButton)
        m_accountsButton->setText(tr("Аккаунты"));
    if (m_settingsButton)
        m_settingsButton->setText(tr("Настройки"));
    if (m_logsButton)
        m_logsButton->setText(tr("Логи"));
    if (m_aboutButton)
        m_aboutButton->setText(tr("О лаунчере"));
}

void FrendsSidebar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0.0, QColor(46, 127, 184));
    gradient.setColorAt(0.5, QColor(33, 96, 146));
    gradient.setColorAt(1.0, QColor(23, 66, 105));
    painter.fillRect(rect(), gradient);

    // Faceted low-poly pattern, matching the TLauncher side panel look
    painter.setPen(Qt::NoPen);
    const QColor facets[] = {
        QColor(255, 255, 255, 14), QColor(0, 0, 0, 20), QColor(255, 255, 255, 8),
        QColor(0, 0, 0, 12),      QColor(255, 255, 255, 20), QColor(0, 0, 0, 26),
    };

    const qreal w = width();
    const qreal h = height();
    QPolygonF polygon;
    polygon << QPointF(0, h * 0.30) << QPointF(w * 0.55, h * 0.12) << QPointF(w, h * 0.34) << QPointF(w, h * 0.62)
            << QPointF(w * 0.42, h * 0.48);
    painter.setBrush(facets[0]);
    painter.drawPolygon(polygon);

    polygon.clear();
    polygon << QPointF(0, h * 0.62) << QPointF(w * 0.48, h * 0.52) << QPointF(w, h * 0.70) << QPointF(w, h) << QPointF(0, h);
    painter.setBrush(facets[1]);
    painter.drawPolygon(polygon);

    polygon.clear();
    polygon << QPointF(0, h * 0.30) << QPointF(w * 0.55, h * 0.12) << QPointF(w * 0.48, h * 0.52) << QPointF(0, h * 0.62);
    painter.setBrush(facets[2]);
    painter.drawPolygon(polygon);

    polygon.clear();
    polygon << QPointF(w * 0.55, h * 0.12) << QPointF(w, h * 0.34) << QPointF(w, h * 0.70) << QPointF(w * 0.48, h * 0.52);
    painter.setBrush(facets[3]);
    painter.drawPolygon(polygon);

    // Left edge highlight
    painter.fillRect(QRect(0, 0, 1, height()), QColor(255, 255, 255, 40));
}
