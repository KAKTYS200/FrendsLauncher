// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2025 Trial97 <alexandru.tripon97@gmail.com>
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
#include "ExtractZipTask.h"
#include <QtConcurrent>
#include "FileSystem.h"
#include "archive/ArchiveReader.h"
#include "archive/ArchiveWriter.h"

namespace MMCZip {

void ExtractZipTask::executeTask()
{
    m_zipFuture = QtConcurrent::run(QThreadPool::globalInstance(), [this]() { return extractZip(); });
    connect(&m_zipWatcher, &QFutureWatcher<ZipResult>::finished, this, &ExtractZipTask::finish);
    m_zipWatcher.setFuture(m_zipFuture);
}

auto ExtractZipTask::extractZip() -> ZipResult
{
    auto target = m_outputDir.absolutePath();
    auto target_top_dir = QUrl::fromLocalFile(target);

    QStringList extracted;

    qDebug() << "Extracting subdir" << m_subdirectory << "from" << m_input.getZipName() << "to" << target;
    if (!m_input.collectFiles()) {
        return ZipResult(tr("Failed to enumerate files in archive"));
    }
    if (m_input.getFiles().isEmpty()) {
        logWarning(tr("Extracting empty archives seems odd..."));
        return ZipResult();
    }

    auto extPtr = ArchiveWriter::createDiskWriter();
    auto ext = extPtr.get();

    setStatus("Extracting files...");
    setProgress(0, m_input.getFiles().count());

    // Unpacking a modpack is tens of thousands of files, and every setStatus()
    // or setProgress() call emits a signal into the GUI thread. Doing it per
    // file turned a two second unzip into minutes of hopping between threads
    // while the window repainted its animation, so the counter is local here
    // and only the visible steps are published.
    const int totalFiles = m_input.getFiles().count();
    const int reportEvery = qMax(1, totalFiles / 200);
    int seenFiles = 0;

    ZipResult result;
    auto fileName = m_input.getZipName();
    if (!m_input.parse([this, &result, &target, &target_top_dir, ext, &extracted, totalFiles, reportEvery,
                        &seenFiles](ArchiveReader::File* f) {
            if (m_zipFuture.isCanceled())
                return false;

            // Publish progress in a few hundred steps rather than per file: the
            // bar still moves, but the GUI thread is not woken for every entry.
            seenFiles++;
            if (seenFiles % reportEvery == 0)
                setProgress(seenFiles, totalFiles);

            QString file_name = f->filename();
            if (!file_name.startsWith(m_subdirectory)) {
                f->skip();
                return true;
            }

            auto relative_file_name = QDir::fromNativeSeparators(file_name.mid(m_subdirectory.size()));
            auto original_name = relative_file_name;
            if (seenFiles % reportEvery == 0)
                setStatus("Unpacking: " + relative_file_name);

            // Fix subdirs/files ending with a / getting transformed into absolute paths
            if (relative_file_name.startsWith('/'))
                relative_file_name = relative_file_name.mid(1);

            // Fix weird "folders with a single file get squashed" thing
            QString sub_path;
            if (relative_file_name.contains('/') && !relative_file_name.endsWith('/')) {
                sub_path = relative_file_name.section('/', 0, -2) + '/';
                FS::ensureFolderPathExists(FS::PathCombine(target, sub_path));

                relative_file_name = relative_file_name.split('/').last();
            }

            QString target_file_path;
            if (relative_file_name.isEmpty()) {
                target_file_path = target + '/';
            } else {
                target_file_path = FS::PathCombine(target_top_dir.toLocalFile(), sub_path, relative_file_name);
                if (relative_file_name.endsWith('/') && !target_file_path.endsWith('/'))
                    target_file_path += '/';
            }

            if (!target_top_dir.isParentOf(QUrl::fromLocalFile(target_file_path))) {
                result = ZipResult(tr("Extracting %1 was cancelled, because it was effectively outside of the target path %2")
                                       .arg(relative_file_name, target));
                return false;
            }

            if (!f->writeFile(ext, target_file_path, target)) {
                result = ZipResult(tr("Failed to extract file %1 to %2").arg(original_name, target_file_path));
                return false;
            }
            extracted.append(target_file_path);

            // One debug line per extracted file was 10 000 flushed writes to
            // disk on a large pack, which alone slowed the unzip down visibly.
            // The summary below says what happened; the file list is only
            // written when something actually goes wrong.
            return true;
        })) {
        FS::removeFiles(extracted);
        return result.has_value() ? result : ZipResult(tr("Failed to parse file %1").arg(fileName));
    }
    setProgress(totalFiles, totalFiles);
    qDebug() << "Extracted" << extracted.size() << "files from" << fileName << "into" << target;
    return ZipResult();
}

void ExtractZipTask::finish()
{
    if (m_zipFuture.isCanceled()) {
        emitAborted();
    } else if (auto result = m_zipFuture.result(); result.has_value()) {
        emitFailed(result.value());
    } else {
        emitSucceeded();
    }
}

bool ExtractZipTask::abort()
{
    if (m_zipFuture.isRunning()) {
        m_zipFuture.cancel();
        // NOTE: Here we don't do `emitAborted()` because it will be done when `m_build_zip_future` actually cancels, which may not occur
        // immediately.
        return true;
    }
    return false;
}

}  // namespace MMCZip
