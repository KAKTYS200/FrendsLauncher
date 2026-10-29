// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "CustomPage.h"
#include "ui_CustomPage.h"

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>

#include "Application.h"
#include "Filter.h"
#include "SysInfo.h"
#include "Version.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "minecraft/VanillaInstanceCreationTask.h"
#include "ui/frends/InstanceCreationSink.h"

namespace {
struct LoaderInfo {
    const char* label;
    const char* uid;
};

// Order matters: the first entry is the default selection.
const LoaderInfo s_loaders[] = {
    { QT_TRANSLATE_NOOP("CustomPage", "Vanilla"), nullptr },
    { QT_TRANSLATE_NOOP("CustomPage", "Forge"), "net.minecraftforge" },
    { QT_TRANSLATE_NOOP("CustomPage", "NeoForge"), "net.neoforged" },
    { QT_TRANSLATE_NOOP("CustomPage", "Fabric"), "net.fabricmc.fabric-loader" },
    { QT_TRANSLATE_NOOP("CustomPage", "Quilt"), "org.quiltmc.quilt-loader" },
    { QT_TRANSLATE_NOOP("CustomPage", "LiteLoader"), "com.mumfrey.liteloader" },
};

// Vanilla/Fabric/Quilt may leave the loader version empty; these need an explicit pick.
bool loaderRequiresVersion(const QString& uid)
{
    return uid == QLatin1String("net.minecraftforge") || uid == QLatin1String("net.neoforged") ||
           uid == QLatin1String("com.mumfrey.liteloader");
}

class ComboVersion : public BaseVersion {
   public:
    explicit ComboVersion(QString id) : m_id(std::move(id)) {}
    QString descriptor() const override { return m_id; }
    QString name() const override { return m_id; }
    QString typeString() const override { return QString(); }

   private:
    QString m_id;
};

/// Translates the raw version type ("release", "snapshot", …) for display.
class VersionTypeDelegate : public QStyledItemDelegate {
   public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);

        static const QHash<QString, QString> names = {
            { QStringLiteral("release"), CustomPage::tr("релиз") },
            { QStringLiteral("snapshot"), CustomPage::tr("снапшот") },
            { QStringLiteral("old_beta"), CustomPage::tr("старая бета") },
            { QStringLiteral("old_alpha"), CustomPage::tr("альфа") },
        };

        const auto it = names.constFind(option->text);
        if (it != names.constEnd())
            option->text = it.value();
    }
};

QString comboStyle()
{
    return QStringLiteral(
        "QComboBox { background: #FFFFFF; color: #585858; border: 1px solid #C9C9C9; border-radius: 2px; padding: 4px 8px; }"
        "QComboBox:disabled { background: #F2F2F2; color: #9A9A9A; }"
        "QComboBox::drop-down { width: 20px; border: none; background: #E9E9E9; }"
        "QComboBox QAbstractItemView { background: #FFFFFF; color: #585858; selection-background-color: #F9D900;"
        " selection-color: #4A3B00; outline: none; }");
}

QString inputStyle()
{
    return QStringLiteral(
        "QLineEdit { background: #FFFFFF; color: #585858; border: 1px solid #C9C9C9; border-radius: 2px;"
        " padding: 4px 8px; selection-background-color: #BFDDF2; }"
        "QLineEdit:focus { border: 1px solid #29A0D8; }");
}
}  // namespace

