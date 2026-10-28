// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QTcpServer>

class QTcpSocket;

/**
 * Local stand-in for Mojang's session server, used for offline accounts.
 *
 * A vanilla client verifies itself against Mojang on every multiplayer
 * connect: it posts its access token to
 *   POST {sessionhost}/session/minecraft/join
 * and the server side asks
 *   GET  {sessionhost}/session/minecraft/hasJoined?username=..&serverId=..
 * An offline account has no token Mojang accepts ("0"), so both calls 401 and
 * the joining player is dropped with "Failed to log in: Invalid session".
 *
 * This server answers those calls locally. authlib is pointed at it with the
 * JVM properties minecraft.api.{auth,account,session,services}.host (all four
 * must be set: authlib ignores the override if any of the first three is
 * missing). Only the session server is impersonated - the join is accepted
 * and hasJoined answers with the offline UUID of the requested name, the same
 * UUID the launcher uses when it creates the account, so ids stay consistent
 * between what the client sends and what the host verifies.
 *
 * Everything binds to 127.0.0.1 and the flags are only passed for offline
 * sessions, so accounts with a real Mojang login keep using Mojang.
 *
 * Endpoints:
 *   POST /session/minecraft/join                - 204, always accepted
 *   GET  /session/minecraft/hasJoined?username= - 200 + profile, always found
 *   GET  /session/minecraft/profile/<uuid>      - 200 + profile
 *   GET  /player/attributes                     - 200 + privileges payload
 *   GET  /                                      - short status page
 */
class FrendSessionServer : public QObject {
    Q_OBJECT

   public:
    explicit FrendSessionServer(QObject* parent = nullptr);

    /// Starts listening; port 0 lets the system pick a free one.
    bool start(quint16 port = 0);
    void stop();
    bool isRunning() const { return m_server.isListening(); }
    quint16 port() const { return m_server.serverPort(); }

    /// The base URL put into the minecraft.api.*.host JVM properties.
    QString baseUrl() const;

   private:
    void handleConnection();
    void handleRequest(QTcpSocket* socket, const QByteArray& request);
    void respond(QTcpSocket* socket, int status, const QByteArray& contentType, const QByteArray& body);
    void respondJson(QTcpSocket* socket, const QJsonObject& json);
    void respondNoContent(QTcpSocket* socket);
    void respondNotFound(QTcpSocket* socket);

    /// The profile authlib expects back: id, name, properties (with the
    /// Frend Skin textures payload when the launcher knows the skin).
    QJsonObject profileJson(const QString& name) const;

    QTcpServer m_server;
};
