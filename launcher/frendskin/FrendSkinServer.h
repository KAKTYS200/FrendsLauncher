// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QHash>
#include <QObject>
#include <QTcpServer>

class QTcpSocket;

namespace FrendSkins {
/**
 * Master switch for the whole skin system.
 *
 * Off for now: the feature is turned off outright, not merely hidden in the
 * UI. Nothing is published, nothing is seeded into the client cache and no
 * "textures" property is handed to the game, so a bound skin cannot leak into
 * a launch through some other path. Flip this to true to bring the feature
 * back; the rest of the plumbing is untouched.
 */
inline constexpr bool ENABLED = false;
}  // namespace FrendSkins

/**
 * "Frend Skin": the launcher-side half of a skin system for offline accounts.
 *
 * Minecraft asks Mojang for the skin of a player, and an offline account has
 * none there. This class keeps the skins the launcher holds and publishes them
 * in the two forms a vanilla client can consume:
 *
 *   - over HTTP on http://127.0.0.1:<port>/, for anything that wants to read
 *     them as files;
 *   - as a "textures" profile property plus a pre-seeded client-side cache, for
 *     the game itself (see texturesProperty() and seedClientCache()).
 *
 * Endpoints:
 *   GET /skin/<player>.png  - the skin file itself
 *   GET /skins.json         - manifest: { "player": { "skin": "...", "model": "classic" } }
 *   GET /                   - a short status page, handy for checking it runs
 */
class FrendSkinServer : public QObject {
    Q_OBJECT

   public:
    explicit FrendSkinServer(QObject* parent = nullptr);

    /// Publishes a skin for a player name. The file is read on every request,
    /// so replacing the PNG takes effect without a restart.
    void setSkin(const QString& player, const QString& filePath, const QString& model);
    void removeSkin(const QString& player);
    void clear();

    /// Reads the bindings file: { "player": { "path": "...", "model": "classic" } }.
    void load(const QString& filePath);
    /// Binds a skin to a player and writes the bindings file back.
    bool bind(const QString& filePath, const QString& player, const QString& skinPath, const QString& model);

    /// Starts listening; port 0 lets the system pick a free one.
    bool start(quint16 port = 0);
    void stop();
    bool isRunning() const { return m_server.isListening(); }
    quint16 port() const { return m_server.serverPort(); }

    /// The URL a client-side skin source should ask for a player's skin.
    QString skinUrl(const QString& player) const;
    QString baseUrl() const;

    // ---- what the vanilla client needs -------------------------------------
    //
    // A vanilla client only learns about a skin through the "textures" profile
    // property, and only accepts it when the URL points into *.minecraft.net.
    // It then prefers its own on-disk cache over the network, so the launcher
    // does not have to serve the file at all: it has to describe the texture
    // and put the PNG where the client looks for it.

    bool hasSkin(const QString& player) const;
    QString model(const QString& player) const;

    /// The texture identity as the client sees it: the basename of the URL.
    /// Derived from the PNG content, so editing a skin changes the identity and
    /// the client cannot serve a stale picture from its cache.
    QString textureHash(const QString& player) const;

    /// URL put into the textures property. The host is deliberately inside
    /// .minecraft.net: TextureUrlChecker drops any other domain. Nothing ever
    /// connects to it, the client finds the file in its cache first.
    QString textureUrl(const QString& player) const;

    /// Writes the skin into the client cache the game reads before going to the
    /// network: <assetsDir>/skins/<sha1(hash)[0:2]>/<sha1(hash)>, where sha1 is
    /// taken over the UTF-16LE bytes of the hash (Guava's hashUnencodedChars).
    /// Returns the file written, or an empty string on failure.
    QString seedClientCache(const QString& player, const QString& assetsDir) const;

    /// Value of the "textures" profile property, base64 encoded.
    QString texturesProperty(const QString& player, const QString& uuid) const;

    /// The player name whose OfflinePlayer id is the given undashed uuid, or
    /// an empty string when no published skin matches. Used to answer session
    /// profile lookups, which arrive by id only.
    QString playerForUuid(const QString& uuid) const;

    /// The whole --userProperties argument: {"textures":["<property>"]}.
    /// Empty when no skin is published for the player.
    QString userProperties(const QString& player, const QString& uuid) const;

   private:
    void save(const QString& filePath) const;
    void handleConnection();
    void respond(QTcpSocket* socket, int status, const QByteArray& contentType, const QByteArray& body);
    void respondNotFound(QTcpSocket* socket);

    struct Entry {
        QString path;
        QString model;
    };

    QTcpServer m_server;
    QHash<QString, Entry> m_skins;
};
