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
#include <QClipboard>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "Application.h"
#include "FrendsPalette.h"
#include "FrendsScale.h"
#include "settings/SettingsObject.h"
#include "ui/pages/instance/McClient.h"
#include "ui/pages/instance/McResolver.h"

namespace {
constexpr int DEFAULT_PORT = 25565;
constexpr int TOP_SIZE = 10;

// Ranking columns
constexpr int TOP_RANK = 0;
constexpr int TOP_NAME = 1;
constexpr int TOP_PLAYERS = 2;
constexpr int TOP_PING = 3;

// Own-list columns
constexpr int MINE_NAME = 0;
constexpr int MINE_ADDRESS = 1;
constexpr int MINE_PLAYERS = 2;
constexpr int MINE_PING = 3;

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

/// Groups digits so a four-figure player count stays readable.
QString formatCount(int value)
{
    return QLocale().toString(value);
}
}  // namespace

FrendsServersPage::FrendsServersPage(QWidget* parent) : QWidget(parent)
{
    // The catalogue behind the ranking. Every address is a real public server;
    // the order here means nothing, the table is sorted by players online.
    m_catalogue = {
        { QStringLiteral("FunTime"), QStringLiteral("mc.funtime.su") },
        { QStringLiteral("HolyWorld"), QStringLiteral("mc.holyworld.ru") },
        { QStringLiteral("ReallyWorld"), QStringLiteral("mc.reallyworld.ru") },
        { QStringLiteral("Sunrise"), QStringLiteral("mc.sunrisemc.ru") },
        { QStringLiteral("MasedWorld"), QStringLiteral("mc.masedworld.net") },
        { QStringLiteral("VimeWorld"), QStringLiteral("mc.vimeworld.ru") },
        { QStringLiteral("MineLand"), QStringLiteral("mpa.mineland.net") },
        { QStringLiteral("HypeMC"), QStringLiteral("mc.hypemc.pro") },
        { QStringLiteral("LastMine"), QStringLiteral("mc.lastmine.me") },
        { QStringLiteral("GrandMine"), QStringLiteral("mc.grandmine.net") },
        { QStringLiteral("Hypixel"), QStringLiteral("mc.hypixel.net") },
        { QStringLiteral("CubeCraft"), QStringLiteral("play.cubecraft.net") },
        { QStringLiteral("Wynncraft"), QStringLiteral("play.wynncraft.com") },
        { QStringLiteral("2b2t"), QStringLiteral("connect.2b2t.org") },
    };

    loadMine();
    buildPanels();

    // Ten minutes is short enough that the ranking stays current and long
    // enough that the launcher is not pinging servers all day.
    m_timer = new QTimer(this);
    m_timer->setInterval(REFRESH_MINUTES * 60 * 1000);
    connect(m_timer, &QTimer::timeout, this, [this] { refresh(true); });

    retranslate();
    applyScale();
}

