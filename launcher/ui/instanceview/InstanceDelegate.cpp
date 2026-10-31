// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include "InstanceDelegate.h"
#include <QApplication>
#include <QDebug>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QTextLayout>
#include <QTextOption>
#include <QtMath>

#include <QIcon>
#include <QTextEdit>
#include "BaseInstance.h"
#include "InstanceList.h"
#include "minecraft/MinecraftInstance.h"
#include "ui/frends/FrendsScale.h"
#include "InstanceView.h"

ListViewDelegate::ListViewDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void drawSelectionRect(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect)
{
    if ((option.state & QStyle::State_Selected))
        painter->fillRect(rect, option.palette.brush(QPalette::Highlight));
    else {
        QColor backgroundColor = option.palette.color(QPalette::Window);
        backgroundColor.setAlpha(160);
        painter->fillRect(rect, QBrush(backgroundColor));
    }
}

void drawFocusRect(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect)
{
    if (!(option.state & QStyle::State_HasFocus))
        return;
    QStyleOptionFocusRect opt;
    opt.direction = option.direction;
    opt.fontMetrics = option.fontMetrics;
    opt.palette = option.palette;
    opt.rect = rect;
    // opt.state           = option.state | QStyle::State_KeyboardFocusChange |
    // QStyle::State_Item;
    auto col = option.state & QStyle::State_Selected ? QPalette::Highlight : QPalette::Base;
    opt.backgroundColor = option.palette.color(col);
    // Apparently some widget styles expect this hint to not be set
    painter->setRenderHint(QPainter::Antialiasing, false);

    QStyle* style = option.widget ? option.widget->style() : QApplication::style();

    style->drawPrimitive(QStyle::PE_FrameFocusRect, &opt, painter, option.widget);

    painter->setRenderHint(QPainter::Antialiasing);
}

// TODO this can be made a lot prettier
void drawProgressOverlay(QPainter* painter, const QStyleOptionViewItem& option, const int value, const int maximum)
{
    if (maximum == 0 || value == maximum) {
        return;
    }

    painter->save();

    qreal percent = (qreal)value / (qreal)maximum;
    QColor color = option.palette.color(QPalette::Dark);
    color.setAlphaF(0.70f);
    painter->setBrush(color);
    painter->setPen(QPen(QBrush(), 0));
    painter->drawPie(option.rect, 90 * 16, -percent * 360 * 16);

    painter->restore();
}

void drawBadges(QPainter* painter, const QStyleOptionViewItem& option, BaseInstance* instance, QIcon::Mode mode, QIcon::State state)
{
    QList<QString> pixmaps;
    if (instance->isRunning()) {
        pixmaps.append("status-running");
    } else if (instance->hasCrashed() || instance->hasVersionBroken()) {
        pixmaps.append("status-bad");
    }
    if (instance->hasUpdateAvailable()) {
        pixmaps.append("checkupdate");
    }

    static const int itemSide = 24;
    static const int spacing = 1;
    const int itemsPerRow = qMax(1, qFloor(double(option.rect.width() + spacing) / double(itemSide + spacing)));
    const int rows = qCeil((double)pixmaps.size() / (double)itemsPerRow);
    QListIterator<QString> it(pixmaps);
    painter->translate(option.rect.topLeft());
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < itemsPerRow; ++x) {
            if (!it.hasNext()) {
                return;
            }
            // FIXME: inject this.
            auto icon = QIcon::fromTheme(it.next());
            // opt.icon.paint(painter, iconbox, Qt::AlignCenter, mode, state);
            const QPixmap pixmap;
            // itemSide
            QRect badgeRect(option.rect.width() - x * itemSide + qMax(x - 1, 0) * spacing - itemSide,
                            y * itemSide + qMax(y - 1, 0) * spacing, itemSide, itemSide);
            icon.paint(painter, badgeRect, Qt::AlignCenter, mode, state);
        }
    }
    painter->translate(-option.rect.topLeft());
}

void ListViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal s = FrendsScale::factor(opt.widget);
    const bool selected = opt.state & QStyle::State_Selected;

    QIcon::Mode mode = QIcon::Normal;
    if (!(opt.state & QStyle::State_Enabled))
        mode = QIcon::Disabled;
    else if (selected)
        mode = QIcon::Selected;
    QIcon::State iconState = opt.state & QStyle::State_Open ? QIcon::On : QIcon::Off;

    // One tile: rounded plate, big artwork, the name and a version chip.
    const QRectF tile = QRectF(opt.rect).adjusted(2, 2, -2, -2);
    const qreal radius = 10 * s;

    QPainterPath plate;
    plate.addRoundedRect(tile, radius, radius);
    painter->setClipPath(plate);

    if (selected) {
        QLinearGradient fill(tile.topLeft(), tile.bottomLeft());
        fill.setColorAt(0.0, QColor(104, 152, 82, 240));
        fill.setColorAt(1.0, QColor(62, 104, 50, 240));
        painter->fillPath(plate, fill);
    } else {
        painter->fillPath(plate, QColor(26, 29, 36, 200));
    }

    const int iconSide = int(qRound(56 * s));
    const QRect iconRect(int(tile.center().x() - iconSide / 2.0), int(tile.top() + 9 * s), iconSide, iconSide);
    opt.icon.paint(painter, iconRect, Qt::AlignCenter, mode, iconState);

    // Name
    QFont nameFont = opt.font;
    nameFont.setPixelSize(qMax(9, int(qRound(11 * s))));
    nameFont.setBold(false);
    painter->setFont(nameFont);
    painter->setPen(QColor(0xF2, 0xF3, 0xF5));

    const int nameTop = iconRect.bottom() + int(qRound(7 * s));
    const QRect nameRect(int(tile.left()), nameTop, int(tile.width()), int(qRound(15 * s)));
    const QString name = opt.fontMetrics.elidedText(opt.text, Qt::ElideRight, nameRect.width() - int(8 * s));
    painter->drawText(nameRect, Qt::AlignHCenter | Qt::AlignVCenter, name);

    // Version chip. The selected tile wears it in solid green, the rest in a
    // muted grey line, which keeps the row calm until you pick something.
    QString version;
    if (auto* instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>())) {
        if (auto* minecraft = dynamic_cast<MinecraftInstance*>(instance))
            version = minecraft->minecraftVersion();
    }

    if (!version.isEmpty()) {
        QFont chipFont = opt.font;
        chipFont.setPixelSize(qMax(8, int(qRound(9.5 * s))));
        chipFont.setBold(selected);
        painter->setFont(chipFont);

        const QRectF chip(tile.left() + 6 * s, nameRect.bottom() + 3 * s, tile.width() - 12 * s, 18 * s);
        if (selected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0x5F, 0xBF, 0x3F));
            painter->drawRoundedRect(chip, 5 * s, 5 * s);
            painter->setPen(QColor(0xFF, 0xFF, 0xFF));
        } else {
            painter->setPen(QColor(255, 255, 255, 150));
        }
        painter->drawText(chip, Qt::AlignCenter, version);
    }

    painter->setClipping(false);

    // Outline last, so it sits on top of the artwork instead of under it.
    painter->setBrush(Qt::NoBrush);
    if (selected)
        painter->setPen(QPen(QColor(0x86, 0xD9, 0x62), qMax(1.5, 2.0 * s)));
    else
        painter->setPen(QPen(QColor(255, 255, 255, 18), 1));
    painter->drawRoundedRect(tile, radius, radius);

    auto instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
    if (instance) {
        painter->save();
        painter->setClipPath(plate);
        drawBadges(painter, opt, instance, mode, iconState);
        painter->restore();
    }

    // Downloading or launching: a line creeps along the bottom edge of the tile.
    const int progressMax = index.data(InstanceViewRoles::ProgressMaximumRole).toInt();
    const int progressValue = index.data(InstanceViewRoles::ProgressValueRole).toInt();
    if (progressMax > 0 && progressValue < progressMax) {
        const qreal ratio = qreal(progressValue) / qreal(progressMax);
        const QRectF track(tile.left(), tile.bottom() - 3 * s, tile.width(), 3 * s);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 40));
        painter->drawRect(track);
        painter->setBrush(QColor(0xF9, 0xD9, 0x00));
        painter->drawRect(QRectF(track.left(), track.top(), track.width() * ratio, track.height()));
    }

    painter->restore();
}

QSize ListViewDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    // The view lays tiles out on a fixed 100px pitch, so only the height varies.
    return QSize(100, 116);
}

class NoReturnTextEdit : public QTextEdit {
    Q_OBJECT
   public:
    explicit NoReturnTextEdit(QWidget* parent) : QTextEdit(parent)
    {
        setTextInteractionFlags(Qt::TextEditorInteraction);
        setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    }
    bool event(QEvent* event) override
    {
        auto eventType = event->type();
        if (eventType == QEvent::KeyPress || eventType == QEvent::KeyRelease) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            auto key = keyEvent->key();
            if ((key == Qt::Key_Return || key == Qt::Key_Enter) && eventType == QEvent::KeyPress) {
                emit editingDone();
                return true;
            }
            if (key == Qt::Key_Tab) {
                return true;
            }
        }
        return QTextEdit::event(event);
    }
   signals:
    void editingDone();
};

void ListViewDelegate::updateEditorGeometry(QWidget* editor,
                                            const QStyleOptionViewItem& option,
                                            [[maybe_unused]] const QModelIndex& index) const
{
    const int iconSize = 48;
    QRect textRect = option.rect;
    // QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    textRect.adjust(0, iconSize + 5, 0, 0);
    editor->setGeometry(textRect);
}

void ListViewDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto text = index.data(Qt::EditRole).toString();
    QTextEdit* realEditor = qobject_cast<NoReturnTextEdit*>(editor);
    realEditor->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    realEditor->append(text);
    realEditor->selectAll();
    realEditor->document()->clearUndoRedoStacks();
}

void ListViewDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    QTextEdit* realEditor = qobject_cast<NoReturnTextEdit*>(editor);
    QString text = realEditor->toPlainText();
    text.replace(QChar('\n'), QChar(' '));
    text = text.trimmed();
    // Prevent instance names longer than 128 chars
    text.truncate(128);
    if (text.size() != 0) {
        emit textChanged(model->data(index).toString(), text);
        model->setData(index, text);
    }
}

QWidget* ListViewDelegate::createEditor(QWidget* parent,
                                        [[maybe_unused]] const QStyleOptionViewItem& option,
                                        [[maybe_unused]] const QModelIndex& index) const
{
    auto editor = new NoReturnTextEdit(parent);
    connect(editor, &NoReturnTextEdit::editingDone, this, &ListViewDelegate::editingDone);
    return editor;
}

void ListViewDelegate::editingDone()
{
    NoReturnTextEdit* editor = qobject_cast<NoReturnTextEdit*>(sender());
    emit commitData(editor);
    emit closeEditor(editor);
}

#include "InstanceDelegate.moc"
