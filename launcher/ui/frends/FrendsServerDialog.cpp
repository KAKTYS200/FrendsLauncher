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

#include "FrendsServerDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QUrlQuery>
#include <QVBoxLayout>

#include <functional>

#include "Application.h"
#include "MMCZip.h"
#include "settings/SettingsObject.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "java/JavaUtils.h"

namespace {
// The catalog endpoints (Mojang, Paper, Modrinth, CurseForge) live in
// FrendsServerCatalog; these are the ones this dialog still calls itself.
const QString kFabricMetaUrl = QStringLiteral("https://meta.fabricmc.net/v2/versions");
const QString kForgeMavenUrl = QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/");
const QString kNeoForgeMavenUrl = QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/");
/// CurseForge hands out download links per file id rather than putting them in
/// the catalog answer, so the dialog resolves them itself.
const QString kCurseModsUrl = QStringLiteral("https://api.curseforge.com/v1/mods/");

/// Files inside a modpack must never escape the server folder.
QString safeRelativePath(const QString& path)
{
    QString clean = QDir::fromNativeSeparators(path);
    while (clean.startsWith(QLatin1Char('/')))
        clean.remove(0, 1);
    if (clean.contains(QLatin1String("..")))
        return {};
    return clean;
}

void fetchJson(QNetworkAccessManager* net, const QUrl& url, const QByteArray& apiKey,
               std::function<void(const QJsonDocument&, const QString&)> done)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FrendsLauncher/1.1"));
    if (!apiKey.isEmpty())
        request.setRawHeader("x-api-key", apiKey);

    auto* reply = net->get(request);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, done] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done(QJsonDocument(), reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isNull())
            done(QJsonDocument(), QObject::tr("ответ сервера не разобрался"));
        else
            done(doc, QString());
    });
}
}  // namespace

FrendsServerDialog::FrendsServerDialog(QWidget* parent) : QDialog(parent)
{
    m_net = new QNetworkAccessManager(this);
    m_process = new QProcess(this);
    m_installer = new QProcess(this);
    m_catalog = new FrendsServerCatalog(m_net, this);

    connect(m_catalog, &FrendsServerCatalog::minecraftVersionsReady, this, [this](const QVector<QString>& versions) {
        m_vanillaVersions = versions;
        if (currentCore() != Core::Vanilla)
            return;
        m_version->clear();
        for (const auto& id : qAsConst(m_vanillaVersions))
            m_version->addItem(id);
        setStatus(tr("Выберите версию и нажмите «Скачать сервер»."));
    });

    connect(m_catalog, &FrendsServerCatalog::paperVersionsReady, this, [this](const QVector<QString>& versions) {
        m_paperVersions = versions;
        if (currentCore() != Core::Paper)
            return;
        m_version->clear();
        for (const auto& id : qAsConst(m_paperVersions))
            m_version->addItem(id);
        setStatus(tr("Выберите версию и нажмите «Скачать сервер»."));
    });

    connect(m_catalog, &FrendsServerCatalog::packsReady, this, &FrendsServerDialog::showPacks);
    connect(m_catalog, &FrendsServerCatalog::packFilesReady, this, &FrendsServerDialog::showPackFiles);
    connect(m_catalog, &FrendsServerCatalog::failed, this, [this](const QString& message) { setStatus(message, true); });

    buildUi();
    applyStyle();
    loadVersions();
}

FrendsServerDialog::~FrendsServerDialog()
{
    for (auto* process : { m_process, m_installer }) {
        if (process->state() != QProcess::NotRunning) {
            process->write("stop\n");
            process->waitForFinished(4000);
            process->kill();
            process->waitForFinished(2000);
        }
    }
}