void FrendsServersPage::buildPanels()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(16, 16, 16, 14);

    m_title = new QLabel(this);
    layout->addWidget(m_title);

    m_subtitle = new QLabel(this);
    m_subtitle->setWordWrap(true);
    layout->addWidget(m_subtitle);

    auto* panels = new QHBoxLayout();
    panels->setSpacing(14);

    // ---- ranking -----------------------------------------------------------
    QWidget* topHeader = nullptr;
    QWidget* topBody = nullptr;
    m_topPanel = makePanel(tr("Топ-10 по онлайну"), &topHeader, &topBody, &m_topTitle, &m_topHint);

    auto* topHeaderRow = qobject_cast<QHBoxLayout*>(topHeader->layout());
    m_topRefresh = new QPushButton(topHeader);
    m_topRefresh->setCursor(Qt::PointingHandCursor);
    connect(m_topRefresh, &QPushButton::clicked, this, [this] { refresh(true); });
    topHeaderRow->addWidget(m_topRefresh);

    auto* topLayout = qobject_cast<QVBoxLayout*>(topBody->layout());
    m_topTable = new QTableWidget(0, 4, topBody);
    topLayout->addWidget(m_topTable, 1);

    // ---- own list ----------------------------------------------------------
    QWidget* mineHeader = nullptr;
    QWidget* mineBody = nullptr;
    m_minePanel = makePanel(tr("Мои серверы"), &mineHeader, &mineBody, &m_mineTitle, &m_mineHint);

    auto* mineHeaderRow = qobject_cast<QHBoxLayout*>(mineHeader->layout());
    m_mineRefresh = new QPushButton(mineHeader);
    m_mineRefresh->setCursor(Qt::PointingHandCursor);
    connect(m_mineRefresh, &QPushButton::clicked, this, [this] { refresh(true); });
    mineHeaderRow->addWidget(m_mineRefresh);

    m_removeButton = new QPushButton(mineHeader);
    m_removeButton->setCursor(Qt::PointingHandCursor);
    connect(m_removeButton, &QPushButton::clicked, this, &FrendsServersPage::removeMine);
    mineHeaderRow->addWidget(m_removeButton);

    auto* mineLayout = qobject_cast<QVBoxLayout*>(mineBody->layout());

    auto* addRow = new QHBoxLayout();
    addRow->setSpacing(6);
    m_addName = new QLineEdit(mineBody);
    m_addAddress = new QLineEdit(mineBody);
    m_addButton = new QPushButton(mineBody);
    m_addButton->setCursor(Qt::PointingHandCursor);
    connect(m_addButton, &QPushButton::clicked, this, &FrendsServersPage::addMine);
    connect(m_addAddress, &QLineEdit::returnPressed, this, &FrendsServersPage::addMine);
    addRow->addWidget(m_addName, 2);
    addRow->addWidget(m_addAddress, 3);
    addRow->addWidget(m_addButton);
    mineLayout->addLayout(addRow);

    m_mineTable = new QTableWidget(0, 4, mineBody);
    mineLayout->addWidget(m_mineTable, 1);

    panels->addWidget(m_topPanel, 1);
    panels->addWidget(m_minePanel, 1);
    layout->addLayout(panels, 1);

    for (auto* table : { m_topTable, m_mineTable }) {
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setShowGrid(false);
        table->setAlternatingRowColors(false);
        table->setFocusPolicy(Qt::StrongFocus);
        table->setWordWrap(false);
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setHighlightSections(false);
        table->horizontalHeader()->setStretchLastSection(false);

        // Double click joins the server with the build selected on the main page.
        connect(table, &QTableWidget::doubleClicked, this, [this, table] {
            const auto address = selectedAddress(table, table == m_topTable ? m_topView : m_mine);
            if (!address.isEmpty())
                emit playRequested(address);
        });
        connect(table, &QTableWidget::itemSelectionChanged, this, [this] {
            updateTopStatus();
            updateMineStatus();
        });
    }

    m_topTable->horizontalHeader()->setSectionResizeMode(TOP_RANK, QHeaderView::ResizeToContents);
    m_topTable->horizontalHeader()->setSectionResizeMode(TOP_NAME, QHeaderView::Stretch);
    m_topTable->horizontalHeader()->setSectionResizeMode(TOP_PLAYERS, QHeaderView::ResizeToContents);
    m_topTable->horizontalHeader()->setSectionResizeMode(TOP_PING, QHeaderView::ResizeToContents);

    m_mineTable->horizontalHeader()->setSectionResizeMode(MINE_NAME, QHeaderView::ResizeToContents);
    m_mineTable->horizontalHeader()->setSectionResizeMode(MINE_ADDRESS, QHeaderView::Stretch);
    m_mineTable->horizontalHeader()->setSectionResizeMode(MINE_PLAYERS, QHeaderView::ResizeToContents);
    m_mineTable->horizontalHeader()->setSectionResizeMode(MINE_PING, QHeaderView::ResizeToContents);

    rebuildTop();
    rebuildMine();
}

QWidget* FrendsServersPage::makePanel(const QString& title, QWidget** headerOut, QWidget** bodyOut, QLabel** titleOut, QLabel** hintOut)
{
    auto* panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("frendsPanel"));
    panel->setFrameShape(QFrame::NoFrame);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(8);

    auto* header = new QWidget(panel);
    auto* headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(8);

    auto* label = new QLabel(title, header);
    label->setObjectName(QStringLiteral("frendsPanelTitle"));
    headerRow->addWidget(label);
    headerRow->addStretch(1);

    auto* hint = new QLabel(header);
    hint->setObjectName(QStringLiteral("frendsPanelHint"));
    headerRow->addWidget(hint);

    layout->addWidget(header);

    auto* body = new QWidget(panel);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(8);
    layout->addWidget(body, 1);

    if (headerOut)
        *headerOut = header;
    if (bodyOut)
        *bodyOut = body;
    if (titleOut)
        *titleOut = label;
    if (hintOut)
        *hintOut = hint;
    return panel;
}

