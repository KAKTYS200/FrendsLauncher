// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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
 *      Authors: Andrew Okin
 *               Peterix
 *               Orochimarufan <orochimarufan.x3@gmail.com>
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

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QFont>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QResizeEvent>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>
#include <QWidgetAction>
#include <QIcon>
#include <QJsonArray>
#include <QProcess>
#include <QStandardPaths>
#include <memory>

#include <BaseInstance.h>
#include <BuildConfig.h>
#include <DesktopServices.h>
#include <InstanceList.h>
#include <MMCZip.h>
#include <QObjectPtr.h>
#include <icons/IconList.h>
#include <java/JavaInstallList.h>
#include <java/JavaUtils.h>
#include <launch/LaunchTask.h>
#include <minecraft/launch/MinecraftTarget.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/auth/AccountList.h>
#include <net/ApiDownload.h>
#include <net/NetJob.h>
#include <news/NewsChecker.h>
#include <tasks/SequentialTask.h>
#include <tools/BaseProfiler.h>
#include <updater/ExternalUpdater.h>
#include "InstanceWindow.h"

#include "settings/Setting.h"
#include "settings/SettingsObject.h"
#include "ui/GuiUtil.h"
#include "ui/ViewLogWindow.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/CopyInstanceDialog.h"
#include "ui/dialogs/CreateShortcutDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ExportInstanceDialog.h"
#include "ui/dialogs/ExportPackDialog.h"
#include "ui/dialogs/IconPickerDialog.h"
#include "ui/dialogs/ImportResourceDialog.h"
#include "ui/dialogs/NewInstanceDialog.h"
#include "ui/dialogs/NewsDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "ui/instanceview/InstanceDelegate.h"
#include "ui/instanceview/InstanceProxyModel.h"
#include "ui/instanceview/InstanceView.h"
#include "ui/themes/ITheme.h"
#include "ui/themes/ThemeManager.h"
#include "frendskin/FrendSkinServer.h"
#include "FrendsSelfUpdate.h"
#include "ui/frends/FrendsBottomBar.h"
#include "ui/frends/FrendsScale.h"
#include "ui/frends/FrendsServersPage.h"
#include "ui/frends/FrendsCreatePage.h"
#include "ui/frends/FrendsSidebar.h"
#include "ui/widgets/LabeledToolButton.h"

#include "minecraft/PackProfile.h"
#include "minecraft/VersionFile.h"
#include "minecraft/WorldList.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "minecraft/mod/TexturePackFolderModel.h"
#include "minecraft/mod/tasks/LocalResourceParse.h"

#include "modplatform/ModIndex.h"
#include "modplatform/flame/FlameAPI.h"
#include "modplatform/flame/FlameModIndex.h"

#include "KonamiCode.h"

#include "InstanceCopyTask.h"
#include "InstanceDirUpdate.h"

#include "Json.h"

#include "MMCTime.h"

namespace {
QString profileInUseFilter(const QString& profile, bool used)
{
    if (used) {
        return QObject::tr("%1 (in use)").arg(profile);
    } else {
        return profile;
    }
}
}  // namespace

/**
 * A slow rainbow wash over the whole window. It is a click-through overlay, so
 * the launcher stays fully usable while the easter egg runs.
 */
class RainbowOverlay : public QWidget {
   public:
    explicit RainbowOverlay(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        m_timer.setInterval(40);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this] {
            m_phase += 0.0025;
            if (m_phase >= 1.0)
                m_phase -= 1.0;
            update();
        });
    }

    bool isActive() const { return m_timer.isActive(); }

    void start()
    {
        show();
        raise();
        m_timer.start();
    }

    void stop()
    {
        m_timer.stop();
        hide();
    }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        QLinearGradient gradient(0, 0, width(), height());
        const int stops = 14;
        for (int i = 0; i <= stops; ++i) {
            const qreal position = qreal(i) / stops;
            qreal hue = m_phase + position;
            if (hue >= 1.0)
                hue -= 1.0;
            gradient.setColorAt(position, QColor::fromHsvF(hue, 0.85, 1.0, 0.26));
        }
        p.fillRect(rect(), gradient);
    }

   private:
    QTimer m_timer;
    qreal m_phase = 0.0;
};

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowIcon(APPLICATION->logo());
    setWindowTitle(APPLICATION->applicationDisplayName());
#ifndef QT_NO_ACCESSIBILITY
    setAccessibleName(BuildConfig.LAUNCHER_DISPLAYNAME);
