// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Octol1ttle <l1ttleofficial@outlook.com>
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

#include "EnsureOfflineContent.h"

#include <QFileInfo>

#include "minecraft/AssetsUtils.h"
#include "minecraft/PackProfile.h"
#include "minecraft/update/AssetUpdateTask.h"
#include "minecraft/update/LibrariesTask.h"

EnsureOfflineContent::EnsureOfflineContent(LaunchTask* parent, MinecraftInstance* instance) : LaunchStep(parent), m_instance(instance)
{}

QStringList EnsureOfflineContent::missingLibraries() const
{
    const auto profile = m_instance->getPackProfile()->getProfile();
    QStringList allJars;
    profile->getLibraryFiles(m_instance->runtimeContext(), allJars, allJars, m_instance->getLocalLibraryPath(), m_instance->binRoot(),
                             false);

    QStringList missing;
    for (const auto& jar : allJars) {
        if (!QFileInfo::exists(jar)) {
            missing.append(jar);
        }
    }
    return missing;
}

bool EnsureOfflineContent::assetsIncomplete() const
{
    auto assets = m_instance->getPackProfile()->getProfile()->getMinecraftAssets();
    if (!assets) {
        return false;
    }

    // AssetUpdateTask and AssetObject resolve their paths against the launcher's
    // working directory, so the check has to use the same base or the two would
    // disagree about which files exist.
    const QString indexPath = QStringLiteral("assets/indexes/%1.json").arg(assets->id);
    if (!QFileInfo::exists(indexPath)) {
        return true;
    }

    AssetsIndex index;
    if (!AssetsUtils::loadAssetsIndexJson(assets->id, indexPath, index)) {
        // An unreadable index is as good as a missing one - let the task refetch it.
        return true;
    }

    for (auto& object : index.objects) {
        if (!QFileInfo::exists(object.getLocalPath())) {
            return true;
        }
    }
    return false;
}

void EnsureOfflineContent::reportMissingLibraries(const QStringList& missing)
{
    emit logLine("Missing libraries:", MessageLevel::Error);
    for (const auto& jar : missing) {
        emit logLine("  " + jar, MessageLevel::Error);
    }
}

void EnsureOfflineContent::executeTask()
{
    const QStringList missing = missingLibraries();
    if (!missing.isEmpty()) {
        reportMissingLibraries(missing);
        m_pending.append(Part::Libraries);
    }

    if (assetsIncomplete()) {
        emit logLine(tr("Some game assets (textures, sounds, language files) are missing."), MessageLevel::Error);
        m_pending.append(Part::Assets);
    }

    if (m_pending.isEmpty()) {
        emitSucceeded();
        return;
    }

    emit logLine(tr("Downloading the missing game files..."), MessageLevel::Warning);
    startNextPart();
}

void EnsureOfflineContent::startNextPart()
{
    if (m_pending.isEmpty()) {
        finish();
        return;
    }

    const Part part = m_pending.takeFirst();
    // Assigned in two statements on purpose: the two task types share no common
    // type short of Task, so a conditional expression would not compile here.
    if (part == Part::Libraries) {
        m_current = makeShared<LibrariesTask>(m_instance);
    } else {
        m_current = makeShared<AssetUpdateTask>(m_instance);
    }

    connect(m_current.get(), &Task::succeeded, this, &EnsureOfflineContent::onPartSucceeded);
    connect(m_current.get(), &Task::failed, this, &EnsureOfflineContent::onPartFailed);
    connect(m_current.get(), &Task::progress, this, &Task::setProgress);
    m_current->start();
}

void EnsureOfflineContent::onPartSucceeded()
{
    m_current.reset();
    startNextPart();
}

void EnsureOfflineContent::onPartFailed(const QString& reason)
{
    m_current.reset();
    emit logLine(tr("\nЭту сборку нельзя запустить: файлы игры не скачаны, и скачать их не удалось.\n"
                    "Проверьте подключение к интернету и запустите снова — лаунчер попробует ещё раз.\n"
                    "Подробности: %1")
                     .arg(reason),
                 MessageLevel::Fatal);
    emitFailed(reason);
}

void EnsureOfflineContent::finish()
{
    // The downloads can succeed while still leaving gaps (a mirror serving the
    // wrong file, an index that lists an object it never sends), so verify the
    // result instead of trusting the tasks.
    const QStringList stillMissing = missingLibraries();
    const bool assetsStillIncomplete = assetsIncomplete();

    if (stillMissing.isEmpty() && !assetsStillIncomplete) {
        emitSucceeded();
        return;
    }

    if (!stillMissing.isEmpty()) {
        reportMissingLibraries(stillMissing);
    }
    if (assetsStillIncomplete) {
        emit logLine(tr("The game assets are still incomplete."), MessageLevel::Error);
    }

    emit logLine(tr("\nЭту сборку нельзя запустить: часть файлов игры не скачана.\n"
                    "Откройте сборку, вкладку «Версия» и нажмите «Скачать всё» — или поставьте галочку "
                    "«Обновить клиент» внизу и нажмите «Войти в игру». Нужен интернет."),
                 MessageLevel::Fatal);
    emitFailed("Required game files are missing");
}

bool EnsureOfflineContent::abort()
{
    return m_current ? m_current->abort() : false;
}
