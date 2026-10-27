// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
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

#include "FrendsServersPage.h"

#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QColor>
#include <QElapsedTimer>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>

#include "FrendsScale.h"
#include "ui/pages/instance/McClient.h"
#include "ui/pages/instance/McResolver.h"

namespace {
constexpr int COL_NAME = 0;
constexpr int COL_ADDRESS = 1;
constexpr int COL_PLAYERS = 2;
constexpr int COL_VERSION = 3;
constexpr int COL_PING = 4;

constexpr int DEFAULT_PORT = 25565;

/// Splits "host" or "host:port" into the two parts.
void splitAddress(const QString& address, QString& host, int& port)
{
    host = address.trimmed();
    port = DEFAULT_PORT;

    const int colon = host.lastIndexOf(QLatin1Char(':'));
    if (colon > 0) {
        bool ok = false;
        const int parsed = host.mid(colon + 1).toInt(&ok);
        if (ok) {
            port = parsed;
            host = host.left(colon);
        }
    }
}

/// Minecraft MOTDs may carry §-style formatting codes; they are useless here.
QString stripFormatting(QString text)
{
    static const QRegularExpression codes(QStringLiteral("\u00a7[0-9a-fk-orA-FK-OR]"));
    text.remove(codes);
    return text.simplified();
}

QString motdFromJson(const QJsonValue& description)
{
    if (description.isString())
        return stripFormatting(description.toString());

    if (description.isObject()) {
        const auto object = description.toObject();
        QString text = object.value("text").toString();
        for (const auto& part : object.value("extra").toArray())
            text += part.isString() ? part.toString() : part.toObject().value("text").toString();
        return stripFormatting(text);
    }

    return QString();
}

// Ping colours. The bands are tuned for the latency a player actually sees
// when connecting to a public server from a home connection: anything under
// a quarter of a second feels instant, a second and above is painful.
// The palette comes from the original launcher (#F9D900 accent, #F3622D
// accent 2, #FF0000 error); the yellow and the red are darkened so that
// they stay readable as text on the white table background.
const QColor COLOR_FAST(31, 122, 63);      // < 250 ms
const QColor COLOR_MEDIUM(176, 132, 0);    // < 600 ms
const QColor COLOR_SLOW(0xF3, 0x62, 0x2D); // < 1000 ms
const QColor COLOR_BAD(191, 48, 48);       // >= 1000 ms
const QColor COLOR_OFFLINE(150, 60, 60);
const QColor COLOR_UNKNOWN(120, 120, 120);
}  // namespace