#endif

    // instance toolbar stuff
    {
        // Qt doesn't like vertical moving toolbars, so we have to force them...
        // See https://github.com/PolyMC/PolyMC/issues/493
        connect(ui->instanceToolBar, &QToolBar::orientationChanged,
                [this](Qt::Orientation) { ui->instanceToolBar->setOrientation(Qt::Vertical); });

        // if you try to add a widget to a toolbar in a .ui file
        // qt designer will delete it when you save the file >:(
        changeIconButton = new LabeledToolButton(this);
        changeIconButton->setObjectName(QStringLiteral("changeIconButton"));
        changeIconButton->setIcon(QIcon::fromTheme("news"));
        changeIconButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        connect(changeIconButton, &QToolButton::clicked, this, &MainWindow::on_actionChangeInstIcon_triggered);
        ui->instanceToolBar->insertWidgetBefore(ui->actionLaunchInstance, changeIconButton);

        renameButton = new LabeledToolButton(this);
        renameButton->setObjectName(QStringLiteral("renameButton"));
        renameButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        connect(renameButton, &QToolButton::clicked, this, &MainWindow::on_actionRenameInstance_triggered);
        ui->instanceToolBar->insertWidgetBefore(ui->actionLaunchInstance, renameButton);

        ui->instanceToolBar->insertSeparator(ui->actionLaunchInstance);

        // restore the instance toolbar settings
        auto const setting_name = QString("WideBarVisibility_%1").arg(ui->instanceToolBar->objectName());
        instanceToolbarSetting = APPLICATION->settings()->getOrRegisterSetting(setting_name);

        ui->instanceToolBar->setVisibilityState(QByteArray::fromBase64(instanceToolbarSetting->get().toString().toUtf8()));

        ui->instanceToolBar->addContextMenuAction(ui->newsToolBar->toggleViewAction());
        ui->instanceToolBar->addContextMenuAction(ui->instanceToolBar->toggleViewAction());
        ui->instanceToolBar->addContextMenuAction(ui->actionToggleStatusBar);
        ui->instanceToolBar->addContextMenuAction(ui->actionLockToolbars);
    }

    // set the menu for the folders help, accounts, and export tool buttons
    {
        auto foldersMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionFoldersButton));
        ui->actionFoldersButton->setMenu(ui->foldersMenu);
        foldersMenuButton->setPopupMode(QToolButton::InstantPopup);

        helpMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionHelpButton));
        ui->actionHelpButton->setMenu(new QMenu(this));
        ui->actionHelpButton->menu()->addActions(ui->helpMenu->actions());
        ui->actionHelpButton->menu()->removeAction(ui->actionCheckUpdate);
        helpMenuButton->setPopupMode(QToolButton::InstantPopup);

        auto accountMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionAccountsButton));
        accountMenuButton->setPopupMode(QToolButton::InstantPopup);

        auto exportInstanceMenu = new QMenu(this);
        exportInstanceMenu->addAction(ui->actionExportInstanceZip);
        exportInstanceMenu->addAction(ui->actionExportInstanceMrPack);
        exportInstanceMenu->addAction(ui->actionExportInstanceFlamePack);
        ui->actionExportInstance->setMenu(exportInstanceMenu);
    }

    // hide, disable and show stuff
    {
        ui->actionReportBug->setVisible(!BuildConfig.BUG_TRACKER_URL.isEmpty());
        ui->actionMATRIX->setVisible(!BuildConfig.MATRIX_URL.isEmpty());
        ui->actionDISCORD->setVisible(!BuildConfig.DISCORD_URL.isEmpty());
        ui->actionREDDIT->setVisible(!BuildConfig.SUBREDDIT_URL.isEmpty());

        ui->actionCheckUpdate->setVisible(APPLICATION->updaterEnabled());

#ifndef Q_OS_MAC
        ui->actionAddToPATH->setVisible(false);
#endif

        // disabled until we have an instance selected
        ui->instanceToolBar->setEnabled(false);
        setInstanceActionsEnabled(false);

        // add a close button at the end of the main toolbar when running on gamescope / steam deck
        // this is only needed on gamescope because it defaults to an X11/XWayland session and
        // does not implement decorations
        if (qgetenv("XDG_CURRENT_DESKTOP") == "gamescope") {
            ui->mainToolBar->addAction(ui->actionCloseWindow);
        }

        ui->actionViewJavaFolder->setEnabled(BuildConfig.JAVA_DOWNLOADER_ENABLED);
    }

    {  // logs viewing
        connect(ui->actionViewLog, &QAction::triggered, this, [] { APPLICATION->showLogWindow(); });
    }

    // add the toolbar toggles to the view menu
    ui->viewMenu->addAction(ui->instanceToolBar->toggleViewAction());
    ui->viewMenu->addAction(ui->newsToolBar->toggleViewAction());

    updateThemeMenu();
    updateMainToolBar();
    // OSX magic.
    setUnifiedTitleAndToolBarOnMac(true);

    // Global shortcuts
    {
        // you can't set QKeySequence::StandardKey shortcuts in qt designer >:(
        ui->actionAddInstance->setShortcut(QKeySequence::New);
        ui->actionSettings->setShortcut(QKeySequence::Preferences);
        ui->actionUndoTrashInstance->setShortcut(QKeySequence::Undo);
        ui->actionDeleteInstance->setShortcuts({ QKeySequence(tr("Backspace")), QKeySequence::Delete });
        ui->actionCloseWindow->setShortcut(QKeySequence::Close);
        connect(ui->actionCloseWindow, &QAction::triggered, APPLICATION, &Application::closeCurrentWindow);

        // FIXME: This is kinda weird. and bad. We need some kind of managed shutdown.
        auto q = new QShortcut(QKeySequence::Quit, this);
        connect(q, &QShortcut::activated, APPLICATION, &Application::quit);
    }

    // Konami Code
    {
        secretEventFilter = new KonamiCode(this);
        connect(secretEventFilter, &KonamiCode::triggered, this, &MainWindow::konamiTriggered);
    }

    // Add the news label to the news toolbar.
    {
        m_newsChecker.reset(new NewsChecker(APPLICATION->network(), BuildConfig.NEWS_RSS_URL));
        newsLabel = new QToolButton();
        newsLabel->setIcon(QIcon::fromTheme("news"));
        newsLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        newsLabel->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        newsLabel->setFocusPolicy(Qt::NoFocus);
        ui->newsToolBar->insertWidget(ui->actionMoreNews, newsLabel);

        connect(newsLabel, &QAbstractButton::clicked, this, &MainWindow::newsButtonClicked);
        connect(m_newsChecker.get(), &NewsChecker::newsLoaded, this, &MainWindow::updateNewsLabel);
        updateNewsLabel();
    }

    // Create the instance list widget
    {
        view = new InstanceView(ui->centralWidget);

        view->setSelectionMode(QAbstractItemView::SingleSelection);
        // FIXME: leaks ListViewDelegate
        auto delegate = new ListViewDelegate(this);
        view->setItemDelegate(delegate);
        view->setFrameShape(QFrame::NoFrame);
        // do not show ugly blue border on the mac
        view->setAttribute(Qt::WA_MacShowFocusRect, false);
        connect(delegate, &ListViewDelegate::textChanged, this, [this](QString before, QString after) {
            if (auto newRoot = askToUpdateInstanceDirName(m_selectedInstance, before, after, this); !newRoot.isEmpty()) {
                auto oldID = m_selectedInstance->id();
                auto newID = QFileInfo(newRoot).fileName();
                QString origGroup(APPLICATION->instances()->getInstanceGroup(oldID));
                bool syncGroup = origGroup != GroupId() && oldID != newID;
                if (syncGroup)
                    APPLICATION->instances()->setInstanceGroup(oldID, GroupId());

                refreshInstances();
                setSelectedInstanceById(newID);

                if (syncGroup)
                    APPLICATION->instances()->setInstanceGroup(newID, origGroup);
            }
        });

        view->installEventFilter(this);

    // The jeb_ easter egg listens to every text field of the launcher.
    qApp->installEventFilter(this);

    frendsInitFrendSkin();
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &QWidget::customContextMenuRequested, this, &MainWindow::showInstanceContextMenu);
        connect(view, &InstanceView::droppedURLs, this, &MainWindow::processURLs, Qt::QueuedConnection);

        proxymodel = new InstanceProxyModel(this);
        proxymodel->setSourceModel(APPLICATION->instances());
        proxymodel->sort(0);
        connect(proxymodel, &InstanceProxyModel::dataChanged, this, &MainWindow::instanceDataChanged);

        view->setModel(proxymodel);
        view->setSourceOfGroupCollapseStatus(
            [](const QString& groupName) -> bool { return APPLICATION->instances()->isGroupCollapsed(groupName); });
        connect(view, &InstanceView::groupStateChanged, APPLICATION->instances(), &InstanceList::on_GroupStateChanged);

        // The content area hosts both the instance list and the Frends pages
        // (currently "Серверы"); the stack keeps a single card background.
        m_contentStack = new QStackedWidget(ui->contentCard);
        m_contentStack->setFrameShape(QFrame::NoFrame);
        m_contentStack->addWidget(view);
        ui->contentLayout->addWidget(m_contentStack);
    }
    // The cat background
    {
        // set the cat action priority here so you can still see the action in qt designer
        ui->actionCAT->setPriority(QAction::LowPriority);
        bool cat_enable = APPLICATION->settings()->get("TheCat").toBool();
        ui->actionCAT->setChecked(cat_enable);
        connect(ui->actionCAT, &QAction::toggled, this, &MainWindow::onCatToggled);
        connect(APPLICATION, &Application::currentCatChanged, this, &MainWindow::onCatChanged);
        setCatBackground(cat_enable);
    }

    // Togglable status bar
    {
        bool statusBarVisible = APPLICATION->settings()->get("StatusBarVisible").toBool();
        ui->actionToggleStatusBar->setChecked(statusBarVisible);
        connect(ui->actionToggleStatusBar, &QAction::toggled, this, &MainWindow::setStatusBarVisibility);
        setStatusBarVisibility(statusBarVisible);
    }

    // Lock toolbars
    {
        bool toolbarsLocked = APPLICATION->settings()->get("ToolbarsLocked").toBool();
        ui->actionLockToolbars->setChecked(toolbarsLocked);
        connect(ui->actionLockToolbars, &QAction::toggled, this, &MainWindow::lockToolbars);
        lockToolbars(toolbarsLocked);
    }
    // start instance when double-clicked
    connect(view, &InstanceView::activated, this, &MainWindow::instanceActivated);

    // track the selection -- update the instance toolbar
    connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::instanceChanged);

    // track icon changes and update the toolbar!
    connect(APPLICATION->icons(), &IconList::iconUpdated, this, &MainWindow::iconUpdated);

    // model reset -> selection is invalid. All the instance pointers are wrong.
    connect(APPLICATION->instances(), &InstanceList::dataIsInvalid, this, &MainWindow::selectionBad);

    // handle newly added instances
    connect(APPLICATION->instances(), &InstanceList::instanceSelectRequest, this, &MainWindow::instanceSelectRequest);

    // When the global settings page closes, we want to know about it and update our state
    connect(APPLICATION, &Application::globalSettingsApplied, this, &MainWindow::globalSettingsClosed);

    m_statusLeft = new QLabel(tr("No instance selected"), this);
    m_statusCenter = new QLabel(tr("Total playtime: 0s"), this);
    statusBar()->addPermanentWidget(m_statusLeft, 1);
    statusBar()->addPermanentWidget(m_statusCenter, 0);

    // Add "manage accounts" button, right align
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->mainToolBar->insertWidget(ui->actionAccountsButton, spacer);

    // Use undocumented property... https://stackoverflow.com/questions/7121718/create-a-scrollbar-in-a-submenu-qt
    ui->accountsMenu->setStyleSheet("QMenu { menu-scrollable: 1; }");

    repopulateAccountsMenu();

    // Update the menu when the active account changes.
    // Shouldn't have to use lambdas here like this, but if I don't, the compiler throws a fit.
    // Template hell sucks...
    connect(APPLICATION->accounts(), &AccountList::defaultAccountChanged, [this] { defaultAccountChanged(); });
    connect(APPLICATION->accounts(), &AccountList::listChanged, [this] { defaultAccountChanged(); });

    // Show initial account
    defaultAccountChanged();

    // TODO: refresh accounts here?
    // auto accounts = APPLICATION->accounts();

    // load the news
    {
        m_newsChecker->reloadNews();
        updateNewsLabel();
    }

    if (APPLICATION->updaterEnabled()) {
        bool updatesAllowed = APPLICATION->updatesAreAllowed();
        updatesAllowedChanged(updatesAllowed);

        connect(ui->actionCheckUpdate, &QAction::triggered, this, &MainWindow::checkForUpdates);

        // set up the updater object.
        auto updater = APPLICATION->updater();

        if (updater) {
            connect(updater, &ExternalUpdater::canCheckForUpdatesChanged, this, &MainWindow::updatesAllowedChanged);
        }
    }

    connect(ui->actionUndoTrashInstance, &QAction::triggered, this, &MainWindow::undoTrashInstance);

    setSelectedInstanceById(APPLICATION->settings()->get("SelectedInstance").toString());

    // removing this looks stupid
    view->setFocus();

    retranslateUi();
}

// macOS always has a native menu bar, so these fixes are not applicable
// Other systems may or may not have a native menu bar (most do not - it seems like only Ubuntu Unity does)
#ifndef Q_OS_MAC
void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Alt && !APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool())
        ui->menuBar->setVisible(!ui->menuBar->isVisible());
    else
        QMainWindow::keyReleaseEvent(event);
}
#endif

void MainWindow::retranslateUi()
{
    if (m_selectedInstance) {
        m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
    } else {
        m_statusLeft->setText(tr("No instance selected"));
    }

    ui->retranslateUi(this);

    MinecraftAccountPtr defaultAccount = APPLICATION->accounts()->defaultAccount();
    if (defaultAccount) {
        auto profileLabel = profileInUseFilter(defaultAccount->displayName(), defaultAccount->isInUse());
        ui->actionAccountsButton->setText(profileLabel);
    }

    changeIconButton->setToolTip(ui->actionChangeInstIcon->toolTip());
    renameButton->setToolTip(ui->actionRenameInstance->toolTip());

    // replace the %1 with the launcher display name in some actions
    if (helpMenuButton->toolTip().contains("%1"))
        helpMenuButton->setToolTip(helpMenuButton->toolTip().arg(BuildConfig.LAUNCHER_DISPLAYNAME));

    for (auto action : ui->helpMenu->actions()) {
        if (action->text().contains("%1"))
            action->setText(action->text().arg(BuildConfig.LAUNCHER_DISPLAYNAME));
        if (action->toolTip().contains("%1"))
            action->setToolTip(action->toolTip().arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    }

    setupFrendsUi();
}

MainWindow::~MainWindow() {}

QMenu* MainWindow::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(ui->mainToolBar->toggleViewAction());

    filteredMenu->addAction(ui->actionToggleStatusBar);
    filteredMenu->addAction(ui->actionLockToolbars);

    return filteredMenu;
}
void MainWindow::setStatusBarVisibility(bool state)
{
    statusBar()->setVisible(state);
    APPLICATION->settings()->set("StatusBarVisible", state);
}
void MainWindow::lockToolbars(bool state)
{
    ui->mainToolBar->setMovable(!state);
    ui->instanceToolBar->setMovable(!state);
    ui->newsToolBar->setMovable(!state);
    APPLICATION->settings()->set("ToolbarsLocked", state);
}

void MainWindow::konamiTriggered()
{
    QString gradient =
        " stop:0 rgba(125, 0, 0, 255), stop:0.166 rgba(125, 125, 0, 255), stop:0.333 rgba(0, 125, 0, 255), stop:0.5 rgba(0, 125, 125, "
        "255), stop:0.666 rgba(0, 0, 125, 255), stop:0.833 rgba(125, 0, 125, 255), stop:1 rgba(125, 0, 0, 255));";
    QString stylesheet = "background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0," + gradient;
    if (ui->mainToolBar->styleSheet() == stylesheet) {
        ui->mainToolBar->setStyleSheet("");
        ui->instanceToolBar->setStyleSheet("");
        ui->centralWidget->setStyleSheet("");
        ui->newsToolBar->setStyleSheet("");
        ui->statusBar->setStyleSheet("");
        qDebug() << "Super Secret Mode DEACTIVATED!";
    } else {
        ui->mainToolBar->setStyleSheet(stylesheet);
        ui->instanceToolBar->setStyleSheet("background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1," + gradient);
        ui->centralWidget->setStyleSheet("background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1," + gradient);
        ui->newsToolBar->setStyleSheet(stylesheet);
        ui->statusBar->setStyleSheet(stylesheet);
        qDebug() << "Super Secret Mode ACTIVATED!";
    }
}

