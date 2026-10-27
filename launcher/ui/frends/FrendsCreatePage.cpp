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

#include "FrendsCreatePage.h"

#include <QComboBox>
#include <QFrame>
#include <QIcon>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "Application.h"
#include "FrendsScale.h"
#include "InstanceTask.h"
#include "icons/IconList.h"
#include "settings/SettingsObject.h"
#include "ui/pages/modplatform/CustomPage.h"
#include "ui/pages/modplatform/atlauncher/AtlPage.h"
#include "ui/pages/modplatform/flame/FlamePage.h"
#include "ui/pages/modplatform/ftb/FtbPage.h"
#include "ui/pages/modplatform/modrinth/ModrinthPage.h"

/**
 * One source of builds, drawn as a card: a coloured square with the initial,
 * the name and a short line of what it gives you. CurseForge gets a filled
 * orange card so it is impossible to miss.
 */
class ProviderTile : public QPushButton {
   public:
    ProviderTile(QString title, QString subtitle, QColor accent, bool highlighted, QWidget* parent)
        : QPushButton(parent), m_title(std::move(title)), m_subtitle(std::move(subtitle)),
          m_accent(std::move(accent)), m_highlighted(highlighted)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAttribute(Qt::WA_Hover, true);
    }

    void setActive(bool active)
    {
        if (m_active == active)
            return;
        m_active = active;
        update();
    }

    /// The real logo of the source; when it is missing the initial is drawn.
    void setBrandIcon(const QIcon& icon)
    {
        m_icon = icon;
        update();
    }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal radius = height() * 0.11;

        QLinearGradient background(box.topLeft(), box.bottomLeft());
        if (m_highlighted) {
            background.setColorAt(0.0, m_accent.lighter(114));
            background.setColorAt(1.0, m_accent.darker(112));
        } else if (m_active) {
            background.setColorAt(0.0, QColor(255, 253, 240));
            background.setColorAt(1.0, QColor(255, 246, 204));
        } else if (underMouse()) {
            background.setColorAt(0.0, QColor(255, 255, 255));
            background.setColorAt(1.0, QColor(243, 247, 250));
        } else {
            background.setColorAt(0.0, QColor(255, 255, 255));
            background.setColorAt(1.0, QColor(247, 250, 252));
        }

        QColor border = m_highlighted ? m_accent.darker(125) : QColor(220, 227, 234);
        qreal borderWidth = 1.0;
        if (m_active) {
            border = QColor(249, 217, 0);
            borderWidth = 2.0;
        }
        p.setBrush(background);
        p.setPen(QPen(border, borderWidth));
        p.drawRoundedRect(box, radius, radius);

        // The logo of the source on a light square, or the initial when the
        // theme has no logo for it.
        const qreal square = height() * 0.54;
        const QRectF badgeBox(box.left() + height() * 0.16, box.center().y() - square / 2.0, square, square);
        const qreal badgeRadius = square * 0.26;

        if (m_icon.isNull()) {
            p.setPen(Qt::NoPen);
            p.setBrush(m_highlighted ? QColor(255, 255, 255, 235) : m_accent);
            p.drawRoundedRect(badgeBox, badgeRadius, badgeRadius);

            QFont initial = font();
            initial.setBold(true);
            initial.setPixelSize(int(square * 0.56));
            p.setFont(initial);
            p.setPen(m_highlighted ? m_accent.darker(118) : QColor(255, 255, 255));
            p.drawText(badgeBox, Qt::AlignCenter, m_title.left(1).toUpper());
        } else {
            // The logo fills the badge like an app icon, clipped to the square.
            const QPixmap logo = m_icon.pixmap(badgeBox.size().toSize());
            QPainterPath clip;
            clip.addRoundedRect(badgeBox, badgeRadius, badgeRadius);
            p.save();
            p.setClipPath(clip);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255));
            p.drawRoundedRect(badgeBox, badgeRadius, badgeRadius);
            if (!logo.isNull())
                p.drawPixmap(badgeBox.toRect(), logo);
            p.restore();

            p.setPen(QPen(QColor(0, 0, 0, 30), 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(badgeBox.adjusted(0.5, 0.5, -0.5, -0.5), badgeRadius, badgeRadius);
        }

        // Title and the line under it.
        const qreal textLeft = badgeBox.right() + height() * 0.16;
        const QRectF textArea(textLeft, box.top(), box.right() - textLeft - 8, box.height());

        QFont titleFont = font();
        titleFont.setBold(true);
        titleFont.setPixelSize(int(height() * 0.235));
        p.setFont(titleFont);
        p.setPen(m_highlighted ? QColor(255, 255, 255) : QColor(58, 58, 58));
        p.drawText(QRectF(textArea.left(), textArea.top() + height() * 0.16, textArea.width(), height() * 0.36),
                   Qt::AlignLeft | Qt::AlignVCenter, m_title);

        QFont subtitleFont = font();
        subtitleFont.setPixelSize(int(height() * 0.165));
        p.setFont(subtitleFont);
        p.setPen(m_highlighted ? QColor(255, 235, 226) : QColor(138, 138, 138));
        p.drawText(QRectF(textArea.left(), textArea.top() + height() * 0.5, textArea.width(), height() * 0.32),
                   Qt::AlignLeft | Qt::AlignVCenter, m_subtitle);
    }

   private:
    QString m_title;
    QString m_subtitle;
    QColor m_accent;
    QIcon m_icon;
    bool m_highlighted = false;
    bool m_active = false;
};

