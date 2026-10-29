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
#include <QPushButton>
#include <QStyle>
#include <QToolButton>

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
    return QStringLiteral(
               "QComboBox {"
               "  background: #ffffff;"
               "  color: #2b2b2b;"
               "  border: none;"
               "  border-radius: 2px;"
               "  padding: %1px %2px;"
               "  min-height: %3px;"
               "  font-size: %4px;"
               "}"
               "QComboBox::drop-down {"
               "  width: %5px;"
               "  border: none;"
               "  background: #e9e9e9;"
               "}"
               // Styling drop-down drops the native indicator, so the arrow has
               // to be drawn back in - otherwise the field ends in a blank grey
               // block. A real image is used because the CSS border-triangle
               // trick does not apply to QSS sub-controls.
               "QComboBox::down-arrow {"
               "  image: url(:/frends/combo-arrow-down.svg);"
               "  width: %6px;"
               "  height: %7px;"
               "}"
               "QComboBox QAbstractItemView {"
               "  background: #ffffff;"
               "  color: #2b2b2b;"
               "  selection-background-color: #d9edc4;"
               "  selection-color: #2b2b2b;"
               "  outline: none;"
               "}")
        .arg(qRound(4 * scale))
        .arg(qRound(8 * scale))
        .arg(qRound(22 * scale))
        .arg(qMax(9, int(qRound(12 * scale))))
        .arg(qRound(22 * scale))
        .arg(qMax(8, int(qRound(10 * scale))))
        .arg(qMax(5, int(qRound(6 * scale))));
}

QString iconButtonStyle()
{
    return QStringLiteral(
        "QToolButton {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 2px;"
        "}"
        "QToolButton:hover { background: rgba(255, 255, 255, 40); border-radius: 4px; }"
        "QToolButton:pressed { background: rgba(0, 0, 0, 30); border-radius: 4px; }");
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

    auto* stripRow = new QHBoxLayout();
    stripRow->setContentsMargins(FrendsScale::px(14, this), 0, FrendsScale::px(14, this), 0);
    stripRow->setSpacing(FrendsScale::px(8, this));

    m_downloadLabel = new QLabel(this);
    m_downloadLabel->hide();
    stripRow->addWidget(m_downloadLabel);

    m_downloadBar = new QProgressBar(this);
    m_downloadBar->setTextVisible(false);
    m_downloadBar->hide();
    stripRow->addWidget(m_downloadBar, 1);

    m_downloadCancel = new QPushButton(this);
    m_downloadCancel->setCursor(Qt::PointingHandCursor);
    m_downloadCancel->setFocusPolicy(Qt::NoFocus);
    connect(m_downloadCancel, &QPushButton::clicked, this, &FrendsBottomBar::downloadCancelled);
    m_downloadCancel->hide();
    stripRow->addWidget(m_downloadCancel);

    outer->addLayout(stripRow);

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
        button->setStyleSheet(iconButtonStyle());
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
        // range on every status change would keep wiping the fill.
        if (!m_downloadBar->isVisible()) {
            m_downloadBar->setRange(0, 0);
            m_downloadBar->show();
        }
    }
    if (m_downloadCancel)
        m_downloadCancel->show();
    // Make room for the strip, otherwise the control row gets squashed.
    setFixedHeight(FrendsScale::px(62, this) + qMax(6, FrendsScale::px(16, this)));
}

void FrendsBottomBar::setDownloadProgress(qint64 current, qint64 total)
{
    if (!m_downloadBar || !m_downloadBar->isVisible())
        return;

    if (total > 0) {
        const qint64 percent = qBound<qint64>(0, current * 100 / total, static_cast<qint64>(100));
        m_downloadBar->setRange(0, 100);
        m_downloadBar->setValue(int(percent));
    } else {
        m_downloadBar->setRange(0, 0);
    }
}

void FrendsBottomBar::hideDownloadProgress()
{
    if (m_downloadLabel)
        m_downloadLabel->hide();
    if (m_downloadBar)
        m_downloadBar->hide();
    if (m_downloadCancel)
        m_downloadCancel->hide();
    setFixedHeight(FrendsScale::px(62, this));
}

void FrendsBottomBar::applyScale()
{
    const qreal s = FrendsScale::factor(this);

    setFixedHeight(FrendsScale::px(62, this));
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

    m_modsButton->setFixedWidth(FrendsScale::px(56, this));
    m_modsButton->setMinimumHeight(FrendsScale::px(42, this));

    const QSize iconSize(FrendsScale::px(22, this), FrendsScale::px(22, this));
    // m_launcherUpdateButton used to be left out here, so its icon kept the
    // style default and came out visibly bigger than its neighbours'.
    for (auto* button : { m_refreshButton, m_launcherUpdateButton, m_folderButton, m_settingsButton })
        button->setIconSize(iconSize);
}

