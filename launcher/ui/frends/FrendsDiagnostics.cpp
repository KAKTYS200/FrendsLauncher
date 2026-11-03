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

#include "FrendsDiagnostics.h"

#include <QApplication>
#include <QCoreApplication>
#include <QScreen>
#include <QThread>
#include <QUuid>

#include "Application.h"
#include "BuildConfig.h"
#include "settings/SettingsObject.h"

#ifdef Q_OS_WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {
constexpr auto INSTALL_ID_KEY = "FrendsInstallId";
constexpr auto LOCAL_STATS_KEY = "FrendsLocalStatistics";
constexpr auto LOCAL_STATS_COUNT_KEY = "FrendsLocalLaunchCount";

QString newIdentifier()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

quint64 totalPhysicalMemoryMb()
{
#ifdef Q_OS_WIN32
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
        return status.ullTotalPhys / (1024ull * 1024ull);
#endif
    return 0;
}
}  // namespace

namespace FrendsDiagnostics {

QString installId()
{
    if (!APPLICATION)
        return QStringLiteral("-");

    auto settings = APPLICATION->settings();
    settings->getOrRegisterSetting(INSTALL_ID_KEY, QString());

    auto stored = settings->get(INSTALL_ID_KEY).toString();
    if (stored.isEmpty()) {
        stored = newIdentifier();
        settings->set(INSTALL_ID_KEY, stored);
    }
    return stored;
}

QString resetInstallId()
{
    const auto fresh = newIdentifier();
    if (APPLICATION) {
        auto settings = APPLICATION->settings();
        settings->getOrRegisterSetting(INSTALL_ID_KEY, QString());
        settings->set(INSTALL_ID_KEY, fresh);
    }
    return fresh;
}

bool localStatisticsEnabled()
{
    if (!APPLICATION)
        return false;

    auto settings = APPLICATION->settings();
    settings->getOrRegisterSetting(LOCAL_STATS_KEY, false);
    return settings->get(LOCAL_STATS_KEY).toBool();
}

void noteLaunch()
{
    if (!localStatisticsEnabled())
        return;

    auto settings = APPLICATION->settings();
    settings->getOrRegisterSetting(LOCAL_STATS_COUNT_KEY, 0);
    settings->set(LOCAL_STATS_COUNT_KEY, settings->get(LOCAL_STATS_COUNT_KEY).toInt() + 1);
}

int localLaunchCount()
{
    if (!APPLICATION)
        return 0;

    auto settings = APPLICATION->settings();
    settings->getOrRegisterSetting(LOCAL_STATS_COUNT_KEY, 0);
    return settings->get(LOCAL_STATS_COUNT_KEY).toInt();
}

QString fingerprintReport()
{
    QStringList lines;

    // The report is a free function, so it has no tr() of its own; the class
    // context has to be named explicitly or these lines never reach a .ts file.
    auto text = [](const char* source) { return QCoreApplication::translate("FrendsDiagnostics", source); };

    lines << text("Лаунчер: %1 %2").arg(BuildConfig.LAUNCHER_DISPLAYNAME, BuildConfig.printableVersionString());
    lines << text("Qt: %1 (собрано с %2)").arg(QString::fromLatin1(qVersion()), QString::fromLatin1(QT_VERSION_STR));
    lines << text("ОС: %1").arg(QSysInfo::prettyProductName());
    lines << text("Ядро: %1").arg(QSysInfo::kernelVersion());
    lines << text("Архитектура: %1 (%2-bit)").arg(QSysInfo::currentCpuArchitecture()).arg(QSysInfo::WordSize);
    lines << text("Логических ядер CPU: %1").arg(QThread::idealThreadCount());

    const auto ramMb = totalPhysicalMemoryMb();
    if (ramMb > 0)
        lines << text("Оперативная память: %1 МБ").arg(ramMb);

    lines << text("Разрешение экрана: %1x%2 @ %3x")
                 .arg(qApp->primaryScreen() ? qApp->primaryScreen()->geometry().width() : 0)
                 .arg(qApp->primaryScreen() ? qApp->primaryScreen()->geometry().height() : 0)
                 .arg(qApp->primaryScreen() ? qApp->primaryScreen()->devicePixelRatio() : 1.0);

    if (APPLICATION) {
        const auto javaPath = APPLICATION->settings()->get("JavaPath").toString();
        lines << text("Java: %1").arg(javaPath.isEmpty() ? text("выбирается автоматически") : javaPath);
    }

    lines << text("Идентификатор установки: %1").arg(installId());

    return lines.join(QLatin1Char('\n'));
}

}  // namespace FrendsDiagnostics