namespace {
// The platform pages have no stylesheet of their own, so the colours of the
// launcher theme used to show through and the pages looked out of place next
// to the light Frends content area.
QString platformPageStyle()
{
    return QStringLiteral(
        "QWidget { color: #585858; }"
        "QListView, QTreeView { background: #FFFFFF; alternate-background-color: #FFFFFF; color: #585858;"
        " border: 1px solid #D5DCE3; border-radius: 3px; outline: none; }"
        "QListView::item, QTreeView::item { padding: 4px 6px; border: none; }"
        "QListView::item:selected, QTreeView::item:selected { background: #F9D900; color: #4A3B00; }"
        "QListView::item:hover, QTreeView::item:hover { background: #F2F6F9; }"
        "QTextBrowser, QTextEdit, QPlainTextEdit { background: #FFFFFF; color: #585858; border: 1px solid #D5DCE3;"
        " border-radius: 3px; }"
        "QLineEdit { background: #FFFFFF; color: #585858; border: 1px solid #C9C9C9; border-radius: 2px; padding: 4px 8px; }"
        "QLineEdit:focus { border: 1px solid #29A0D8; }"
        "QComboBox { background: #FFFFFF; color: #585858; border: 1px solid #C9C9C9; border-radius: 2px; padding: 4px 8px; }"
        "QComboBox::drop-down { width: 20px; border: none; background: #E9E9E9; }"
        "QComboBox QAbstractItemView { background: #FFFFFF; color: #585858; selection-background-color: #F9D900;"
        " selection-color: #4A3B00; outline: none; }"
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #fdfdfd, stop:0.5 #ececec,"
        " stop:1 #e2e2e2); color: #333333; border: 1px solid #B9B9B9; border-radius: 4px; padding: 4px 10px; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #eaeaea); }"
        "QScrollBar:vertical { background: #F2F6F9; width: 10px; margin: 0; border: none; }"
        "QScrollBar::handle:vertical { background: #C2CBD4; border-radius: 5px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #A9B4C0; }"
        "QScrollBar:horizontal { background: #F2F6F9; height: 10px; margin: 0; border: none; }"
        "QScrollBar::handle:horizontal { background: #C2CBD4; border-radius: 5px; min-width: 30px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }"
        "QScrollBar::add-page, QScrollBar::sub-page { background: none; }");
}

/// Replaces the English leftovers of the upstream pages with Russian ones.
void translatePlatformPage(QWidget* page)
{
    if (auto* search = page->findChild<QLineEdit*>("searchEdit"))
        search->setPlaceholderText(QObject::tr("Поиск и фильтр…"));
    if (auto* filter = page->findChild<QPushButton*>("filterButton"))
        filter->setText(QObject::tr("Фильтры"));

    // The row colours of the list come from the widget palette, so the dark
    // theme would still show on every second row.
    if (auto* view = page->findChild<QListView*>()) {
        QPalette palette = view->palette();
        palette.setColor(QPalette::Base, QColor(0xFF, 0xFF, 0xFF));
        palette.setColor(QPalette::AlternateBase, QColor(0xFF, 0xFF, 0xFF));
        palette.setColor(QPalette::Text, QColor(0x58, 0x58, 0x58));
        palette.setColor(QPalette::WindowText, QColor(0x58, 0x58, 0x58));
        palette.setColor(QPalette::Highlight, QColor(0xF9, 0xD9, 0x00));
        palette.setColor(QPalette::HighlightedText, QColor(0x4A, 0x3B, 0x00));
        view->setPalette(palette);
    }
}
}  // namespace

