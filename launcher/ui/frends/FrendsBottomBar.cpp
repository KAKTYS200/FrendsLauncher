// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
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

#include "FrendsBottomBar.h"

#include <QCheckBox>
#include <QComboBox>
#include <QScreen>
#include <QTimer>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QApplication>
#include <QProgressBar>
#include <QScopedValueRollback>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>

#include "FrendsPalette.h"
#include "FrendsScale.h"

namespace {
/**
 * A combo box whose list opens above the widget. The bar sits at the very
 * bottom of the window, so a list opening downwards would cover the whole
 * launcher instead of hanging off the bar.
 */
class UpwardComboBox : public QComboBox {
   public:
    using QComboBox::QComboBox;

   protected:
    void showPopup() override
    {
        QComboBox::showPopup();

        QWidget* popup = view() ? view()->window() : nullptr;
        if (!popup)
            return;

        const QPoint anchor = mapToGlobal(QPoint(0, 0));
        const QRect screenGeometry = screen() ? screen()->availableGeometry() : QRect(anchor, QSize(1, 1));

        // The container sizes itself right after being shown, so the move is
        // deferred until the layout has settled.
        QTimer::singleShot(0, popup, [popup, anchor, screenGeometry] {
            int targetY = anchor.y() - popup->height();
            if (targetY < screenGeometry.top())
                targetY = screenGeometry.top();
            popup->move(anchor.x(), targetY);
        });
    }
};

QString comboStyle(qreal scale)
{
    const auto& theme = FrendsTheme::colors();
    return QStringLiteral(
               "QComboBox {"
               "  background: %9;"
               "  color: %10;"
               "  border: 1px solid %11;"
               "  border-radius: %8px;"
               "  padding: %1px %2px;"
               "  min-height: %3px;"
               "  font-size: %4px;"
               "}"
               "QComboBox:hover { border: 1px solid %12; }"
               "QComboBox::drop-down {"
               "  width: %5px;"
               "  border: none;"
               "  background: transparent;"
               "}"
               // Styling drop-down drops the native indicator, so the arrow has
               // to be drawn back in - otherwise the field ends in a blank grey
               // block. A real image is used because the CSS border-triangle
               // trick does not apply to QSS sub-controls.
               "QComboBox::down-arrow {"
               "  image: url(:/frends/combo-arrow-down-light.svg);"
               "  width: %6px;"
               "  height: %7px;"
               "}"
               "QComboBox QAbstractItemView {"
               "  background: %9;"
               "  color: %10;"
               "  border: 1px solid %11;"
               "  selection-background-color: %13;"
               "  selection-color: %14;"
               "  outline: none;"
               "}")
        .arg(qRound(4 * scale))
        .arg(qRound(8 * scale))
        .arg(qRound(22 * scale))
        .arg(qMax(9, int(qRound(12 * scale))))
        .arg(qRound(22 * scale))
        .arg(qMax(8, int(qRound(10 * scale))))
        .arg(qMax(5, int(qRound(6 * scale))))
        .arg(qMax(4, int(qRound(6 * scale))))
        .arg(theme.field.name())
        .arg(theme.text.name())
        .arg(FrendsTheme::css(theme.fieldBorder))
        .arg(FrendsTheme::css(theme.text))
        .arg(theme.navActiveTop.name())
        .arg(theme.textOnAccent.name());
}

QString iconButtonStyle(qreal scale)
{
    const auto& theme = FrendsTheme::colors();
    // Square, slightly raised plates: on the panel a bare glyph floats away
    // from the row, the plate gives it the same weight as the play button.
    return QStringLiteral(
               "QToolButton {"
               "  background: %3;"
               "  border: 1px solid %4;"
               "  border-radius: %1px;"
               "  padding: %2px;"
               "}"
               "QToolButton:hover { background: %5; border: 1px solid %6; }"
               "QToolButton:pressed { background: %7; }")
        .arg(qMax(4, int(qRound(6 * scale))))
        .arg(qMax(2, int(qRound(5 * scale))))
        .arg(FrendsTheme::css(theme.buttonPlate))
        .arg(FrendsTheme::css(theme.buttonPlateBorder))
        .arg(FrendsTheme::css(theme.buttonPlate.lighter(135)))
        .arg(FrendsTheme::css(theme.text))
        .arg(FrendsTheme::css(theme.buttonPlate.darker(115)));
}
}  // namespace