void FrendsServerDialog::buildUi()
{
    setWindowTitle(tr("Создать сервер"));
    setMinimumWidth(620);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto* title = new QLabel(tr("Сервер Minecraft"), this);
    title->setObjectName("serverTitle");
    layout->addWidget(title);

    auto* subtitle = new QLabel(
        tr("Ядро, модпак с Modrinth или серверную сборку CurseForge можно скачать и запустить прямо здесь."), this);
    subtitle->setObjectName("serverSubtitle");
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);

    m_core = new QComboBox(this);
    m_core->addItem(tr("Ванильный (Mojang)"), QStringLiteral("vanilla"));
    m_core->addItem(tr("Paper (оптимизированный)"), QStringLiteral("paper"));
    // Modpack servers are built but not offered yet: the loader install for
    // Forge/NeoForge needs a round of testing before players see it.
    // m_core->addItem(tr("Модпак Modrinth"), QStringLiteral("modrinth"));
    // m_core->addItem(tr("Модпак CurseForge"), QStringLiteral("curseforge"));
    form->addRow(tr("Ядро:"), m_core);

    m_version = new QComboBox(this);
    m_version->setMinimumContentsLength(14);
    form->addRow(tr("Версия:"), m_version);

    auto* searchRow = new QHBoxLayout();
    searchRow->setSpacing(6);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Название сборки, например Better MC"));
    m_searchButton = new QPushButton(tr("Найти"), this);
    m_searchButton->setCursor(Qt::PointingHandCursor);
    searchRow->addWidget(m_search, 1);
    searchRow->addWidget(m_searchButton);
    form->addRow(tr("Поиск сборки:"), searchRow);

    m_pack = new QComboBox(this);
    m_pack->setMinimumContentsLength(24);
    form->addRow(tr("Сборка:"), m_pack);

    m_packFile = new QComboBox(this);
    m_packFile->setMinimumContentsLength(24);
    form->addRow(tr("Версия сборки:"), m_packFile);

    auto* folderRow = new QHBoxLayout();
    folderRow->setSpacing(6);
    m_folder = new QLineEdit(this);
    m_folder->setText(QDir(APPLICATION->dataRoot()).filePath(QStringLiteral("servers/minecraft")));
    m_browseButton = new QPushButton(tr("Обзор…"), this);
    m_browseButton->setCursor(Qt::PointingHandCursor);
    folderRow->addWidget(m_folder, 1);
    folderRow->addWidget(m_browseButton);
    form->addRow(tr("Папка сервера:"), folderRow);

    m_memory = new QSpinBox(this);
    m_memory->setRange(512, 32768);
    m_memory->setSingleStep(512);
    m_memory->setSuffix(tr(" МБ"));
    m_memory->setValue(4096);
    form->addRow(tr("Память:"), m_memory);

    layout->addLayout(form);

    m_eula = new QCheckBox(tr("Принимаю Minecraft EULA (файл eula.txt)"), this);
    m_eula->setChecked(true);
    layout->addWidget(m_eula);

    m_status = new QLabel(tr("Загружаю список версий…"), this);
    m_status->setObjectName("serverStatus");
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_log = new QPlainTextEdit(this);
    m_log->setObjectName("serverLog");
    m_log->setReadOnly(true);
    m_log->setMinimumHeight(150);
    m_log->setMaximumBlockCount(4000);
    m_log->setPlaceholderText(tr("Здесь появится консоль сервера."));
    layout->addWidget(m_log, 1);

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(8);
    m_downloadButton = new QPushButton(tr("Скачать сервер"), this);
    m_startButton = new QPushButton(tr("Запустить"), this);
    m_stopButton = new QPushButton(tr("Остановить"), this);
    m_closeButton = new QPushButton(tr("Закрыть"), this);
    for (auto* button : { m_downloadButton, m_startButton, m_stopButton, m_closeButton })
        button->setCursor(Qt::PointingHandCursor);

    m_downloadButton->setObjectName("serverPrimary");
    m_startButton->setObjectName("serverPrimary");
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(false);

    buttons->addWidget(m_downloadButton);
    buttons->addWidget(m_startButton);
    buttons->addWidget(m_stopButton);
    buttons->addStretch(1);
    buttons->addWidget(m_closeButton);
    layout->addLayout(buttons);

    connect(m_core, &QComboBox::currentIndexChanged, this, [this, form] {
        const auto core = currentCore();
        const bool modpack = core == Core::Modrinth || core == Core::CurseForge;
        form->setRowVisible(m_version, !modpack);
        form->setRowVisible(m_search, modpack);
        form->setRowVisible(m_pack, modpack);
        form->setRowVisible(m_packFile, modpack);

        m_jarPath.clear();
        m_startButton->setEnabled(false);

        if (core == Core::Vanilla || core == Core::Paper) {
            m_version->clear();
            const auto& list = core == Core::Paper ? m_paperVersions : m_vanillaVersions;
            for (const auto& id : list)
                m_version->addItem(id);
            setStatus(list.isEmpty() ? tr("Список версий не загрузился.") : tr("Выберите версию и нажмите «Скачать сервер»."),
                      list.isEmpty());
        } else if (core == Core::Modrinth) {
            loadModrinthPacks();
        } else {
            loadCurseForgePacks();
        }
    });

    connect(m_searchButton, &QPushButton::clicked, this, [this] {
        if (currentCore() == Core::Modrinth)
            loadModrinthPacks();
        else
            loadCurseForgePacks();
    });
    connect(m_search, &QLineEdit::returnPressed, m_searchButton, &QPushButton::click);

    connect(m_pack, &QComboBox::currentIndexChanged, this, [this] {
        m_packFile->clear();
        m_packFiles.clear();
        const QString id = m_pack->currentData().toString();
        if (id.isEmpty())
            return;
        setStatus(tr("Загружаю версии сборки…"));
        if (currentCore() == Core::Modrinth)
            loadModrinthFiles(id);
        else
            loadCurseForgeFiles(id);
    });

    connect(m_browseButton, &QPushButton::clicked, this, &FrendsServerDialog::browseFolder);
    connect(m_downloadButton, &QPushButton::clicked, this, &FrendsServerDialog::startDownload);
    connect(m_startButton, &QPushButton::clicked, this, &FrendsServerDialog::startServer);
    connect(m_stopButton, &QPushButton::clicked, this, &FrendsServerDialog::stopServer);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::close);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        appendLog(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        appendLog(QString::fromLocal8Bit(m_process->readAllStandardError()));
    });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        appendLog(tr("\n[сервер остановлен, код %1]\n").arg(code));
        setStatus(tr("Сервер остановлен."));
        m_startButton->setEnabled(!m_jarPath.isEmpty());
        m_stopButton->setEnabled(false);
        m_downloadButton->setEnabled(true);
    });

    connect(m_installer, &QProcess::readyReadStandardOutput, this, [this] {
        appendLog(QString::fromLocal8Bit(m_installer->readAllStandardOutput()));
    });
    connect(m_installer, &QProcess::readyReadStandardError, this, [this] {
        appendLog(QString::fromLocal8Bit(m_installer->readAllStandardError()));
    });
    // One hook, made once: the installer is synchronous, so it has to know what
    // to do when the jar it was given finishes.
    connect(m_installer, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        appendLog(tr("[установка %1 завершена, код %2]\n").arg(m_installerLabel).arg(code));
        if (code != 0) {
            setBusy(false);
            setStatus(tr("Установщик %1 завершился с ошибкой %2, подробности в консоли.")
                          .arg(m_installerLabel)
                          .arg(code),
                      true);
            return;
        }
        QFile::remove(m_installerJarPath);
        finalizeServer();
    });

    // Modpacks bring their own rows, so start on the plain version picker.
    m_core->setCurrentIndex(0);
    form->setRowVisible(m_search, false);
    form->setRowVisible(m_pack, false);
    form->setRowVisible(m_packFile, false);
}

