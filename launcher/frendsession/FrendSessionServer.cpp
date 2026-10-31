// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "FrendSessionServer.h"

#include "Application.h"
#include "frendskin/FrendSkinServer.h"
#include "minecraft/auth/MinecraftAccount.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QUrlQuery>

namespace {
/// A request is complete once the header block arrived and, for requests
/// carrying a body, all of the body. authlib POSTs a small JSON payload to
/// /session/minecraft/join and must see the reply only after sending it all,
/// otherwise the socket close may interrupt the write and the client reports
/// a connection error instead of a successful join.
QByteArray completeRequest(QByteArray& buffer)
{
    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return {};

    const QByteArray head = buffer.left(headerEnd);
    int contentLength = 0;
    static const QRegularExpression lengthRx("Content-Length:\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
    const auto match = lengthRx.match(QString::fromLatin1(head));
    if (match.hasMatch())
        contentLength = match.captured(1).toInt();

    const int total = headerEnd + 4 + contentLength;
    if (buffer.size() < total)
        return {};

    const QByteArray request = buffer.left(total);
    buffer.remove(0, total);
    return request;
}

/// "GET /path?query HTTP/1.1" -> { method, path, query }
struct ParsedRequest {
    QString method;
    QString path;
    QString query;
};

ParsedRequest parseRequestLine(const QByteArray& request)
{
    ParsedRequest out;
    const int lineEnd = request.indexOf("\r\n");
    if (lineEnd < 0)
        return out;
    const QList<QByteArray> parts = request.left(lineEnd).split(' ');
    if (parts.size() < 2)
        return out;

    out.method = QString::fromLatin1(parts.at(0));
    const QString target = QString::fromUtf8(parts.at(1));
    const int queryStart = target.indexOf(QLatin1Char('?'));
    if (queryStart >= 0) {
        out.path = target.left(queryStart);
        out.query = target.mid(queryStart + 1);
    } else {
        out.path = target;
    }
    return out;
}
}  // namespace

FrendSessionServer::FrendSessionServer(QObject* parent) : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &FrendSessionServer::handleConnection);
}

bool FrendSessionServer::start(quint16 port)
{
    if (m_server.isListening())
        return true;
    // Loopback only: the server speaks for the game this launcher started and
    // must never be reachable from the network.
    if (!m_server.listen(QHostAddress::LocalHost, port)) {
        qWarning() << "[frend-session] failed to listen on" << port << ':' << m_server.errorString();
        return false;
    }
    qInfo() << "[frend-session] serving offline sessions on" << baseUrl();
    return true;
}

void FrendSessionServer::stop()
{
    m_server.close();
}

QString FrendSessionServer::baseUrl() const
{
    return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort());
}

void FrendSessionServer::handleConnection()
{
    while (auto* socket = m_server.nextPendingConnection()) {
        auto* buffer = new QByteArray();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
            buffer->append(socket->readAll());

            // Exactly one request per connection. Every reply carries
            // "Connection: close", so responding tears the socket down, which
            // frees this buffer through the destroyed() handler below. Looping
            // here to look for a second request used to read that freed buffer:
            // the game sends its session calls back to back, the heap got
            // corrupted and the launcher died with 0xC0000005 shortly after
            // Minecraft started.
            const QByteArray request = completeRequest(*buffer);
            if (request.isEmpty())
                return;

            handleRequest(socket, request);
        });
        // The buffer has to outlive the reply: disconnectFromHost() runs the
        // reply path synchronously and the socket is only deleted afterwards.
        connect(socket, &QObject::destroyed, this, [buffer] { delete buffer; });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