FrendsBottomBar::FrendsBottomBar(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAutoFillBackground(false);

    // The download strip lives above the control row, so the progress of an
    // install is visible without opening anything.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* stripRow = new QHBoxLayout(m_downloadStrip = new QWidget(this));
    m_downloadStrip->hide();
    stripRow->setContentsMargins(FrendsScale::px(14, this), FrendsScale::px(6, this), FrendsScale::px(14, this),
                                 FrendsScale::px(2, this));
    stripRow->setSpacing(FrendsScale::px(8, this));

    m_downloadLabel = new QLabel(this);
    m_downloadLabel->hide();
    stripRow->addWidget(m_downloadLabel);

    m_downloadBar = new QProgressBar(this);
    m_downloadBar->setTextVisible(false);
    m_downloadBar->hide();
    stripRow->addWidget(m_downloadBar, 1);

    m_downloadPercent = new QLabel(this);
    m_downloadPercent->hide();
    m_downloadPercent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    // Width and font are set in styleWidgets(), where the scale is known.
    stripRow->addWidget(m_downloadPercent);

    m_downloadCancel = new QPushButton(this);
    m_downloadCancel->setCursor(Qt::PointingHandCursor);
    m_downloadCancel->setFocusPolicy(Qt::NoFocus);
    connect(m_downloadCancel, &QPushButton::clicked, this, &FrendsBottomBar::downloadCancelled);
    m_downloadCancel->hide();
    stripRow->addWidget(m_downloadCancel);

    outer->addWidget(m_downloadStrip);

    auto* rowHost = new QWidget(this);
    m_layout = new QHBoxLayout(rowHost);
    outer->addWidget(rowHost, 1);

    // One click to add another account, right where the accounts live.
    m_addAccountButton = new QToolButton(this);
    m_addAccountButton->setText(QStringLiteral("+"));
    m_addAccountButton->setCursor(Qt::PointingHandCursor);
    m_addAccountButton->setFocusPolicy(Qt::NoFocus);
    connect(m_addAccountButton, &QToolButton::clicked, this, &FrendsBottomBar::addAccountRequested);
    m_layout->addWidget(m_addAccountButton);

    m_accountCombo = new UpwardComboBox(this);
    m_accountCombo->setToolTip(tr("Аккаунт"));
    connect(m_accountCombo, QOverload<int>::of(&QComboBox::activated), this, &FrendsBottomBar::accountActivated);
    m_layout->addWidget(m_accountCombo);

    m_instanceCombo = new UpwardComboBox(this);
    m_instanceCombo->setToolTip(tr("Сборка"));
    connect(m_instanceCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        const QString id = m_instanceCombo->itemData(index).toString();
        if (!id.isEmpty())
            emit instanceActivated(id);
    });
    m_layout->addWidget(m_instanceCombo);

    m_updateCheck = new QCheckBox(this);
    m_updateCheck->setCursor(Qt::PointingHandCursor);
    // Checked by default: the launcher then downloads whatever is missing
    // before the game starts, instead of failing with missing libraries.
    m_updateCheck->setChecked(true);
    m_layout->addWidget(m_updateCheck);

    m_layout->addStretch(1);

    m_playButton = new QPushButton(this);
    m_playButton->setCursor(Qt::PointingHandCursor);
    connect(m_playButton, &QPushButton::clicked, this, &FrendsBottomBar::playRequested);
    m_layout->addWidget(m_playButton);

    m_modsButton = new QPushButton(this);
    m_modsButton->setCursor(Qt::PointingHandCursor);
    m_modsButton->setFocusPolicy(Qt::NoFocus);
    connect(m_modsButton, &QPushButton::clicked, this, &FrendsBottomBar::modsRequested);
    m_layout->addWidget(m_modsButton);

    // White glyphs of the bundled icon theme, so the buttons read clearly on
    // the green panel instead of the thin style defaults.
    auto themedIcon = [](const QString& name, QStyle::StandardPixmap fallback) {
        QIcon icon(QStringLiteral(":/icons/flat_white/scalable/%1.svg").arg(name));
        return icon.isNull() ? QApplication::style()->standardIcon(fallback) : icon;
    };

    auto makeIconButton = [this, &themedIcon](const QString& name, QStyle::StandardPixmap fallback) {
        auto* button = new QToolButton(this);
        button->setIcon(themedIcon(name, fallback));
        button->setCursor(Qt::PointingHandCursor);
        // These buttons carry only a glyph, so nothing sets their text and a
        // screen reader (or UI automation) would see four nameless buttons.
        // The name is filled in by retranslate().
        button->setAccessibleName(name);
        m_layout->addWidget(button);
        return button;
    };

    m_refreshButton = makeIconButton(QStringLiteral("refresh"), QStyle::SP_BrowserReload);
    connect(m_refreshButton, &QToolButton::clicked, this, &FrendsBottomBar::refreshRequested);

    // One-click launcher update, like the other launchers have.
    m_launcherUpdateButton = makeIconButton(QStringLiteral("checkupdate"), QStyle::SP_ArrowDown);
    connect(m_launcherUpdateButton, &QToolButton::clicked, this, &FrendsBottomBar::launcherUpdateRequested);

    m_folderButton = makeIconButton(QStringLiteral("viewfolder"), QStyle::SP_DirOpenIcon);
    connect(m_folderButton, &QToolButton::clicked, this, &FrendsBottomBar::folderRequested);

    m_settingsButton = makeIconButton(QStringLiteral("settings"), QStyle::SP_FileDialogDetailedView);
    connect(m_settingsButton, &QToolButton::clicked, this, &FrendsBottomBar::settingsRequested);

    applyScale();
    retranslate();
}