void FrendsServerDialog::applyStyle()
{
    setStyleSheet(QStringLiteral(
        "QDialog { background: #EEF2F7; }"
        "QLabel#serverTitle { color: #2F3A45; font-size: 18px; font-weight: 800; background: transparent; }"
        "QLabel#serverSubtitle { color: #7A838D; font-size: 12px; background: transparent; }"
        "QLabel#serverStatus { color: #585858; font-size: 12px; background: transparent; }"
        "QLabel { color: #585858; font-size: 13px; background: transparent; }"
        "QLineEdit, QComboBox, QSpinBox { background: #FFFFFF; color: #3A3A3A; border: 1px solid #C9D2DB;"
        " border-radius: 4px; padding: 4px 8px; min-height: 20px; }"
        "QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1px solid #29A0D8; }"
        "QComboBox QAbstractItemView { background: #FFFFFF; color: #3A3A3A; selection-background-color: #F9D900;"
        " selection-color: #4A3B00; outline: none; }"
        "QCheckBox { color: #585858; font-size: 12px; background: transparent; }"
        "QPlainTextEdit#serverLog { background: #1E2329; color: #D6DEE6; border: 1px solid #B9C4CF;"
        " border-radius: 4px; font-family: Consolas, 'Courier New', monospace; font-size: 12px; }"
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #fdfdfd, stop:0.5 #ececec,"
        " stop:1 #e2e2e2); color: #333333; border: 1px solid #B9B9B9; border-radius: 5px; padding: 6px 16px;"
        " font-size: 13px; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #eaeaea); }"
        "QPushButton:disabled { color: #9A9A9A; background: #EDEDED; border: 1px solid #D5D5D5; }"
        "QPushButton#serverPrimary { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FF8A4C,"
        " stop:1 #E0491B); color: #FFFFFF; border: 1px solid #C94316; font-weight: 700; }"
        "QPushButton#serverPrimary:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFA068,"
        " stop:1 #EE5A26); }"
        "QPushButton#serverPrimary:disabled { background: #E7E7E7; color: #9A9A9A; border: 1px solid #D5D5D5; }"));
}

FrendsServerDialog::Core FrendsServerDialog::currentCore() const
{
    const QString id = m_core->currentData().toString();
    if (id == QLatin1String("paper"))
        return Core::Paper;
    if (id == QLatin1String("modrinth"))
        return Core::Modrinth;
    if (id == QLatin1String("curseforge"))
        return Core::CurseForge;
    return Core::Vanilla;
}

void FrendsServerDialog::loadVersions()
{
    m_catalog->loadMinecraftVersions();
    m_catalog->loadPaperVersions();
}

void FrendsServerDialog::loadModrinthPacks()
{
    setStatus(tr("Ищу сборки на Modrinth…"));
    m_catalog->loadModrinthPacks(m_search->text());
}

void FrendsServerDialog::loadCurseForgePacks()
{
    setStatus(tr("Ищу сборки на CurseForge…"));
    m_catalog->loadCurseForgePacks(m_search->text(), APPLICATION->getFlameAPIKey());
}

void FrendsServerDialog::loadModrinthFiles(const QString& projectId)
{
    m_catalog->loadModrinthFiles(projectId);
}

void FrendsServerDialog::loadCurseForgeFiles(const QString& modId)
{
    m_catalog->loadCurseForgeFiles(modId, APPLICATION->getFlameAPIKey());
}