void FrendsServersPage::retranslate()
{
    m_title->setText(tr("Серверы"));
    m_topTitle->setText(tr("Топ-10 по онлайну"));
    m_mineTitle->setText(tr("Мои серверы"));
    m_subtitle->setText(tr("Слева — десять публичных серверов с самым большим онлайном, список сам обновляется "
                           "каждые %1 минут. Справа — ваши серверы: добавьте адрес, и он появится в списке. "
                           "Двойной клик по строке запускает сборку и подключает её к серверу.")
                           .arg(REFRESH_MINUTES));

    m_topRefresh->setText(tr("Обновить"));
    m_mineRefresh->setText(tr("Обновить"));
    m_removeButton->setText(tr("Удалить"));
    m_addButton->setText(tr("Добавить"));
    m_addName->setPlaceholderText(tr("Название"));
    m_addAddress->setPlaceholderText(tr("Адрес, например mc.example.net"));

    m_topTable->setHorizontalHeaderLabels({ QStringLiteral("#"), tr("Сервер"), tr("Игроки"), tr("Пинг") });
    m_mineTable->setHorizontalHeaderLabels({ tr("Сервер"), tr("Адрес"), tr("Игроки"), tr("Пинг") });

    updateTopStatus();
    updateMineStatus();
}

void FrendsServersPage::applyScale()
{
    const int titleFont = FrendsScale::px(18, this);
    const int bodyFont = FrendsScale::px(14, this);
    const int smallFont = FrendsScale::px(12, this);
    const int buttonHeight = FrendsScale::px(28, this);

    const auto& theme = FrendsTheme::colors();

    m_title->setStyleSheet(QStringLiteral("color: %1; font-size: %2px; font-weight: 700; background: transparent;")
                               .arg(theme.text.name())
                               .arg(titleFont));
    m_subtitle->setStyleSheet(QStringLiteral("color: %1; font-size: %2px; background: transparent;")
                                  .arg(FrendsTheme::css(theme.textMuted))
                                  .arg(bodyFont));

    for (auto* panel : { m_topPanel, m_minePanel }) {
        panel->setStyleSheet(QStringLiteral("#frendsPanel { background: %1; border: 1px solid %2; border-radius: 12px; }")
                                 .arg(FrendsTheme::css(theme.tile))
                                 .arg(FrendsTheme::css(theme.tileBorder)));
    }

    for (auto* label : { m_topTitle, m_mineTitle }) {
        label->setStyleSheet(QStringLiteral("color: %1; font-size: %2px; font-weight: 700; background: transparent;")
                                 .arg(theme.text.name())
                                 .arg(bodyFont + 1));
    }
    for (auto* hint : { m_topHint, m_mineHint }) {
        hint->setStyleSheet(
            QStringLiteral("color: %1; font-size: %2px; background: transparent;").arg(FrendsTheme::css(theme.textMuted)).arg(smallFont));
    }

    const auto plateStyle = QStringLiteral(
                                "QPushButton {"
                                "  background: %1;"
                                "  color: %2;"
                                "  border: 1px solid %3;"
                                "  border-radius: 4px;"
                                "  padding: 3px 12px;"
                                "  font-size: %4px;"
                                "}"
                                "QPushButton:hover { background: %5; }"
                                "QPushButton:pressed { background: %6; }"
                                "QPushButton:disabled { color: %7; border-color: %8; }")
                                .arg(FrendsTheme::css(theme.buttonPlate))
                                .arg(theme.text.name())
                                .arg(FrendsTheme::css(theme.buttonPlateBorder))
                                .arg(smallFont)
                                .arg(FrendsTheme::css(theme.buttonPlate.lighter(135)))
                                .arg(FrendsTheme::css(theme.buttonPlate.darker(115)))
                                .arg(FrendsTheme::css(theme.textMuted))
                                .arg(FrendsTheme::css(theme.tileBorder));

    const auto accentStyle = QStringLiteral(
                                 "QPushButton {"
                                 "  background: %1;"
                                 "  color: %2;"
                                 "  border: 1px solid %3;"
                                 "  border-radius: 4px;"
                                 "  padding: 3px 14px;"
                                 "  font-size: %4px;"
                                 "  font-weight: 700;"
                                 "}"
                                 "QPushButton:hover { background: %5; }"
                                 "QPushButton:pressed { background: %6; }")
                                 .arg(theme.navActiveTop.name())
                                 .arg(theme.textOnAccent.name())
                                 .arg(theme.navActiveBottom.name())
                                 .arg(smallFont)
                                 .arg(theme.navActiveTop.lighter(115).name())
                                 .arg(theme.navActiveBottom.darker(105).name());

    for (auto* button : { m_topRefresh, m_mineRefresh, m_removeButton }) {
        button->setStyleSheet(plateStyle);
        button->setFixedHeight(buttonHeight);
    }
    m_addButton->setStyleSheet(accentStyle);
    m_addButton->setFixedHeight(buttonHeight);

    const auto fieldStyle = QStringLiteral(
                                "QLineEdit {"
                                "  background: %1;"
                                "  color: %2;"
                                "  border: 1px solid %3;"
                                "  border-radius: 4px;"
                                "  padding: 4px 8px;"
                                "  font-size: %4px;"
                                "}"
                                "QLineEdit:focus { border-color: %5; }")
                                .arg(FrendsTheme::css(theme.field))
                                .arg(theme.text.name())
                                .arg(FrendsTheme::css(theme.fieldBorder))
                                .arg(bodyFont)
                                .arg(theme.navActiveBottom.name());

    m_addName->setStyleSheet(fieldStyle);
    m_addAddress->setStyleSheet(fieldStyle);
    m_addName->setFixedHeight(buttonHeight);
    m_addAddress->setFixedHeight(buttonHeight);

    const auto tableStyle =
        QStringLiteral(
            "QTableWidget { background: transparent; border: none; color: %1; font-size: %2px;"
            "  selection-background-color: %3; selection-color: %4; }"
            "QTableWidget::item { padding: %5px %6px; border-bottom: 1px solid %7; }"
            "QTableWidget::item:selected { background: %3; color: %4; }"
            "QHeaderView::section { background: transparent; color: %8; border: none;"
            "  border-bottom: 1px solid %7; padding: %5px %6px; font-size: %9px; font-weight: 700; }")
            .arg(theme.text.name())
            .arg(bodyFont)
            .arg(FrendsTheme::css(theme.dark ? QColor(255, 217, 74, 60) : QColor(249, 217, 0, 110)))
            .arg(theme.text.name())
            .arg(FrendsScale::px(5, this))
            .arg(FrendsScale::px(6, this))
            .arg(FrendsTheme::css(theme.tileBorder))
            .arg(FrendsTheme::css(theme.textMuted))
            .arg(smallFont);

    for (auto* table : { m_topTable, m_mineTable }) {
        table->setStyleSheet(tableStyle);
        table->verticalHeader()->setDefaultSectionSize(FrendsScale::px(28, this));
        table->horizontalHeader()->setFixedHeight(FrendsScale::px(26, this));
    }
}