void FrendsBottomBar::showDownloadProgress(const QString& label)
{
    if (m_downloadLabel) {
        m_downloadLabel->setText(label);
        m_downloadLabel->show();
    }
    if (m_downloadBar) {
        // Busy mode only until the first real progress arrives; resetting the
        // range on every status change would keep wiping the fill. isHidden()
        // rather than isVisible(): a download can start before the main window
        // is on screen, and then nothing would ever be marked visible.
        if (m_downloadBar->isHidden()) {
            m_downloadBar->setRange(0, 0);
            m_downloadBar->show();
        }
    }
    if (m_downloadPercent) {
        // A fresh task starts without a known percentage; the busy bar carries
        // the message until the first progress() arrives.
        m_downloadPercent->setText(QString());
        m_downloadPercent->hide();
    }
    if (m_downloadCancel)
        m_downloadCancel->show();
    if (m_downloadStrip)
        m_downloadStrip->show();
    // The strip has its own row above the controls, so the panel simply grows
    // by that row. Forcing a height here used to fight with applyScale().
    updateGeometry();
}

void FrendsBottomBar::setDownloadProgress(qint64 current, qint64 total)
{
    // isHidden(), not isVisible(): the strip can be asked to show progress while
    // the window is still being built, and isVisible() would then be false and
    // the percentage would never appear.
    if (!m_downloadBar || m_downloadBar->isHidden())
        return;

    if (total > 0) {
        const qint64 percent = qBound<qint64>(0, current * 100 / total, static_cast<qint64>(100));
        m_downloadBar->setRange(0, 100);
        m_downloadBar->setValue(int(percent));
        if (m_downloadPercent) {
            m_downloadPercent->setText(QStringLiteral("%1%").arg(percent));
            m_downloadPercent->show();
        }
    } else {
        m_downloadBar->setRange(0, 0);
        if (m_downloadPercent)
            m_downloadPercent->hide();
    }
}

void FrendsBottomBar::hideDownloadProgress()
{
    if (m_downloadLabel)
        m_downloadLabel->hide();
    if (m_downloadBar)
        m_downloadBar->hide();
    if (m_downloadPercent)
        m_downloadPercent->hide();
    if (m_downloadCancel)
        m_downloadCancel->hide();
    if (m_downloadStrip)
        m_downloadStrip->hide();
    updateGeometry();
}

