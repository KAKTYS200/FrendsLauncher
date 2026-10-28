// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Frends Launcher Contributors
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

#include "TLauncherImportTask.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QDebug>

#include "Exception.h"
#include "FileSystem.h"
#include "Json.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/INISettingsObject.h"

namespace {

/// Every launch argument as a flat string list, ignoring the rule wrappers.
QStringList flattenGameArguments(const QJsonObject& doc)
{
    QStringList out;
    const auto args = doc.value("arguments").toObject().value("game").toArray();
    for (const auto& arg : args) {
        if (arg.isString()) {
            out.append(arg.toString());
        } else if (arg.isObject()) {
            for (const auto& value : arg.toObject().value("values").toArray())
                out.append(value.toString());
        }
    }
    return out;
}

/// Value that follows a `--flag value` pair in the flattened argument list.
QString argumentValue(const QStringList& args, const QString& flag)
{
    for (int i = 0; i + 1 < args.size(); ++i) {
        if (args.at(i) == flag)
            return args.at(i + 1);
    }
    return {};
}

QStringList libraryNames(const QJsonObject& doc)
{
    QStringList names;
    for (const auto& lib : doc.value("libraries").toArray()) {
        const auto name = lib.toObject().value("name").toString();
        if (!name.isEmpty())
            names.append(name);
    }
    return names;
}

/// First `1.x[.y]` looking token inside a string that is also a known version.
QString minecraftVersionIn(const QString& text, const QSet<QString>& known)
{
    static const QRegularExpression re(QStringLiteral("\\b(1\\.\\d+(?:\\.\\d+)?)\\b"));
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const auto candidate = it.next().captured(1);
        if (known.contains(candidate))
            return candidate;
    }
    return {};
}

}  // namespace

QStringList TLauncherImportTask::listVersionFolders(const QString& versionsDir)
{
    QStringList out;
    QDir dir(versionsDir);
    if (!dir.exists())
        return out;

    for (const auto& entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        // A version folder is identified by "<folder>/<folder>.json"; the
        // `versions/` directory also holds loose files such as manifests.
        const auto json = FS::PathCombine(entry.absoluteFilePath(), entry.fileName() + QStringLiteral(".json"));
        if (QFileInfo::exists(json))
            out.append(entry.absoluteFilePath());
    }
    return out;
}

TLauncherVersionInfo TLauncherImportTask::inspect(const QString& versionFolder, const QStringList& knownMinecraftVersions)
{
    TLauncherVersionInfo info;
    const QDir folder(versionFolder);
    info.id = folder.dirName();

    const auto jsonPath = FS::PathCombine(versionFolder, info.id + QStringLiteral(".json"));
    if (!QFileInfo::exists(jsonPath)) {
        info.problem = QObject::tr("В папке нет файла %1.json").arg(info.id);
        return info;
    }

    QJsonObject doc;
    try {
        doc = Json::requireObject(Json::requireDocument(jsonPath, QStringLiteral("version json")));
    } catch (const Exception& e) {
        info.problem = QObject::tr("Не удалось прочитать JSON: %1").arg(e.cause());
        return info;
    }

    const QSet<QString> known(knownMinecraftVersions.begin(), knownMinecraftVersions.end());
    const auto libs = libraryNames(doc);
    const auto gameArgs = flattenGameArguments(doc);

    // 1. NeoForge puts the exact loader and game version into the launch
    //    arguments, which beats guessing from transitive helper libraries.
    const auto neoForge = argumentValue(gameArgs, QStringLiteral("--fml.neoForgeVersion"));
    const auto neoMc = argumentValue(gameArgs, QStringLiteral("--fml.mcVersion"));
    if (!neoForge.isEmpty() && known.contains(neoMc)) {
        info.minecraftVersion = neoMc;
        info.loaderUid = QStringLiteral("net.neoforged");
        info.loaderVersion = neoForge;
        info.detectedBy = QStringLiteral("аргументы запуска NeoForge");
        return info;
    }

    // 2. Forge: net.minecraftforge:forge:<mc>-<loader>[:classifier]
    static const QRegularExpression forgeRe(QStringLiteral("^net\\.minecraftforge:forge:([0-9][^-]*)-(.+)$"));
    for (const auto& name : libs) {
        const auto match = forgeRe.match(name);
        if (match.hasMatch() && known.contains(match.captured(1))) {
            info.minecraftVersion = match.captured(1);
            info.loaderUid = QStringLiteral("net.minecraftforge");
            // The artefact may carry a classifier after a colon; it is not part
            // of the loader version.
            info.loaderVersion = match.captured(2).section(QLatin1Char(':'), 0, 0);
            info.detectedBy = QStringLiteral("библиотека Forge");
            return info;
        }
    }

    // 3. Modern Forge ships the version on the fml loader library instead.
    static const QRegularExpression fmlRe(QStringLiteral("^net\\.minecraftforge:fmlloader:([0-9][^-]*)-(.+)$"));
    for (const auto& name : libs) {
        const auto match = fmlRe.match(name);
        if (match.hasMatch() && known.contains(match.captured(1))) {
            info.minecraftVersion = match.captured(1);
            info.loaderUid = QStringLiteral("net.minecraftforge");
            info.loaderVersion = match.captured(2).section(QLatin1Char(':'), 0, 0);
            info.detectedBy = QStringLiteral("библиотека FML");
            return info;
        }
    }

    // 4. Fabric: the intermediary library names the game version, the loader
    //    library names the loader version.
    static const QRegularExpression fabMcRe(QStringLiteral("^net\\.fabricmc:intermediary:(\\S+)$"));
    static const QRegularExpression fabLoaderRe(QStringLiteral("^net\\.fabricmc:fabric-loader:(\\S+)$"));
    QString fabricMc;
    QString fabricLoader;
    for (const auto& name : libs) {
        const auto mcMatch = fabMcRe.match(name);
        if (mcMatch.hasMatch() && known.contains(mcMatch.captured(1)))
            fabricMc = mcMatch.captured(1);
        const auto loaderMatch = fabLoaderRe.match(name);
        if (loaderMatch.hasMatch())
            fabricLoader = loaderMatch.captured(1);
    }
    if (!fabricMc.isEmpty()) {
        info.minecraftVersion = fabricMc;
        info.loaderUid = fabricLoader.isEmpty() ? QString() : QStringLiteral("net.fabricmc.fabric-loader");
        info.loaderVersion = fabricLoader;
        info.detectedBy = QStringLiteral("библиотеки Fabric");
        return info;
    }

    // 5. OptiFine-only builds have no loader component at all. The base game
    //    version is encoded in the OptiFine artefact name.
    static const QRegularExpression optifineRe(QStringLiteral("^optifine:OptiFine:(\\S+)$"));
    for (const auto& name : libs) {
        const auto match = optifineRe.match(name);
        if (!match.hasMatch())
            continue;
        const auto candidate = match.captured(1).section(QLatin1Char('_'), 0, 0);
        if (known.contains(candidate)) {
            info.minecraftVersion = candidate;
            info.detectedBy = QStringLiteral("библиотека OptiFine");
            return info;
        }
    }

    // 6. Last resort: the version is often right in the folder or id name.
    for (const auto& text : { info.id, doc.value("id").toString() }) {
        const auto candidate = minecraftVersionIn(text, known);
        if (!candidate.isEmpty()) {
            info.minecraftVersion = candidate;
            info.detectedBy = QStringLiteral("название версии");
            return info;
        }
    }

    info.problem = QObject::tr("Не удалось определить версию Minecraft — в папке нет ни библиотек загрузчика, "
                               "ни номера версии в названии");
    return info;
}

