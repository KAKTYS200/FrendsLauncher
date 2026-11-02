// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Temporary benchmark for the batched unzip progress reporting.
 *
 *  A modpack unzip used to call setProgress()/setStatus() for every single
 *  entry in the archive, and every call crosses into the GUI thread. With
 *  10 000+ files that drowned the event loop. This test builds a zip of that
 *  size, extracts it, and asserts that the number of published updates stays
 *  bounded and that every file still ends up on disk.
 */
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <archive/ArchiveWriter.h>
#include <archive/ExtractZipTask.h>
#include <tasks/Task.h>

class ExtractZipTaskTest : public QObject {
    Q_OBJECT

   private slots:
    void testBatchedProgress();
};

namespace {
constexpr int kFileCount = 10000;
constexpr int kDirCount = 50;
const QByteArray kPayload = QByteArrayLiteral("frends launcher unzip benchmark\n");
}  // namespace

void ExtractZipTaskTest::testBatchedProgress()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Build a source tree and zip it: the task must unpack a real archive with
    // thousands of entries, not a handful of synthetic ones.
    const QString srcDir = tmp.filePath("src");
    QVERIFY(QDir(tmp.path()).mkpath("src"));
    for (int d = 0; d < kDirCount; d++)
        QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("src/assets/dir%1").arg(d)));

    QList<QPair<QString, QString>> entries;  // (absolute source, path inside the zip)
    for (int i = 0; i < kFileCount; i++) {
        const QString rel = QStringLiteral("assets/dir%1/file%2.txt").arg(i % kDirCount).arg(i);
        QFile f(srcDir + '/' + rel);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(kPayload);
        f.close();
        entries.append({ srcDir + '/' + rel, rel });
    }

    const QString zipPath = tmp.filePath("pack.zip");
    {
        MMCZip::ArchiveWriter writer(zipPath);
        QVERIFY(writer.open());
        for (const auto& entry : entries)
            QVERIFY(writer.addFile(entry.first, entry.second));
        QVERIFY(writer.close());
    }
    qInfo() << "built zip of" << QFileInfo(zipPath).size() << "bytes";

    const QString outDir = tmp.filePath("out");
    QVERIFY(QDir(tmp.path()).mkpath("out"));

    MMCZip::ExtractZipTask task(zipPath, QDir(outDir));

    QSignalSpy progressSpy(&task, &Task::progress);
    QSignalSpy statusSpy(&task, &Task::status);
    QSignalSpy finishedSpy(&task, &Task::finished);

    QElapsedTimer timer;
    timer.start();
    task.start();
    QVERIFY(finishedSpy.wait(180000));
    const qint64 elapsedMs = timer.elapsed();

    // Every file must be on disk and intact.
    int extracted = 0;
    for (int i = 0; i < kFileCount; i++) {
        const QString rel = QStringLiteral("assets/dir%1/file%2.txt").arg(i % kDirCount).arg(i);
        QFile f(outDir + '/' + rel);
        if (f.open(QIODevice::ReadOnly)) {
            if (f.readAll() == kPayload)
                extracted++;
            f.close();
        }
    }
    qInfo() << "Extracted" << extracted << "of" << kFileCount << "files in" << elapsedMs << "ms";
    qInfo() << "progress signals:" << progressSpy.count() << " status signals:" << statusSpy.count();
    QCOMPARE(extracted, kFileCount);

    // The whole point of the change: a bounded number of updates, not one per file.
    QVERIFY2(progressSpy.count() <= 250, qPrintable(QStringLiteral("too many progress signals: %1").arg(progressSpy.count())));
    QVERIFY2(statusSpy.count() <= 250, qPrintable(QStringLiteral("too many status signals: %1").arg(statusSpy.count())));

    // Progress must reach the end, otherwise the bar would sit at 97%.
    QVERIFY(!progressSpy.isEmpty());
    const auto last = progressSpy.last();
    QCOMPARE(last.at(0).toLongLong(), last.at(1).toLongLong());
}

QTEST_GUILESS_MAIN(ExtractZipTaskTest)

#include "ExtractZipTask_test.moc"