void FrendsBottomBar::applyScale()
{
    const qreal s = FrendsScale::factor(this);

    // A minimum, not a fixed height: the download strip lives above the control
    // row, and a fixed height would clip it away the moment it appears.
    setMinimumHeight(FrendsScale::px(62, this));
    setMaximumHeight(QWIDGETSIZE_MAX);
    // Tight margins and gaps: the row is the busiest place in the launcher and
    // at the base window width it only fits with everything drawn in close.
    m_layout->setContentsMargins(FrendsScale::px(10, this), FrendsScale::px(10, this), FrendsScale::px(10, this),
                                 FrendsScale::px(10, this));
    m_layout->setSpacing(FrendsScale::px(7, this));

    styleWidgets(s);

    // The account and version pickers carry the whole profile ("kaktys2009
    // (offline)" and a full instance name), and at the old 230px ceiling both
    // were elided to a couple of characters. They keep a wide cap, but the
    // minimum is capped by what actually fits: the row holds two combos, the
    // play button, the mods button, the update checkbox and four icon buttons,
    // and at the 1067px base window width anything above ~210px per combo
    // pushes the right-hand buttons out of the window and on top of each other.
    const int comboWidth = FrendsScale::px(440, this);
    // The bar has a lot to fit. The combos still give way before the buttons,
    // but they keep enough room to stay readable: at 150px a name like
    // "kaktys2009 (offline)" was already cut, and the whole point of the wider
    // cap was to show the real profile instead of an ellipsis.
    const int comboMinWidth = FrendsScale::px(210, this);
    for (auto* combo : { m_accountCombo, m_instanceCombo }) {
        combo->setMinimumWidth(comboMinWidth);
        combo->setMaximumWidth(comboWidth);
        combo->setIconSize(QSize(FrendsScale::px(16, this), FrendsScale::px(16, this)));
    }

    const QSize playSize(FrendsScale::px(180, this), FrendsScale::px(42, this));
    m_playButton->setMinimumSize(playSize);
    m_playButton->setMaximumHeight(playSize.height());

    m_modsButton->setFixedWidth(FrendsScale::px(76, this));
    m_modsButton->setMinimumHeight(FrendsScale::px(42, this));

    const QSize iconSize(FrendsScale::px(22, this), FrendsScale::px(22, this));
    // m_launcherUpdateButton used to be left out here, so its icon kept the
    // style default and came out visibly bigger than its neighbours'.
    const int iconButtonSide = FrendsScale::px(36, this);
    for (auto* button : { m_refreshButton, m_launcherUpdateButton, m_folderButton, m_settingsButton }) {
        button->setIconSize(iconSize);
        button->setFixedSize(iconButtonSide, iconButtonSide);
    }
}