void FrendsServersPage::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        // The launcher theme changed: the panels carry their colours in a
        // stylesheet, so they have to be handed the new ones.
        applyScale();
        restyle();
    }
}

void FrendsServersPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_started)
        return;
    m_started = true;
    refresh(true);
    m_timer->start();
}

void FrendsServersPage::refresh(bool force)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // Opening the page twice in a row must not re-ping everything.
    if (!force && m_lastRefreshMs != 0 && now - m_lastRefreshMs < 60 * 1000)
        return;
    m_lastRefreshMs = now;

    m_pending = m_catalogue.size() + m_mine.size();

    for (const auto& entry : m_catalogue)
        ping(entry.address);
    for (const auto& entry : m_mine)
        ping(entry.address);

    if (m_pending == 0) {
        rebuildTop();
        rebuildMine();
        return;
    }

    updateTopStatus();

    // A server that never answers must not freeze the list, so the tables are
    // rebuilt once the deadline passes even if some pings are still out.
    QTimer::singleShot(8000, this, [this] {
        if (m_pending > 0) {
            m_pending = 0;
            rebuildTop();
            rebuildMine();
        }
    });
}

void FrendsServersPage::ping(const QString& address)
{
    QString host;
    int port = DEFAULT_PORT;
    splitAddress(address, host, port);

    auto* timer = new QElapsedTimer();
    timer->start();

    auto* resolver = new McResolver(this, host, port);
    connect(resolver, &McResolver::succeeded, this, [this, address, host, timer](QString ip, int resolvedPort) {
        auto* client = new McClient(this, host, ip, static_cast<uint16_t>(resolvedPort));

        connect(client, &McClient::succeeded, this, [this, address, timer](QJsonObject data) {
            Status status;
            status.answered = true;
            status.online = true;

            const auto players = data.value("players").toObject();
            status.players = players.value("online").toInt();
            status.capacity = players.value("max").toInt();
            status.version = data.value("version").toObject().value("name").toString();
            status.motd = motdFromJson(data.value("description"));
            status.latency = timer->elapsed();

            delete timer;
            onPinged(address, status);
        });
        connect(client, &McClient::failed, this, [this, address, timer](QString) {
            Status status;
            status.answered = true;
            delete timer;
            onPinged(address, status);
        });
        connect(client, &McClient::finished, client, &QObject::deleteLater);

        // Only the status handshake is measured: the SRV/A lookup above is DNS
        // latency, not server latency, and it used to inflate the numbers.
        timer->restart();
        client->getStatusData();
    });
    connect(resolver, &McResolver::failed, this, [this, address, timer](QString) {
        Status status;
        status.answered = true;
        delete timer;
        onPinged(address, status);
    });
    connect(resolver, &McResolver::finished, resolver, &QObject::deleteLater);

    resolver->ping();
}

