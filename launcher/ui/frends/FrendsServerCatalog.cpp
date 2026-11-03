// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "FrendsServerCatalog.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace {
const QString kManifestUrl = QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");
const QString kPaperVersionsUrl = QStringLiteral("https://fill.papermc.io/v3/projects/paper");
const QString kModrinthSearchUrl = QStringLiteral("https://api.modrinth.com/v2/search");
const QString kModrinthProjectUrl = QStringLiteral("https://api.modrinth.com/v2/project/");
const QString kCurseSearchUrl = QStringLiteral("https://api.curseforge.com/v1/mods/search");
const QString kCurseModsUrl = QStringLiteral("https://api.curseforge.com/v1/mods/");

/// Snapshot, release candidate and pre-release ids are noise in a server list.
bool isStableVersion(const QString& id)
{
    return !id.contains(QLatin1String("-rc")) && !id.contains(QLatin1String("-pre")) &&
           !id.contains(QLatin1String("snapshot"));
}
}  // namespace

FrendsServerCatalog::FrendsServerCatalog(QNetworkAccessManager* net, QObject* parent) : QObject(parent), m_net(net)
{
}

void FrendsServerCatalog::fetch(const QUrl& url, const QByteArray& apiKey,
                                std::function<void(const QJsonDocument&, const QString&)> done)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FrendsLauncher/1.1"));
    if (!apiKey.isEmpty())
        request.setRawHeader("x-api-key", apiKey);

    auto* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done(QJsonDocument(), reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isNull())
            done(QJsonDocument(), tr("ответ сервера не разобрался"));
        else
            done(doc, QString());
    });
}

void FrendsServerCatalog::loadMinecraftVersions()
{
    fetch(QUrl(kManifestUrl), {}, [this](const QJsonDocument& doc, const QString& error) {
        if (!error.isEmpty()) {
            emit failed(tr("Не удалось загрузить список версий Minecraft: %1").arg(error));
            return;
        }
        QVector<QString> versions;
        for (const auto& entry : doc.object().value("versions").toArray()) {
            const auto object = entry.toObject();
            if (object.value("type").toString() != QLatin1String("release"))
                continue;
            const QString id = object.value("id").toString();
            if (id.isEmpty() || !isStableVersion(id))
                continue;
            versions.push_back(id);
            m_versionUrls.insert(id, QUrl(object.value("url").toString()));
        }
        emit minecraftVersionsReady(versions);
    });
}

void FrendsServerCatalog::loadPaperVersions()
{
    // Paper's v3 catalog already groups versions by major line, newest first.
    fetch(QUrl(kPaperVersionsUrl), {}, [this](const QJsonDocument& doc, const QString&) {
        const auto groups = doc.object().value("versions").toObject();
        if (groups.isEmpty())
            return;  // a dead Paper catalog is not fatal, vanilla still works
        QVector<QString> versions;
        for (auto it = groups.begin(); it != groups.end(); ++it) {
            for (const auto& value : it.value().toArray()) {
                const QString id = value.toString();
                if (!id.isEmpty() && isStableVersion(id))
                    versions.push_back(id);
            }
        }
        emit paperVersionsReady(versions);
    });
}

void FrendsServerCatalog::loadModrinthPacks(const QString& search)
{
    QUrl url(kModrinthSearchUrl);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("query"), search.trimmed());
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("20"));
    query.addQueryItem(QStringLiteral("index"), QStringLiteral("relevance"));
    query.addQueryItem(QStringLiteral("facets"), QStringLiteral("[[\"project_type:modpack\"]]"));
    url.setQuery(query);

    fetch(url, {}, [this](const QJsonDocument& doc, const QString& error) {
        if (!error.isEmpty()) {
            emit failed(tr("Modrinth не ответил: %1").arg(error));
            return;
        }
        QVector<Pack> packs;
        for (const auto& hit : doc.object().value("hits").toArray()) {
            const auto object = hit.toObject();
            Pack pack;
            pack.id = object.value("project_id").toString();
            pack.title = object.value("title").toString();
            pack.note = object.value("author").toString();
            if (pack.id.isEmpty() || pack.title.isEmpty())
                continue;
            packs.push_back(pack);
        }
        emit packsReady(packs);
    });
}