void MainWindow::showInstanceContextMenu(const QPoint& pos)
{
    QList<QAction*> actions;

    QAction* actionSep = new QAction("", this);
    actionSep->setSeparator(true);

    bool onInstance = view->indexAt(pos).isValid();
    if (onInstance) {
        // reuse the file menu actions
        actions = ui->fileMenu->actions();

        // remove the add instance action, launcher settings action and close action
        actions.removeFirst();
        actions.removeLast();
        actions.removeLast();

        actions.prepend(ui->actionChangeInstIcon);
        actions.prepend(ui->actionRenameInstance);

        // add header
        actions.prepend(actionSep);
        QAction* actionVoid = new QAction(m_selectedInstance->name(), this);
        actionVoid->setEnabled(false);
        actions.prepend(actionVoid);
    } else {
        auto group = view->groupNameAt(pos);

        QAction* actionVoid = new QAction(group.isNull() ? BuildConfig.LAUNCHER_DISPLAYNAME : group, this);
        actionVoid->setEnabled(false);

        QAction* actionCreateInstance = new QAction(tr("&Create instance"), this);
        actionCreateInstance->setToolTip(ui->actionAddInstance->toolTip());
        if (!group.isNull()) {
            QVariantMap instance_action_data;
            instance_action_data["group"] = group;
            actionCreateInstance->setData(instance_action_data);
        }

        connect(actionCreateInstance, &QAction::triggered, this, &MainWindow::on_actionAddInstance_triggered);

        actions.prepend(actionSep);
        actions.prepend(actionVoid);
        actions.append(actionCreateInstance);
        if (!group.isNull()) {
            QAction* actionDeleteGroup = new QAction(tr("&Delete group"), this);
            connect(actionDeleteGroup, &QAction::triggered, this, [this, group] { deleteGroup(group); });
            actions.append(actionDeleteGroup);

            QAction* actionRenameGroup = new QAction(tr("&Rename group"), this);
            connect(actionRenameGroup, &QAction::triggered, this, [this, group] { renameGroup(group); });
            actions.append(actionRenameGroup);
        }
    }
    QMenu myMenu;
    myMenu.addActions(actions);
    /*
    if (onInstance)
        myMenu.setEnabled(m_selectedInstance->canLaunch());
    */
    myMenu.exec(view->mapToGlobal(pos));
}

void MainWindow::updateMainToolBar()
{
    // The Frends layout replaces the stock launcher chrome with the custom
    // sidebar and bottom bar, so the toolbars and the menu bar stay hidden.
    ui->menuBar->setVisible(false);
    ui->mainToolBar->setVisible(false);
    ui->newsToolBar->setVisible(false);
    ui->instanceToolBar->setVisible(false);
}

void MainWindow::setupFrendsUi()
{
    // Called again on language changes: only build the custom chrome once.
    if (m_frendsSidebar && m_frendsBottomBar) {
        retranslateFrendsUi();
        return;
    }

    // Qt appends QGuiApplication::applicationDisplayName to the native title,
    // and that name is already "Frends Launcher", so the window title stays empty.
    setWindowTitle(QString());

    // The original launcher opens at 1067x837 (gui.frame.width / gui.frame.height)
    // and draws its own chrome in Calibri; keep both unless the user already has
    // a saved geometry from a previous session.
    if (APPLICATION->settings()->get("MainWindowGeometry").toString().isEmpty())
        resize(FrendsScale::BASE_WIDTH, FrendsScale::BASE_HEIGHT);

    QFont frendsFont(QStringLiteral("Calibri"));
    frendsFont.setPointSize(10);
    setFont(frendsFont);

    ui->menuBar->setVisible(false);
    ui->mainToolBar->setVisible(false);
    ui->newsToolBar->setVisible(false);
    ui->instanceToolBar->setVisible(false);
    ui->statusBar->setVisible(false);

    view->setStyleSheet(QStringLiteral("QTreeView { background: transparent; border: none; color: #2b2b2b; }"));
    view->viewport()->setAutoFillBackground(false);
    // The stock overlay says "Add Instance"; the Frends chrome calls it "Создать сборку".
    view->setEmptyStateText(tr("Добро пожаловать!") + "\n" + tr("Нажмите «Создать сборку», чтобы начать."));
    ui->contentCard->setAttribute(Qt::WA_StyledBackground, true);

    m_frendsSidebar = new FrendsSidebar(this);
    ui->horizontalLayout->addWidget(m_frendsSidebar);
    // The instance list is the landing page, so its entry starts highlighted.
    m_frendsSidebar->setActivePage(QStringLiteral("instances"));

    m_frendsBottomBar = new FrendsBottomBar(this);
    ui->bottomBarLayout->addWidget(m_frendsBottomBar);

    if (m_contentStack) {
        m_serversPage = new FrendsServersPage(m_contentStack);
        m_contentStack->addWidget(m_serversPage);
        connect(m_serversPage, &FrendsServersPage::playRequested, this, &MainWindow::frendsPlayOnServer);

        // The build form is a tab of the main window, not a separate dialog.
        m_createPage = new FrendsCreatePage(m_contentStack);
        m_contentStack->addWidget(m_createPage);
        connect(m_createPage, &FrendsCreatePage::taskReady, this, &MainWindow::instanceFromInstanceTask);
        connect(m_createPage, &FrendsCreatePage::openApiSettingsRequested, this,
                [this] { APPLICATION->ShowGlobalSettings(this, "apis"); });
        connect(m_createPage, &FrendsCreatePage::dismissed, this, [this] {
            if (m_contentStack)
                m_contentStack->setCurrentIndex(0);
            if (m_frendsSidebar)
                m_frendsSidebar->setActivePage(QStringLiteral("instances"));
        });
    }

    connect(m_frendsSidebar, &FrendsSidebar::navigationRequested, this, &MainWindow::frendsNavigate);
    connect(m_frendsBottomBar, &FrendsBottomBar::playRequested, this, &MainWindow::frendsPlayClicked);
    connect(m_frendsBottomBar, &FrendsBottomBar::refreshRequested, this, &MainWindow::refreshInstances);
    connect(m_frendsBottomBar, &FrendsBottomBar::folderRequested, this, &MainWindow::on_actionViewSelectedInstFolder_triggered);
    connect(m_frendsBottomBar, &FrendsBottomBar::settingsRequested, this, &MainWindow::on_actionSettings_triggered);
    connect(m_frendsBottomBar, &FrendsBottomBar::modsRequested, this, &MainWindow::frendsShowCreatePage);
    connect(m_frendsBottomBar, &FrendsBottomBar::launcherUpdateRequested, this, &MainWindow::frendsUpdateLauncher);
    connect(m_frendsBottomBar, &FrendsBottomBar::accountActivated, this, &MainWindow::frendsAccountActivated);
    connect(m_frendsBottomBar, &FrendsBottomBar::addAccountRequested, this,
            &MainWindow::on_actionManageAccounts_triggered);
    connect(m_frendsBottomBar, &FrendsBottomBar::instanceActivated, this, &MainWindow::frendsInstanceActivated);

    connect(APPLICATION->accounts(), &AccountList::listChanged, this, &MainWindow::frendsSyncAccounts);
    connect(APPLICATION->accounts(), &AccountList::defaultAccountChanged, this, &MainWindow::frendsSyncAccounts);
    connect(APPLICATION->instances(), &InstanceList::instancesChanged, this, &MainWindow::frendsSyncInstances);

    // The theme follows both the window size and the user preference.
    connect(APPLICATION->settings(), &SettingsObject::SettingChanged, this, [this](const Setting& setting, QVariant) {
        if (setting.id() == QLatin1String("FrendsUiScale"))
            applyFrendsScale();
    });

    frendsSyncAccounts();
    frendsSyncInstances();
    frendsUpdateInstanceState();
    applyFrendsScale();
}

void MainWindow::applyFrendsScale()
{
    if (!m_frendsSidebar || !m_frendsBottomBar)
        return;

    const qreal scale = FrendsScale::factor(this);
    if (qFuzzyCompare(scale, m_lastFrendsScale))
        return;

    m_lastFrendsScale = scale;
    m_frendsSidebar->applyScale();
    m_frendsBottomBar->applyScale();
    if (m_serversPage)
        m_serversPage->applyScale();
    if (m_createPage)
        m_createPage->applyScale();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    applyFrendsScale();
    if (m_rainbowOverlay)
        m_rainbowOverlay->setGeometry(rect());
}

/// Starts the local skin publisher, so a client-side skin source can read the
/// skins the player picked for their accounts.
///
/// Application::initFrendSkin() already did this during startup: the window can
/// be skipped entirely ("--launch" without "--show-window"), and the launcher
/// must publish the skins in that case too. Kept as a no-op entry point so the
/// existing call site stays meaningful.
void MainWindow::frendsInitFrendSkin()
{
    auto* server = APPLICATION->frendSkin();
    if (server && !server->isRunning())
        APPLICATION->initFrendSkin();
}

/// Turns the rainbow easter egg on, or off when it is already running.
void MainWindow::frendsToggleRainbow()
{
    if (!m_rainbowOverlay) {
        m_rainbowOverlay = new RainbowOverlay(this);
        m_rainbowOverlay->setGeometry(rect());
    }
    auto* overlay = static_cast<RainbowOverlay*>(m_rainbowOverlay);
    if (overlay->isActive()) {
        overlay->stop();
    } else {
        overlay->setGeometry(rect());
        overlay->start();
    }
}

void MainWindow::retranslateFrendsUi()
{
    // Qt appends QGuiApplication::applicationDisplayName to the native title,
    // and that name is already "Frends Launcher", so the window title stays empty.
    setWindowTitle(QString());
    if (m_frendsSidebar)
        m_frendsSidebar->retranslate();
    if (m_frendsBottomBar)
        m_frendsBottomBar->retranslate();
    frendsSyncAccounts();
    frendsSyncInstances();
}

void MainWindow::frendsNavigate(const QString& page)
{
    if (page == QLatin1String("accounts")) {
        APPLICATION->ShowGlobalSettings(this, "accounts");
    } else if (page == QLatin1String("settings")) {
        on_actionSettings_triggered();
    } else if (page == QLatin1String("mods")) {
        frendsOpenMods();
    } else if (page == QLatin1String("add")) {
        frendsShowCreatePage();
        return;
    } else if (page == QLatin1String("logs")) {
        APPLICATION->showLogWindow();
    } else if (page == QLatin1String("about")) {
        on_actionAbout_triggered();
    } else if (page == QLatin1String("news")) {
        on_actionMoreNews_triggered();
    } else if (page == QLatin1String("servers")) {
        if (m_contentStack && m_serversPage) {
            m_contentStack->setCurrentWidget(m_serversPage);
            m_serversPage->refresh();
            if (m_frendsSidebar)
                m_frendsSidebar->setActivePage(page);
        }
        return;
    } else if (page == QLatin1String("instances")) {
        refreshInstances();
    }

    // Any page that opens a dialog leaves the instance list in place behind it.
    if (m_contentStack)
        m_contentStack->setCurrentIndex(0);
    if (m_frendsSidebar)
        m_frendsSidebar->setActivePage(QStringLiteral("instances"));
}