void FrendsServersPage::onPinged(const QString& address, const Status& status)
{
    m_status.insert(address, status);

    if (m_pending > 0)
        m_pending--;
    if (m_pending == 0) {
        rebuildTop();
        rebuildMine();
    }
}

void FrendsServersPage::rebuildTop()
{
    struct Row {
        FrendsServerEntry entry;
        Status status;
    };

    QList<Row> rows;
    rows.reserve(m_catalogue.size());
    for (const auto& entry : m_catalogue)
        rows.append(Row{ entry, m_status.value(entry.address) });

    // Online first, most players first; anything unanswered sinks to the bottom
    // instead of disappearing, so the player can see the server exists.
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        const bool aOnline = a.status.online;
        const bool bOnline = b.status.online;
        if (aOnline != bOnline)
            return aOnline;
        if (a.status.players != b.status.players)
            return a.status.players > b.status.players;
        return a.entry.name < b.entry.name;
    });

    if (rows.size() > TOP_SIZE)
        rows = rows.mid(0, TOP_SIZE);

    m_topView.clear();
    m_topTable->setRowCount(rows.size());

    for (int row = 0; row < rows.size(); row++) {
        const auto& entry = rows.at(row).entry;
        const auto& status = rows.at(row).status;
        m_topView.append(entry);

        auto* rank = new QTableWidgetItem(QString::number(row + 1));
        rank->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_topTable->setItem(row, TOP_RANK, rank);

        auto* name = new QTableWidgetItem(entry.name);
        name->setToolTip(QStringLiteral("%1\n%2").arg(entry.address, status.motd));
        m_topTable->setItem(row, TOP_NAME, name);

        auto* players = new QTableWidgetItem(!status.answered ? QStringLiteral("…")
                                                 : status.online ? formatCount(status.players)
                                                                 : QStringLiteral("—"));
        players->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_topTable->setItem(row, TOP_PLAYERS, players);

        auto* latency = new QTableWidgetItem(!status.answered ? QStringLiteral("…")
                                                 : status.online ? tr("%1 мс").arg(status.latency)
                                                                 : tr("офлайн"));
        latency->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_topTable->setItem(row, TOP_PING, latency);
    }

    restyle();
    updateTopStatus();
}

void FrendsServersPage::rebuildMine()
{
    m_mineTable->setRowCount(m_mine.size());

    for (int row = 0; row < m_mine.size(); row++) {
        const auto& entry = m_mine.at(row);
        const auto status = m_status.value(entry.address);

        auto* name = new QTableWidgetItem(entry.name);
        name->setToolTip(status.motd.isEmpty() ? entry.address : status.motd);
        m_mineTable->setItem(row, MINE_NAME, name);

        auto* address = new QTableWidgetItem(entry.address);
        m_mineTable->setItem(row, MINE_ADDRESS, address);

        auto* players = new QTableWidgetItem(!status.answered ? QStringLiteral("…")
                                                 : status.online ? QStringLiteral("%1 / %2").arg(formatCount(status.players), formatCount(status.capacity))
                                                                 : QStringLiteral("—"));
        players->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_mineTable->setItem(row, MINE_PLAYERS, players);

        auto* latency = new QTableWidgetItem(!status.answered ? QStringLiteral("…")
                                                 : status.online ? tr("%1 мс").arg(status.latency)
                                                                 : tr("офлайн"));
        latency->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_mineTable->setItem(row, MINE_PING, latency);
    }

    restyle();
    updateMineStatus();
}