void FrendsServerDialog::showPacks(const QVector<Pack>& packs)
{
    m_packs = packs;
    m_pack->clear();
    m_packFile->clear();
    m_packFiles.clear();
    for (const auto& pack : packs)
        m_pack->addItem(pack.note.isEmpty() ? pack.title : tr("%1 — %2").arg(pack.title, pack.note), pack.id);
    setStatus(packs.isEmpty() ? tr("Ничего не нашлось, попробуйте другой запрос.") : tr("Выберите сборку из списка."),
              packs.isEmpty());
}

void FrendsServerDialog::showPackFiles(const QVector<PackFile>& files)
{
    m_packFiles = files;
    m_packFile->clear();
    for (const auto& file : files)
        m_packFile->addItem(file.label);
    setStatus(files.isEmpty() ? tr("У этой сборки нет скачиваемых версий.")
                              : tr("Выберите версию сборки и нажмите «Скачать сервер»."),
              files.isEmpty());
}

void FrendsServerDialog::browseFolder()
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Папка сервера"), m_folder->text());
    if (!chosen.isEmpty())
        m_folder->setText(QDir::toNativeSeparators(chosen));
}

void FrendsServerDialog::startDownload()
{
    if (m_busy)
        return;

    m_jarPath.clear();
    m_launchMode.clear();
    m_startButton->setEnabled(false);
    setBusy(true);

    switch (currentCore()) {
        case Core::Vanilla:
        case Core::Paper: {
            const QString version = m_version->currentText();
            if (version.isEmpty()) {
                setBusy(false);
                setStatus(tr("Сначала выберите версию."), true);
                return;
            }
            if (currentCore() == Core::Paper)
                resolvePaper(version);
            else
                resolveVanilla(version);
            return;
        }
        case Core::Modrinth: {
            const int index = m_packFile->currentIndex();
            if (index < 0 || index >= m_packFiles.size()) {
                setBusy(false);
                setStatus(tr("Сначала выберите сборку и её версию."), true);
                return;
            }
            downloadModpack(QUrl(m_packFiles.at(index).url), true);
            return;
        }
        case Core::CurseForge: {
            const int index = m_packFile->currentIndex();
            if (index < 0 || index >= m_packFiles.size()) {
                setBusy(false);
                setStatus(tr("Сначала выберите сборку и её версию."), true);
                return;
            }
            const QString modId = m_pack->currentData().toString();
            const int fileId = m_packFiles.at(index).fileId;
            const QString key = APPLICATION->getFlameAPIKey();
            if (key.isEmpty()) {
                setBusy(false);
                setStatus(tr("Для CurseForge нужен API-ключ в настройках."), true);
                return;
            }
            setStatus(tr("Готовлю ссылку на скачивание…"));
            fetchJson(m_net, QUrl(kCurseModsUrl + modId + QStringLiteral("/files/%1/download-url").arg(fileId)),
                      key.toUtf8(), [this](const QJsonDocument& doc, const QString& error) {
                          if (!error.isEmpty()) {
                              setBusy(false);
                              setStatus(tr("CurseForge не дал ссылку: %1").arg(error), true);
                              return;
                          }
                          const QUrl url(doc.object().value("data").toString());
                          if (!url.isValid()) {
                              setBusy(false);
                              setStatus(tr("Автор запретил сторонние загрузки этой сборки."), true);
                              return;
                          }
                          downloadModpack(url, false);
                      });
            return;
        }
    }
}

void FrendsServerDialog::resolveVanilla(const QString& version)
{
    const QUrl url = m_catalog->minecraftVersionUrl(version);
    if (!url.isValid()) {
        setBusy(false);
        setStatus(tr("Для версии %1 нет серверного ядра.").arg(version), true);
        return;
    }
    fetchJson(m_net, url, {}, [this, version](const QJsonDocument& doc, const QString& error) {
        if (!error.isEmpty()) {
            setBusy(false);
            setStatus(tr("Не удалось получить метаданные версии: %1").arg(error), true);
            return;
        }
        const auto server = doc.object().value("downloads").toObject().value("server").toObject();
        const QUrl jar(server.value("url").toString());
        if (!jar.isValid()) {
            setBusy(false);
            setStatus(tr("У версии %1 нет серверного jar.").arg(version), true);
            return;
        }
        downloadSingleFile(jar, QStringLiteral("server-%1.jar").arg(version));
    });
}