FrendsServersPage::FrendsServersPage(QWidget* parent) : QWidget(parent)
{
    // Public servers, addressed honestly. The list is the player's own pick:
    // every address below was checked with a live status ping before shipping.
    m_servers = {
        { QStringLiteral("LastMine"), QStringLiteral("mc.lastmine.me"),
          QStringLiteral("Выживание, BedWars, SkyWars, Murder; версии 1.12–1.21") },
        { QStringLiteral("MineLand"), QStringLiteral("mpa.mineland.net"),
          QStringLiteral("BedWars, анархия, скайблок; версии 1.8–26.x") },
        { QStringLiteral("HypeMC"), QStringLiteral("mc.hypemc.pro"),
          QStringLiteral("Мини-игры и выживание; версии 1.8–26.3") },
    };

    auto* layout = new QVBoxLayout(this);
    // The vertical rhythm of the original launcher: 16 / 14 / 9 / 7 / 5 px.
    layout->setSpacing(9);
    layout->setContentsMargins(16, 16, 16, 14);

    m_title = new QLabel(this);
    m_title->setStyleSheet(QStringLiteral("color: #1f3c56; font-weight: 800; background: transparent;"));
    layout->addWidget(m_title);

    m_subtitle = new QLabel(this);
    m_subtitle->setWordWrap(true);
    m_subtitle->setStyleSheet(QStringLiteral("color: #5b6b7a; background: transparent;"));
    layout->addWidget(m_subtitle);

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(8);

    m_refreshButton = new QPushButton(this);
    m_refreshButton->setCursor(Qt::PointingHandCursor);
    connect(m_refreshButton, &QPushButton::clicked, this, &FrendsServersPage::refresh);
    toolbar->addWidget(m_refreshButton);

    m_copyButton = new QPushButton(this);
    m_copyButton->setCursor(Qt::PointingHandCursor);
    connect(m_copyButton, &QPushButton::clicked, this, [this] {
        const auto address = selectedAddress();
        if (!address.isEmpty())
            QApplication::clipboard()->setText(address);
    });
    toolbar->addWidget(m_copyButton);

    m_playButton = new QPushButton(this);
    m_playButton->setCursor(Qt::PointingHandCursor);
    connect(m_playButton, &QPushButton::clicked, this, [this] {
        const auto address = selectedAddress();
        if (!address.isEmpty())
            emit playRequested(address);
    });
    toolbar->addWidget(m_playButton);

    toolbar->addStretch(1);

    m_status = new QLabel(this);
    m_status->setStyleSheet(QStringLiteral("color: #5b6b7a; background: transparent;"));
    toolbar->addWidget(m_status);

    layout->addLayout(toolbar);

    m_table = new QTableWidget(m_servers.size(), 5, this);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(false);
    m_table->setFocusPolicy(Qt::StrongFocus);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setSectionResizeMode(COL_NAME, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_ADDRESS, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(COL_PLAYERS, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_VERSION, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_PING, QHeaderView::ResizeToContents);

    for (int row = 0; row < m_servers.size(); row++) {
        const auto& server = m_servers.at(row);

        auto* nameItem = new QTableWidgetItem(server.name);
        nameItem->setToolTip(server.note);
        m_table->setItem(row, COL_NAME, nameItem);

        m_table->setItem(row, COL_ADDRESS, new QTableWidgetItem(server.address));
        // The address reads as a link: the original launcher uses #0071BC for
        // links and #039ED3 on hover, and the address is the one value in
        // this table the player may want to copy by hand.
        auto* addressItem = m_table->item(row, COL_ADDRESS);
        addressItem->setForeground(QColor(0x00, 0x71, 0xBC));
        // Links in the original launcher are underlined, and the address is
        // the one value a player may want to copy by hand.
        QFont addressFont = m_table->font();
        addressFont.setUnderline(true);
        addressItem->setFont(addressFont);
        m_table->setItem(row, COL_PLAYERS, new QTableWidgetItem(QStringLiteral("—")));
        m_table->setItem(row, COL_VERSION, new QTableWidgetItem(QStringLiteral("—")));
        m_table->setItem(row, COL_PING, new QTableWidgetItem(QStringLiteral("…")));
    }

    m_table->selectRow(0);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &FrendsServersPage::updateStatus);
    connect(m_table, &QTableWidget::doubleClicked, this, [this](const QModelIndex&) {
        const auto address = selectedAddress();
        if (!address.isEmpty())
            emit playRequested(address);
    });

    layout->addWidget(m_table, 1);

    // A short legend so the ping colours mean something without a tooltip.
    m_legend = new QLabel(this);
    m_legend->setTextFormat(Qt::RichText);
    m_legend->setWordWrap(true);
    layout->addWidget(m_legend);

    retranslate();
    applyScale();
}

void FrendsServersPage::retranslate()
{
    m_title->setText(tr("Серверы"));
    m_subtitle->setText(tr("Публичные серверы Minecraft. Адреса показаны как есть — никаких скрытых партнёрских размещений. "
                           "Двойной клик по строке запускает выбранную сборку и подключает её к серверу."));
    m_refreshButton->setText(tr("Обновить"));
    m_copyButton->setText(tr("Копировать адрес"));
    m_playButton->setText(tr("Играть"));

    m_table->setHorizontalHeaderLabels({ tr("Сервер"), tr("Адрес"), tr("Игроки"), tr("Версия"), tr("Пинг") });

    // The legend mirrors the thresholds used in onPingSucceeded().
    m_legend->setText(tr("Пинг: %1 — до 250 мс, %2 — до 600 мс, %3 — до 1000 мс, %4 — дольше. "
                         "Чем ниже значение, тем комфортнее играть.")
                          .arg(QStringLiteral("<span style=\"color:#1f7a3f\">●</span>"))
                          .arg(QStringLiteral("<span style=\"color:#b08400\">●</span>"))
                          .arg(QStringLiteral("<span style=\"color:#f3622d\">●</span>"))
                          .arg(QStringLiteral("<span style=\"color:#bf3030\">●</span>")));

    updateStatus();
}