void MainWindow::frendsPlayOnServer(const QString& address)
{
    if (!m_selectedInstance) {
        QMessageBox::information(this, tr("Серверы"),
                                 tr("Сначала выберите сборку в списке слева, чтобы подключиться к серверу."));
        if (m_contentStack)
            m_contentStack->setCurrentIndex(0);
        return;
    }

    if (m_selectedInstance->isRunning())
        return;

    auto target = std::make_shared<MinecraftTarget>(MinecraftTarget::parse(address, false));
    APPLICATION->launch(m_selectedInstance, LaunchMode::Normal, target);
}

void MainWindow::frendsSyncAccounts()
{
    if (!m_frendsBottomBar)
        return;

    auto* combo = m_frendsBottomBar->accountCombo();
    const QSignalBlocker blocker(combo);
    combo->clear();

    auto* accounts = APPLICATION->accounts();
    const auto defaultAccount = accounts->defaultAccount();
    int current = -1;

    for (int i = 0; i < accounts->count(); i++) {
        const auto account = accounts->at(i);
        const auto face = account->getFace(16, 16);
        combo->addItem(face.isNull() ? QIcon::fromTheme("noaccount") : QIcon(face), account->displayName(), i);
        if (account == defaultAccount)
            current = i;
    }

    if (accounts->count() == 0) {
        combo->addItem(tr("Нет аккаунтов"), -1);
    } else if (current >= 0) {
        combo->setCurrentIndex(current);
    }
}

void MainWindow::frendsSyncInstances()
{
    if (!m_frendsBottomBar)
        return;

    auto* combo = m_frendsBottomBar->instanceCombo();
    const QSignalBlocker blocker(combo);
    combo->clear();

    auto* instances = APPLICATION->instances();
    const QString selectedId = m_selectedInstance ? m_selectedInstance->id() : QString();
    int current = -1;

    for (int i = 0; i < instances->count(); i++) {
        const auto instance = instances->at(i);
        const auto icon = APPLICATION->icons()->getIcon(instance->iconKey());
        combo->addItem(icon, instance->name(), instance->id());
        if (instance->id() == selectedId)
            current = i;
    }

    if (instances->count() == 0) {
        combo->addItem(tr("Нет сборок"), QString());
    } else if (current >= 0) {
        combo->setCurrentIndex(current);
    }
}

void MainWindow::frendsPlayClicked()
{
    if (!m_selectedInstance && APPLICATION->instances()->count() > 0) {
        setSelectedInstanceById(APPLICATION->instances()->at(0)->id());
    }

    if (!m_selectedInstance) {
        on_actionAddInstance_triggered();
        return;
    }

    if (APPLICATION->accounts()->count() == 0) {
        APPLICATION->ShowGlobalSettings(this, "accounts");
        return;
    }

    if (m_selectedInstance->isRunning()) {
        on_actionKillInstance_triggered();
        return;
    }

    BaseInstance* instance = m_selectedInstance;

    if (m_frendsBottomBar->updateCheckBox()->isChecked()) {
        auto updateTasks = instance->createUpdateTask();
        if (!updateTasks.isEmpty()) {
            auto* task = new SequentialTask(tr("Обновление сборки"));
            for (const auto& updateTask : updateTasks)
                task->addTask(updateTask);

            // Launch only after the files are really there. Starting the game
            // while the download is still running is exactly what makes
            // Minecraft report missing libraries.
            connect(task, &Task::succeeded, this, [this, instance] {
                if (instance == m_selectedInstance)
                    APPLICATION->launch(instance);
            });

            runTaskWithStrip(task);
            refreshInstances();
            return;
        }
    }

    APPLICATION->launch(instance);
}

void MainWindow::frendsAccountActivated(int index)
{
    auto* combo = m_frendsBottomBar->accountCombo();
    const int accountIndex = combo->itemData(index).toInt();
    auto* accounts = APPLICATION->accounts();
    if (accountIndex >= 0 && accountIndex < accounts->count())
        accounts->setDefaultAccount(accounts->at(accountIndex));
}

void MainWindow::frendsInstanceActivated(const QString& id)
{
    if (!id.isEmpty())
        setSelectedInstanceById(id);
}

void MainWindow::frendsShowCreatePage()
{
    if (!m_contentStack || !m_createPage) {
        // No tab available (the plain layout is not set up), fall back to the
        // old modal dialog.
        on_actionAddInstance_triggered();
        return;
    }

    m_createPage->resetForm();
    m_contentStack->setCurrentWidget(m_createPage);
    if (m_frendsSidebar)
        m_frendsSidebar->setActivePage(QStringLiteral("add"));
}

/// Asks GitHub for the newest release and offers to install it.
void MainWindow::frendsUpdateLauncher()
{
    if (m_launcherUpdateReply) {
        // A check or a download is already running.
        return;
    }

    if (m_frendsBottomBar)
        m_frendsBottomBar->showDownloadProgress(tr("Проверка обновления лаунчера…"));

    // The newest release is read straight from the project's GitHub page, so
    // publishing a tag is all it takes to ship an update: no manifest to keep
    // in sync and no file to point at by hand.
    QNetworkRequest request(
        QUrl(QStringLiteral("%1/releases/latest").arg(BuildConfig.UPDATER_GITHUB_REPO)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());

    m_launcherUpdateReply = APPLICATION->network()->get(request);
    connect(m_launcherUpdateReply, &QNetworkReply::finished, this, [this] {
        auto* reply = m_launcherUpdateReply.data();
        m_launcherUpdateReply.clear();
        reply->deleteLater();

        if (m_frendsBottomBar)
            m_frendsBottomBar->hideDownloadProgress();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[frends-update] release check failed:" << reply->errorString();
            CustomMessageBox::selectable(this, tr("Обновление лаунчера"),
                                         tr("Не удалось проверить обновления на GitHub: %1").arg(reply->errorString()),
                                         QMessageBox::Warning)
                ->exec();
            return;
        }

        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            qWarning() << "[frends-update] unexpected answer:" << parseError.errorString();
            CustomMessageBox::selectable(this, tr("Обновление лаунчера"), tr("GitHub вернул неожиданный ответ."),
                                         QMessageBox::Warning)
                ->exec();
            return;
        }

        const auto release = document.object();
        const auto tag = release.value("tag_name").toString();
        const auto releaseName = release.value("name").toString(tag);
        const auto currentVersion = BuildConfig.versionString();
        qWarning() << "[frends-update] latest tag:" << tag << "current:" << currentVersion;

        if (!FrendsSelfUpdate::isNewerVersion(tag, currentVersion)) {
            CustomMessageBox::selectable(this, tr("Обновление лаунчера"),
                                         tr("У вас последняя версия лаунчера (%1).").arg(currentVersion),
                                         QMessageBox::Information)
                ->exec();
            return;
        }

        // The portable archive is what carries the whole build; replacing only
        // the executable would leave the Qt libraries at the old version.
        QString downloadUrl;
        for (const auto& value : release.value("assets").toArray()) {
            const auto asset = value.toObject();
            const auto name = asset.value("name").toString();
            if (!name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive))
                continue;
            // Prefer the portable build when a release has several archives.
            if (downloadUrl.isEmpty() || name.contains(QStringLiteral("portable"), Qt::CaseInsensitive))
                downloadUrl = asset.value("browser_download_url").toString();
        }

        if (downloadUrl.isEmpty()) {
            CustomMessageBox::selectable(this, tr("Обновление лаунчера"),
                                         tr("В релизе %1 нет архива с лаунчером.").arg(tag), QMessageBox::Warning)
                ->exec();
            return;
        }

        auto* box = CustomMessageBox::selectable(this, tr("Обновление лаунчера"),
                                                 tr("Доступна новая версия: %1\nУ вас установлена: %2\n\n"
                                                    "Скачать и установить сейчас?")
                                                     .arg(releaseName, currentVersion),
                                                 QMessageBox::Question, QMessageBox::Yes | QMessageBox::No);
        connect(box, &QDialog::finished, this, [this, downloadUrl, tag](int result) {
            // QMessageBox reports the pressed standard button, not QDialog::Accepted.
            if (result != QMessageBox::Yes)
                return;
            frendsDownloadLauncherUpdate(downloadUrl, tag);
        });
        box->exec();
    });
}