void FrendsServersPage::restyle()
{
    const auto& theme = FrendsTheme::colors();

    // Colours per row carry meaning, so they are applied after every rebuild.
    for (int row = 0; row < m_topTable->rowCount(); row++) {
        const auto status = m_status.value(m_topView.value(row).address);
        const QColor colour = !status.answered ? theme.textMuted
                              : status.online ? QColor(0x6F, 0xC7, 0x4B)
                                              : QColor(0xE0, 0x6C, 0x5A);
        if (auto* item = m_topTable->item(row, TOP_PING))
            item->setForeground(colour);
        if (auto* item = m_topTable->item(row, TOP_PLAYERS))
            item->setForeground(status.online ? theme.text : theme.textMuted);
    }

    for (int row = 0; row < m_mineTable->rowCount(); row++) {
        const auto status = m_status.value(m_mine.at(row).address);
        const QColor colour = !status.answered ? theme.textMuted
                              : status.online ? QColor(0x6F, 0xC7, 0x4B)
                                              : QColor(0xE0, 0x6C, 0x5A);
        if (auto* item = m_mineTable->item(row, MINE_PING))
            item->setForeground(colour);
        if (auto* item = m_mineTable->item(row, MINE_ADDRESS))
            item->setForeground(theme.dark ? QColor(0x6F, 0xB6, 0xF2) : QColor(0x00, 0x71, 0xBC));
    }
}

void FrendsServersPage::updateTopStatus()
{
    if (m_pending > 0) {
        m_topHint->setText(tr("Обновляю…"));
    } else {
        m_topHint->setText(tr("Каждые %1 мин").arg(REFRESH_MINUTES));
    }
}

void FrendsServersPage::updateMineStatus()
{
    const bool empty = m_mine.isEmpty();
    m_mineHint->setText(empty ? tr("Список пуст") : tr("Серверов: %1").arg(m_mine.size()));
    m_removeButton->setEnabled(!empty && m_mineTable->currentRow() >= 0);
}

QString FrendsServersPage::selectedAddress(QTableWidget* table, const QList<FrendsServerEntry>& entries) const
{
    const int row = table->currentRow();
    if (row < 0 || row >= entries.size())
        return QString();
    return entries.at(row).address;
}

void FrendsServersPage::loadMine()
{
    m_mine.clear();
    if (!APPLICATION)
        return;

    auto settings = APPLICATION->settings();
    settings->getOrRegisterSetting("FrendsMyServers", QString());
    const auto raw = settings->get("FrendsMyServers").toString();
    if (raw.isEmpty())
        return;

    const auto document = QJsonDocument::fromJson(raw.toUtf8());
    for (const auto& value : document.array()) {
        const auto object = value.toObject();
        const auto address = object.value("address").toString().trimmed();
        if (address.isEmpty())
            continue;
        auto name = object.value("name").toString().trimmed();
        if (name.isEmpty())
            name = address;
        m_mine.append(FrendsServerEntry{ name, address });
    }
}

void FrendsServersPage::saveMine()
{
    if (!APPLICATION)
        return;

    QJsonArray array;
    for (const auto& entry : m_mine) {
        QJsonObject object;
        object.insert("name", entry.name);
        object.insert("address", entry.address);
        array.append(object);
    }

    APPLICATION->settings()->set("FrendsMyServers",
                                 QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

void FrendsServersPage::addMine()
{
    const auto address = m_addAddress->text().trimmed();
    if (address.isEmpty()) {
        m_addAddress->setFocus();
        return;
    }

    for (const auto& entry : m_mine) {
        if (entry.address.compare(address, Qt::CaseInsensitive) == 0) {
            m_addAddress->clear();
            return;
        }
    }

    auto name = m_addName->text().trimmed();
    if (name.isEmpty())
        name = address;

    m_mine.append(FrendsServerEntry{ name, address });
    saveMine();

    m_addName->clear();
    m_addAddress->clear();

    rebuildMine();
    ping(address);
    m_pending++;
}

void FrendsServersPage::removeMine()
{
    const int row = m_mineTable->currentRow();
    if (row < 0 || row >= m_mine.size())
        return;

    m_mine.removeAt(row);
    saveMine();
    rebuildMine();
}
