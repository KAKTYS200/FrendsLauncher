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

#include <QStringList>

/**
 * Updating the launcher from GitHub, without a separate updater binary.
 *
 * Windows will not let a running executable (or its Qt DLLs) be overwritten, so
 * the new build cannot simply unpack itself over the old one. Instead the new
 * build is started from the temporary folder it was unpacked into, and it is
 * that copy which overwrites the installed files and restarts the launcher.
 */
namespace FrendsSelfUpdate {

/// The argument the freshly unpacked build is started with.
inline constexpr auto kApplyFlag = "--frends-apply-update";

/**
 * Copies the running build over `targetDir` and starts it from there.
 *
 * Expects `kApplyFlag`, the install directory and the directory holding the new
 * build as arguments. Both paths are passed explicitly because this runs before
 * QCoreApplication exists, and its path helpers return nothing at that point.
 *
 * Only does anything when the process was started with kApplyFlag; the return
 * value says whether that was the case, in which case the caller must exit
 * instead of showing the launcher.
 */
bool runUpdateHelperIfRequested(const QStringList& arguments);

/// Deletes unpack folders left behind by earlier updates. Best effort.
void cleanupStaleUpdates();

/// True when `remote` (a release tag such as "v2.1") is newer than `current`.
bool isNewerVersion(const QString& remote, const QString& current);

}  // namespace FrendsSelfUpdate