FrendsCreatePage::FrendsCreatePage(QWidget* parent) : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(10);
    m_layout->setContentsMargins(0, 0, 0, 0);

    // Header: what this page is for, in two short lines.
    auto* header = new QVBoxLayout();
    header->setSpacing(2);
    m_title = new QLabel(this);
    m_subtitle = new QLabel(this);
    header->addWidget(m_title);
    header->addWidget(m_subtitle);
    m_layout->addLayout(header);

    // The sources, as cards.
    m_tileRow = new QHBoxLayout();
    m_tileRow->setSpacing(10);
    m_layout->addLayout(m_tileRow);

    m_stack = new QStackedWidget(this);

    // 0: a build made from scratch — the same form the dialog uses.
    m_form = new CustomPage(this, this);
    m_form->opened();
    m_stack->addWidget(m_form);
    m_providerPage.push_back(nullptr);
    addProviderTile(tr("Своя сборка"), tr("Выбрать версию"), QColor(0x5B, 0x7C, 0x99), false,
                    QIcon(QStringLiteral(":/icons/multimc/32x32/instances/grass_legacy.png")));

    // 1: Modrinth.
    auto* modrinth = new ModrinthPage(this, this);
    m_stack->addWidget(modrinth);
    m_providerPage.push_back(modrinth);
    modrinth->setStyleSheet(platformPageStyle());
    translatePlatformPage(modrinth);
    if (auto* versionLabel = modrinth->findChild<QLabel*>("label"))
        versionLabel->setText(tr("Версия сборки:"));
    addProviderTile(QStringLiteral("Modrinth"), tr("Каталог модпаков"), QColor(0x1B, 0xD9, 0x6A), false,
                    QIcon(QStringLiteral(":/frends/brands/modrinth.png")));

    // 2: CurseForge. It is always on the page — with a key it shows the real
    // catalog, without one it explains where to get the key.
    if (APPLICATION->capabilities() & Application::SupportsFlame) {
        auto* flame = new FlamePage(this, this);
        m_stack->addWidget(flame);
        m_providerPage.push_back(flame);
        flame->setStyleSheet(platformPageStyle());
        translatePlatformPage(flame);
        addProviderTile(QStringLiteral("CurseForge"), tr("Скачать сборку"), QColor(0xF1, 0x64, 0x36), true,
                        QIcon(QStringLiteral(":/frends/brands/curseforge.png")));
    } else {
        m_stack->addWidget(buildCurseForgeStub());
        m_providerPage.push_back(nullptr);
        addProviderTile(QStringLiteral("CurseForge"), tr("Нужен API-ключ"), QColor(0xF1, 0x64, 0x36), true,
                        QIcon(QStringLiteral(":/frends/brands/curseforge.png")));
    }

    // 3: ATLauncher.
    auto* atl = new AtlPage(this, this);
    m_stack->addWidget(atl);
    m_providerPage.push_back(atl);
    atl->setStyleSheet(platformPageStyle());
    translatePlatformPage(atl);
    addProviderTile(QStringLiteral("ATLauncher"), tr("Каталог модпаков"), QColor(0x2E, 0x9E, 0x4F), false,
                    QIcon(QStringLiteral(":/frends/brands/atlauncher.png")));

    // 4: FTB.
    auto* ftb = new FtbPage(this, this);
    m_stack->addWidget(ftb);
    m_providerPage.push_back(ftb);
    ftb->setStyleSheet(platformPageStyle());
    translatePlatformPage(ftb);
    addProviderTile(QStringLiteral("FTB"), tr("Каталог модпаков"), QColor(0xE4, 0x00, 0x2B), false,
                    QIcon(QStringLiteral(":/frends/brands/ftb.png")));

    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFixedHeight(1);
    separator->setStyleSheet(QStringLiteral("QFrame { background: #E3E9EF; border: none; }"));
    m_layout->addWidget(separator);

    m_layout->addWidget(m_stack, 1);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    m_layout->addWidget(m_hint);

    // The name and the buttons of the platform pages, which have none of
    // their own. For a build from scratch the form brings its own.
    m_nameLabel = new QLabel(this);
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setMaxLength(128);
    m_installButton = new QPushButton(this);
    m_installButton->setCursor(Qt::PointingHandCursor);
    m_installButton->setEnabled(false);
    m_cancelButton = new QPushButton(this);
    m_cancelButton->setCursor(Qt::PointingHandCursor);

    auto* nameRow = new QHBoxLayout();
    nameRow->setSpacing(8);
    nameRow->addWidget(m_nameLabel);
    nameRow->addWidget(m_nameEdit, 1);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);
    buttonRow->addStretch(1);
    buttonRow->addWidget(m_installButton);
    buttonRow->addWidget(m_cancelButton);

    m_layout->addLayout(nameRow);
    m_layout->addLayout(buttonRow);

    connect(m_installButton, &QPushButton::clicked, this, &FrendsCreatePage::acceptCreation);
    connect(m_cancelButton, &QPushButton::clicked, this, &FrendsCreatePage::rejectCreation);

    m_group = APPLICATION->settings()->get("LastUsedGroupForNewInstance").toString();

    retranslate();
    applyScale();
    selectProvider(0);
}

