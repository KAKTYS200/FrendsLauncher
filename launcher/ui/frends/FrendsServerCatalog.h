// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QJsonDocument>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

#include <functional>

class QNetworkAccessManager;

/**
 * The catalog side of making a server: which Minecraft versions exist, and
 * which modpacks Modrinth and CurseForge list.
 *
 * Kept apart from FrendsServerDialog so that the dialog stays about the
 * download-queue-and-process workflow. This half only talks to the three APIs
 * and reports what came back; it never touches widgets.
 */
class FrendsServerCatalog : public QObject {
    Q_OBJECT

   public:
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

    explicit FrendsServerCatalog(QNetworkAccessManager* net, QObject* parent = nullptr);

    /// Stable release ids, newest first, plus the URL of each version's metadata.
    void loadMinecraftVersions();
    /// Stable Paper builds, newest first.
    void loadPaperVersions();

    /// The metadata address of every version the last load reported, kept by the
    /// catalog so the download step can ask for a version id alone.
    QUrl minecraftVersionUrl(const QString& version) const { return m_versionUrls.value(version); }

    void loadModrinthPacks(const QString& search);
    void loadModrinthFiles(const QString& projectId);
    /// `key` is the CurseForge API key; an empty one is reported as a failure.
    void loadCurseForgePacks(const QString& search, const QString& key);
    void loadCurseForgeFiles(const QString& modId, const QString& key);

   signals:
    void minecraftVersionsReady(const QVector<QString>& versions);
    void paperVersionsReady(const QVector<QString>& versions);
    void packsReady(const QVector<FrendsServerCatalog::Pack>& packs);
    void packFilesReady(const QVector<FrendsServerCatalog::PackFile>& files);
    /// Everything that can go wrong arrives here with a message for the status line.
    void failed(const QString& message);

   private:
    void fetch(const QUrl& url, const QByteArray& apiKey,
               std::function<void(const QJsonDocument&, const QString&)> done);

    QNetworkAccessManager* m_net = nullptr;
    /// Version id to its metadata json, straight from the Mojang manifest.
    QHash<QString, QUrl> m_versionUrls;
};