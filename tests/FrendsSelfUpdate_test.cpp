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

   private:
    static QString sha256Digest(const QByteArray& content)
    {
        return QStringLiteral("sha256:") +
               QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());
    }
};

QTEST_GUILESS_MAIN(FrendsSelfUpdateTest)

#include "FrendsSelfUpdate_test.moc"