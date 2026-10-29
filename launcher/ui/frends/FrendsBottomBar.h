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

#include <QWidget>

class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QProgressBar;
class QPushButton;
class QToolButton;

class FrendsBottomBar : public QWidget {
    Q_OBJECT

   public:
    explicit FrendsBottomBar(QWidget* parent = nullptr);

    QComboBox* accountCombo() const { return m_accountCombo; }
    QComboBox* instanceCombo() const { return m_instanceCombo; }
    QCheckBox* updateCheckBox() const { return m_updateCheck; }

    void setRunning(bool running);
    void setPlayEnabled(bool enabled);
    void retranslate();

    /// Recomputes every pixel metric from FrendsScale::factor().
    void applyScale();

    /// The download strip above the control row. While a build is being
    /// downloaded the strip fills from left to right; without it the panel
    /// looks exactly as before.
    void showDownloadProgress(const QString& label);
    void setDownloadProgress(qint64 current, qint64 total);
    void hideDownloadProgress();

   signals:
    void playRequested();
    void refreshRequested();
    void folderRequested();
    void settingsRequested();
    void modsRequested();
    /// The + next to the account selector: add another account.
    void addAccountRequested();
    /// The one-click launcher update button was pressed.
    void launcherUpdateRequested();
    void accountActivated(int index);
    void instanceActivated(const QString& id);
    /// The cancel button of the download strip was pressed.
    void downloadCancelled();

   protected:
    void paintEvent(QPaintEvent* event) override;

   private:
    void styleWidgets(qreal scale);

    QHBoxLayout* m_layout = nullptr;
    QToolButton* m_addAccountButton = nullptr;
    QComboBox* m_accountCombo = nullptr;
    QComboBox* m_instanceCombo = nullptr;
    QCheckBox* m_updateCheck = nullptr;
    QPushButton* m_playButton = nullptr;
    QPushButton* m_modsButton = nullptr;
    QToolButton* m_refreshButton = nullptr;
    QToolButton* m_launcherUpdateButton = nullptr;
    QToolButton* m_folderButton = nullptr;
    QToolButton* m_settingsButton = nullptr;
    QProgressBar* m_downloadBar = nullptr;
    QLabel* m_downloadLabel = nullptr;
    QPushButton* m_downloadCancel = nullptr;
    /// Holds the download strip. Hidden as a whole, so its margins do not keep
    /// eating height when nothing is downloading.
    QWidget* m_downloadStrip = nullptr;
    bool m_running = false;
};