void FrendsCreatePage::addProviderTile(const QString& title, const QString& subtitle, const QColor& accent,
                                       bool highlighted, const QIcon& icon)
{
    const int index = m_tiles.size();
    auto* tile = new ProviderTile(title, subtitle, accent, highlighted, this);
    if (!icon.isNull())
        tile->setBrandIcon(icon);
    connect(tile, &QPushButton::clicked, this, [this, index] { selectProvider(index); });
    m_tileRow->addWidget(tile, 1);
    m_tiles.push_back(tile);
}

QWidget* FrendsCreatePage::buildCurseForgeStub()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addStretch(1);

    auto* mark = new QLabel(page);
    mark->setAlignment(Qt::AlignCenter);
    mark->setFixedSize(FrendsScale::px(64, this), FrendsScale::px(64, this));
    mark->setStyleSheet(QStringLiteral("QLabel { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                                       " stop:0 #FF8A4C, stop:1 #E0491B); color: #FFFFFF; border-radius: 14px;"
                                       " font-size: 34px; font-weight: 800; }"));
    mark->setText(QStringLiteral("C"));
    layout->addWidget(mark, 0, Qt::AlignHCenter);

    auto* heading = new QLabel(tr("Каталог CurseForge"), page);
    heading->setAlignment(Qt::AlignCenter);
    heading->setStyleSheet(QStringLiteral("color: #3A3A3A; font-size: 19px; font-weight: 700; background: transparent;"));
    layout->addWidget(heading);

    auto* text = new QLabel(
        tr("CurseForge отдаёт модпаки только по официальному API-ключу.\n"
           "Ключ бесплатный: создайте его на curseforge.com/account/api-keys\n"
           "и вставьте в настройках лаунчера на странице «Сервисы»."),
        page);
    text->setAlignment(Qt::AlignCenter);
    text->setWordWrap(true);
    text->setStyleSheet(QStringLiteral("color: #6E6E6E; font-size: 13px; background: transparent;"));
    layout->addWidget(text);

    auto* openSettings = new QPushButton(tr("Открыть настройки и вставить ключ"), page);
    openSettings->setCursor(Qt::PointingHandCursor);
    openSettings->setFixedHeight(FrendsScale::px(34, this));
    openSettings->setStyleSheet(QStringLiteral(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FF8A4C, stop:1 #E0491B);"
        " color: #FFFFFF; border: 1px solid #C94316; border-radius: 6px; font-size: 14px; font-weight: 700;"
        " padding: 6px 18px; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFA068, stop:1 #EE5A26); }"));
    connect(openSettings, &QPushButton::clicked, this, &FrendsCreatePage::openApiSettingsRequested);
    layout->addWidget(openSettings, 0, Qt::AlignHCenter);

    layout->addStretch(2);
    return page;
}