void FrendsBottomBar::styleWidgets(qreal scale)
{
    // Restyling a child sends StyleChange, which can come back here through
    // changeEvent(); the guard stops that from looping.
    if (m_styling)
        return;
    QScopedValueRollback<bool> guard(m_styling, true);

    const auto& theme = FrendsTheme::colors();
    const int fontSize = qMax(9, int(qRound(12 * scale)));

    if (m_downloadLabel) {
        m_downloadLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-size: %2px; background: transparent; }").arg(theme.text.name()).arg(fontSize));
    }
    if (m_downloadCancel) {
        m_downloadCancel->setText(tr("Отмена"));
        m_downloadCancel->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                           " border-radius: 3px; font-size: %4px; padding: 2px 10px; }"
                           "QPushButton:hover { background: %5; }")
                .arg(FrendsTheme::css(theme.buttonPlate))
                .arg(theme.text.name())
                .arg(FrendsTheme::css(theme.buttonPlateBorder))
                .arg(fontSize)
                .arg(FrendsTheme::css(theme.buttonPlate.lighter(130))));
        m_downloadCancel->setFixedHeight(qMax(10, FrendsScale::px(20, this)));
    }
    if (m_downloadBar) {
        // The strip sits on the panel: a muted track with the brand yellow fill.
        // It is the one thing the player watches during an install, so it gets
        // real height instead of a hairline sliver.
        const int barHeight = qMax(14, FrendsScale::px(26, this));
        m_downloadBar->setStyleSheet(QStringLiteral(
                                         "QProgressBar { background: %2; border: none;"
                                         " border-radius: 5px; height: %1px; text-align: center; }"
                                         "QProgressBar::chunk { background: %3; border-radius: 4px; }")
                                         .arg(barHeight)
                                         .arg(FrendsTheme::css(theme.dark ? QColor(255, 255, 255, 28) : QColor(0, 0, 0, 26)))
                                         .arg(theme.navActiveTop.name()));
        m_downloadBar->setFixedHeight(barHeight);
    }
    if (m_downloadPercent) {
        // Bold plus a fixed width, so the label does not shove the bar around
        // when the percentage goes from one digit to three.
        QFont percentFont = m_downloadPercent->font();
        percentFont.setBold(true);
        m_downloadPercent->setFont(percentFont);
        m_downloadPercent->setFixedWidth(qMax(34, FrendsScale::px(46, this)));
        m_downloadPercent->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-size: %2px; background: transparent; }").arg(theme.text.name()).arg(fontSize));
    }

    if (m_addAccountButton) {
        const int side = qMax(18, FrendsScale::px(30, this));
        m_addAccountButton->setFixedSize(side, side);
        m_addAccountButton->setStyleSheet(
            QStringLiteral("QToolButton { background: %1; color: %2;"
                           " border: 1px solid %3; border-radius: %4px;"
                           " font-size: %5px; font-weight: 700; padding: 0px; }"
                           "QToolButton:hover { background: %6; border: 1px solid %7; }"
                           "QToolButton:pressed { background: %8; }")
                .arg(theme.field.name())
                .arg(theme.text.name())
                .arg(FrendsTheme::css(theme.fieldBorder))
                .arg(qMax(4, int(qRound(6 * scale))))
                .arg(qMax(12, FrendsScale::px(17, this)))
                .arg(FrendsTheme::css(theme.field.lighter(115)))
                .arg(FrendsTheme::css(theme.text))
                .arg(FrendsTheme::css(theme.field.darker(115))));
        m_addAccountButton->setToolTip(tr("Добавить аккаунт"));
    }
    m_accountCombo->setStyleSheet(comboStyle(scale));
    m_instanceCombo->setStyleSheet(comboStyle(scale));

    const int indicator = qMax(10, int(qRound(14 * scale)));
    m_updateCheck->setStyleSheet(QStringLiteral(
                                     "QCheckBox { color: %4; font-size: %1px; spacing: %2px; background: transparent; }"
                                     "QCheckBox::indicator { width: %3px; height: %3px; border: 1px solid %5;"
                                     " background: %6; border-radius: 3px; }"
                                     "QCheckBox::indicator:checked { background: %7; border: 1px solid %7; }")
                                     .arg(qMax(8, fontSize - 1))
                                     .arg(qRound(6 * scale))
                                     .arg(indicator)
                                     .arg(theme.text.name())
                                     .arg(FrendsTheme::css(theme.fieldBorder))
                                     .arg(theme.field.name())
                                     .arg(theme.navActiveTop.name()));

    // The one button the whole launcher exists for, so it carries the warm
    // accent instead of the flat plate the others use.
    m_playButton->setStyleSheet(QStringLiteral(
                                    "QPushButton {"
                                    "  background: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 %3, stop:1 %4);"
                                    "  color: #ffffff;"
                                    "  border: 1px solid rgba(255, 255, 255, 60);"
                                    "  border-radius: %2px;"
                                    "  font-size: %1px;"
                                    "  font-weight: 700;"
                                    "}"
                                    "QPushButton:hover { background: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 %5, "
                                    "stop:1 %6); }"
                                    "QPushButton:pressed { background: %7; }"
                                    "QPushButton:disabled { background: %8; color: %9; border: 1px solid %10; }")
                                    .arg(qMax(10, int(qRound(15 * scale))))
                                    .arg(qMax(4, int(qRound(7 * scale))))
                                    .arg(theme.playTop.name())
                                    .arg(theme.playBottom.name())
                                    .arg(theme.playTop.lighter(112).name())
                                    .arg(theme.playBottom.lighter(110).name())
                                    .arg(theme.playBottom.darker(112).name())
                                    .arg(FrendsTheme::css(theme.buttonPlate))
                                    .arg(FrendsTheme::css(theme.textMuted))
                                    .arg(FrendsTheme::css(theme.buttonPlateBorder)));

    m_modsButton->setStyleSheet(QStringLiteral(
                                    "QPushButton {"
                                    "  background: %3;"
                                    "  border: 1px solid %4;"
                                    "  border-radius: %2px;"
                                    "  color: %5;"
                                    "  font-size: %1px;"
                                    "  font-weight: 700;"
                                    "  letter-spacing: 0.3px;"
                                    "}"
                                    "QPushButton:hover { background: %6; border: 1px solid %7; }"
                                    "QPushButton:pressed { background: %8; }")
                                    .arg(qMax(8, int(qRound(10 * scale))))
                                    .arg(qMax(4, int(qRound(7 * scale))))
                                    .arg(FrendsTheme::css(theme.buttonPlate))
                                    .arg(FrendsTheme::css(theme.buttonPlateBorder))
                                    .arg(theme.text.name())
                                    .arg(FrendsTheme::css(theme.buttonPlate.lighter(135)))
                                    .arg(FrendsTheme::css(theme.text))
                                    .arg(FrendsTheme::css(theme.buttonPlate.darker(115))));

    for (auto* button : { m_refreshButton, m_launcherUpdateButton, m_folderButton, m_settingsButton })
        button->setStyleSheet(iconButtonStyle(scale));
}

