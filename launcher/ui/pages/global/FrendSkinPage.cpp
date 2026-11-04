// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
 *  Settings page for the built-in skin system (Frend Skin).
 */

#include "FrendSkinPage.h"
#include "ui_FrendSkinPage.h"

#include <QMessageBox>

#include "Application.h"
#include "frendskin/FrendSkinServer.h"
#include "minecraft/auth/AccountList.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/skins/SkinManageDialog.h"

FrendSkinPage::FrendSkinPage(QWidget* parent) : QWidget(parent), ui(new Ui::FrendSkinPage)
{
    ui->setupUi(this);
    loadSettings();
}

FrendSkinPage::~FrendSkinPage()
{
    delete ui;
}

bool FrendSkinPage::apply()
{
    auto* server = APPLICATION->frendSkin();
    const bool enabled = ui->skinEnabledCheckBox->isChecked();
    APPLICATION->settings()->set("FrendSkinEnabled", enabled);

    if (enabled) {
        // Reloads the bindings file and (re)starts the publisher. The game only
        // needs it at launch time, so starting here is all the runtime work.
        APPLICATION->initFrendSkin();
    } else if (server) {
        // The in-memory skins go too: without the listener they would still
        // match frendsSkinApplies() and leak into the next launch.
        server->stop();
        server->clear();
    }
    return true;
}

void FrendSkinPage::loadSettings()
{
    ui->skinEnabledCheckBox->setChecked(APPLICATION->settings()->get("FrendSkinEnabled").toBool());
}

void FrendSkinPage::on_openSkinLibraryButton_clicked()
{
    auto account = APPLICATION->accounts()->defaultAccount();
    if (!account) {
        CustomMessageBox::selectable(this, tr("Скины"),
                                     tr("Нет ни одного аккаунта — сначала добавьте его на странице «Аккаунты»."),
                                     QMessageBox::Information)
            ->exec();
        return;
    }
    SkinManageDialog dialog(this, account);
    dialog.exec();
}

void FrendSkinPage::retranslate()
{
    ui->retranslateUi(this);
}