CustomPage::CustomPage(InstanceCreationSink* creationSink, QWidget* parent)
    : QWidget(parent), sink(creationSink), ui(new Ui::CustomPage)
{
    ui->setupUi(this);

    applyStyle();

    for (const auto& loader : s_loaders) {
        ui->loaderTypeCombo->addItem(tr(loader.label), QString::fromLatin1(loader.uid ? loader.uid : ""));
    }

    connect(ui->versionList, &VersionSelectWidget::selectedVersionChanged, this, &CustomPage::setSelectedVersion);
    connect(ui->loaderVersionList, &VersionSelectWidget::selectedVersionChanged, this, &CustomPage::setSelectedLoaderVersion);
    connect(ui->gameVersionCombo, QOverload<int>::of(&QComboBox::activated), this, &CustomPage::gameVersionComboChanged);
    connect(ui->loaderTypeCombo, QOverload<int>::of(&QComboBox::activated), this, &CustomPage::loaderTypeComboChanged);
    connect(ui->loaderVersionCombo, QOverload<int>::of(&QComboBox::activated), this, &CustomPage::loaderVersionComboChanged);
    connect(ui->instNameTextBox, &QLineEdit::textEdited, this, [this](const QString&) { m_nameEditedByUser = true; });
    connect(ui->instNameTextBox, &QLineEdit::textChanged, this, [this] { suggestCurrent(); });
    connect(ui->useLauncherMemoryCheck, &QCheckBox::toggled, this, [this](bool checked) { ui->memoryRow->setVisible(!checked); });
    // The toggle used to reveal a hidden panel; now it filters the visible list.
    connect(ui->listsToggleButton, &QPushButton::toggled, this, &CustomPage::versionFilterToggled);
    connect(ui->createButton, &QPushButton::clicked, this, [this] { this->sink->acceptCreation(); });
    connect(ui->cancelButton, &QPushButton::clicked, this, [this] { this->sink->rejectCreation(); });

    ui->memoryRow->setVisible(!ui->useLauncherMemoryCheck->isChecked());
    ui->memorySpin->setMinimum(512);
    ui->memorySpin->setMaximum(SysInfo::defaultMaxJvmMem());
    ui->memorySpin->setSingleStep(256);
    ui->memorySpin->setValue(2048);

    ui->createButton->setEnabled(false);
    ui->versionsPanel->setVisible(false);
}

CustomPage::~CustomPage()
{
    delete ui;
}

void CustomPage::applyStyle()
{
    // The Frends look reuses the palette of the launcher the interface is
    // modelled on: #585858 primary text, #ACACAC secondary, #F9D900 for the
    // call to action and #F3622D as the second accent. Buttons follow the
    // "modena" text-button spec of that same source: grey gradient, 1 px
    // #b9b9b9 border, 4 px radius, #333333 text.
    setStyleSheet(QStringLiteral(
        "QWidget#CustomPage { background: #ECECEC; }"
        "QWidget#body { background: #ECECEC; }"
        "QLabel { color: #585858; font-size: 13px; }"
        "QLabel#headerLabel { color: #FFFFFF; font-size: 18px; font-weight: 700; padding-left: 18px; }"
        "QLabel#minecraftListLabel { font-weight: 700; color: #585858; }"
        "QLabel#nameLabel, QLabel#loaderTypeLabel, QLabel#loaderVersionLabel, QLabel#gameVersionLabel,"
        " QLabel#loaderListLabel { font-weight: 700; color: #585858; }"
        "QLabel#memoryLabel, QLabel#skinsLabel, QLabel#fpsLabel { color: #ACACAC; }"
        "QCheckBox { color: #585858; font-size: 13px; spacing: 8px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #B9B9B9; border-radius: 2px; background: #FFFFFF; }"
        "QCheckBox::indicator:checked { background: #F9D900; border: 1px solid #D8BB00; }"
        "QPushButton#createButton { background: #F9D900; color: #4A3B00; border: 1px solid #D8BB00;"
        " border-radius: 4px; font-size: 14px; font-weight: 700; padding: 6px 16px; }"
        "QPushButton#createButton:enabled:hover { background: #FFE733; }"
        "QPushButton#createButton:enabled:pressed { background: #E3C500; }"
        "QPushButton#createButton:disabled { background: #E4E4E4; color: #9A9A9A; border: 1px solid #D5D5D5; }"
        "QPushButton#cancelButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 #fdfdfd, stop:0.5 #ececec, stop:1 #e2e2e2);"
        " color: #333333; border: 1px solid #B9B9B9; border-radius: 4px; font-size: 14px; padding: 6px 16px; }"
        "QPushButton#cancelButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 #ffffff, stop:1 #eaeaea); }"
        "QPushButton#cancelButton:pressed { background: #DCDCDC; }"
        "QPushButton#listsToggleButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 #fdfdfd, stop:0.5 #ececec, stop:1 #e2e2e2);"
        " color: #333333; border: 1px solid #B9B9B9; border-radius: 4px; font-size: 12px; padding: 6px 12px; }"
        "QPushButton#listsToggleButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 #ffffff, stop:1 #eaeaea); }"
        "QPushButton#listsToggleButton:checked { background: #F9D900; color: #4A3B00; border: 1px solid #D8BB00; }"
        "QSpinBox { background: #FFFFFF; color: #585858; border: 1px solid #B9B9B9; border-radius: 2px; padding: 4px 6px; }"
        // The launcher theme paints the version list in its own colours, so the
        // list, its header and the search box are forced to the light Frends
        // palette to match the rest of the form.
        "QTreeView { background: #FFFFFF; color: #585858; border: 1px solid #D5DCE3; border-radius: 3px;"
        " alternate-background-color: #FFFFFF; outline: none; }"
        "QTreeView::item { padding: 3px 6px; border: none; }"
        "QTreeView::item:selected { background: #F9D900; color: #4A3B00; }"
        "QTreeView::item:hover { background: #F2F6F9; }"
        "QHeaderView::section { background: #F2F6F9; color: #7A7A7A; border: none;"
        " border-bottom: 1px solid #D9E2EA; padding: 5px 6px; font-weight: 700; }"
        "QHeaderView::section:hover { background: #E8EFF5; }"
        "QLineEdit { background: #FFFFFF; color: #585858; border: 1px solid #C9C9C9; border-radius: 2px; padding: 4px 8px; }"
        "QLineEdit:focus { border: 1px solid #29A0D8; }"
        "QProgressBar { background: #E4E9EE; border: none; border-radius: 2px; height: 6px; text-align: center; }"
        "QProgressBar::chunk { background: #F9D900; border-radius: 2px; }"
        "QScrollBar:vertical { background: #F2F6F9; width: 10px; margin: 0; border: none; }"
        "QScrollBar::handle:vertical { background: #C2CBD4; border-radius: 5px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #A9B4C0; }"
        "QScrollBar:horizontal { background: #F2F6F9; height: 10px; margin: 0; border: none; }"
        "QScrollBar::handle:horizontal { background: #C2CBD4; border-radius: 5px; min-width: 30px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }"
        "QScrollBar::add-page, QScrollBar::sub-page { background: none; }"));

    ui->instNameTextBox->setStyleSheet(inputStyle());
    ui->loaderTypeCombo->setStyleSheet(comboStyle());
    ui->gameVersionCombo->setStyleSheet(comboStyle());
    ui->loaderVersionCombo->setStyleSheet(comboStyle());

    // The item colours of the list come from the widget palette rather than
    // from the style sheet, so it is set separately.
    if (auto* view = ui->versionList->view()) {
        QPalette palette = view->palette();
        palette.setColor(QPalette::Base, QColor(0xFF, 0xFF, 0xFF));
        palette.setColor(QPalette::Window, QColor(0xFF, 0xFF, 0xFF));
        palette.setColor(QPalette::Text, QColor(0x58, 0x58, 0x58));
        palette.setColor(QPalette::WindowText, QColor(0x58, 0x58, 0x58));
        palette.setColor(QPalette::Highlight, QColor(0xF9, 0xD9, 0x00));
        palette.setColor(QPalette::HighlightedText, QColor(0x4A, 0x3B, 0x00));
        view->setPalette(palette);
        view->setAutoFillBackground(true);
        view->setItemDelegate(new VersionTypeDelegate(view));
    }
}

