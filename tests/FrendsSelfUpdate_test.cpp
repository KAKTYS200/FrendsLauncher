// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Frends Launcher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include <QCryptographicHash>

#include "FrendsSelfUpdate.h"

class FrendsSelfUpdateTest : public QObject {
    Q_OBJECT
   private slots:
    void test_IsNewerVersion_data()
    {
        QTest::addColumn<QString>("remote");
        QTest::addColumn<QString>("current");
        QTest::addColumn<bool>("expected");

        QTest::newRow("same") << "1.2" << "1.2.0" << false;
        QTest::newRow("same with v") << "v1.2" << "1.2" << false;
        QTest::newRow("patch newer") << "1.3" << "1.2.0" << true;
        QTest::newRow("patch older") << "1.1" << "1.2.0" << false;
        // A plain string compare gets both of these backwards.
        QTest::newRow("1.10 beats 1.9") << "v1.10" << "1.9" << true;
        QTest::newRow("1.9 loses to 1.10") << "v1.9" << "1.10" << false;
        QTest::newRow("shorter pads with zero") << "1.2" << "1.2.0" << false;
        QTest::newRow("longer wins") << "1.2.1" << "1.2" << true;
        QTest::newRow("prerelease tail ignored") << "1.3-beta" << "1.2" << true;
        QTest::newRow("no version at all") << "" << "1.2" << false;
        QTest::newRow("v only") << "v" << "1.2" << false;
    }
    void test_IsNewerVersion()
    {
        QFETCH(QString, remote);
        QFETCH(QString, current);
        QFETCH(bool, expected);

        QCOMPARE(FrendsSelfUpdate::isNewerVersion(remote, current), expected);
    }

    void test_LatestReleaseApiUrl()
    {
        QCOMPARE(FrendsSelfUpdate::latestReleaseApiUrl(QStringLiteral("https://github.com/Owner/Repo")),
                 QStringLiteral("https://api.github.com/repos/Owner/Repo/releases/latest"));
        // A trailing slash and a .git suffix are both what a clone URL looks like.
        QCOMPARE(FrendsSelfUpdate::latestReleaseApiUrl(QStringLiteral("https://github.com/Owner/Repo/")),
                 QStringLiteral("https://api.github.com/repos/Owner/Repo/releases/latest"));
        QCOMPARE(FrendsSelfUpdate::latestReleaseApiUrl(QStringLiteral("https://github.com/Owner/Repo.git")),
                 QStringLiteral("https://api.github.com/repos/Owner/Repo/releases/latest"));
        // An address that is already an API endpoint is left alone.
        QCOMPARE(FrendsSelfUpdate::latestReleaseApiUrl(QStringLiteral("https://api.github.com/repos/Owner/Repo")),
                 QStringLiteral("https://api.github.com/repos/Owner/Repo/releases/latest"));
    }