void MainWindow::frendsDownloadLauncherUpdate(const QString& url, const QString& version)
{
    const auto tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const auto updateDir = QDir(tempRoot).absoluteFilePath(QStringLiteral("frends-update-%1").arg(version));
    const auto archivePath = updateDir + QStringLiteral(".zip");

    qWarning() << "[frends-update] downloading" << url << "into" << updateDir;
    m_frendsBottomBar->showDownloadProgress(tr("Скачивание лаунчера…"));

    QNetworkRequest request{ QUrl(url) };
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());
    m_launcherUpdateReply = APPLICATION->network()->get(request);
    auto* reply = m_launcherUpdateReply.data();

    connect(reply, &QNetworkReply::downloadProgress, m_frendsBottomBar, &FrendsBottomBar::setDownloadProgress);

    connect(reply, &QNetworkReply::finished, this, [this, reply, updateDir, archivePath, version] {
        m_launcherUpdateReply.clear();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[frends-update] download failed:" << reply->errorString();
            m_frendsBottomBar->hideDownloadProgress();
            reply->deleteLater();
            CustomMessageBox::selectable(this, tr("Обновление лаунчера"),
                                         tr("Не удалось скачать обновление: %1").arg(reply->errorString()),
                                         QMessageBox::Warning)
                ->exec();
            return;
        }

        QDir().mkpath(QFileInfo(archivePath).absolutePath());
        QFile file(archivePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            m_frendsBottomBar->hideDownloadProgress();
            reply->deleteLater();
            CustomMessageBox::selectable(this, tr("Обновление лаунчера"),
                                         tr("Не удалось сохранить файл: %1").arg(file.errorString()),
                                         QMessageBox::Warning)
                ->exec();
            return;
        }
        file.write(reply->readAll());
        file.close();
        reply->deleteLater();

        m_frendsBottomBar->hideDownloadProgress();

        // Unpack next to the download, so the new build can be started from
        // there and copy itself over the installed one.
        QDir(updateDir).removeRecursively();
        QDir().mkpath(updateDir);
        if (!MMCZip::extractDir(archivePath, updateDir)) {
            CustomMessageBox::selectable(this, tr("Обновление лаунчера"),
                                         tr("Не удалось распаковать скачанный архив."), QMessageBox::Critical)
                ->exec();
            return;
        }
        QFile::remove(archivePath);

        const auto newExe = QDir(updateDir).absoluteFilePath(QStringLiteral("frendslauncher.exe"));
        if (!QFileInfo::exists(newExe)) {
            CustomMessageBox::selectable(this, tr("Обновление лаунчера"),
                                         tr("В скачанном архиве нет frendslauncher.exe."), QMessageBox::Critical)
                ->exec();
            return;
        }

        auto* box = CustomMessageBox::selectable(this, tr("Обновление лаунчера"),
                                                 tr("Лаунчер %1 скачан.\n\nУстановить и перезапустить сейчас?")
                                                     .arg(version),
                                                 QMessageBox::Question, QMessageBox::Yes | QMessageBox::No);
        connect(box, &QDialog::finished, this, [this, updateDir, newExe, version](int result) {
            if (result != QMessageBox::Yes)
                return;

            // Windows refuses to replace a running executable and its libraries,
            // so the unpacked copy is started and it does the replacing once
            // this process is gone.
            QStringList forwarded = QCoreApplication::arguments();
            if (!forwarded.isEmpty())
                forwarded.removeFirst();
            // Both directories are spelled out: the helper runs before the Qt
            // application object exists, so it cannot ask for its own path.
            forwarded << QString::fromLatin1(FrendsSelfUpdate::kApplyFlag)
                      << QCoreApplication::applicationDirPath() << updateDir;

            qWarning() << "[frends-update] handing over to" << newExe << forwarded;
            if (!QProcess::startDetached(newExe, forwarded)) {
                CustomMessageBox::selectable(this, tr("Обновление лаунчера"),
                                             tr("Не удалось запустить новую версию из %1.").arg(updateDir),
                                             QMessageBox::Critical)
                    ->exec();
                return;
            }
            APPLICATION->quit();
        });
        box->exec();
    });
}
void MainWindow::frendsOpenMods()
{
    if (!m_selectedInstance && APPLICATION->instances()->count() > 0)
        setSelectedInstanceById(APPLICATION->instances()->at(0)->id());

    if (!m_selectedInstance) {
        // Nothing to put mods into yet, so offer to create a build first.
        frendsShowCreatePage();
        return;
    }

    auto* minecraftInstance = dynamic_cast<MinecraftInstance*>(m_selectedInstance);
    auto* mods = minecraftInstance ? minecraftInstance->loaderModList() : nullptr;
    if (!mods) {
        CustomMessageBox::selectable(this, tr("Mods"),
                                     tr("This instance does not support mods. Please choose a modded instance first."),
                                     QMessageBox::Information)
            ->exec();
        return;
    }

    ResourceDownload::ModDownloadDialog dialog(this, mods, m_selectedInstance);
    dialog.exec();
    mods->update();
}

void MainWindow::frendsUpdateInstanceState()
{
    if (!m_frendsBottomBar)
        return;

    const bool running = m_selectedInstance != nullptr && m_selectedInstance->isRunning();
    m_frendsBottomBar->setRunning(running);
    m_frendsBottomBar->setPlayEnabled(true);
    frendsSyncInstances();
}

void MainWindow::updateLaunchButton()
{
    QMenu* launchMenu = ui->actionLaunchInstance->menu();
    if (launchMenu)
        launchMenu->clear();
    else
        launchMenu = new QMenu(this);
    if (m_selectedInstance)
        m_selectedInstance->populateLaunchMenu(launchMenu);
    ui->actionLaunchInstance->setMenu(launchMenu);
}

void MainWindow::updateThemeMenu()
{
    QMenu* themeMenu = ui->actionChangeTheme->menu();

    if (themeMenu) {
        themeMenu->clear();
    } else {
        themeMenu = new QMenu(this);
    }

    auto themes = APPLICATION->themeManager()->getValidApplicationThemes();

    QActionGroup* themesGroup = new QActionGroup(this);

    for (auto* theme : themes) {
        QAction* themeAction = themeMenu->addAction(theme->name());

        themeAction->setCheckable(true);
        if (APPLICATION->settings()->get("ApplicationTheme").toString() == theme->id()) {
            themeAction->setChecked(true);
        }
        themeAction->setActionGroup(themesGroup);

        connect(themeAction, &QAction::triggered, [theme]() {
            APPLICATION->themeManager()->setApplicationTheme(theme->id());
            APPLICATION->settings()->set("ApplicationTheme", theme->id());
        });
    }

    ui->actionChangeTheme->setMenu(themeMenu);
}

void MainWindow::repopulateAccountsMenu()
{
    ui->accountsMenu->clear();

    // NOTE: this is done so the accounts button text is not set to the accounts menu title
    QMenu* accountsButtonMenu = ui->actionAccountsButton->menu();
    if (accountsButtonMenu) {
        accountsButtonMenu->clear();
    } else {
        accountsButtonMenu = new QMenu(this);
        ui->actionAccountsButton->setMenu(accountsButtonMenu);
    }

    auto accounts = APPLICATION->accounts();
    MinecraftAccountPtr defaultAccount = accounts->defaultAccount();

    QString active_profileId = "";
    if (defaultAccount) {
        // this can be called before accountMenuButton exists
        if (ui->actionAccountsButton) {
            auto profileLabel = profileInUseFilter(defaultAccount->displayName(), defaultAccount->isInUse());
            ui->actionAccountsButton->setText(profileLabel);
        }
    }

    QActionGroup* accountsGroup = new QActionGroup(this);

    if (accounts->count() <= 0) {
        ui->actionNoAccountsAdded->setEnabled(false);
        ui->accountsMenu->addAction(ui->actionNoAccountsAdded);
    } else {
        // TODO: Nicer way to iterate?
        for (int i = 0; i < accounts->count(); i++) {
            MinecraftAccountPtr account = accounts->at(i);
            auto profileLabel = profileInUseFilter(account->displayName(), account->isInUse());
            QAction* action = new QAction(profileLabel, this);
            action->setData(i);
            action->setCheckable(true);
            action->setActionGroup(accountsGroup);
            if (defaultAccount == account) {
                action->setChecked(true);
            }

            auto face = account->getFace();
            if (!face.isNull()) {
                action->setIcon(face);
            } else {
                action->setIcon(QIcon::fromTheme("noaccount"));
            }

            const int highestNumberKey = 9;
            if (i < highestNumberKey) {
                action->setShortcut(QKeySequence(tr("Ctrl+%1").arg(i + 1)));
            }

            ui->accountsMenu->addAction(action);
            connect(action, &QAction::triggered, this, &MainWindow::changeActiveAccount);
        }
    }

    ui->accountsMenu->addSeparator();

    ui->actionNoDefaultAccount->setData(-1);
    ui->actionNoDefaultAccount->setChecked(!defaultAccount);
    ui->actionNoDefaultAccount->setActionGroup(accountsGroup);

    ui->accountsMenu->addAction(ui->actionNoDefaultAccount);

    connect(ui->actionNoDefaultAccount, &QAction::triggered, this, &MainWindow::changeActiveAccount);

    ui->accountsMenu->addSeparator();
    ui->accountsMenu->addAction(ui->actionManageAccounts);

    accountsButtonMenu->addActions(ui->accountsMenu->actions());
}

void MainWindow::updatesAllowedChanged(bool allowed)
{
    if (!APPLICATION->updaterEnabled()) {
        return;
    }
    ui->actionCheckUpdate->setEnabled(allowed);
}

/*
 * Assumes the sender is a QAction
 */
void MainWindow::changeActiveAccount()
{
    QAction* sAction = (QAction*)sender();

    // Profile's associated Mojang username
    if (sAction->data().typeId() != QMetaType::Int)
        return;

    QVariant action_data = sAction->data();
    bool valid = false;
    int index = action_data.toInt(&valid);
    if (!valid) {
        index = -1;
    }
    auto accounts = APPLICATION->accounts();
    accounts->setDefaultAccount(index == -1 ? nullptr : accounts->at(index));
    defaultAccountChanged();
}