TLauncherImportTask::TLauncherImportTask(const TLauncherVersionInfo& info, QString versionFolder)
    : m_info(info), m_versionFolder(std::move(versionFolder))
{}

std::unique_ptr<MinecraftInstance> TLauncherImportTask::createInstance()
{
    setStatus(tr("Создание сборки из «%1»").arg(m_info.id));

    auto inst = std::make_unique<MinecraftInstance>(m_globalSettings,
                                                    std::make_unique<INISettingsObject>(FS::PathCombine(m_stagingPath, "instance.cfg")),
                                                    m_stagingPath);
    SettingsObject::Lock lock(inst->settings());

    auto components = inst->getPackProfile();
    components->buildingFromScratch();
    components->setComponentVersion("net.minecraft", m_info.minecraftVersion, true);

    if (!m_info.loaderUid.isEmpty() && !m_info.loaderVersion.isEmpty())
        components->setComponentVersion(m_info.loaderUid, m_info.loaderVersion);

    inst->setName(name());
    inst->setIconKey(m_instIcon);

    // Carry the player's own content across: mods, configs, saves and options
    // are the whole reason for importing an existing version in the first place.
    // The jars and libraries are deliberately not copied - they are downloaded
    // again from the official metadata.
    static const QStringList carried = {
        "mods", "config", "defaultconfigs", "resourcepacks", "shaderpacks", "saves",
        "options.txt", "optionsof.txt", "optionsshaders.txt", "servers.dat", "usercache.json",
    };

    const QString targetRoot = FS::PathCombine(m_stagingPath, ".minecraft");
    if (!FS::ensureFolderPathExists(targetRoot)) {
        qWarning() << "[tlauncher-import] cannot create" << targetRoot;
        return inst;
    }

    for (const auto& entry : carried) {
        const auto source = FS::PathCombine(m_versionFolder, entry);
        if (!QFileInfo::exists(source))
            continue;
        const auto target = FS::PathCombine(targetRoot, entry);

        const QFileInfo sourceInfo(source);
        if (sourceInfo.isDir()) {
            QDirIterator it(source, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                const auto relative = QDir(source).relativeFilePath(it.filePath());
                const auto destination = FS::PathCombine(target, relative);
                if (it.fileInfo().isDir()) {
                    FS::ensureFolderPathExists(destination);
                } else {
                    FS::ensureFolderPathExists(QFileInfo(destination).absolutePath());
                    if (!QFile::copy(it.filePath(), destination))
                        qWarning() << "[tlauncher-import] could not copy" << it.filePath();
                }
            }
        } else {
            if (!QFile::copy(source, target))
                qWarning() << "[tlauncher-import] could not copy" << source;
        }
    }

    return inst;
}