void FrendsBottomBar::retranslate()
{
    m_accountCombo->setToolTip(tr("Аккаунт"));
    m_instanceCombo->setToolTip(tr("Сборка"));
    m_updateCheck->setText(tr("Обновить клиент"));
    m_playButton->setText(m_running ? tr("Остановить") : tr("Войти в игру"));
    if (m_modsButton) {
        // The button opens the build tab now, so the label says what it does.
        m_modsButton->setText(QStringLiteral("СОЗДАТЬ\nСБОРКУ"));
        m_modsButton->setToolTip(tr("Открыть вкладку создания сборки"));
    }
    if (m_refreshButton) {
        m_refreshButton->setToolTip(tr("Обновить список сборок"));
        m_refreshButton->setAccessibleName(tr("Обновить список сборок"));
    }
    if (m_launcherUpdateButton) {
        m_launcherUpdateButton->setToolTip(tr("Обновить лаунчер"));
        m_launcherUpdateButton->setAccessibleName(tr("Обновить лаунчер"));
    }
    if (m_folderButton) {
        m_folderButton->setToolTip(tr("Открыть папку сборки"));
        m_folderButton->setAccessibleName(tr("Открыть папку сборки"));
    }
    if (m_settingsButton) {
        m_settingsButton->setToolTip(tr("Настройки"));
        m_settingsButton->setAccessibleName(tr("Настройки"));
    }
}

void FrendsBottomBar::setRunning(bool running)
{
    m_running = running;
    m_playButton->setText(running ? tr("Остановить") : tr("Войти в игру"));
}

void FrendsBottomBar::setPlayEnabled(bool enabled)
{
    m_playButton->setEnabled(enabled);
}

void FrendsBottomBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    const auto& theme = FrendsTheme::colors();

    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0.0, theme.panelTop);
    gradient.setColorAt(1.0, theme.panelBottom);
    painter.fillRect(rect(), gradient);

    // Hairline on top: the bar is a separate surface from the content above it.
    painter.fillRect(QRect(0, 0, width(), 1), theme.panelEdge);
}

void FrendsBottomBar::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        // The launcher theme changed: restyle every child from the new palette.
        // Deferred, because restyling inside the palette event would call
        // polish() on a style that is still being replaced.
        if (m_restylePending)
            return;
        m_restylePending = true;
        QTimer::singleShot(0, this, [this] {
            m_restylePending = false;
            styleWidgets(FrendsScale::factor(this));
            update();
        });
    }
}