void CustomPage::openedImpl()
{
    if (!initialized) {
        auto vlist = APPLICATION->metadataIndex()->get("net.minecraft");
        ui->versionList->initialize(vlist.get());

        // The list loads asynchronously, so the "quick pick" combo is filled
        // whenever rows arrive rather than once, right now.
        if (auto* model = ui->versionList->view()->model()) {
            connect(model, &QAbstractItemModel::rowsInserted, this, [this] { populateQuickPick(); });
            connect(model, &QAbstractItemModel::modelReset, this, [this] { populateQuickPick(); });
        }
        populateQuickPick();

        initialized = true;
    } else {
        suggestCurrent();
    }

    // Fills the "Версия загрузчика" combo right away, so it does not sit
    // empty while the player is still picking a Minecraft version.
    loaderFilterChanged();
}

void CustomPage::retranslate()
{
    ui->retranslateUi(this);
}

bool CustomPage::shouldDisplay() const
{
    return true;
}

BaseVersion::Ptr CustomPage::selectedVersion() const
{
    return m_selectedVersion;
}

BaseVersion::Ptr CustomPage::selectedLoaderVersion() const
{
    return m_selectedLoaderVersion;
}

QString CustomPage::selectedLoader() const
{
    return m_selectedLoader;
}

void CustomPage::setSelectedVersion(BaseVersion::Ptr version)
{
    if (!version)
        return;

    m_selectedVersion = version;
    m_versionSetByUser = true;

    const QSignalBlocker blocker(ui->gameVersionCombo);
    const int index = ui->gameVersionCombo->findData(version->descriptor());
    if (index >= 0) {
        ui->gameVersionCombo->setCurrentIndex(index);
    } else {
        ui->gameVersionCombo->addItem(version->descriptor(), version->descriptor());
        ui->gameVersionCombo->setCurrentIndex(ui->gameVersionCombo->count() - 1);
    }

    // Without this the user had to invent a name before "Создать сборку"
    // became clickable, which looked as if the dialog was broken.
    autoFillName(version->descriptor());

    loaderFilterChanged();
    suggestCurrent();
}

