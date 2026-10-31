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

#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QWidget>

class QEvent;
class QJsonObject;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTimer;

struct FrendsServerEntry {
    QString name;
    QString address;
};

/**
 * The "Серверы" page.
 *
 * Two panels side by side. The left one ranks public servers by the number of
 * players online and re-pings them every ten minutes; the right one is the
 * player's own list, which they can add to and remove from. Both panels show
 * the real address of every server: there is no paid placement here.
 */
class FrendsServersPage : public QWidget {
    Q_OBJECT

   public:
    explicit FrendsServersPage(QWidget* parent = nullptr);

    /// Recomputes every pixel metric from FrendsScale::factor().
    void applyScale();
    void retranslate();

    /// How often the top list re-pings its servers.
    static constexpr int REFRESH_MINUTES = 10;

   public slots:
    /// Re-pings both lists. Without `force` a refresh that follows closely on
    /// the previous one is skipped, so flicking between pages costs nothing.
    void refresh(bool force = false);

   signals:
    /// Emitted when the player asks to join a server with the selected build.
    void playRequested(const QString& address);

   protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

   private:
    /// What a status ping told us about one address.
    struct Status {
        bool answered = false;
        bool online = false;
        int players = 0;
        int capacity = 0;
        QString version;
        QString motd;
        qint64 latency = -1;
    };

    void buildPanels();
    QWidget* makePanel(const QString& title, QWidget** headerOut, QWidget** bodyOut, QLabel** titleOut, QLabel** hintOut);

    void ping(const QString& address);
    void onPinged(const QString& address, const Status& status);

    void rebuildTop();
    void rebuildMine();
    void loadMine();
    void saveMine();
    void addMine();
    void removeMine();

    void updateTopStatus();
    void updateMineStatus();
    void restyle();

    QString selectedAddress(QTableWidget* table, const QList<FrendsServerEntry>& entries) const;

    // Panels
    QWidget* m_topPanel = nullptr;
    QWidget* m_minePanel = nullptr;
    QLabel* m_topTitle = nullptr;
    QLabel* m_mineTitle = nullptr;
    QLabel* m_topHint = nullptr;
    QLabel* m_mineHint = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;

    QPushButton* m_topRefresh = nullptr;
    QPushButton* m_mineRefresh = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QLineEdit* m_addName = nullptr;
    QLineEdit* m_addAddress = nullptr;

    QTableWidget* m_topTable = nullptr;
    QTableWidget* m_mineTable = nullptr;

    QTimer* m_timer = nullptr;
    bool m_started = false;
    /// Guards applyScale() against re-entering itself: setting the panel
    /// stylesheets sends StyleChange, which calls back into changeEvent().
    bool m_applyingScale = false;
    /// A restyle is already queued for the next event loop pass.
    bool m_restylePending = false;

    /// The catalogue that feeds the ranking, and the player's own list.
    QList<FrendsServerEntry> m_catalogue;
    QList<FrendsServerEntry> m_mine;
    /// The ranking as currently displayed, so a row index maps back to a server.
    QList<FrendsServerEntry> m_topView;

    /// Latest ping result per address; the ranking is sorted from this.
    QHash<QString, Status> m_status;
    /// Pings still running; the tables are rebuilt when this reaches zero.
    int m_pending = 0;

    qint64 m_lastRefreshMs = 0;
};
