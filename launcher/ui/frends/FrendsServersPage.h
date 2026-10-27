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

#include <QList>
#include <QString>
#include <QWidget>

class QJsonObject;
class QLabel;
class QPushButton;
class QTableWidget;

struct FrendsServerEntry {
    QString name;
    QString address;
    QString note;
};

/**
 * The "Серверы" page.
 *
 * The original TLauncher ships a partner server list (InnerStateServer-1.3.json)
 * where every entry carries "hideAddress": true, so the address is never shown
 * and the placement is paid for. This page keeps the same idea but shows the
 * real address of every server and never reorders the list for money.
 */
class FrendsServersPage : public QWidget {
    Q_OBJECT

   public:
    explicit FrendsServersPage(QWidget* parent = nullptr);

    /// Recomputes every pixel metric from FrendsScale::factor().
    void applyScale();
    void retranslate();

   public slots:
    /// Re-pings every server in the list.
    void refresh();

   signals:
    /// Emitted when the user asks to join a server with the selected instance.
    void playRequested(const QString& address);

   private:
    void pingRow(int row);
    void onPingSucceeded(int row, const QJsonObject& data, qint64 latencyMs);
    void onPingFailed(int row);
    void updateStatus();
    QString selectedAddress() const;

    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_legend = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_playButton = nullptr;
    QTableWidget* m_table = nullptr;

    QList<FrendsServerEntry> m_servers;
    int m_pending = 0;
    int m_checked = 0;
};
