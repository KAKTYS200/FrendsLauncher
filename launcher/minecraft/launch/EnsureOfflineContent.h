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

#pragma once

#include "launch/LaunchStep.h"
#include "minecraft/MinecraftInstance.h"

/**
 * Guarantees that everything the game reads at startup is actually on disk.
 *
 * A normal launch relies on the update tasks to fetch libraries and assets, but
 * an offline launch skips them entirely - so an incomplete download used to end
 * in "part of the libraries is not downloaded" with no way to recover from
 * inside the launcher. This step fills the gaps itself: it checks what is
 * missing and only then downloads that part. When everything is in place it does
 * nothing at all and adds no network round-trip.
 *
 * Both checks are needed because the two failures look completely different to
 * the player: missing libraries prevent the game from starting, while missing
 * assets let it start with a broken main menu (no panorama background, missing
 * sounds and fonts).
 */
class EnsureOfflineContent : public LaunchStep {
    Q_OBJECT

   public:
    explicit EnsureOfflineContent(LaunchTask* parent, MinecraftInstance* instance);
    ~EnsureOfflineContent() override = default;

    void executeTask() override;
    bool canAbort() const override { return m_current != nullptr; }
    bool abort() override;

   private:
    enum class Part {
        Libraries,
        Assets,
    };

    /// Every library file the profile needs and the disk does not have.
    QStringList missingLibraries() const;
    /// True when the assets index or any object it lists is absent.
    bool assetsIncomplete() const;

    void reportMissingLibraries(const QStringList& missing);
    void startNextPart();
    void onPartSucceeded();
    void onPartFailed(const QString& reason);
    void finish();

    MinecraftInstance* m_instance;
    QList<Part> m_pending;
    Task::Ptr m_current;
};
