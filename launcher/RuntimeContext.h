// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include <QSet>
#include <QString>
#include <QSysInfo>
#include "SysInfo.h"
#include "settings/SettingsObject.h"

struct RuntimeContext {
    QString javaArchitecture;
    QString javaRealArchitecture;
    QString system = SysInfo::currentSystem();

    QString mappedJavaRealArchitecture() const
    {
        // The instance only learns which Java it will use once Java has been
        // detected, but the profile is parsed before that as well. Without a
        // fallback the classifier would come out as "windows-" (no match for
        // any rule), every OS-restricted library would be dropped from the
        // profile, and the launcher would never download the natives.
        const QString arch = javaRealArchitecture.isEmpty() ? QSysInfo::currentCpuArchitecture() : javaRealArchitecture;
        if (arch == "amd64")
            return "x86_64";
        if (arch == "i386" || arch == "i686")
            return "x86";
        if (arch == "aarch64")
            return "arm64";
        if (arch == "arm" || arch == "armhf")
            return "arm32";
        return arch;
    }

    void updateFromInstanceSettings(SettingsObject* instanceSettings)
    {
        javaArchitecture = instanceSettings->get("JavaArchitecture").toString();
        javaRealArchitecture = instanceSettings->get("JavaRealArchitecture").toString();
    }

    QString getClassifier() const { return system + "-" + mappedJavaRealArchitecture(); }

    // "Legacy" refers to the fact that Mojang assumed that these are the only two architectures
    bool isLegacyArch() const
    {
        const QString mapped = mappedJavaRealArchitecture();
        return mapped == "x86_64" || mapped == "x86";
    }

    bool classifierMatches(QString target) const
    {
        // try to match precise classifier "[os]-[arch]"
        bool x = target == getClassifier();
        // try to match imprecise classifier on legacy architectures "[os]".
        // Only a bare OS name may fall back: a target that names an architecture
        // ("windows-x86") must not match plain "windows" on a 64-bit system, or
        // the 32-bit natives get pulled in next to the 64-bit ones.
        if (!x && isLegacyArch() && !target.contains(QLatin1Char('-')))
            x = target == system;

        return x;
    }
};
