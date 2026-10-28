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

#include "TLauncherPage.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QDebug>

#include "Application.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "ui/frends/InstanceCreationSink.h"

TLauncherPage::TLauncherPage(InstanceCreationSink* sink, QWidget* parent) : QWidget(parent), m_sink(sink)
{
    auto* layout = new QVBoxLayout(this);

    auto* intro = new QLabel(tr("Укажите папку <b>versions</b> от TLauncher или официального лаунчера. "
                                "Сборка будет пересоздана как обычная версия лаунчера, а моды, конфиги и "
                                "сохранения перенесутся."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* pathRow = new QHBoxLayout();
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("Путь к папке versions"));
    auto* browseButton = new QPushButton(tr("Обзор…"), this);
    pathRow->addWidget(m_pathEdit, 1);
    pathRow->addWidget(browseButton);
    layout->addLayout(pathRow);

    auto* reloadButton = new QPushButton(tr("Показать версии"), this);
    layout->addWidget(reloadButton);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint);

    connect(browseButton, &QPushButton::clicked, this, &TLauncherPage::browse);
    connect(reloadButton, &QPushButton::clicked, this, &TLauncherPage::reload);
    connect(m_pathEdit, &QLineEdit::returnPressed, this, &TLauncherPage::reload);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &TLauncherPage::onSelectionChanged);

    setFolder(defaultVersionsDir());
}

QString TLauncherPage::defaultVersionsDir() const
{
    // Both TLauncher and the official launcher keep their versions in
    // %APPDATA%\.minecraft\versions. QStandardPaths::GenericDataLocation points
    // at AppData\Local on Windows, which is the wrong branch, so the roaming
    // folder is read straight from the environment first.
    QStringList candidates;
#ifdef Q_OS_WIN
    const auto appData = qEnvironmentVariable("APPDATA");
    if (!appData.isEmpty())
        candidates.append(QDir(appData).absoluteFilePath(QStringLiteral(".minecraft/versions")));
#endif
    candidates.append(QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                          .absoluteFilePath(QStringLiteral(".minecraft/versions")));
    candidates.append(QDir::home().absoluteFilePath(QStringLiteral(".minecraft/versions")));

    for (const auto& candidate : candidates) {
        if (QDir(candidate).exists())
            return QDir::toNativeSeparators(candidate);
    }
    return candidates.isEmpty() ? QString() : QDir::toNativeSeparators(candidates.first());
}

void TLauncherPage::retranslate()
{
    // The page is built in code, so there is no generated retranslateUi() to call.
}

void TLauncherPage::openedImpl()
{
    if (m_list->count() == 0)
        reload();
}

void TLauncherPage::browse()
{
    const auto dir = QFileDialog::getExistingDirectory(this, tr("Папка versions"), m_pathEdit->text());
    if (!dir.isEmpty()) {
        setFolder(dir);
        reload();
    }
}

void TLauncherPage::setFolder(const QString& dir)
{
    m_pathEdit->setText(QDir::toNativeSeparators(dir));
}

void TLauncherPage::reload()
{
    m_list->clear();
    m_hint->clear();

    const auto dir = QDir::fromNativeSeparators(m_pathEdit->text()).trimmed();
    const auto folders = TLauncherImportTask::listVersionFolders(dir);
    if (folders.isEmpty()) {
        m_hint->setText(tr("В этой папке нет версий. Выберите папку <b>versions</b> внутри .minecraft."));
        if (m_sink)
            m_sink->setSuggestedPack();
        return;
    }

    // The base game version is validated against the launcher's own metadata, so
    // a stray "1.0.1" inside a library name cannot be mistaken for the game.
    QStringList knownVersions;
    if (auto list = APPLICATION->metadataIndex()->get("net.minecraft")) {
        list->waitToLoad();
        for (const auto& version : list->versions())
            knownVersions.append(version->descriptor());
    }

    int importable = 0;
    for (const auto& folder : folders) {
        const auto info = TLauncherImportTask::inspect(folder, knownVersions);

        QString label = info.id;
        if (info.isImportable()) {
            ++importable;
            label += QStringLiteral("  —  %1").arg(info.minecraftVersion);
            if (!info.loaderUid.isEmpty())
                label += QStringLiteral(" + %1 %2").arg(info.loaderUid.section(QLatin1Char('.'), -1), info.loaderVersion);
        } else {
            label += QStringLiteral("  —  ") + tr("не импортируется");
        }

        auto* item = new QListWidgetItem(label, m_list);
        item->setData(Qt::UserRole, folder);
        item->setData(Qt::UserRole + 1, info.isImportable());
        item->setToolTip(info.isImportable() ? tr("Определено по: %1").arg(info.detectedBy) : info.problem);
        if (!info.isImportable())
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }

    m_hint->setText(tr("Найдено версий: %1, из них можно импортировать: %2.").arg(folders.size()).arg(importable));

    if (importable == 0 && m_sink)
        m_sink->setSuggestedPack();
}

void TLauncherPage::onSelectionChanged()
{
    if (!m_sink)
        return;

    auto* item = m_list->currentItem();
    if (!item || !item->data(Qt::UserRole + 1).toBool()) {
        m_sink->setSuggestedPack();
        return;
    }

    const auto folder = item->data(Qt::UserRole).toString();

    QStringList knownVersions;
    if (auto list = APPLICATION->metadataIndex()->get("net.minecraft")) {
        list->waitToLoad();
        for (const auto& version : list->versions())
            knownVersions.append(version->descriptor());
    }

    const auto info = TLauncherImportTask::inspect(folder, knownVersions);
    if (!info.isImportable()) {
        m_sink->setSuggestedPack();
        return;
    }

    // The name is what the player will see in the instance list, so keep the
    // original folder name - it is usually a meaningful pack title.
    m_sink->setSuggestedPack(info.id, new TLauncherImportTask(info, folder));
    m_sink->setSuggestedIcon("default");
}
