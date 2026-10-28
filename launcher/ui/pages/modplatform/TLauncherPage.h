// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Frends Launcher Contributors
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

#include "TLauncherImportTask.h"
#include "ui/frends/InstanceCreationSink.h"
#include "ui/pages/BasePage.h"

class QLineEdit;
class QListWidget;
class QLabel;
class QPushButton;

/**
 * Lets the player point at a TLauncher `versions` folder and turn any version
 * from it into a normal instance.
 *
 * The list is filled from the folder itself, so it shows what is really there
 * instead of asking the player to type a name and hoping it exists.
 *
 * The host is an InstanceCreationSink rather than a concrete dialog: the same
 * page is used both by the modal NewInstanceDialog and by the in-window
 * FrendsCreatePage.
 */
class TLauncherPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit TLauncherPage(InstanceCreationSink* sink, QWidget* parent = nullptr);

    QString displayName() const override { return tr("Из TLauncher"); }
    QIcon icon() const override { return QIcon::fromTheme("view-refresh"); }
    QString id() const override { return "tlauncher"; }
    QString helpPage() const override { return QString(); }

    void openedImpl() override;
    void retranslate() override;

   private slots:
    void browse();
    void reload();
    void onSelectionChanged();

   private:
    QString defaultVersionsDir() const;
    void setFolder(const QString& dir);

    InstanceCreationSink* m_sink = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_hint = nullptr;
};
