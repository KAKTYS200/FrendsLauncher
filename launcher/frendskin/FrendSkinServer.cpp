// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "FrendSkinServer.h"

#include "minecraft/auth/MinecraftAccount.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>

namespace {
/// Guava's Hashing.sha1().hashUnencodedChars(), which is what the client uses to
/// name the skin cache file: every UTF-16 code unit is fed as two little-endian
/// bytes. It has to match byte for byte or the game will not find the file.
QByteArray sha1OfChars(const QString& text)
{
    QByteArray raw;
    raw.reserve(text.size() * 2);
    for (const QChar chr : text) {
        const ushort unit = chr.unicode();
        raw.append(char(unit & 0xFF));
        raw.append(char((unit >> 8) & 0xFF));
    }
    return QCryptographicHash::hash(raw, QCryptographicHash::Sha1);
}

/// The textures payload carries a java.util.UUID, which only parses in its
/// dashed form. Offline accounts store the id without dashes.
QString dashedUuid(QString uuid)
{
    uuid.remove(QLatin1Char('-'));
    if (uuid.size() != 32)
        return uuid;
    return uuid.mid(0, 8) + QLatin1Char('-') + uuid.mid(8, 4) + QLatin1Char('-') + uuid.mid(12, 4) + QLatin1Char('-') +
           uuid.mid(16, 4) + QLatin1Char('-') + uuid.mid(20);
}
}  // namespace

FrendSkinServer::FrendSkinServer(QObject* parent) : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &FrendSkinServer::handleConnection);
}

void FrendSkinServer::setSkin(const QString& player, const QString& filePath, const QString& model)
{
    if (player.isEmpty() || filePath.isEmpty()) {
        removeSkin(player);
        return;
    }
    // Stored lower-cased: the JSON file is edited by hand, so "SLIM", "Slim" and
    // "slim" all have to mean the same thing.
    const QString normalised = model.trimmed().toLower();
    m_skins.insert(player.toLower(), Entry{ filePath, normalised.isEmpty() ? QStringLiteral("classic") : normalised });
}

void FrendSkinServer::removeSkin(const QString& player)
{
    m_skins.remove(player.toLower());
}

void FrendSkinServer::clear()
{
    m_skins.clear();
}

void FrendSkinServer::load(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const auto entry = it.value().toObject();
        setSkin(it.key(), entry.value("path").toString(), entry.value("model").toString());
    }
}

bool FrendSkinServer::bind(const QString& filePath, const QString& player, const QString& skinPath,
                           const QString& model)
{
    if (player.isEmpty() || skinPath.isEmpty())
        return false;
    setSkin(player, skinPath, model);
    save(filePath);
    return true;
}

