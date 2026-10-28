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

#include <QList>
#include <QStringList>
#include <QWidget>

class QLabel;
class QPushButton;
class QSpacerItem;
class QVBoxLayout;

class FrendsSidebar : public QWidget {
    Q_OBJECT

   public:
    explicit FrendsSidebar(QWidget* parent = nullptr);

    void retranslate();

    /// Recomputes every pixel metric from FrendsScale::factor().
    void applyScale();

    /// Highlights the entry that belongs to the page currently on screen.
    /// Pages that are not part of the content stack (dialogs, external
    /// links) leave the highlight where it was.
    void setActivePage(const QString& page);

   signals:
    void navigationRequested(const QString& page);

   protected:
    void paintEvent(QPaintEvent* event) override;

   private:
    QPushButton* addNavigationButton(const QString& page);
    void setNavigationTexts();
    QString navigationButtonStyle(qreal scale) const;
    QString activeNavigationButtonStyle(qreal scale) const;
    void updateActivePageStyles();

    QVBoxLayout* m_layout = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QLabel* m_version = nullptr;
    QSpacerItem* m_spacerTop = nullptr;
    QSpacerItem* m_spacerBottom = nullptr;
    QList<QPushButton*> m_navButtons;
    /// Page key each navigation button stands for, in the order they were added.
    QStringList m_navPages;
    QString m_activePage;

    QPushButton* m_newsButton = nullptr;
    QPushButton* m_instancesButton = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_serversButton = nullptr;
    QPushButton* m_accountsButton = nullptr;
    QPushButton* m_settingsButton = nullptr;
    QPushButton* m_logsButton = nullptr;
    QPushButton* m_aboutButton = nullptr;
};