void FrendsBottomBar::styleWidgets(qreal scale)
{
    const int fontSize = qMax(9, int(qRound(12 * scale)));

    if (m_downloadLabel) {
        m_downloadLabel->setStyleSheet(
            QStringLiteral("QLabel { color: #ffffff; font-size: %1px; background: transparent; }").arg(fontSize));
    }
    if (m_downloadCancel) {
        m_downloadCancel->setText(tr("Отмена"));
        m_downloadCancel->setStyleSheet(
            QStringLiteral("QPushButton { background: rgba(0, 0, 0, 70); color: #ffffff; border: 1px solid rgba(0, 0, 0, 50);"
                           " border-radius: 3px; font-size: %1px; padding: 2px 10px; }"
                           "QPushButton:hover { background: rgba(0, 0, 0, 110); }").arg(fontSize));
        m_downloadCancel->setFixedHeight(qMax(6, FrendsScale::px(18, this)));
    }
    if (m_downloadBar) {
        // The strip sits on the green panel, so it gets a dark track and the
        // brand yellow as the fill.
        m_downloadBar->setStyleSheet(QStringLiteral(
                                         "QProgressBar { background: rgba(0, 0, 0, 90); border: 1px solid rgba(0, 0, 0, 60);"
                                         " border-radius: 3px; height: %1px; text-align: center; }"
                                         "QProgressBar::chunk { background: #F9D900; border-radius: 2px; }")
                                         .arg(qMax(6, FrendsScale::px(8, this))));
        m_downloadBar->setFixedHeight(qMax(6, FrendsScale::px(10, this)));
    }

    if (m_addAccountButton) {
        const int side = qMax(18, FrendsScale::px(24, this));
        m_addAccountButton->setFixedSize(side, side);
        m_addAccountButton->setStyleSheet(
            QStringLiteral("QToolButton { background: rgba(255, 255, 255, 60); color: #ffffff;"
                           " border: 1px solid rgba(255, 255, 255, 110); border-radius: 5px;"
                           " font-size: %1px; font-weight: 700; padding: 0px; }"
                           "QToolButton:hover { background: rgba(255, 255, 255, 120); }"
                           "QToolButton:pressed { background: rgba(0, 0, 0, 40); }")
                .arg(qMax(12, FrendsScale::px(17, this))));
        m_addAccountButton->setToolTip(tr("Добавить аккаунт"));
    }
    m_accountCombo->setStyleSheet(comboStyle(scale));
    m_instanceCombo->setStyleSheet(comboStyle(scale));

    const int indicator = qMax(10, int(qRound(14 * scale)));
    m_updateCheck->setStyleSheet(QStringLiteral(
                                     "QCheckBox { color: #ffffff; font-size: %1px; spacing: %2px; background: transparent; }"
                                     "QCheckBox::indicator { width: %3px; height: %3px; border: 1px solid #ffffff;"
                                     " background: #ffffff; border-radius: 1px; }"
                                     "QCheckBox::indicator:checked { background: #F9D900; border: 1px solid #C9B400; }")
                                     .arg(qMax(8, fontSize - 1))
                                     .arg(qRound(6 * scale))
                                     .arg(indicator));

    m_playButton->setStyleSheet(QStringLiteral(
                                    "QPushButton {"
                                    "  background: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #ffd25a, stop:1 #f2b32c);"
                                    "  color: #ffffff;"
                                    "  border: none;"
                                    "  border-radius: 4px;"
                                    "  font-size: %1px;"
                                    "  font-weight: 700;"
                                    "}"
                                    "QPushButton:hover { background: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #ffdd7a, "
                                    "stop:1 #f7bf3f); }"
                                    "QPushButton:pressed { background: #e3a522; }"
                                    "QPushButton:disabled { background: #c9c9c9; color: #f0f0f0; }")
                                    .arg(qMax(10, int(qRound(15 * scale)))));

    m_modsButton->setStyleSheet(QStringLiteral(
                                    "QPushButton {"
                                    "  background: transparent;"
                                    "  border: none;"
                                    "  color: #ffffff;"
                                    "  font-size: %1px;"
                                    "  font-weight: 800;"
                                    "  letter-spacing: 0.5px;"
                                    "}"
                                    "QPushButton:hover { background: rgba(255, 255, 255, 35); border-radius: 4px; }"
                                    "QPushButton:pressed { background: rgba(0, 0, 0, 30); border-radius: 4px; }")
                                    .arg(qMax(8, int(qRound(11 * scale)))));
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
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0.0, QColor(122, 191, 84));
    gradient.setColorAt(0.5, QColor(96, 165, 62));
    gradient.setColorAt(1.0, QColor(74, 138, 45));
    painter.fillRect(rect(), gradient);

    painter.fillRect(QRect(0, 0, width(), 1), QColor(255, 255, 255, 60));
}