void FrendsServerCatalog::loadModrinthFiles(const QString& projectId)
{
    fetch(QUrl(kModrinthProjectUrl + projectId + QStringLiteral("/version")), {},
          [this](const QJsonDocument& doc, const QString& error) {
              if (!error.isEmpty()) {
                  emit failed(tr("Modrinth не отдал версии сборки: %1").arg(error));
                  return;
              }
              QVector<PackFile> files;
              for (const auto& entry : doc.array()) {
                  const auto object = entry.toObject();
                  QString fileUrl;
                  for (const auto& file : object.value("files").toArray()) {
                      const auto f = file.toObject();
                      if (f.value("primary").toBool() || fileUrl.isEmpty())
                          fileUrl = f.value("url").toString();
                  }
                  if (fileUrl.isEmpty())
                      continue;

                  QStringList games;
                  for (const auto& g : object.value("game_versions").toArray())
                      games << g.toString();
                  QStringList loaders;
                  for (const auto& l : object.value("loaders").toArray())
                      loaders << l.toString();

                  PackFile file;
                  file.url = fileUrl;
                  file.label = QStringLiteral("%1 — %2 / %3")
                                   .arg(object.value("version_number").toString(),
                                        games.isEmpty() ? QStringLiteral("?") : games.first(),
                                        loaders.isEmpty() ? QStringLiteral("vanilla")
                                                          : loaders.join(QStringLiteral(", ")));
                  files.push_back(file);
              }
              emit packFilesReady(files);
          });
}

void FrendsServerCatalog::loadCurseForgePacks(const QString& search, const QString& key)
{
    if (key.isEmpty()) {
        emit failed(tr("Для CurseForge нужен API-ключ: настройки → «Службы» → ключ CurseForge. "
                       "Без ключа сборки берутся с Modrinth."));
        return;
    }

    QUrl url(kCurseSearchUrl);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("gameId"), QStringLiteral("432"));
    query.addQueryItem(QStringLiteral("classId"), QStringLiteral("4471"));
    query.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("20"));
    query.addQueryItem(QStringLiteral("sortField"), QStringLiteral("2"));
    query.addQueryItem(QStringLiteral("sortOrder"), QStringLiteral("desc"));
    if (!search.trimmed().isEmpty())
        query.addQueryItem(QStringLiteral("searchFilter"), search.trimmed());
    url.setQuery(query);

    fetch(url, key.toUtf8(), [this](const QJsonDocument& doc, const QString& error) {
        if (!error.isEmpty()) {
            emit failed(tr("CurseForge не ответил: %1").arg(error));
            return;
        }
        QVector<Pack> packs;
        for (const auto& entry : doc.object().value("data").toArray()) {
            const auto object = entry.toObject();
            Pack pack;
            pack.id = QString::number(object.value("id").toInt());
            pack.title = object.value("name").toString();
            pack.note = object.value("slug").toString();
            if (pack.id.isEmpty() || pack.title.isEmpty())
                continue;
            packs.push_back(pack);
        }
        emit packsReady(packs);
    });
}

void FrendsServerCatalog::loadCurseForgeFiles(const QString& modId, const QString& key)
{
    if (key.isEmpty()) {
        emit failed(tr("Для CurseForge нужен API-ключ в настройках."));
        return;
    }

    QUrl url(kCurseModsUrl + modId + QStringLiteral("/files"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("50"));
    url.setQuery(query);

    fetch(url, key.toUtf8(), [this](const QJsonDocument& doc, const QString& error) {
        if (!error.isEmpty()) {
            emit failed(tr("CurseForge не отдал файлы сборки: %1").arg(error));
            return;
        }
        QVector<PackFile> files;
        for (const auto& entry : doc.object().value("data").toArray()) {
            const auto object = entry.toObject();
            const int fileId = object.value("id").toInt();
            if (fileId == 0)
                continue;

            QStringList tags;
            for (const auto& g : object.value("gameVersions").toArray())
                tags << g.toString();

            PackFile file;
            file.label = QStringLiteral("%1 — %2").arg(object.value("displayName").toString(),
                                                      tags.join(QStringLiteral(" / ")));
            // A server pack is a ready-to-run folder, so prefer it when the
            // author published one; otherwise the client zip is unpacked.
            const int serverPackId = object.value("serverPackFileId").toInt();
            file.serverPack = serverPackId != 0;
            file.fileId = serverPackId != 0 ? serverPackId : fileId;
            files.push_back(file);
        }
        emit packFilesReady(files);
    });
}