void FrendsServerDialog::resolvePaper(const QString& version)
{
    fetchJson(m_net, QUrl(QStringLiteral("https://fill.papermc.io/v3/projects/paper/versions/%1/builds").arg(version)), {},
              [this, version](const QJsonDocument& doc, const QString& error) {
                  if (!error.isEmpty()) {
                      setBusy(false);
                      setStatus(tr("Paper не отдал сборки для %1: %2").arg(version, error), true);
                      return;
                  }
                  const auto builds = doc.array();
                  if (builds.isEmpty()) {
                      setBusy(false);
                      setStatus(tr("У Paper нет сборок для %1.").arg(version), true);
                      return;
                  }
                  // The catalog is newest first; the first entry is the current build.
                  const auto build = builds.first().toObject();
                  const auto server = build.value("downloads").toObject().value("server:default").toObject();
                  const QUrl jar(server.value("url").toString());
                  if (!jar.isValid()) {
                      setBusy(false);
                      setStatus(tr("Paper не дал ссылку на jar для %1.").arg(version), true);
                      return;
                  }
                  downloadSingleFile(jar, QStringLiteral("paper-%1-%2.jar").arg(version).arg(build.value("id").toInt()));
              });
}

void FrendsServerDialog::downloadSingleFile(const QUrl& url, const QString& fileName)
{
    const QDir folder(serverFolder());
    if (!folder.exists() && !QDir().mkpath(folder.absolutePath())) {
        setBusy(false);
        setStatus(tr("Не удалось создать папку %1.").arg(folder.absolutePath()), true);
        return;
    }

    const QString target = folder.filePath(fileName);
    auto* file = new QFile(target, this);
    if (!file->open(QIODevice::WriteOnly)) {
        file->deleteLater();
        setBusy(false);
        setStatus(tr("Не удалось записать %1.").arg(target), true);
        return;
    }

    setStatus(tr("Скачиваю %1…").arg(fileName));
    auto* reply = m_net->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::downloadProgress, this, [this, fileName](qint64 received, qint64 total) {
        if (total > 0)
            setStatus(tr("Скачиваю %1 — %2 из %3 МБ…")
                          .arg(fileName)
                          .arg(received / (1024 * 1024))
                          .arg(total / (1024 * 1024)));
    });
    connect(reply, &QNetworkReply::readyRead, this, [reply, file] { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, target] {
        reply->deleteLater();
        file->write(reply->readAll());
        file->close();
        file->deleteLater();

        setBusy(false);
        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(target);
            setStatus(tr("Загрузка не удалась: %1").arg(reply->errorString()), true);
            return;
        }
        m_jarPath = target;
        m_launchMode = QStringLiteral("jar");
        writeServerFiles();
        m_startButton->setEnabled(true);
        setStatus(tr("Сервер готов: %1").arg(QDir::toNativeSeparators(target)));
        appendLog(tr("[ядро сохранено: %1]\n").arg(QDir::toNativeSeparators(target)));
    });
}

void FrendsServerDialog::downloadModpack(const QUrl& url, bool mrpack)
{
    const QDir folder(serverFolder());
    if (!folder.exists() && !QDir().mkpath(folder.absolutePath())) {
        setBusy(false);
        setStatus(tr("Не удалось создать папку %1.").arg(folder.absolutePath()), true);
        return;
    }

    m_archiveIsMrpack = mrpack;
    m_tempArchive = folder.filePath(mrpack ? QStringLiteral(".frends-pack.mrpack") : QStringLiteral(".frends-pack.zip"));

    auto* file = new QFile(m_tempArchive, this);
    if (!file->open(QIODevice::WriteOnly)) {
        file->deleteLater();
        setBusy(false);
        setStatus(tr("Не удалось записать %1.").arg(m_tempArchive), true);
        return;
    }

    setStatus(tr("Скачиваю сборку…"));
    auto* reply = m_net->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0)
            setStatus(tr("Скачиваю сборку — %1 из %2 МБ…").arg(received / (1024 * 1024)).arg(total / (1024 * 1024)));
    });
    connect(reply, &QNetworkReply::readyRead, this, [reply, file] { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file] {
        reply->deleteLater();
        file->write(reply->readAll());
        file->close();
        file->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(m_tempArchive);
            setBusy(false);
            setStatus(tr("Сборка не скачалась: %1").arg(reply->errorString()), true);
            return;
        }
        unpackModpack();
    });
}