void FrendsCreatePage::selectProvider(int index)
{
    if (index < 0 || index >= m_tiles.size())
        return;

    m_currentProvider = index;
    m_stack->setCurrentIndex(index);

    for (int i = 0; i < m_tiles.size(); i++)
        m_tiles.at(i)->setActive(i == index);

    // The platform pages only fetch their lists once they are opened.
    if (index < m_providerPage.size() && m_providerPage.at(index))
        m_providerPage.at(index)->opened();

    const bool own = usesOwnForm();
    m_nameLabel->setVisible(!own);
    m_nameEdit->setVisible(!own);
    m_installButton->setVisible(!own);
    m_cancelButton->setVisible(!own);

    if (own) {
        m_task.reset();
        m_nameEdit->clear();
    }

    if (m_hint) {
        if (own)
            m_hint->setText(tr("Выберите версию Minecraft слева и нажмите «Создать сборку»."));
        else if (index == 2 && !(APPLICATION->capabilities() & Application::SupportsFlame))
            m_hint->setText(tr("CurseForge заработает сразу после того, как вы добавите API-ключ."));
        else
            m_hint->setText(tr("Найдите сборку в каталоге, выберите версию и нажмите «Установить сборку»."));
    }

    updateInstallButton();
}

bool FrendsCreatePage::usesOwnForm() const
{
    return m_currentProvider == 0;
}

void FrendsCreatePage::retranslate()
{
    if (m_title)
        m_title->setText(tr("Создание сборки"));
    if (m_subtitle)
        m_subtitle->setText(tr("Своя сборка на любую версию или готовый модпак из каталога"));
    if (m_hint) {
        m_hint->setText(usesOwnForm()
                            ? tr("Выберите версию Minecraft слева и нажмите «Создать сборку».")
                            : tr("Найдите сборку в каталоге, выберите версию и нажмите «Установить сборку»."));
    }
    if (m_nameLabel)
        m_nameLabel->setText(tr("Название сборки"));
    if (m_installButton)
        m_installButton->setText(tr("Установить сборку"));
    if (m_cancelButton)
        m_cancelButton->setText(tr("Отмена"));
    if (m_form)
        m_form->retranslate();
}

