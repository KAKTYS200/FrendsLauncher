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

#include <QString>

#include "InstanceCreationTask.h"

/**
 * What a TLauncher-style version folder turned out to contain.
 *
 * TLauncher stores every version as a flat JSON with all libraries inlined, so
 * there is no `inheritsFrom` to follow. The base game version and the mod
 * loader have to be recovered from the contents instead.
 */
struct TLauncherVersionInfo {
    QString id;
    QString minecraftVersion;
    QString loaderUid;      // "net.minecraftforge" / "net.neoforged" / "net.fabricmc.fabric-loader" / empty
    QString loaderVersion;
    QString detectedBy;     // human-readable note about which signal was used
    QString problem;        // non-empty when the folder cannot be imported

    bool isImportable() const { return problem.isEmpty() && !minecraftVersion.isEmpty(); }
};

/**
 * Imports a version folder from a TLauncher (or plain vanilla launcher)
 * `versions/` directory as a normal instance.
 *
 * The instance is rebuilt from the official metadata: only the base version and
 * the loader are taken from the folder, everything else is downloaded again the
 * usual way. That way the result is a properly managed instance instead of a
 * pile of copied jars.
 */
class TLauncherImportTask final : public InstanceCreationTask {
    Q_OBJECT

   public:
    TLauncherImportTask(const TLauncherVersionInfo& info, QString versionFolder);

    /** Reads a version folder and works out what it is. Never throws. */
    static TLauncherVersionInfo inspect(const QString& versionFolder, const QStringList& knownMinecraftVersions);

    /** Lists the version folders inside a `versions/` directory. */
    static QStringList listVersionFolders(const QString& versionsDir);

    std::unique_ptr<MinecraftInstance> createInstance() override;

   private:
    TLauncherVersionInfo m_info;
    QString m_versionFolder;
};