    void test_Digest_data()
    {
        QTest::addColumn<QByteArray>("content");
        QTest::addColumn<QString>("digest");
        QTest::addColumn<bool>("expected");

        const QByteArray payload = "frends launcher archive contents";

        QTest::newRow("matching") << payload << sha256Digest(payload) << true;
        QTest::newRow("mismatching") << payload << QStringLiteral("sha256:") + QStringLiteral("00").repeated(32)
                                     << false;
        // An older release carries no digest; a missing one must not block the
        // update, or everything published before the check existed stops working.
        QTest::newRow("no digest") << payload << QString() << true;
        QTest::newRow("malformed") << payload << QStringLiteral("deadbeef") << false;
        QTest::newRow("unknown algorithm") << payload << QStringLiteral("md5:deadbeef") << false;
        QTest::newRow("empty body") << payload << QStringLiteral("sha256:") << false;
    }
    void test_Digest()
    {
        QFETCH(QByteArray, content);
        QFETCH(QString, digest);
        QFETCH(bool, expected);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(QStringLiteral("archive.zip"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(content);
        file.close();

        QCOMPARE(FrendsSelfUpdate::archiveMatchesDigest(path, digest), expected);
    }

    void test_DigestMissingFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // A file that is not there cannot be hashed, so it cannot be trusted.
        QVERIFY(!FrendsSelfUpdate::archiveMatchesDigest(dir.filePath(QStringLiteral("absent.zip")),
                                                       QStringLiteral("sha256:") + QStringLiteral("00").repeated(32)));
    }

    // The helper copies the new build over the old one and restarts; the test
    // exe path it would restart is a plain text file, and starting that fails
    // silently, which is exactly what the production process does not do.
    void test_ApplyRemovesStaleFilesInPortable()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto source = dir.filePath(QStringLiteral("source"));
        const auto target = dir.filePath(QStringLiteral("target"));
        QVERIFY(QDir().mkpath(source));
        QVERIFY(QDir().mkpath(target));

        // The new release: launcher binary and two DLLs.
        QVERIFY(writeFile(QDir(source).filePath(QStringLiteral("frendslauncher.exe")), "new exe"));
        QVERIFY(writeFile(QDir(source).filePath(QStringLiteral("fresh.dll")), "new dll"));
        QVERIFY(writeFile(QDir(source).filePath(QStringLiteral("new.dll")), "another dll"));

        // The old install: the same files, a DLL the release dropped, and user
        // data that must survive: accounts, the portable marker, and mods or
        // natives inside instances/.
        QVERIFY(writeFile(QDir(target).filePath(QStringLiteral("frendslauncher.exe")), "old exe"));
        QVERIFY(writeFile(QDir(target).filePath(QStringLiteral("fresh.dll")), "old dll"));
        QVERIFY(writeFile(QDir(target).filePath(QStringLiteral("stale.dll")), "dropped"));
        QVERIFY(writeFile(QDir(target).filePath(QStringLiteral("accounts.json")), "{}"));
        QVERIFY(writeFile(QDir(target).filePath(QStringLiteral("portable.txt")), ""));
        QVERIFY(QDir().mkpath(QDir(target).filePath(QStringLiteral("instances/some/natives"))));
        QVERIFY(writeFile(QDir(target).filePath(QStringLiteral("instances/some/natives/lwjgl.dll")), "user dll"));

        const QStringList arguments{ QDir(target).filePath(QStringLiteral("frendslauncher.exe")),
                                     QStringLiteral("--frends-apply-update"), target, source };
        QVERIFY(FrendsSelfUpdate::runUpdateHelperIfRequested(arguments));

        // New files arrived and the dropped DLL is gone...
        QVERIFY(QFile::exists(QDir(target).filePath(QStringLiteral("new.dll"))));
        QVERIFY(!QFile::exists(QDir(target).filePath(QStringLiteral("stale.dll"))));
        // ...while user data and the portable marker survived.
        QVERIFY(QFile::exists(QDir(target).filePath(QStringLiteral("accounts.json"))));
        QVERIFY(QFile::exists(QDir(target).filePath(QStringLiteral("portable.txt"))));
        QVERIFY(QFile::exists(QDir(target).filePath(QStringLiteral("instances/some/natives/lwjgl.dll"))));
    }

    void test_ApplyRemovesEveryStaleFileInInstalledTarget()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto source = dir.filePath(QStringLiteral("source"));
        const auto target = dir.filePath(QStringLiteral("target"));
        QVERIFY(QDir().mkpath(source));
        QVERIFY(QDir().mkpath(target));

        QVERIFY(writeFile(QDir(source).filePath(QStringLiteral("frendslauncher.exe")), "new exe"));
        QVERIFY(writeFile(QDir(source).filePath(QStringLiteral("new.dll")), "new dll"));

        // An installed folder holds launcher files only, so without portable.txt
        // everything missing from the release counts as stale, including a
        // leftover accounts.json.
        QVERIFY(writeFile(QDir(target).filePath(QStringLiteral("frendslauncher.exe")), "old exe"));
        QVERIFY(writeFile(QDir(target).filePath(QStringLiteral("stale.dll")), "old dll"));
        QVERIFY(writeFile(QDir(target).filePath(QStringLiteral("accounts.json")), "{}"));

        const QStringList arguments{ QDir(target).filePath(QStringLiteral("frendslauncher.exe")),
                                     QStringLiteral("--frends-apply-update"), target, source };
        QVERIFY(FrendsSelfUpdate::runUpdateHelperIfRequested(arguments));

        QVERIFY(QFile::exists(QDir(target).filePath(QStringLiteral("new.dll"))));
        QVERIFY(!QFile::exists(QDir(target).filePath(QStringLiteral("stale.dll"))));
        QVERIFY(!QFile::exists(QDir(target).filePath(QStringLiteral("accounts.json"))));
    }

    static bool writeFile(const QString& path, const QByteArray& content)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        return file.write(content) == content.size();
    }

   private:
    static QString sha256Digest(const QByteArray& content)
    {
        return QStringLiteral("sha256:") +
               QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());
    }
};

QTEST_GUILESS_MAIN(FrendsSelfUpdateTest)

#include "FrendsSelfUpdate_test.moc"