void FrendsServerDialog::unpackModpack()
{
    const QDir folder(serverFolder());

    if (!m_archiveIsMrpack) {
        // A CurseForge server pack is a ready-to-run folder: unpack and go.
        setStatus(tr("Распаковываю серверную сборку…"));
        appendLog(tr("[распаковка %1]\n").arg(QFileInfo(m_tempArchive).fileName()));
        MMCZip::extractDir(m_tempArchive, folder.absolutePath());
        QFile::remove(m_tempArchive);
        finalizeServer();
        return;
    }

    QTemporaryDir temp;
    if (!temp.isValid() || !MMCZip::extractFile(m_tempArchive, QStringLiteral("modrinth.index.json"), temp.path())) {
        QFile::remove(m_tempArchive);
        setBusy(false);
        setStatus(tr("В сборке нет modrinth.index.json — файл повреждён."), true);
        return;
    }

    QFile index(temp.filePath(QStringLiteral("modrinth.index.json")));
    if (!index.open(QIODevice::ReadOnly)) {
        QFile::remove(m_tempArchive);
        setBusy(false);
        setStatus(tr("Не удалось прочитать описание сборки."), true);
        return;
    }
    const auto root = QJsonDocument::fromJson(index.readAll()).object();
    index.close();

    const auto dependencies = root.value("dependencies").toObject();
    m_packMinecraft = dependencies.value("minecraft").toString();
    if (dependencies.contains("fabric-loader")) {
        m_packLoader = QStringLiteral("fabric");
        m_packLoaderVersion = dependencies.value("fabric-loader").toString();
    } else if (dependencies.contains("neoforge")) {
        m_packLoader = QStringLiteral("neoforge");
        m_packLoaderVersion = dependencies.value("neoforge").toString();
    } else if (dependencies.contains("forge")) {
        m_packLoader = QStringLiteral("forge");
        m_packLoaderVersion = dependencies.value("forge").toString();
    } else if (dependencies.contains("quilt-loader")) {
        m_packLoader = QStringLiteral("quilt");
        m_packLoaderVersion = dependencies.value("quilt-loader").toString();
    }

    // overrides are the pack author's configs; server-overrides replace them
    // when the pack ships server-specific ones.
    MMCZip::extractDir(m_tempArchive, QStringLiteral("overrides"), folder.absolutePath());
    MMCZip::extractDir(m_tempArchive, QStringLiteral("server-overrides"), folder.absolutePath());
    QFile::remove(m_tempArchive);

    QVector<QueueItem> queue;
    for (const auto& entry : root.value("files").toArray()) {
        const auto object = entry.toObject();
        const auto env = object.value("env").toObject();
        if (env.value("server").toString() == QLatin1String("unsupported"))
            continue;
        const QString path = safeRelativePath(object.value("path").toString());
        const auto downloads = object.value("downloads").toArray();
        if (path.isEmpty() || downloads.isEmpty())
            continue;
        queue.push_back({ QUrl(downloads.first().toString()), path });
    }

    appendLog(tr("[сборка: Minecraft %1, загрузчик %2 %3, файлов %4]\n")
                  .arg(m_packMinecraft.isEmpty() ? QStringLiteral("?") : m_packMinecraft,
                       m_packLoader.isEmpty() ? QStringLiteral("vanilla") : m_packLoader,
                       m_packLoaderVersion,
                       QString::number(queue.size())));
    queueDownloads(queue);
}

void FrendsServerDialog::queueDownloads(const QVector<QueueItem>& items)
{
    m_queue = items;
    m_queueDone = 0;
    m_queueTotal = items.size();
    if (m_queueTotal == 0) {
        installLoader();
        return;
    }
    startNextQueuedDownload();
}

void FrendsServerDialog::startNextQueuedDownload()
{
    if (m_queueDone >= m_queueTotal) {
        appendLog(tr("[все файлы сборки на месте]\n"));
        installLoader();
        return;
    }

    const auto item = m_queue.at(m_queueDone);
    setStatus(tr("Скачиваю файлы сборки: %1 из %2 — %3")
                  .arg(m_queueDone + 1)
                  .arg(m_queueTotal)
                  .arg(QFileInfo(item.path).fileName()));

    const QString target = QDir(serverFolder()).filePath(item.path);
    QDir().mkpath(QFileInfo(target).absolutePath());

    auto* reply = m_net->get(QNetworkRequest(item.url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, target] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QFile file(target);
            if (file.open(QIODevice::WriteOnly))
                file.write(reply->readAll());
        } else {
            appendLog(tr("[пропущено: %1 — %2]\n").arg(QFileInfo(target).fileName(), reply->errorString()));
        }
        m_queueDone++;
        startNextQueuedDownload();
    });
}

void FrendsServerDialog::installLoader()
{
    if (m_packLoader.isEmpty()) {
        finalizeServer();
        return;
    }

    if (m_packLoader == QLatin1String("fabric")) {
        setStatus(tr("Ставлю Fabric %1…").arg(m_packLoaderVersion));
        // Fabric meta builds a ready launch jar, so no installer has to run.
        fetchJson(m_net, QUrl(kFabricMetaUrl + QStringLiteral("/installer")), {},
                  [this](const QJsonDocument& doc, const QString&) {
                      QString installer = QStringLiteral("1.0.1");
                      const auto list = doc.array();
                      if (!list.isEmpty())
                          installer = list.first().toObject().value("version").toString(installer);
                      const QUrl url(QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/%1/%2/%3/server/jar")
                                         .arg(m_packMinecraft, m_packLoaderVersion, installer));
                      downloadSingleFile(url, QStringLiteral("fabric-server-launch.jar"));
                  });
        return;
    }

    if (m_packLoader == QLatin1String("forge")) {
        const QString name = QStringLiteral("%1-%2").arg(m_packMinecraft, m_packLoaderVersion);
        downloadInstaller(QUrl(kForgeMavenUrl + name + QStringLiteral("/forge-%1-installer.jar").arg(name)),
                          QStringLiteral("forge-installer.jar"), tr("Forge %1").arg(m_packLoaderVersion));
        return;
    }

    if (m_packLoader == QLatin1String("neoforge")) {
        downloadInstaller(QUrl(kNeoForgeMavenUrl +
                               QStringLiteral("%1/neoforge-%1-installer.jar").arg(m_packLoaderVersion)),
                          QStringLiteral("neoforge-installer.jar"), tr("NeoForge %1").arg(m_packLoaderVersion));
        return;
    }

    setBusy(false);
    setStatus(tr("Сборки на Quilt пока не поддерживаются — выберите другую версию сборки."), true);
}

