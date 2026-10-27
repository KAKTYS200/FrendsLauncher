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

#include <memory>

#include <QColor>
#include <QIcon>
#include <QVector>
#include <QWidget>

#include "ui/frends/InstanceCreationSink.h"
#include "ui/pages/BasePage.h"

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
class CustomPage;
class InstanceTask;
class ProviderTile;

/**
 * The "Сборки" page of the main window.
 *
 * A build can either be made from scratch (pick a Minecraft version) or taken
 * from one of the modpack platforms. The sources are picked with large cards
 * instead of a dropdown, so the catalogs are visible at a glance — CurseForge
 * in particular gets a coloured card of its own.
 *
 * Nothing is a separate window: the page lives in the content stack next to
 * the instance list and the servers page, and the install runs in place.
 */
class FrendsCreatePage : public QWidget, public InstanceCreationSink {
    Q_OBJECT

   public:
    explicit FrendsCreatePage(QWidget* parent = nullptr);

    void applyScale();
    void retranslate();

    /// Clears the form so the next build starts from a clean state.
    void resetForm();

    void setSuggestedPack(const QString& name = QString(), InstanceTask* task = nullptr) override;
    void setSuggestedPack(const QString& name, QString version, InstanceTask* task = nullptr) override;
    void setSuggestedIcon(const QString& key) override;
    void setSuggestedIconFromFile(const QString& path, const QString& name) override;
    QString instName() const override;
    void acceptCreation() override;
    void rejectCreation() override;

   signals:
    /// The user finished or cancelled; the window should show the instance list.
    void dismissed();
    /// A build is fully assembled and ready to be created by MainWindow.
    void taskReady(InstanceTask* task);
    /// CurseForge needs its API key, and the player asked to open the settings.
    void openApiSettingsRequested();

   private slots:
    void selectProvider(int index);

   private:
    bool usesOwnForm() const;
    void updateInstallButton();
    void addProviderTile(const QString& title, const QString& subtitle, const QColor& accent, bool highlighted,
                         const QIcon& icon = QIcon());
    QWidget* buildCurseForgeStub();

    QVBoxLayout* m_layout = nullptr;
    QHBoxLayout* m_tileRow = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_hint = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QPushButton* m_installButton = nullptr;
    QPushButton* m_cancelButton = nullptr;

    CustomPage* m_form = nullptr;
    /// The page behind each card; null where a card has none of its own.
    QVector<BasePage*> m_providerPage;
    /// One card per source, index for index with the stack.
    QVector<ProviderTile*> m_tiles;
    int m_currentProvider = 0;

    std::unique_ptr<InstanceTask> m_task;
    QString m_iconKey;
    QString m_group;
    QString m_suggestedVersion;
    QString m_iconPath;
    QString m_iconName;
};
