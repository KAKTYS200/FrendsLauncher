// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Frends Launcher - Minecraft Launcher
 *  Settings page for the built-in skin system (Frend Skin).
 */

#pragma once

#include <QWidget>

#include "ui/pages/BasePage.h"

namespace Ui {
class FrendSkinPage;
}

class FrendSkinPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit FrendSkinPage(QWidget* parent = 0);
    ~FrendSkinPage();

    QString displayName() const override { return tr("Скины"); }
    QIcon icon() const override { return QIcon::fromTheme("accounts"); }
    QString id() const override { return "frend-skin-settings"; }
    bool apply() override;
    void retranslate() override;

   private:
    void loadSettings();

   private slots:
    void on_openSkinLibraryButton_clicked();

   private:
    Ui::FrendSkinPage* ui;
};