void FrendsServerDialog::downloadInstaller(const QUrl& url, const QString& fileName, const QString& label)
{
    const QDir folder(serverFolder());
    const QString target = folder.filePath(fileName);

    auto* file = new QFile(target, this);
    if (!file->open(QIODevice::WriteOnly)) {
        file->deleteLater();
        setBusy(false);
        setStatus(tr("Не удалось записать %1.").arg(target), true);
        return;
    }

    setStatus(tr("Скачиваю установщик %1…").arg(label));
    auto* reply = m_net->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::readyRead, this, [reply, file] { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, target, label] {
        reply->deleteLater();
        file->write(reply->readAll());
        file->close();
        file->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(target);
            setBusy(false);
            setStatus(tr("Установщик %1 не скачался: %2").arg(label, reply->errorString()), true);
            return;
        }
        runInstaller(target, label);
    });
}

void FrendsServerDialog::runInstaller(const QString& jarPath, const QString& label)
{
    const QString java = javaExecutable();
    if (java.isEmpty()) {
        setBusy(false);
        setStatus(tr("Java не найдена. Укажите путь к ней в настройках лаунчера, страница «Java»."), true);
        return;
    }

    setStatus(tr("Устанавливаю %1, это занимает пару минут…").arg(label));
    appendLog(tr("\n[установка %1]\n").arg(label));

    m_installerJarPath = jarPath;
    m_installerLabel = label;
    m_installer->setWorkingDirectory(serverFolder());
    m_installer->setProgram(java);
    m_installer->setArguments({ QStringLiteral("-jar"), QFileInfo(jarPath).fileName(), QStringLiteral("--installServer") });

    // The installer is synchronous on purpose: its libraries have to be in
    // place before the server can be started, and its log is worth reading.
    m_installer->start();
    if (!m_installer->waitForStarted(5000)) {
        setBusy(false);
        setStatus(tr("Установщик не запустился: %1").arg(m_installer->errorString()), true);
    }
}

void FrendsServerDialog::finalizeServer()
{
    writeServerFiles();
    setBusy(false);
    m_startButton->setEnabled(true);
    m_downloadButton->setEnabled(true);
    setStatus(tr("Сервер собран в %1. Нажмите «Запустить».").arg(QDir::toNativeSeparators(serverFolder())));
}

void FrendsServerDialog::writeServerFiles()
{
    const QDir folder(serverFolder());

    // eula.txt is what the server checks before it will start; the player ticks
    // the box above, so the file is written only when allowed.
    const QString eulaPath = folder.filePath(QStringLiteral("eula.txt"));
    if (m_eula->isChecked() && !QFileInfo::exists(eulaPath)) {
        QFile eula(eulaPath);
        if (eula.open(QIODevice::WriteOnly | QIODevice::Text))
            eula.write("# Generated by Frends Launcher\neula=true\n");
    }

    // Work out how the server is started: a ready jar, the run script Forge and
    // NeoForge leave behind, or their argument files.
    QString jar = QFileInfo(m_jarPath).fileName();
    if (jar.isEmpty() || !folder.exists(jar)) {
        jar.clear();
        for (const auto& entry : folder.entryList({ QStringLiteral("*.jar") }, QDir::Files)) {
            if (entry.contains(QLatin1String("installer"), Qt::CaseInsensitive))
                continue;
            jar = entry;
            break;
        }
    }

    if (folder.exists(QStringLiteral("run.bat"))) {
        m_launchMode = QStringLiteral("bat");
        m_jarPath = folder.filePath(QStringLiteral("run.bat"));
        // Forge reads the heap size from this file, not from the command line.
        QFile jvmArgs(folder.filePath(QStringLiteral("user_jvm_args.txt")));
        if (jvmArgs.open(QIODevice::WriteOnly | QIODevice::Text))
            jvmArgs.write(QStringLiteral("-Xmx%1M\n").arg(m_memory->value()).toUtf8());
    } else if (!jar.isEmpty()) {
        m_launchMode = QStringLiteral("jar");
        m_jarPath = folder.filePath(jar);
    } else {
        m_launchMode = QStringLiteral("args");
        m_jarPath = folder.absolutePath();
    }

    const QString java = javaExecutable();
    QFile script(folder.filePath(QStringLiteral("start.bat")));
    if (script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        script.write("@echo off\r\n");
        script.write("title Frends Launcher Server\r\n");
        const QString exe = QDir::toNativeSeparators(java.isEmpty() ? QStringLiteral("java") : java);
        if (m_launchMode == QLatin1String("bat")) {
            script.write("call run.bat\r\n");
        } else if (m_launchMode == QLatin1String("jar")) {
            script.write(QStringLiteral("\"%1\" -Xmx%2M -jar \"%3\" nogui\r\n")
                             .arg(exe)
                             .arg(m_memory->value())
                             .arg(jar)
                             .toUtf8());
        } else {
            script.write(QStringLiteral("\"%1\" -Xmx%2M @libraries/net/minecraftforge/forge/win_args.txt\r\n")
                             .arg(exe)
                             .arg(m_memory->value())
                             .toUtf8());
        }
        script.write("pause\r\n");
    }
}