void CustomPage::setSelectedLoaderVersion(BaseVersion::Ptr version)
{
    m_selectedLoaderVersion = version;

    if (version) {
        const QSignalBlocker blocker(ui->loaderVersionCombo);
        const int index = ui->loaderVersionCombo->findData(version->descriptor());
        if (index >= 0)
            ui->loaderVersionCombo->setCurrentIndex(index);
    }

    suggestCurrent();
}

void CustomPage::gameVersionComboChanged(int index)
{
    Q_UNUSED(index);

    const QString id = ui->gameVersionCombo->currentData().toString();
    if (id.isEmpty())
        return;

    ui->versionList->setCurrentVersion(id);
    ui->versionList->selectCurrent();

    // The list can still be loading; fall back to a synthetic version entry.
    if (!m_selectedVersion || m_selectedVersion->descriptor() != id) {
        m_selectedVersion = std::make_shared<ComboVersion>(id);
        m_versionSetByUser = true;
    }

    loaderFilterChanged();
    suggestCurrent();
}

void CustomPage::loaderTypeComboChanged(int index)
{
    Q_UNUSED(index);

    m_selectedLoader = ui->loaderTypeCombo->currentData().toString();
    m_selectedLoaderVersion.reset();

    loaderFilterChanged();
    suggestCurrent();
}

void CustomPage::loaderVersionComboChanged(int index)
{
    Q_UNUSED(index);

    const QString id = ui->loaderVersionCombo->currentData().toString();
    if (id.isEmpty())
        return;

    ui->loaderVersionList->setCurrentVersion(id);
    ui->loaderVersionList->selectCurrent();
    suggestCurrent();
}

void CustomPage::loaderFilterChanged()
{
    m_selectedLoader = ui->loaderTypeCombo->currentData().toString();

    const QSignalBlocker blocker(ui->loaderVersionCombo);
    ui->loaderVersionCombo->clear();

    if (m_selectedLoader.isEmpty()) {
        ui->loaderVersionCombo->addItem(tr("Не требуется"), QString());
        ui->loaderVersionCombo->setEnabled(false);
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");
        ui->loaderVersionList->setEmptyString(tr("Загрузчик не выбран."));
        ui->loaderVersionList->setEmptyMode(VersionListView::String);
        // Vanilla needs no loader version, so the second list stays hidden.
        ui->versionsPanel->setVisible(false);
        return;
    }

    QString minecraftVersion;
    if (m_selectedVersion)
        minecraftVersion = m_selectedVersion->descriptor();

    if (minecraftVersion.isEmpty()) {
        ui->loaderVersionCombo->addItem(tr("Сначала выберите версию игры"), QString());
        ui->loaderVersionCombo->setEnabled(false);
        return;
    }

    const bool fabricLike =
        m_selectedLoader == QLatin1String("net.fabricmc.fabric-loader") || m_selectedLoader == QLatin1String("org.quiltmc.quilt-loader");

    if (fabricLike) {
        if (Version(minecraftVersion) >= Version("1.14"))
            ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "");
        else
            ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");
    } else {
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, minecraftVersion);
    }

    auto vlist = APPLICATION->metadataIndex()->get(m_selectedLoader);
    ui->loaderVersionList->initialize(vlist.get());
    ui->loaderVersionList->selectRecommended();
    ui->loaderVersionList->setEmptyString(tr("Для Minecraft %1 нет доступных версий загрузчика").arg(minecraftVersion));
    // A loader is picked, so its version list belongs on screen.
    ui->versionsPanel->setVisible(true);

    ui->loaderVersionCombo->setEnabled(loaderRequiresVersion(m_selectedLoader));
    if (!loaderRequiresVersion(m_selectedLoader))
        ui->loaderVersionCombo->addItem(tr("Рекомендуемая"), QString());
}

