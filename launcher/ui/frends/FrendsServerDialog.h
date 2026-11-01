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

#include <QDialog>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QProcess;
class QPushButton;
class QSpinBox;

/**
 * Makes a Minecraft server out of nothing: pick a version — or a whole modpack —
 * download it into a folder and start it right here.
 *
 * Four sources:
 *  - the vanilla server from Mojang's version metadata;
 *  - Paper, which most public servers actually run;
 *  - a Modrinth modpack, installed from its .mrpack with the loader it asks for;
 *  - a CurseForge modpack, installed from the author's server pack.
 *
 * The server itself is a normal child process: its console output is shown in
 * the log pane and "stop" is sent through stdin, so the world is saved on the
 * way out instead of being killed.
 */
class FrendsServerDialog : public QDialog {
    Q_OBJECT

   public:
    explicit FrendsServerDialog(QWidget* parent = nullptr);
    ~FrendsServerDialog() override;

   private:
    /// Where the server files come from.
    enum class Core { Vanilla, Paper, Modrinth, CurseForge };

    /// A modpack as the catalogs list it.
    struct Pack
    {
        QString id;
        QString title;
        QString note;
    };

    /// One downloadable build of that modpack.
    struct PackFile
    {
        QString label;
        QString url;
        /// True when CurseForge gave a ready-made server pack instead of a client zip.
        bool serverPack = false;
        /// CurseForge file id, needed for the download-url endpoint.
        int fileId = 0;
    };

    /// A single file of the install, waiting for its turn in the queue.
    struct QueueItem
    {
        QUrl url;
        QString path;
    };

    void buildUi();
    void applyStyle();
    Core currentCore() const;

    void loadVersions();
    void loadModrinthPacks();
    void loadCurseForgePacks();
    void loadModrinthFiles(const QString& projectId);
    void loadCurseForgeFiles(const QString& modId);

    void startDownload();
    void resolveVanilla(const QString& version);
    void resolvePaper(const QString& version);
    void downloadSingleFile(const QUrl& url, const QString& fileName);

    /// Pulls the chosen modpack down, then hands it to unpackModpack().
    void downloadModpack(const QUrl& url, bool mrpack);
    void unpackModpack();
    void queueDownloads(const QVector<QueueItem>& items);
    void startNextQueuedDownload();
    void installLoader();
    /// Fetches a Forge/NeoForge installer jar into the server folder.
    void downloadInstaller(const QUrl& url, const QString& fileName, const QString& label);
    /// Runs `java -jar <installer> --installServer` and waits for it.
    void runInstaller(const QString& jarPath, const QString& label);
    void finalizeServer();
    /// Writes eula.txt, user_jvm_args.txt and a start.bat next to the server.
    void writeServerFiles();

    void startServer();
    void stopServer();
    void appendLog(const QString& text);
    void setStatus(const QString& text, bool error = false);
    void setBusy(bool busy);
    QString serverFolder() const;
    QString javaExecutable() const;
    void browseFolder();

    QComboBox* m_core = nullptr;
    QComboBox* m_version = nullptr;
    QComboBox* m_pack = nullptr;
    QComboBox* m_packFile = nullptr;
    QLineEdit* m_search = nullptr;
    QPushButton* m_searchButton = nullptr;
    QLineEdit* m_folder = nullptr;
    QSpinBox* m_memory = nullptr;
    QCheckBox* m_eula = nullptr;
    QLabel* m_status = nullptr;
    QPlainTextEdit* m_log = nullptr;
    QPushButton* m_browseButton = nullptr;
    QPushButton* m_downloadButton = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QPushButton* m_closeButton = nullptr;

    QNetworkAccessManager* m_net = nullptr;
    QProcess* m_process = nullptr;
    QProcess* m_installer = nullptr;

    /// Version id to its metadata json, straight from the Mojang manifest.
    QHash<QString, QUrl> m_vanillaUrls;
    QVector<QString> m_vanillaVersions;
    QVector<QString> m_paperVersions;

    QVector<Pack> m_packs;
    QVector<PackFile> m_packFiles;

    QVector<QueueItem> m_queue;
    int m_queueDone = 0;
    int m_queueTotal = 0;

    /// The downloaded .mrpack or server-pack zip, removed once unpacked.
    QString m_tempArchive;
    bool m_archiveIsMrpack = false;
    QString m_packMinecraft;
    QString m_packLoader;
    QString m_packLoaderVersion;

    QString m_jarPath;
    QString m_launchMode;  // "jar", "bat" or "args"
    QString m_installerJarPath;
    QString m_installerLabel;
    bool m_busy = false;
};