void MainWindow::defaultAccountChanged()
{
    repopulateAccountsMenu();

    MinecraftAccountPtr account = APPLICATION->accounts()->defaultAccount();

    // FIXME: this needs adjustment for MSA
    if (account && account->profileName() != "") {
        auto profileLabel = profileInUseFilter(account->displayName(), account->isInUse());
        ui->actionAccountsButton->setText(profileLabel);
        auto face = account->getFace();
        if (face.isNull()) {
            ui->actionAccountsButton->setIcon(QIcon::fromTheme("noaccount"));
        } else {
            ui->actionAccountsButton->setIcon(face);
        }
        return;
    }

    // Set the icon to the "no account" icon.
    ui->actionAccountsButton->setIcon(QIcon::fromTheme("noaccount"));
    ui->actionAccountsButton->setText(tr("Accounts"));
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    // Easter egg: type jeb_ into any text field and press Enter to wash the
    // launcher in a slow rainbow, the way a sheep named jeb_ turns rainbow.
    if (ev->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(ev);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // The event travels up the parent chain, so the text is taken from
            // the widget that actually has the focus.
            QString typed;
            QWidget* source = qobject_cast<QWidget*>(obj);
            QWidget* target = QApplication::focusWidget();
            for (QWidget* candidate : { target, source }) {
                if (!candidate)
                    continue;
                if (auto* line = qobject_cast<QLineEdit*>(candidate))
                    typed = line->text();
                else if (auto* plain = qobject_cast<QPlainTextEdit*>(candidate))
                    typed = plain->toPlainText();
                else if (auto* rich = qobject_cast<QTextEdit*>(candidate))
                    typed = rich->toPlainText();
                if (!typed.isEmpty())
                    break;
            }

            if (!typed.isEmpty() && typed.trimmed().compare(QStringLiteral("jeb_"), Qt::CaseInsensitive) == 0) {
                frendsToggleRainbow();
                return true;
            }
        }
    }

    if (obj == view) {
        if (ev->type() == QEvent::KeyPress) {
            secretEventFilter->input(ev);
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);
            switch (keyEvent->key()) {
                    /*
                case Qt::Key_Enter:
                case Qt::Key_Return:
                    activateInstance(m_selectedInstance);
                    return true;
                    */
                case Qt::Key_Delete:
                    on_actionDeleteInstance_triggered();
                    return true;
                case Qt::Key_F5:
                    refreshInstances();
                    return true;
                case Qt::Key_F2:
                    on_actionRenameInstance_triggered();
                    return true;
                default:
                    break;
            }
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::updateNewsLabel()
{
    if (m_newsChecker->isLoadingNews()) {
        newsLabel->setText(tr("Loading news..."));
        newsLabel->setEnabled(false);
        ui->actionMoreNews->setVisible(false);
    } else {
        QList<NewsEntryPtr> entries = m_newsChecker->getNewsEntries();
        if (entries.length() > 0) {
            newsLabel->setText(entries[0]->title);
            newsLabel->setEnabled(true);
            ui->actionMoreNews->setVisible(true);
        } else {
            newsLabel->setText(tr("No news available."));
            newsLabel->setEnabled(false);
            ui->actionMoreNews->setVisible(false);
        }
    }
}

QList<int> stringToIntList(const QString& string)
{
    QStringList split = string.split(',', Qt::SkipEmptyParts);
    QList<int> out;
    for (int i = 0; i < split.size(); ++i) {
        out.append(split.at(i).toInt());
    }
    return out;
}
QString intListToString(const QList<int>& list)
{
    QStringList slist;
    for (int i = 0; i < list.size(); ++i) {
        slist.append(QString::number(list.at(i)));
    }
    return slist.join(',');
}

void MainWindow::onCatToggled(bool state)
{
    setCatBackground(state);
    APPLICATION->settings()->set("TheCat", state);
}

void MainWindow::setCatBackground(bool enabled)
{
    view->setPaintCat(enabled);
    view->viewport()->repaint();
}

/**
 * Runs a task with the download strip above the bottom bar as its only
 * progress UI. There is no modal window: the player keeps using the launcher
 * while the build or the version downloads. The task deletes itself once it
 * is done, so the caller gives up ownership.
 */
void MainWindow::runTaskWithStrip(Task* task)
{
    connect(task, &Task::failed, this, [this](QString reason) {
        CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
    });
    connect(task, &Task::succeeded, this, [this, task]() {
        QStringList warnings = task->warnings();
        if (warnings.count()) {
            CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
        }
    });
    connect(task, &Task::aborted, this, [this] {
        CustomMessageBox::selectable(this, tr("Task aborted"), tr("The task has been aborted by the user."),
                                     QMessageBox::Information)
            ->show();
    });

    if (m_frendsBottomBar) {
        connect(task, &Task::started, m_frendsBottomBar, [this, task] {
            m_currentDownloadTask = task;
            m_frendsBottomBar->showDownloadProgress(tr("Загрузка…"));
        });
        connect(task, &Task::status, m_frendsBottomBar, [this](const QString& status) {
            if (!status.isEmpty())
                m_frendsBottomBar->showDownloadProgress(status);
        });
        connect(task, &Task::progress, m_frendsBottomBar, &FrendsBottomBar::setDownloadProgress);
        connect(task, &Task::finished, m_frendsBottomBar, &FrendsBottomBar::hideDownloadProgress);
        connect(m_frendsBottomBar, &FrendsBottomBar::downloadCancelled, this, [this] {
            if (m_currentDownloadTask && m_currentDownloadTask->canAbort())
                m_currentDownloadTask->abort();
        });
    }

    connect(task, &Task::finished, task, &Task::deleteLater);
    task->start();
}

void MainWindow::instanceFromInstanceTask(InstanceTask* rawTask)
{
    // Ownership moves to the task itself; it deletes itself once it finishes
    // and reports its progress on the strip above the bottom bar.
    Task* task = APPLICATION->instances()->wrapInstanceTask(rawTask);
    runTaskWithStrip(task);
}

void MainWindow::on_actionCopyInstance_triggered()
{
    if (!m_selectedInstance)
        return;

    CopyInstanceDialog copyInstDlg(m_selectedInstance, this);
    if (!copyInstDlg.exec())
        return;

    auto copyTask = new InstanceCopyTask(m_selectedInstance, copyInstDlg.getChosenOptions());
    copyTask->setName(copyInstDlg.instName());
    copyTask->setGroup(copyInstDlg.instGroup());
    copyTask->setIcon(copyInstDlg.iconKey());
    Task* task = APPLICATION->instances()->wrapInstanceTask(copyTask);
    runTaskWithStrip(task);
}

void MainWindow::addInstance(const QString& url, const QMap<QString, QString>& extra_info)
{
    QString groupName;
    do {
        QObject* obj = sender();
        if (!obj)
            break;
        QAction* action = qobject_cast<QAction*>(obj);
        if (!action)
            break;
        auto map = action->data().toMap();
        if (!map.contains("group"))
            break;
        groupName = map["group"].toString();
    } while (0);

    if (groupName.isEmpty()) {
        groupName = APPLICATION->settings()->get("LastUsedGroupForNewInstance").toString();
    }

    NewInstanceDialog newInstDlg(groupName, url, extra_info, this);
    if (!newInstDlg.exec())
        return;

    APPLICATION->settings()->set("LastUsedGroupForNewInstance", newInstDlg.instGroup());

    InstanceTask* creationTask = newInstDlg.extractTask();
    if (creationTask) {
        instanceFromInstanceTask(creationTask);
    }
}

void MainWindow::on_actionAddInstance_triggered()
{
    addInstance();
}

void MainWindow::processURLs(QList<QUrl> urls)
{
    // NOTE: This loop only processes one dropped file!
    for (auto& url : urls) {
        if (url.isEmpty() || url.toString().trimmed().isEmpty())
            continue;

        qDebug() << "Processing" << url;

        // The isLocalFile() check below doesn't work as intended without an explicit scheme.
        if (url.scheme().isEmpty())
            url.setScheme("file");

        ModPlatform::IndexedVersion version;
        QMap<QString, QString> extra_info;
        QUrl local_url;
        if (!url.isLocalFile()) {  // download the remote resource and identify

            const bool isExternalURLImport =
                (url.host().toLower() == "import") ||
                (url.path().startsWith("/import", Qt::CaseInsensitive));

            QUrl dl_url;
            if (url.scheme() == "curseforge" || (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME && url.host() == "install")) {
                // need to find the download link for the modpack / resource
                // format of url curseforge://install?addonId=IDHERE&fileId=IDHERE
                // format of url binaryname://install?platform=curseforge&addonId=IDHERE&fileId=IDHERE
                QUrlQuery query(url);
                
                // check if this is a binaryname:// url
                if (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME) {
                    // check this is an curseforge platform request
                    if (query.queryItemValue("platform").toLower() != "curseforge") {
                        qDebug() << "Invalid mod distribution platform:" << query.queryItemValue("platform");
                        continue;
                    }
                }

                if (query.allQueryItemValues("addonId").isEmpty() || query.allQueryItemValues("fileId").isEmpty()) {
                    qDebug() << "Invalid curseforge link:" << url;
                    continue;
                }

                auto addonId = query.allQueryItemValues("addonId")[0];
                auto fileId = query.allQueryItemValues("fileId")[0];

                extra_info.insert("pack_id", addonId);
                extra_info.insert("pack_version_id", fileId);

                auto api = FlameAPI();
                auto [job, array] = api.getFile(addonId, fileId);

                connect(job.get(), &Task::failed, this,
                        [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
                connect(job.get(), &Task::succeeded, this, [this, array, addonId, fileId, &dl_url, &version] {
                    qDebug() << "Returned CFURL Json:\n" << array->toStdString().c_str();
                    auto doc = Json::requireDocument(*array);
                    auto data = doc.object()["data"].toObject();
                    // No way to find out if it's a mod or a modpack before here
                    // And also we need to check if it ends with .zip, instead of any better way
                    version = FlameMod::loadIndexedPackVersion(data);
                    auto fileName = version.fileName;

                    // Have to use ensureString then use QUrl to get proper url encoding
                    dl_url = QUrl(version.downloadUrl);
                    if (!dl_url.isValid()) {
                        CustomMessageBox::selectable(
                            this, tr("Error"),
                            tr("The modpack, mod, or resource %1 is blocked for third-parties! Please download it manually.").arg(fileName),
                            QMessageBox::Critical)
                            ->show();
                        return;
                    }

                    QFileInfo dl_file(dl_url.fileName());
                });

                {  // drop stack
                    ProgressDialog dlUrlDialod(this);
                    dlUrlDialod.setSkipButton(true, tr("Abort"));
                    dlUrlDialod.execWithTask(job.get());
                }

            } else if (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME && !isExternalURLImport) {
                QVariantMap receivedData;
                const QUrlQuery query(url.query());
                const auto items = query.queryItems();
                for (auto it = items.begin(), end = items.end(); it != end; ++it)
                    receivedData.insert(it->first, it->second);
                emit APPLICATION->oauthReplyRecieved(receivedData);
                continue;
            } else if ((url.scheme() == "prismlauncher" || url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME)
                        && isExternalURLImport) {
                // PrismLauncher URL protocol modpack import
                // works for any prism fork
                // preferred import format: prismlauncher://import?url=ENCODED
                const auto host = url.host().toLower();
                const auto path = url.path();

                QString encodedTarget;

                {
                    QUrlQuery query(url);
                    const auto values = query.allQueryItemValues("url");
                    if (!values.isEmpty()) {
                        encodedTarget = values.first();
                    }
                }

                // alternative import format: prismlauncher://import/ENCODED
                if (encodedTarget.isEmpty()) {

                    QString p = path;

                    if (p.startsWith("/import/", Qt::CaseInsensitive)) {
                        p = p.mid(QString("/import/").size());
                    } else if (host == "import" && p.startsWith("/")) {
                        p = p.mid(1);
                    }

                    if (!p.isEmpty() && p != "/import") {
                        encodedTarget = p;
                    }
                }

                if (encodedTarget.isEmpty()) {
                    CustomMessageBox::selectable(
                        this,
                        tr("Error"),
                        tr("Invalid import link: missing 'url' parameter."),
                        QMessageBox::Critical
                    )->show();
                    continue;
                }

                const QString decodedStr = QUrl::fromPercentEncoding(encodedTarget.toUtf8()).trimmed();

                QUrl target = QUrl::fromUserInput(decodedStr);

                // Validate: only allow http(s)
                if (!target.isValid() || (target.scheme() != "https" && target.scheme() != "http")) {
                    CustomMessageBox::selectable(
                        this,
                        tr("Error"),
                        tr("Invalid import link: URL must be http(s)."),
                        QMessageBox::Critical
                    )->show();
                    continue;
                }

                const auto res = QMessageBox::question(
                    this,
                    tr("Install modpack"),
                    tr("Do you want to download and import a modpack from:\n%1\n\nURL:\n%2")
                        .arg(target.host(), target.toString()),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes
                );
                if (res != QMessageBox::Yes) {
                    continue;
                }

                dl_url = target;
            } else {
                dl_url = url;
            }

            if (!dl_url.isValid()) {
                continue;  // no valid url to download this resource
            }

            const QString path = dl_url.host() + '/' + dl_url.path();
            auto entry = APPLICATION->metacache()->resolveEntry("general", path);
            entry->setStale(true);
            auto dl_job = unique_qobject_ptr<NetJob>(new NetJob(tr("Modpack download"), APPLICATION->network()));
            dl_job->addNetAction(Net::ApiDownload::makeCached(dl_url, entry));
            auto archivePath = entry->getFullPath();

            bool dl_success = false;
            connect(dl_job.get(), &Task::failed, this,
                    [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
            connect(dl_job.get(), &Task::succeeded, this, [&dl_success] { dl_success = true; });

            {  // drop stack
                ProgressDialog dlUrlDialod(this);
                dlUrlDialod.setSkipButton(true, tr("Abort"));
                dlUrlDialod.execWithTask(dl_job.get());
            }

            if (!dl_success) {
                continue;  // no local file to identify
            }
            local_url = QUrl::fromLocalFile(archivePath);

        } else {
            local_url = url;
        }

        auto localFileName = QDir::toNativeSeparators(local_url.toLocalFile());
        QFileInfo localFileInfo(localFileName);

        if (localFileName.isEmpty() || !localFileInfo.exists()) {
            qDebug() << "Ignoring invalid path" << localFileName;
            continue;
        }

        auto type = ResourceUtils::identify(localFileInfo);

        if (ModPlatform::ResourceTypeUtils::VALID_RESOURCES.count(type) == 0) {  // probably instance/modpack
            addInstance(localFileName, extra_info);
            continue;
        }

        if (APPLICATION->instances()->count() <= 0) {
            CustomMessageBox::selectable(this, tr("No instance!"),
                                         tr("No instance available to add the resource to.\nPlease create a new instance before "
                                            "attempting to install this resource again."),
                                         QMessageBox::Critical)
                ->show();
            continue;
        }
        ImportResourceDialog dlg(localFileName, type, this);

        if (dlg.exec() != QDialog::Accepted)
            continue;

        qDebug() << "Adding resource" << localFileName << "to" << dlg.selectedInstanceKey;

        auto inst = APPLICATION->instances()->getInstanceById(dlg.selectedInstanceKey);
        auto minecraftInst = dynamic_cast<MinecraftInstance*>(inst);

        switch (type) {
            case ModPlatform::ResourceType::ResourcePack:
                minecraftInst->resourcePackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::TexturePack:
                minecraftInst->texturePackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::DataPack:
                qWarning() << "Importing of Data Packs not supported at this time. Ignoring" << localFileName;
                break;
            case ModPlatform::ResourceType::Mod:
                minecraftInst->loaderModList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::ShaderPack:
                minecraftInst->shaderPackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::World:
                minecraftInst->worldList()->installWorld(localFileInfo);
                break;
            case ModPlatform::ResourceType::Unknown:
            default:
                qDebug() << "Can't Identify" << localFileName << "Ignoring it.";
                break;
        }
    }
}

void MainWindow::on_actionREDDIT_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.SUBREDDIT_URL));
}

void MainWindow::on_actionDISCORD_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.DISCORD_URL));
}