void CustomPage::suggestCurrent()
{
    if (!isOpened)
        return;

    if (!m_selectedVersion) {
        sink->setSuggestedPack();
        updateCreateButton();
        return;
    }

    const bool loaderNeedsVersion = loaderRequiresVersion(m_selectedLoader);
    const bool hasLoaderVersion = m_selectedLoaderVersion != nullptr;

    if (m_selectedLoader.isEmpty() || !loaderNeedsVersion) {
        sink->setSuggestedPack(m_selectedVersion->descriptor(), new VanillaCreationTask(m_selectedVersion));
    } else if (hasLoaderVersion) {
        sink->setSuggestedPack(m_selectedVersion->descriptor(),
                                 new VanillaCreationTask(m_selectedVersion, m_selectedLoader, m_selectedLoaderVersion));
    } else {
        sink->setSuggestedPack(m_selectedVersion->descriptor());
    }

    sink->setSuggestedIcon("default");

    updateCreateButton();
}

QString CustomPage::instName() const
{
    return ui->instNameTextBox->text();
}

QString CustomPage::suggestedName() const
{
    if (!m_selectedVersion)
        return QString();

    return m_selectedVersion->descriptor();
}

void CustomPage::setCreationEnabled(bool enabled)
{
    ui->createButton->setEnabled(enabled && !instName().trimmed().isEmpty());
}

void CustomPage::autoFillName(const QString& version)
{
    if (m_nameEditedByUser || version.isEmpty())
        return;

    if (ui->instNameTextBox->text() == version)
        return;

    const QSignalBlocker blocker(ui->instNameTextBox);
    ui->instNameTextBox->setText(version);
}

void CustomPage::updateCreateButton()
{
    // Creating a build needs both a Minecraft version and a name; the name is
    // filled in automatically, so picking a version is enough.
    const bool ready = m_selectedVersion != nullptr && !instName().trimmed().isEmpty();
    ui->createButton->setEnabled(ready);
    ui->createButton->setToolTip(ready ? QString() : tr("Выберите версию Minecraft в списке слева."));
}

void CustomPage::versionFilterToggled(bool releasesOnly)
{
    // An empty pattern list means "no filter", so the full list comes back.
    ui->versionList->setFilter(BaseVersionList::TypeRole, releasesOnly ? Filters::equals("release") : Filters::equalsAny());

    // The chosen version may have just been filtered out, so the selection is
    // moved to the recommended release instead of silently disappearing.
    const bool stillVisible =
        m_selectedVersion && (!releasesOnly || m_selectedVersion->typeString() == QLatin1String("release"));
    if (!stillVisible) {
        ui->versionList->selectRecommended();
    }
}

void CustomPage::populateQuickPick()
{
    if (m_quickPickFilled)
        return;

    auto* model = ui->versionList->view()->model();
    if (!model || model->rowCount() == 0)
        return;

    // Only the most recent stable releases go into the shortcut list; the
    // complete list of a thousand versions lives next to it.
    constexpr int kMaxQuickPicks = 15;
    QStringList picks;
    for (int row = 0; row < model->rowCount() && picks.size() < kMaxQuickPicks; row++) {
        const QModelIndex index = model->index(row, 0);
        if (!index.isValid())
            continue;

        if (model->data(index, BaseVersionList::TypeRole).toString() != QLatin1String("release"))
            continue;

        const QString id = model->data(index, BaseVersionList::VersionIdRole).toString();
        if (!id.isEmpty() && !picks.contains(id))
            picks.append(id);
    }

    if (picks.isEmpty())
        return;

    const QSignalBlocker blocker(ui->gameVersionCombo);
    const QString previous = ui->gameVersionCombo->currentData().toString();
    ui->gameVersionCombo->clear();
    for (const QString& id : picks)
        ui->gameVersionCombo->addItem(id, id);

    const int restored = ui->gameVersionCombo->findData(previous);
    if (restored >= 0)
        ui->gameVersionCombo->setCurrentIndex(restored);

    m_quickPickFilled = true;
}

bool CustomPage::useLauncherMemory() const
{
    return ui->useLauncherMemoryCheck->isChecked();
}

int CustomPage::memoryMb() const
{
    return ui->memorySpin->value();
}

bool CustomPage::addOptiFine() const
{
    return ui->optifineCheck->isChecked();
}