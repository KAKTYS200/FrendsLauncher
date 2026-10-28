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

#include "FrendsSelfUpdate.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>

namespace {

constexpr auto kTempPrefix = "frends-update-";

/// Copies one file, retrying while the previous process still holds it open.
bool copyWithRetry(const QString& from, const QString& to)
{
    for (int attempt = 0; attempt < 40; ++attempt) {
        QFile::remove(to);
        if (QFile::copy(from, to))
            return true;
        // The old launcher may take a moment to release its handles.
        QThread::msleep(250);
    }
    return false;
}

/// Every file under `source`, relative to it.
QStringList filesIn(const QString& source)
{
    QStringList out;
    QDirIterator it(source, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        out.append(QDir(source).relativeFilePath(it.filePath()));
    }
    return out;
}

}  // namespace

namespace FrendsSelfUpdate {

bool isNewerVersion(const QString& remote, const QString& current)
{
    // Release tags look like "v2.1" while the build reports "2.1.0", so the two
    // are compared as numbers with the shorter one padded with zeros. A plain
    // string compare would call "2.10" older than "2.9" and "2.1" newer than
    // "2.1.0".
    auto parse = [](QString value) {
        value = value.trimmed();
        if (value.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
            value.remove(0, 1);
        // Drop any pre-release tail ("2.2-beta" -> "2.2").
        const auto dash = value.indexOf(QRegularExpression(QStringLiteral("[-+]")));
        if (dash >= 0)
            value.truncate(dash);

        QList<int> parts;
        for (const auto& piece : value.split(QLatin1Char('.'))) {
            bool ok = false;
            const int number = piece.toInt(&ok);
            parts.append(ok ? number : 0);
        }
        return parts;
    };

    auto remoteParts = parse(remote);
    auto currentParts = parse(current);
    if (remoteParts.isEmpty() || currentParts.isEmpty())
        return false;

    while (remoteParts.size() < currentParts.size())
        remoteParts.append(0);
    while (currentParts.size() < remoteParts.size())
        currentParts.append(0);

    for (int i = 0; i < remoteParts.size(); ++i) {
        if (remoteParts.at(i) != currentParts.at(i))
            return remoteParts.at(i) > currentParts.at(i);
    }
    return false;
}

bool runUpdateHelperIfRequested(const QStringList& arguments)
{
    const int flagIndex = arguments.indexOf(QString::fromLatin1(kApplyFlag));
    if (flagIndex < 0 || flagIndex + 2 >= arguments.size())
        return false;

    const QString targetDir = arguments.at(flagIndex + 1);
    const QString sourceDir = arguments.at(flagIndex + 2);

    qWarning() << "[self-update] applying" << sourceDir << "->" << targetDir;

    int copied = 0;
    int failed = 0;
    for (const auto& relative : filesIn(sourceDir)) {
        const auto from = QDir(sourceDir).absoluteFilePath(relative);
        const auto to = QDir(targetDir).absoluteFilePath(relative);

        QDir().mkpath(QFileInfo(to).absolutePath());
        if (copyWithRetry(from, to))
            ++copied;
        else {
            qWarning() << "[self-update] could not replace" << to;
            ++failed;
        }
    }

    qWarning() << "[self-update] copied" << copied << "files, failed" << failed;

    // Restart the installed copy, forwarding whatever the launcher was started
    // with apart from the helper flag and its two paths.
    QStringList forwarded = arguments;
    forwarded.removeAt(flagIndex + 2);
    forwarded.removeAt(flagIndex + 1);
    forwarded.removeAt(flagIndex);

    const auto exePath = QDir(targetDir).absoluteFilePath(QFileInfo(arguments.first()).fileName());
    qWarning() << "[self-update] restarting" << exePath;
    QProcess::startDetached(exePath, forwarded);

    return true;
}

QString latestReleaseApiUrl(const QString& repositoryUrl)
{
    QString base = repositoryUrl.trimmed();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);

    // An API address may already be configured; only the suffix is missing then.
    if (base.contains(QStringLiteral("api.github.com"))) {
        if (base.endsWith(QStringLiteral("/releases/latest")))
            return base;
        if (base.endsWith(QStringLiteral("/repos")))
            return base + QStringLiteral("/OWNER/REPO");
    }

    if (base.contains(QStringLiteral("github.com"))) {
        const int ownerStart = base.indexOf(QStringLiteral("github.com")) + int(qstrlen("github.com")) + 1;
        QString path = base.mid(ownerStart);
        if (path.endsWith(QStringLiteral(".git")))
            path.chop(4);
        return QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(path);
    }

    return base + QStringLiteral("/releases/latest");
}

void cleanupStaleUpdates()
{
    const auto tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempRoot.isEmpty())
        return;

    const QDir temp(tempRoot);
    for (const auto& entry : temp.entryInfoList(QStringList{ QStringLiteral("frends-update-*") },
                                                QDir::Dirs | QDir::NoDotAndDotDot)) {
        // The folder of the currently running copy cannot be removed, and
        // QDir refuses to do so; that is fine and simply left for next time.
        QDir(entry.absoluteFilePath()).removeRecursively();
    }
}

}  // namespace FrendsSelfUpdate