void MainWindow::on_actionMATRIX_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.MATRIX_URL));
}

void MainWindow::on_actionChangeInstIcon_triggered()
{
    if (!m_selectedInstance)
        return;

    IconPickerDialog dlg(this);
    dlg.execWithSelection(m_selectedInstance->iconKey());
    if (dlg.result() == QDialog::Accepted) {
        m_selectedInstance->setIconKey(dlg.selectedIconKey);
        auto icon = APPLICATION->icons()->getIcon(dlg.selectedIconKey);
        ui->actionChangeInstIcon->setIcon(icon);
        changeIconButton->setIcon(icon);
    }
}

void MainWindow::iconUpdated(QString icon)
{
    if (icon == m_currentInstIcon) {
        auto new_icon = APPLICATION->icons()->getIcon(m_currentInstIcon);
        ui->actionChangeInstIcon->setIcon(new_icon);
        changeIconButton->setIcon(new_icon);
    }
}

void MainWindow::updateInstanceToolIcon(QString new_icon)
{
    m_currentInstIcon = new_icon;
    auto icon = APPLICATION->icons()->getIcon(m_currentInstIcon);
    ui->actionChangeInstIcon->setIcon(icon);
    changeIconButton->setIcon(icon);
}

void MainWindow::setSelectedInstanceById(const QString& id)
{
    if (id.isNull())
        return;
    const QModelIndex index = APPLICATION->instances()->getInstanceIndexById(id);
    if (index.isValid()) {
        QModelIndex selectionIndex = proxymodel->mapFromSource(index);
        view->selectionModel()->setCurrentIndex(selectionIndex, QItemSelectionModel::ClearAndSelect);
        updateStatusCenter();
    }
}

void MainWindow::on_actionChangeInstGroup_triggered()
{
    if (!m_selectedInstance)
        return;

    InstanceId instId = m_selectedInstance->id();
    QString src(APPLICATION->instances()->getInstanceGroup(instId));

    QStringList groups = APPLICATION->instances()->getGroups();
    groups.prepend("");
    int index = groups.indexOf(src);
    bool ok = false;
    QString dst = QInputDialog::getItem(this, tr("Group name"), tr("Enter a new group name."), groups, index, true, &ok);
    dst = dst.simplified();

    if (ok) {
        APPLICATION->instances()->setInstanceGroup(instId, dst);
    }
}

void MainWindow::deleteGroup(QString group)
{
    Q_ASSERT(!group.isEmpty());

    const int reply = QMessageBox::question(this, tr("Delete group"), tr("Are you sure you want to delete the group '%1'?").arg(group),
                                            QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
        APPLICATION->instances()->deleteGroup(group);
}

void MainWindow::renameGroup(QString group)
{
    Q_ASSERT(!group.isEmpty());

    QString name = QInputDialog::getText(this, tr("Rename group"), tr("Enter a new group name."), QLineEdit::Normal, group);
    name = name.simplified();
    if (name.isNull() || name == group)
        return;

    const bool empty = name.isEmpty();
    const bool duplicate = APPLICATION->instances()->getGroups().contains(name, Qt::CaseInsensitive) && group.toLower() != name.toLower();

    if (empty || duplicate) {
        QMessageBox::warning(this, tr("Cannot rename group"), empty ? tr("Cannot set empty name.") : tr("Group already exists. :/"));
        return;
    }

    APPLICATION->instances()->renameGroup(group, name);
}

void MainWindow::undoTrashInstance()
{
    if (!APPLICATION->instances()->undoTrashInstance())
        QMessageBox::warning(
            this, tr("Failed to undo trashing instance"),
            tr("Some instances and shortcuts could not be restored.\nPlease check your trashbin to manually restore them."));
    ui->actionUndoTrashInstance->setEnabled(APPLICATION->instances()->trashedSomething());
}

void MainWindow::on_actionViewLauncherRootFolder_triggered()
{
    DesktopServices::openPath(".");
}

void MainWindow::on_actionViewInstanceFolder_triggered()
{
    QString str = APPLICATION->settings()->get("InstanceDir").toString();
    DesktopServices::openPath(str);
}

void MainWindow::on_actionViewCentralModsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->settings()->get("CentralModsDir").toString(), true);
}

void MainWindow::on_actionViewSkinsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->settings()->get("SkinsDir").toString(), true);
}

void MainWindow::on_actionViewIconThemeFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getIconThemesFolder().path(), true);
}

void MainWindow::on_actionViewWidgetThemeFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getApplicationThemesFolder().path(), true);
}

void MainWindow::on_actionViewCatPackFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getCatPacksFolder().path(), true);
}

void MainWindow::on_actionViewIconsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->icons()->getDirectory(), true);
}

void MainWindow::on_actionViewLogsFolder_triggered()
{
    DesktopServices::openPath("logs", true);
}

void MainWindow::on_actionViewJavaFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->javaPath(), true);
}

void MainWindow::refreshInstances()
{
    APPLICATION->instances()->loadList();
}

void MainWindow::checkForUpdates()
{
    if (APPLICATION->updaterEnabled()) {
        APPLICATION->triggerUpdateCheck();
    } else {
        qWarning() << "Updater not set up. Cannot check for updates.";
    }
}

void MainWindow::on_actionSettings_triggered()
{
    APPLICATION->ShowGlobalSettings(this, "global-settings");
}

void MainWindow::globalSettingsClosed()
{
    // FIXME: quick HACK to make this work. improve, optimize.
    APPLICATION->instances()->loadList();
    proxymodel->invalidate();
    proxymodel->sort(0);
    updateMainToolBar();
    updateLaunchButton();
    updateThemeMenu();
    updateStatusCenter();
    // This needs to be done to prevent UI elements disappearing in the event the config is changed
    // but Prism Launcher exits abnormally, causing the window state to never be saved:
    APPLICATION->settings()->set("MainWindowState", QString::fromUtf8(saveState().toBase64()));
    update();
}

void MainWindow::on_actionEditInstance_triggered()
{
    if (!m_selectedInstance)
        return;

    if (m_selectedInstance->canEdit()) {
        APPLICATION->showInstanceWindow(m_selectedInstance);
    } else {
        CustomMessageBox::selectable(this, tr("Instance not editable"),
                                     tr("This instance is not editable. It may be broken, invalid, or too old. Check logs for details."),
                                     QMessageBox::Critical)
            ->show();
    }
}

void MainWindow::on_actionManageAccounts_triggered()
{
    APPLICATION->ShowGlobalSettings(this, "accounts");
}

void MainWindow::on_actionReportBug_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.BUG_TRACKER_URL));
}

void MainWindow::on_actionClearMetadata_triggered()
{
    // This if contains side effects!
    if (!APPLICATION->metacache()->evictAll()) {
        CustomMessageBox::selectable(this, tr("Error"),
                                     tr("Metadata cache clear Failed!\nTo clear the metadata cache manually, press Folders -> View "
                                        "Launcher Root Folder, and after closing the launcher delete the folder named \"meta\"\n"),
                                     QMessageBox::Warning)
            ->show();
    }

    APPLICATION->metacache()->SaveNow();
}

#ifdef Q_OS_MAC
void MainWindow::on_actionAddToPATH_triggered()
{
    auto binaryPath = APPLICATION->applicationFilePath();
    auto targetPath = QString("/usr/local/bin/%1").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME);
    qDebug() << "Symlinking" << binaryPath << "to" << targetPath;

    QStringList args;
    args << "-e";
    args << QString("do shell script \"mkdir -p /usr/local/bin && ln -sf '%1' '%2'\" with administrator privileges")
                .arg(binaryPath, targetPath);
    auto outcome = QProcess::execute("/usr/bin/osascript", args);
    if (!outcome) {
        QMessageBox::information(this, tr("Successfully added %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                                 tr("%1 was successfully added to your PATH. You can now start it by running `%2`.")
                                     .arg(BuildConfig.LAUNCHER_DISPLAYNAME, BuildConfig.LAUNCHER_APP_BINARY_NAME));
    } else {
        QMessageBox::critical(this, tr("Failed to add %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                              tr("An error occurred while trying to add %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    }
}
#endif

void MainWindow::on_actionOpenWiki_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.WIKI_URL));
}