void FrendsServersPage::applyScale()
{
    const int titleFont = FrendsScale::px(18, this);
    const int bodyFont = FrendsScale::px(14, this);
    const int secondaryFont = FrendsScale::px(13, this);
    const int buttonHeight = FrendsScale::px(30, this);

    // Palette of the original launcher: #585858 primary text, #ACACAC secondary.
    // Typography follows the same source: Calibri, heading 18px bold, body
    // 14-15px, secondary lines a step smaller.
    m_title->setStyleSheet(
        QStringLiteral("color: #585858; font-size: %1px; font-weight: 700; background: transparent;").arg(titleFont));
    m_subtitle->setStyleSheet(
        QStringLiteral("color: #7a7a7a; font-size: %1px; background: transparent;").arg(bodyFont));
    m_status->setStyleSheet(
        QStringLiteral("color: #ACACAC; font-size: %1px; background: transparent;").arg(secondaryFont));
    m_legend->setStyleSheet(
        QStringLiteral("color: #7a7a7a; font-size: %1px; background: transparent;").arg(secondaryFont));

    // Button look is taken from the original launcher's control stylesheet:
    // a modena-like grey gradient for secondary actions and the brand accent
    // #F9D900 for the primary call to action. Padding follows the original
    // "6 x 14" text-button spec, with Calibri 14px and #333 text.
    const auto secondaryStyle = QStringLiteral(
                                    "QPushButton {"
                                    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                                    "      stop:0 #fdfdfd, stop:0.5 #ececec, stop:1 #e2e2e2);"
                                    "  color: #333333;"
                                    "  border: 1px solid #b9b9b9;"
                                    "  border-radius: 4px;"
                                    "  padding: %1px %2px;"
                                    "  font-size: %3px;"
                                    "}"
                                    "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                                    "      stop:0 #ffffff, stop:1 #eaeaea); }"
                                    "QPushButton:pressed { background: #dcdcdc; }"
                                    "QPushButton:disabled { color: #a0a0a0; border-color: #d5d5d5; }")
                                    .arg(FrendsScale::px(6, this))
                                    .arg(FrendsScale::px(14, this))
                                    .arg(bodyFont);

    const auto primaryStyle = QStringLiteral(
                                  "QPushButton {"
                                  "  background: #F9D900;"
                                  "  color: #4a3b00;"
                                  "  border: 1px solid #d8bb00;"
                                  "  border-radius: 4px;"
                                  "  padding: %1px %2px;"
                                  "  font-size: %3px;"
                                  "  font-weight: 700;"
                                  "}"
                                  "QPushButton:hover { background: #ffe733; }"
                                  "QPushButton:pressed { background: #e3c500; }"
                                  "QPushButton:disabled { background: #e8e8e8; color: #a0a0a0; border-color: #d5d5d5; }")
                                  .arg(FrendsScale::px(6, this))
                                  .arg(FrendsScale::px(16, this))
                                  .arg(bodyFont);

    for (auto* button : { m_refreshButton, m_copyButton }) {
        button->setStyleSheet(secondaryStyle);
        button->setFixedHeight(buttonHeight);
    }
    m_playButton->setStyleSheet(primaryStyle);
    m_playButton->setFixedHeight(buttonHeight);

    m_table->setStyleSheet(
        QStringLiteral(
            "QTableWidget {"
            "  background: #ffffff;"
            "  border: 1px solid #d9e2ea;"
            "  border-radius: 4px;"
            "  gridline-color: transparent;"
            "  font-size: %1px;"
            "  color: #585858;"
            "}"
            "QTableWidget::item { padding: %2px %3px; border-bottom: 1px solid #eef2f6; }"
            "QTableWidget::item:selected { background: #dceaf6; color: #2b2b2b; }"
            "QHeaderView::section {"
            "  background: #f2f6f9;"
            "  color: #7a7a7a;"
            "  border: none;"
            "  border-bottom: 1px solid #d9e2ea;"
            "  padding: %4px %3px;"
            "  font-size: %5px;"
            "  font-weight: 700;"
            "}")
            .arg(bodyFont)
            .arg(FrendsScale::px(6, this))
            .arg(FrendsScale::px(8, this))
            .arg(FrendsScale::px(6, this))
            .arg(FrendsScale::px(10, this)));

    m_table->verticalHeader()->setDefaultSectionSize(FrendsScale::px(30, this));
    m_table->horizontalHeader()->setFixedHeight(FrendsScale::px(28, this));
}