void FrendsCreatePage::applyScale()
{
    const int hintFont = FrendsScale::px(13, this);
    const int bodyFont = FrendsScale::px(14, this);
    const int buttonHeight = FrendsScale::px(32, this);
    const int tileHeight = FrendsScale::px(66, this);

    if (m_title) {
        m_title->setStyleSheet(QStringLiteral("color: #2F3A45; font-size: %1px; font-weight: 800; background: transparent;")
                                   .arg(FrendsScale::px(20, this)));
    }
    if (m_subtitle) {
        m_subtitle->setStyleSheet(
            QStringLiteral("color: #8A8A8A; font-size: %1px; background: transparent;").arg(hintFont));
    }
    for (auto* tile : m_tiles)
        tile->setFixedHeight(tileHeight);

    if (m_hint) {
        m_hint->setStyleSheet(
            QStringLiteral("color: #7a7a7a; font-size: %1px; background: transparent;").arg(hintFont));
    }
    if (m_nameLabel) {
        m_nameLabel->setStyleSheet(
            QStringLiteral("color: #585858; font-size: %1px; background: transparent;").arg(bodyFont));
    }
    if (m_nameEdit) {
        m_nameEdit->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #FFFFFF; color: #585858; border: 1px solid #C9C9C9; border-radius: 4px;"
            " padding: 6px 10px; }"
            "QLineEdit:focus { border: 1px solid #29A0D8; }"));
    }
    if (m_installButton) {
        m_installButton->setStyleSheet(QStringLiteral(
                                           "QPushButton { background: #F9D900; color: #4A3B00; border: 1px solid #D8BB00;"
                                           " border-radius: 5px; font-size: %1px; font-weight: 700; padding: 6px 18px; }"
                                           "QPushButton:enabled:hover { background: #FFE733; }"
                                           "QPushButton:enabled:pressed { background: #E3C500; }"
                                           "QPushButton:disabled { background: #E4E4E4; color: #9A9A9A; border: 1px solid #D5D5D5; }")
                                           .arg(bodyFont));
        m_installButton->setFixedHeight(buttonHeight);
    }
    if (m_cancelButton) {
        m_cancelButton->setStyleSheet(QStringLiteral(
                                          "QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                                          " stop:0 #fdfdfd, stop:0.5 #ececec, stop:1 #e2e2e2);"
                                          " color: #333333; border: 1px solid #B9B9B9; border-radius: 5px; font-size: %1px;"
                                          " padding: 6px 18px; }"
                                          "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                                          " stop:0 #ffffff, stop:1 #eaeaea); }")
                                          .arg(bodyFont));
        m_cancelButton->setFixedHeight(buttonHeight);
    }
    if (m_form)
        m_form->applyStyle();
}

void FrendsCreatePage::resetForm()
{
    m_task.reset();
    m_suggestedVersion.clear();
    m_iconPath.clear();
    m_iconName.clear();
    m_nameEdit->clear();
    if (m_form) {
        m_form->opened();
        m_form->retranslate();
    }
    updateInstallButton();
}

void FrendsCreatePage::setSuggestedPack(const QString& name, InstanceTask* task)
{
    m_task.reset(task);
    m_suggestedVersion = name;
    if (!usesOwnForm() && !name.isEmpty())
        m_nameEdit->setText(name);
    updateInstallButton();
}

void FrendsCreatePage::setSuggestedPack(const QString& name, QString version, InstanceTask* task)
{
    m_task.reset(task);
    m_suggestedVersion = version.isEmpty() ? name : version;
    if (!usesOwnForm() && !m_suggestedVersion.isEmpty())
        m_nameEdit->setText(m_suggestedVersion);
    updateInstallButton();
}

void FrendsCreatePage::setSuggestedIcon(const QString& key)
{
    m_iconKey = key;
}

void FrendsCreatePage::setSuggestedIconFromFile(const QString& path, const QString& name)
{
    m_iconPath = path;
    m_iconName = name;
}

QString FrendsCreatePage::instName() const
{
    if (usesOwnForm())
        return m_form ? m_form->instName() : QString();
    return m_nameEdit->text();
}

void FrendsCreatePage::updateInstallButton()
{
    if (!m_installButton)
        return;
    const bool ready = m_task != nullptr && !instName().trimmed().isEmpty();
    m_installButton->setEnabled(ready);
    m_installButton->setToolTip(ready ? QString() : tr("Выберите сборку в каталоге."));
}

void FrendsCreatePage::acceptCreation()
{
    if (!m_task) {
        emit dismissed();
        return;
    }

    QString name = instName().trimmed();
    if (name.isEmpty())
        name = m_suggestedVersion;

    auto* task = m_task.release();

    // A modpack brings its own logo; the vanilla form does not.
    QString iconKey = m_iconKey;
    if (!m_iconPath.isEmpty() && !m_iconName.isEmpty()) {
        APPLICATION->icons()->installIcon(m_iconPath, m_iconName);
        iconKey = m_iconName.mid(0, m_iconName.lastIndexOf('.'));
    }

    task->setName(name);
    task->setGroup(m_group);
    task->setIcon(iconKey.isEmpty() ? QStringLiteral("default") : iconKey);

    APPLICATION->settings()->set("LastUsedGroupForNewInstance", m_group);

    emit taskReady(task);
    emit dismissed();
}

void FrendsCreatePage::rejectCreation()
{
    m_task.reset();
    emit dismissed();
}
