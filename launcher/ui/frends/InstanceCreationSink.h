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

class InstanceTask;

/**
 * What the "create a build" form needs from whoever hosts it.
 *
 * The form itself (CustomPage) only assembles an InstanceTask; whether the
 * result ends up in a modal dialog or in a page inside the main window is the
 * host's business. Both NewInstanceDialog and FrendsCreatePage implement this,
 * which is what lets the very same form be shown either way.
 */
class InstanceCreationSink {
   public:
    virtual ~InstanceCreationSink() = default;

    /// Hands the assembled task over; a null task means "nothing valid chosen".
    virtual void setSuggestedPack(const QString& name = QString(), InstanceTask* task = nullptr) = 0;
    virtual void setSuggestedPack(const QString& name, QString version, InstanceTask* task = nullptr) = 0;
    virtual void setSuggestedIcon(const QString& key) = 0;
    /// A platform provided an icon of its own (a modpack logo, usually).
    virtual void setSuggestedIconFromFile(const QString& path, const QString& name) = 0;

    virtual QString instName() const = 0;

    /// The user confirmed the form.
    virtual void acceptCreation() = 0;
    /// The user cancelled the form.
    virtual void rejectCreation() = 0;
};