void FrendSessionServer::handleRequest(QTcpSocket* socket, const QByteArray& request)
{
    const ParsedRequest parsed = parseRequestLine(request);

    if (parsed.path == QLatin1String("/")) {
        respond(socket, 200, "text/plain; charset=utf-8", "Frends Launcher offline session server is running.");
        return;
    }

    // The call the joining client makes before it is allowed in.
    if (parsed.path == QLatin1String("/session/minecraft/join")) {
        respondNoContent(socket);
        return;
    }

    // The call the host makes to admit the joining player. The name alone is
    // enough: the id comes from the same OfflinePlayer hash the launcher used
    // when it created the account, so both sides agree on the identity.
    if (parsed.path == QLatin1String("/session/minecraft/hasJoined")) {
        const QUrlQuery query(parsed.query);
        const QString name = query.queryItemValue("username");
        if (name.isEmpty()) {
            respondNoContent(socket);
            return;
        }
        respondJson(socket, profileJson(name));
        return;
    }

    // Skin/property lookup for other players, keyed by profile id.
    static const QRegularExpression profileRx("^/session/minecraft/profile/([0-9a-fA-F-]{32,36})$");
    const auto profileMatch = profileRx.match(parsed.path);
    if (profileMatch.hasMatch()) {
        const QString uuid = profileMatch.captured(1).remove(QLatin1Char('-'));
        QString name;
        if (auto* skin = APPLICATION->frendSkin())
            name = skin->playerForUuid(uuid);
        if (name.isEmpty()) {
            respondNoContent(socket);
            return;
        }
        respondJson(socket, profileJson(name));
        return;
    }

    // The user attributes call, which Mojang answers with privileges. For an
    // offline session the launcher reports multiplayer as allowed and turns
    // telemetry off, so the client does not fall back to its offline stub and
    // log a 401 at every start.
    if (parsed.path == QLatin1String("/player/attributes")) {
        QJsonObject privileges;
        const auto privilege = [](bool enabled) {
            QJsonObject obj;
            obj.insert("enabled", enabled);
            return obj;
        };
        privileges.insert("onlineChat", privilege(true));
        privileges.insert("multiplayerServer", privilege(true));
        privileges.insert("multiplayerRealms", privilege(false));
        privileges.insert("telemetry", privilege(false));
        privileges.insert("optionalTelemetry", privilege(false));

        QJsonObject profanity;
        profanity.insert("profanityFilterOn", false);

        QJsonObject root;
        root.insert("privileges", privileges);
        root.insert("profanityFilterPreferences", profanity);
        respondJson(socket, root);
        return;
    }

    respondNotFound(socket);
}

QJsonObject FrendSessionServer::profileJson(const QString& name) const
{
    const QString uuid = MinecraftAccount::uuidFromUsername(name).toString(QUuid::Id128);

    QJsonArray properties;
    // Publish the launcher's own skin, so LAN players do not all look like
    // Steve. The payload is the same one the client gets at launch time.
    // Skipped entirely while the skin system is switched off.
    if (auto* skin = FrendSkins::ENABLED ? APPLICATION->frendSkin() : nullptr; skin && skin->hasSkin(name)) {
        const QString payload = skin->userProperties(name, uuid);
        const auto doc = QJsonDocument::fromJson(payload.toUtf8());
        properties = doc.object().value("textures").toArray();
    }

    QJsonObject root;
    root.insert("id", uuid);
    root.insert("name", name);
    root.insert("properties", properties);
    return root;
}

void FrendSessionServer::respond(QTcpSocket* socket, int status, const QByteArray& contentType, const QByteArray& body)
{
    const QByteArray reason = status == 200 ? "OK" : status == 204 ? "No Content" : status == 404 ? "Not Found" : "Error";
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

void FrendSessionServer::respondJson(QTcpSocket* socket, const QJsonObject& json)
{
    respond(socket, 200, "application/json", QJsonDocument(json).toJson(QJsonDocument::Compact));
}

void FrendSessionServer::respondNoContent(QTcpSocket* socket)
{
    // 204 is what Mojang's own session server answers: join accepted, and a
    // hasJoined lookup that found no entry (authlib turns it into a null
    // profile, which the vanilla server treats as "let them in anyway").
    const QByteArray header = "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    socket->write(header);
    socket->flush();
    socket->disconnectFromHost();
}

void FrendSessionServer::respondNotFound(QTcpSocket* socket)
{
    respond(socket, 404, "text/plain; charset=utf-8", "not found");
}