void FrendsServersPage::refresh()
{
    m_pending = m_servers.size();
    m_checked = 0;

    for (int row = 0; row < m_servers.size(); row++) {
        m_table->item(row, COL_PLAYERS)->setText(QStringLiteral("…"));
        m_table->item(row, COL_VERSION)->setText(QStringLiteral("…"));
        m_table->item(row, COL_PING)->setText(QStringLiteral("…"));
        m_table->item(row, COL_PING)->setForeground(COLOR_UNKNOWN);

        pingRow(row);
    }

    updateStatus();
}

void FrendsServersPage::pingRow(int row)
{
    QString host;
    int port = DEFAULT_PORT;
    splitAddress(m_servers.at(row).address, host, port);

    auto* timer = new QElapsedTimer();
    timer->start();

    auto* resolver = new McResolver(this, host, port);
    connect(resolver, &McResolver::succeeded, this, [this, row, host, timer](QString ip, int resolvedPort) {
        auto* client = new McClient(this, host, ip, static_cast<uint16_t>(resolvedPort));

        connect(client, &McClient::succeeded, this, [this, row, timer](QJsonObject data) {
            onPingSucceeded(row, data, timer->elapsed());
            delete timer;
        });
        connect(client, &McClient::failed, this, [this, row, timer](QString) {
            onPingFailed(row);
            delete timer;
        });
        connect(client, &McClient::finished, client, &QObject::deleteLater);

        // Only the status handshake is measured: the SRV/A lookup above is DNS
        // latency, not server latency, and it used to inflate the numbers.
        timer->restart();
        client->getStatusData();
    });
    connect(resolver, &McResolver::failed, this, [this, row, timer](QString) {
        onPingFailed(row);
        delete timer;
    });
    connect(resolver, &McResolver::finished, resolver, &QObject::deleteLater);

    resolver->ping();
}

void FrendsServersPage::onPingSucceeded(int row, const QJsonObject& data, qint64 latencyMs)
{
    const auto players = data.value("players").toObject();
    const int online = players.value("online").toInt();
    const int max = players.value("max").toInt();

    m_table->item(row, COL_PLAYERS)->setText(QStringLiteral("%1 / %2").arg(online).arg(max));
    m_table->item(row, COL_PLAYERS)->setForeground(COLOR_FAST);

    const auto version = data.value("version").toObject().value("name").toString();
    m_table->item(row, COL_VERSION)->setText(version.isEmpty() ? QStringLiteral("—") : version);

    m_table->item(row, COL_PING)->setText(QStringLiteral("%1 мс").arg(latencyMs));
    // The colour tells the player how the connection will actually feel.
    if (latencyMs < 250) {
        m_table->item(row, COL_PING)->setForeground(COLOR_FAST);
    } else if (latencyMs < 600) {
        m_table->item(row, COL_PING)->setForeground(COLOR_MEDIUM);
    } else if (latencyMs < 1000) {
        m_table->item(row, COL_PING)->setForeground(COLOR_SLOW);
    } else {
        m_table->item(row, COL_PING)->setForeground(COLOR_BAD);
    }

    const auto motd = motdFromJson(data.value("description"));
    if (!motd.isEmpty())
        m_table->item(row, COL_NAME)->setToolTip(motd);

    m_checked++;
    updateStatus();
}

void FrendsServersPage::onPingFailed(int row)
{
    m_table->item(row, COL_PLAYERS)->setText(QStringLiteral("—"));
    m_table->item(row, COL_PLAYERS)->setForeground(COLOR_OFFLINE);
    m_table->item(row, COL_VERSION)->setText(QStringLiteral("—"));
    m_table->item(row, COL_PING)->setText(tr("офлайн"));
    m_table->item(row, COL_PING)->setForeground(COLOR_OFFLINE);

    m_checked++;
    updateStatus();
}

void FrendsServersPage::updateStatus()
{
    if (m_pending == 0)
        m_status->setText(tr("Готово к проверке"));
    else if (m_checked < m_pending)
        m_status->setText(tr("Проверено %1 из %2…").arg(m_checked).arg(m_pending));
    else
        m_status->setText(tr("Проверено %1 из %2").arg(m_checked).arg(m_pending));

    const bool hasSelection = m_table->currentRow() >= 0;
    m_copyButton->setEnabled(hasSelection);
    m_playButton->setEnabled(hasSelection);
}

QString FrendsServersPage::selectedAddress() const
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_servers.size())
        return QString();
    return m_servers.at(row).address;
}
