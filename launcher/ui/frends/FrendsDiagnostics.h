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

#include <QString>

/**
 * Everything the launcher is able to learn about the machine it runs on.
 *
 * The original TLauncher sends the same set of values (CPU, GPU, RAM, OS
 * build, plus two stable installation identifiers) to its statistics
 * endpoints. Frends Launcher never sends them anywhere: the report below is
 * built locally and is only ever shown to the user or copied by hand.
 */
namespace FrendsDiagnostics {
/// Stable random identifier of this installation, created on first use.
QString installId();

/// Generates a new installation identifier and returns it.
QString resetInstallId();

/// Human readable environment report, one "key: value" pair per line.
QString fingerprintReport();

/// True when the user asked to keep a local (never uploaded) launch counter.
bool localStatisticsEnabled();

/// Records a launch for the local counter. Does nothing when statistics are off.
void noteLaunch();

/// Local launch counter value ("запусков: N").
int localLaunchCount();
}  // namespace FrendsDiagnostics