void FrendSkinServer::save(const QString& filePath) const
{
    QJsonObject root;
    for (auto it = m_skins.constBegin(); it != m_skins.constEnd(); ++it) {
        QJsonObject entry;
        entry.insert("path", it->path);
        entry.insert("model", it->model);
        root.insert(it.key(), entry);
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool FrendSkinServer::start(quint16 port)
{
    if (m_server.isListening())
        return true;
    return m_server.listen(QHostAddress::LocalHost, port);
}

void FrendSkinServer::stop()
{
    m_server.close();
}

QString FrendSkinServer::baseUrl() const
{
    return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort());
}

QString FrendSkinServer::skinUrl(const QString& player) const
{
    return baseUrl() + QStringLiteral("/skin/") + player.toLower() + QStringLiteral(".png");
}

bool FrendSkinServer::hasSkin(const QString& player) const
{
    const auto entry = m_skins.value(player.toLower());
    return !entry.path.isEmpty() && QFileInfo::exists(entry.path);
}

QString FrendSkinServer::model(const QString& player) const
{
    return m_skins.value(player.toLower()).model;
}

QString FrendSkinServer::textureHash(const QString& player) const
{
    const auto entry = m_skins.value(player.toLower());
    if (entry.path.isEmpty())
        return {};
    QFile file(entry.path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const auto digest = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha1);
    return QString::fromLatin1(digest.toHex().left(12));
}

QString FrendSkinServer::textureUrl(const QString& player) const
{
    const QString hash = textureHash(player);
    if (hash.isEmpty())
        return {};
    return QStringLiteral("http://frendskin.minecraft.net/texture/") + hash;
}

QString FrendSkinServer::seedClientCache(const QString& player, const QString& assetsDir) const
{
    const auto entry = m_skins.value(player.toLower());
    const QString hash = textureHash(player);
    if (entry.path.isEmpty() || hash.isEmpty() || assetsDir.isEmpty())
        return {};

    const QString name = QString::fromLatin1(sha1OfChars(hash).toHex());
    const QString dir = assetsDir + QStringLiteral("/skins/") + name.left(2);
    const QString target = dir + QLatin1Char('/') + name;

    QFile source(entry.path);
    if (!source.open(QIODevice::ReadOnly))
        return {};
    const QByteArray png = source.readAll();

    // Rewriting an unchanged file would only make the client re-upload the
    // texture on the next frame, so leave an identical copy alone.
    QFile existing(target);
    if (existing.open(QIODevice::ReadOnly) && existing.readAll() == png)
        return target;

    if (!QDir().mkpath(dir))
        return {};
    QFile out(target);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    out.write(png);
    out.close();
    return target;
}

QString FrendSkinServer::playerForUuid(const QString& uuid) const
{
    // Session profile lookups arrive by id; recover the name the same way the
    // id was built in the first place.
    for (auto it = m_skins.constBegin(); it != m_skins.constEnd(); ++it) {
        if (MinecraftAccount::uuidFromUsername(it.key()).toString(QUuid::Id128) == uuid)
            return it.key();
    }
    return {};
}

QString FrendSkinServer::texturesProperty(const QString& player, const QString& uuid) const
{
    const QString url = textureUrl(player);
    // The payload carries a java.util.UUID; without one the client would drop
    // the whole property, so there is nothing to send.
    if (url.isEmpty() || uuid.isEmpty())
        return {};

    QJsonObject skin;
    skin.insert("url", url);
    // "slim" is the only model the client distinguishes; the default is classic.
    if (model(player) == QLatin1String("slim")) {
        QJsonObject metadata;
        metadata.insert("model", "slim");
        skin.insert("metadata", metadata);
    }

    QJsonObject textures;
    textures.insert("SKIN", skin);

    QJsonObject payload;
    payload.insert("timestamp", QDateTime::currentMSecsSinceEpoch());
    payload.insert("profileId", dashedUuid(uuid));
    payload.insert("profileName", player);
    payload.insert("textures", textures);

    return QString::fromLatin1(QJsonDocument(payload).toJson(QJsonDocument::Compact).toBase64());
}

QString FrendSkinServer::userProperties(const QString& player, const QString& uuid) const
{
    const QString property = texturesProperty(player, uuid);
    if (property.isEmpty())
        return {};

    QJsonArray values;
    values.append(property);
    QJsonObject root;
    root.insert("textures", values);
    return QString::fromLatin1(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void FrendSkinServer::handleConnection()
{
    while (auto* socket = m_server.nextPendingConnection()) {
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            const QByteArray request = socket->readAll();
            const QList<QByteArray> parts = request.split(' ');
            if (parts.size() < 2) {
                respondNotFound(socket);
                return;
            }

            const QString path = QString::fromUtf8(parts.at(1));

            if (path == QLatin1String("/")) {
                const QByteArray body = QStringLiteral("Frend Skin: %1 skin(s) published.").arg(m_skins.size()).toUtf8();
                respond(socket, 200, "text/plain; charset=utf-8", body);
                return;
            }

            if (path == QLatin1String("/skins.json")) {
                QJsonObject root;
                for (auto it = m_skins.constBegin(); it != m_skins.constEnd(); ++it) {
                    QJsonObject entry;
                    entry.insert("skin", skinUrl(it.key()));
                    entry.insert("model", it->model);
                    root.insert(it.key(), entry);
                }
                respond(socket, 200, "application/json", QJsonDocument(root).toJson(QJsonDocument::Compact));
                return;
            }

            if (path.startsWith(QLatin1String("/skin/"))) {
                QString name = path.mid(6);
                if (name.endsWith(QLatin1String(".png")))
                    name.chop(4);
                const auto entry = m_skins.value(name.toLower());
                if (entry.path.isEmpty()) {
                    respondNotFound(socket);
                    return;
                }
                QFile file(entry.path);
                if (!file.open(QIODevice::ReadOnly)) {
                    respondNotFound(socket);
                    return;
                }
                respond(socket, 200, "image/png", file.readAll());
                return;
            }

            respondNotFound(socket);
        });

        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

void FrendSkinServer::respond(QTcpSocket* socket, int status, const QByteArray& contentType, const QByteArray& body)
{
    const QByteArray reason = status == 200 ? "OK" : "Not Found";
    QByteArray header = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n";
    header += "Content-Type: " + contentType + "\r\n";
    header += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    header += "Cache-Control: no-store\r\n";
    header += "Connection: close\r\n\r\n";
    socket->write(header);
    socket->write(body);
    socket->flush();
    socket->disconnectFromHost();
}

void FrendSkinServer::respondNotFound(QTcpSocket* socket)
{
    respond(socket, 404, "text/plain; charset=utf-8", "not found");
}