void MainWindow::on_actionMoreNews_triggered()
{
    auto entries = m_newsChecker->getNewsEntries();
    NewsDialog news_dialog(entries, this);
    news_dialog.exec();
}

void MainWindow::newsButtonClicked()
{
    auto entries = m_newsChecker->getNewsEntries();
    NewsDialog news_dialog(entries, this);
    news_dialog.toggleArticleList();
    news_dialog.exec();
}

void MainWindow::onCatChanged(int)
{
    setCatBackground(APPLICATION->settings()->get("TheCat").toBool());
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::on_actionDeleteInstance_triggered()
{
    if (!m_selectedInstance) {
        return;
    }

    if (m_selectedInstance->isRunning()) {
        CustomMessageBox::selectable(this, tr("Cannot Delete Running Instance"),
                                     tr("The selected instance is currently running and cannot be deleted. Please stop the instance before "
                                        "attempting to delete it."),
                                     QMessageBox::Warning, QMessageBox::Ok)
            ->exec();
        return;
    }
    auto id = m_selectedInstance->id();

    QString shortcutStr;
    auto shortcuts = m_selectedInstance->shortcuts();
    if (!shortcuts.isEmpty())
        shortcutStr = tr(" and its %n registered shortcut(s)", "", shortcuts.size());
    auto response = CustomMessageBox::selectable(this, tr("Confirm Deletion"),
                                                 tr("You are about to delete \"%1\"%2.\n"
                                                    "This may be permanent and will completely delete the instance.\n\n"
                                                    "Are you sure?")
                                                     .arg(m_selectedInstance->name(), shortcutStr),
                                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();

    if (response != QMessageBox::Yes)
        return;

    if (!checkLinkedInstances(id, this, tr("Deleting")))
        return;

    if (APPLICATION->instances()->trashInstance(id)) {
        ui->actionUndoTrashInstance->setEnabled(APPLICATION->instances()->trashedSomething());
    } else {
        APPLICATION->instances()->deleteInstance(id);
    }
    APPLICATION->settings()->set("SelectedInstance", QString());
    selectionBad();
}

void MainWindow::on_actionExportInstanceZip_triggered()
{
    if (m_selectedInstance) {
        ExportInstanceDialog dlg(m_selectedInstance, this);
        dlg.exec();
    }
}

void MainWindow::on_actionExportInstanceMrPack_triggered()
{
    if (m_selectedInstance) {
        auto instance = dynamic_cast<MinecraftInstance*>(m_selectedInstance);
        if (instance != nullptr) {
            ExportPackDialog dlg(instance, this);
            dlg.exec();
        }
    }
}

void MainWindow::on_actionExportInstanceFlamePack_triggered()
{
    if (m_selectedInstance) {
        auto instance = dynamic_cast<MinecraftInstance*>(m_selectedInstance);
        if (instance) {
            if (auto cmp = instance->getPackProfile()->getComponent("net.minecraft");
                cmp && cmp->getVersionFile() && cmp->getVersionFile()->type == "snapshot") {
                QMessageBox msgBox(this);
                msgBox.setText("Snapshots are currently not supported by CurseForge modpacks.");
                msgBox.exec();
                return;
            }
            ExportPackDialog dlg(instance, this, ModPlatform::ResourceProvider::FLAME);
            dlg.exec();
        }
    }
}

void MainWindow::on_actionRenameInstance_triggered()
{
    if (m_selectedInstance) {
        view->edit(view->currentIndex());
    }
}

void MainWindow::on_actionViewSelectedInstFolder_triggered()
{
    if (m_selectedInstance) {
        QString str = m_selectedInstance->instanceRoot();
        DesktopServices::openPath(QFileInfo(str));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Save the window state and geometry.
    APPLICATION->settings()->set("MainWindowState", QString::fromUtf8(saveState().toBase64()));
    APPLICATION->settings()->set("MainWindowGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    instanceToolbarSetting->set(QString::fromUtf8(ui->instanceToolBar->getVisibilityState().toBase64()));
    event->accept();
    emit isClosing();
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::instanceActivated(QModelIndex index)
{
    if (!index.isValid())
        return;
    QString id = index.data(InstanceList::InstanceIDRole).toString();
    BaseInstance* inst = APPLICATION->instances()->getInstanceById(id);
    if (!inst)
        return;

    activateInstance(inst);
}

void MainWindow::on_actionLaunchInstance_triggered()
{
    if (m_selectedInstance && !m_selectedInstance->isRunning()) {
        APPLICATION->launch(m_selectedInstance);
    }
}

void MainWindow::activateInstance(BaseInstance* instance)
{
    APPLICATION->launch(instance);
}

void MainWindow::on_actionKillInstance_triggered()
{
    if (m_selectedInstance && m_selectedInstance->isRunning()) {
        APPLICATION->kill(m_selectedInstance);
    }
}

void MainWindow::on_actionCreateInstanceShortcut_triggered()
{
    if (!m_selectedInstance)
        return;

    CreateShortcutDialog shortcutDlg(m_selectedInstance, this);
    if (!shortcutDlg.exec())
        return;
    shortcutDlg.createShortcut();
}

void MainWindow::taskEnd()
{
    QObject* sender = QObject::sender();
    if (sender == m_versionLoadTask)
        m_versionLoadTask = NULL;

    sender->deleteLater();
}

void MainWindow::startTask(Task* task)
{
    connect(task, &Task::succeeded, this, &MainWindow::taskEnd);
    connect(task, &Task::failed, this, &MainWindow::taskEnd);
    task->start();
}

void MainWindow::instanceChanged(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    if (!current.isValid()) {
        APPLICATION->settings()->set("SelectedInstance", QString());
        selectionBad();
        return;
    }
    if (m_selectedInstance) {
        disconnect(m_selectedInstance, &BaseInstance::runningStatusChanged, this, &MainWindow::refreshCurrentInstance);
        disconnect(m_selectedInstance, &BaseInstance::profilerChanged, this, &MainWindow::refreshCurrentInstance);
    }
    QString id = current.data(InstanceList::InstanceIDRole).toString();
    m_selectedInstance = APPLICATION->instances()->getInstanceById(id);
    if (m_selectedInstance) {
        ui->instanceToolBar->setEnabled(true);
        setInstanceActionsEnabled(true);
        ui->actionLaunchInstance->setEnabled(m_selectedInstance->canLaunch());

        ui->actionKillInstance->setEnabled(m_selectedInstance->isRunning());
        ui->actionExportInstance->setEnabled(m_selectedInstance->canExport());
        renameButton->setText(m_selectedInstance->name());
        m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
        updateStatusCenter();
        updateInstanceToolIcon(m_selectedInstance->iconKey());

        updateLaunchButton();

        APPLICATION->settings()->set("SelectedInstance", m_selectedInstance->id());

        connect(m_selectedInstance, &BaseInstance::runningStatusChanged, this, &MainWindow::refreshCurrentInstance);
        connect(m_selectedInstance, &BaseInstance::profilerChanged, this, &MainWindow::refreshCurrentInstance);

        frendsUpdateInstanceState();
    } else {
        APPLICATION->settings()->set("SelectedInstance", QString());
        selectionBad();
        return;
    }
}

void MainWindow::instanceSelectRequest(QString id)
{
    setSelectedInstanceById(id);
}

void MainWindow::instanceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    auto current = view->selectionModel()->currentIndex();
    QItemSelection test(topLeft, bottomRight);
    if (test.contains(current)) {
        instanceChanged(current, current);
    }
}

void MainWindow::selectionBad()
{
    // start by reseting everything...
    m_selectedInstance = nullptr;
    m_statusLeft->setText(tr("No instance selected"));

    statusBar()->clearMessage();
    ui->instanceToolBar->setEnabled(false);
    setInstanceActionsEnabled(false);
    updateLaunchButton();
    renameButton->setText(tr("Rename Instance"));
    updateInstanceToolIcon("grass");

    // ...and then see if we can enable the previously selected instance
    setSelectedInstanceById(APPLICATION->settings()->get("SelectedInstance").toString());

    frendsUpdateInstanceState();
}

void MainWindow::checkInstancePathForProblems()
{
    QString instanceFolder = APPLICATION->settings()->get("InstanceDir").toString();
    if (FS::checkProblemticPathJava(QDir(instanceFolder))) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder contains \'!\' and this is known to cause Java problems!"));
        warning.setInformativeText(tr("You have now two options: <br/>"
                                      " - change the instance folder in the settings <br/>"
                                      " - move this installation of %1 to a different folder")
                                       .arg(BuildConfig.LAUNCHER_DISPLAYNAME));
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    }
    auto tempFolderText =
        tr("This is a problem: <br/>"
           " - The launcher will likely be deleted without warning by the operating system <br/>"
           " - close the launcher now and extract it to a real location, not a temporary folder");
    QString pathfoldername = QDir(instanceFolder).absolutePath();
    if (pathfoldername.contains("Rar$", Qt::CaseInsensitive)) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder contains \'Rar$\' - that means you haven't extracted the launcher archive!"));
        warning.setInformativeText(tempFolderText);
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    } else if (pathfoldername.startsWith(QDir::tempPath()) || pathfoldername.contains("/TempState/")) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder is in a temporary folder: \'%1\'!").arg(QDir::tempPath()));
        warning.setInformativeText(tempFolderText);
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    }
}

void MainWindow::updateStatusCenter()
{
    m_statusCenter->setVisible(APPLICATION->settings()->get("ShowGlobalGameTime").toBool());

    int timePlayed = APPLICATION->instances()->getTotalPlayTime();
    if (timePlayed > 0) {
        m_statusCenter->setText(
            tr("Total playtime: %1")
                .arg(Time::prettifyDuration(timePlayed, APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool())));
    }
}
// "Instance actions" are actions that require an instance to be selected (i.e. "new instance" is not here)
// Actions that also require other conditions (e.g. a running instance) won't be changed.
void MainWindow::setInstanceActionsEnabled(bool enabled)
{
    ui->actionEditInstance->setEnabled(enabled);
    ui->actionChangeInstGroup->setEnabled(enabled);
    ui->actionViewSelectedInstFolder->setEnabled(enabled);
    ui->actionExportInstance->setEnabled(enabled);
    ui->actionDeleteInstance->setEnabled(enabled);
    ui->actionCopyInstance->setEnabled(enabled);
    ui->actionCreateInstanceShortcut->setEnabled(enabled);
}

void MainWindow::refreshCurrentInstance()
{
    auto current = view->selectionModel()->currentIndex();
    instanceChanged(current, current);
}