void FrendsServerDialog::startServer()
{
    if (m_jarPath.isEmpty() || !QFileInfo::exists(m_jarPath)) {
        setStatus(tr("Сначала скачайте сервер."), true);
        return;
    }
    if (m_process->state() != QProcess::NotRunning) {
        setStatus(tr("Сервер уже запущен."), true);
        return;
    }

    const QString java = javaExecutable();
    if (java.isEmpty()) {
        setStatus(tr("Java не найдена. Укажите путь к ней в настройках лаунчера, страница «Java»."), true);
        return;
    }

    m_process->setWorkingDirectory(serverFolder());
    if (m_launchMode == QLatin1String("bat")) {
        m_process->setProgram(QStringLiteral("cmd"));
        m_process->setArguments({ QStringLiteral("/c"), QStringLiteral("run.bat") });
        appendLog(tr("[запуск: run.bat, память %1 МБ]\n").arg(m_memory->value()));
    } else if (m_launchMode == QLatin1String("args")) {
        m_process->setProgram(java);
        m_process->setArguments({ QStringLiteral("-Xmx%1M").arg(m_memory->value()),
                                  QStringLiteral("@libraries/net/minecraftforge/forge/win_args.txt"),
                                  QStringLiteral("nogui") });
        appendLog(tr("[запуск: Forge args]\n"));
    } else {
        m_process->setProgram(java);
        m_process->setArguments({ QStringLiteral("-Xmx%1M").arg(m_memory->value()), QStringLiteral("-jar"),
                                  QFileInfo(m_jarPath).fileName(), QStringLiteral("nogui") });
        appendLog(tr("[запуск: %1 -Xmx%2M -jar %3 nogui]\n")
                      .arg(QDir::toNativeSeparators(java))
                      .arg(m_memory->value())
                      .arg(QFileInfo(m_jarPath).fileName()));
    }

    m_process->start();
    if (!m_process->waitForStarted(5000)) {
        setStatus(tr("Сервер не запустился: %1").arg(m_process->errorString()), true);
        return;
    }

    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_downloadButton->setEnabled(false);
    setStatus(tr("Сервер запущен. Порт по умолчанию: 25565"));
}

void FrendsServerDialog::stopServer()
{
    if (m_process->state() == QProcess::NotRunning)
        return;
    setStatus(tr("Останавливаю сервер, мир сохраняется…"));
    m_process->write("stop\n");
    if (!m_process->waitForFinished(15000)) {
        m_process->terminate();
        if (!m_process->waitForFinished(5000))
            m_process->kill();
    }
}

void FrendsServerDialog::appendLog(const QString& text)
{
    if (text.isEmpty())
        return;
    m_log->moveCursor(QTextCursor::End);
    m_log->insertPlainText(text);
    m_log->moveCursor(QTextCursor::End);
}

void FrendsServerDialog::setStatus(const QString& text, bool error)
{
    m_status->setText(text);
    m_status->setStyleSheet(error ? QStringLiteral("color: #C0392B; font-size: 12px; background: transparent;")
                                 : QStringLiteral("color: #585858; font-size: 12px; background: transparent;"));
}

void FrendsServerDialog::setBusy(bool busy)
{
    m_busy = busy;
    m_downloadButton->setEnabled(!busy);
    m_core->setEnabled(!busy);
    m_version->setEnabled(!busy);
    m_pack->setEnabled(!busy);
    m_packFile->setEnabled(!busy);
    m_search->setEnabled(!busy);
    m_searchButton->setEnabled(!busy);
}

QString FrendsServerDialog::serverFolder() const
{
    QString path = m_folder->text().trimmed();
    if (path.isEmpty())
        path = QDir(APPLICATION->dataRoot()).filePath(QStringLiteral("servers/minecraft"));
    return QDir::fromNativeSeparators(path);
}

QString FrendsServerDialog::javaExecutable() const
{
    const QString configured = APPLICATION->settings()->get("JavaPath").toString();
    if (!configured.isEmpty() && QFileInfo::exists(configured))
        return configured;

    // The saved path goes stale as soon as Java is updated in place, and the
    // launcher already knows how to look for it, so borrow that search.
    JavaUtils utils;
    for (const auto& path : utils.FindJavaPaths()) {
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}
