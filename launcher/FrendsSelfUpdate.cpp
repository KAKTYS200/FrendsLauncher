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
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
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

/// Top-level names of a portable install that hold user data rather than
/// launcher files: instances, downloaded libraries and assets, caches, logs
/// and the alternate "UserData" layout. Everything below them belongs to the
/// user and survives an update untouched.
const QStringList kPortableUserDataDirs = { QStringLiteral("instances"),
                                            QStringLiteral("libraries"),
                                            QStringLiteral("assets"),
                                            QStringLiteral("cache"),
                                            QStringLiteral("mods"),
                                            QStringLiteral("meta"),
                                            QStringLiteral("translations"),
                                            QStringLiteral("logs"),
                                            QStringLiteral("UserData") };

/// Appends every file under `directory` to `out` as a path relative to the
/// top-level call, skipping the given top-level subtrees.
void collectFiles(const QString& directory, const QString& prefix, const QStringList& skippedTopLevels, QStringList& out)
{
    const QDir dir(directory);
    const auto entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const auto& entry : entries) {
        const auto relative = prefix.isEmpty() ? entry.fileName() : prefix + QLatin1Char('/') + entry.fileName();
        if (entry.isDir()) {
            if (prefix.isEmpty() && skippedTopLevels.contains(entry.fileName()))
                continue;
            collectFiles(entry.absoluteFilePath(), relative, skippedTopLevels, out);
        } else {
            out.append(relative);
        }
    }
}

/// True when `targetDir` is a portable install, meaning user data lives in it.
bool isPortableInstall(const QString& targetDir)
{
    return QFile::exists(QDir(targetDir).filePath(QStringLiteral("portable.txt"))) ||
           QDir(targetDir).exists(QStringLiteral("UserData"));
}

/**
 * Removes files `targetDir` has but the new release no longer ships.
 *
 * A DLL dropped from the release must not linger: the next release could ship
 * a new file of the same name, and stale binaries are also the sort of thing
 * scanners flag. A target without portable.txt is a plain installed
 * application folder, so every file missing from the release is removable. In
 * a portable install the folder also holds user data underneath
 * kPortableUserDataDirs, so only launcher binaries (.exe/.dll/.pdb) outside
 * those subtrees are considered, and files like accounts.json, settings or
 * portable.txt itself are always kept. Removal is best effort: a locked file
 * is logged and left for the next update.
 */
void removeStaleFiles(const QString& targetDir, const QStringList& shipped)
{
    const auto shippedSet = QSet<QString>(shipped.cbegin(), shipped.cend());
    const bool portable = isPortableInstall(targetDir);
    const QStringList skippedTopLevels = portable ? kPortableUserDataDirs : QStringList{};
    const QStringList appSuffixes = { QStringLiteral("exe"), QStringLiteral("dll"), QStringLiteral("pdb") };

    QStringList present;
    collectFiles(targetDir, QString(), skippedTopLevels, present);

    int removed = 0;
    int failed = 0;
    for (const auto& relative : present) {
        if (shippedSet.contains(relative))
            continue;
        if (portable && !appSuffixes.contains(QFileInfo(relative).suffix().toLower(), Qt::CaseInsensitive))
            continue;
        if (QFile::remove(QDir(targetDir).absoluteFilePath(relative)))
            ++removed;
        else {
            qWarning() << "[self-update] could not remove obsolete" << relative;
            ++failed;
        }
    }
    qWarning() << "[self-update] removed" << removed << "obsolete files, failed" << failed;
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

bool archiveMatchesDigest(const QString& archivePath, const QString& digest)
{
    const auto trimmed = digest.trimmed();
    if (trimmed.isEmpty())
        return true;

    // GitHub sends "sha256:<hex>". Only that algorithm is understood here; an
    // unknown one is reported rather than waved through, because silently
    // skipping the check is exactly what this function exists to prevent.
    const int separator = trimmed.indexOf(QLatin1Char(':'));
    if (separator <= 0) {
        qWarning() << "[self-update] malformed digest:" << digest;
        return false;
    }
    const auto algorithm = trimmed.left(separator).toLower();
    if (algorithm != QLatin1String("sha256")) {
        qWarning() << "[self-update] unsupported digest algorithm:" << algorithm;
        return false;
    }
    const auto expected = QByteArray::fromHex(trimmed.mid(separator + 1).toLatin1());
    if (expected.isEmpty()) {
        qWarning() << "[self-update] empty digest body:" << digest;
        return false;
    }

    QFile file(archivePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[self-update] cannot open the archive for hashing:" << archivePath << file.errorString();
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        qWarning() << "[self-update] cannot read the archive for hashing:" << archivePath;
        return false;
    }

    const auto actual = hash.result();
    if (actual != expected) {
        qWarning() << "[self-update] digest mismatch: expected" << expected.toHex() << "got" << actual.toHex();
        return false;
    }
    return true;
}

bool runUpdateHelperIfRequested(const QStringList& arguments)
{
    const int flagIndex = arguments.indexOf(QString::fromLatin1(kApplyFlag));
    if (flagIndex < 0 || flagIndex + 2 >= arguments.size())
        return false;

    const QString targetDir = arguments.at(flagIndex + 1);
    const QString sourceDir = arguments.at(flagIndex + 2);

    qWarning() << "[self-update] applying" << sourceDir << "->" << targetDir;

    const auto shipped = filesIn(sourceDir);
    int copied = 0;
    int failed = 0;
    for (const auto& relative : shipped) {
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

    // Now that the new files are in place, files the release no longer ships
    // must not stay behind (see removeStaleFiles for what is kept).
    removeStaleFiles(targetDir, shipped);
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
    // "https://api.github.com/repos/Owner/Repo" must not be run through the
    // web-address branch below, which would look for "repos/" and produce
    // ".../repos/repos/Owner/Repo".
    if (base.contains(QStringLiteral("api.github.com"))) {
        if (base.endsWith(QStringLiteral("/releases/latest")))
            return base;
        return base + QStringLiteral("/releases/latest");
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
