// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 Tayou <git@tayou.org>
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

#include "VisualGroup.h"

#include <QApplication>
#include <QDebug>
#include <QModelIndex>
#include <QPainter>
#include <QtMath>
#include <utility>

#include "InstanceView.h"
#include "ui/frends/FrendsPalette.h"

VisualGroup::VisualGroup(QString text, InstanceView* view) : view(view), text(std::move(text)), collapsed(false) {}

VisualGroup::VisualGroup(const VisualGroup* other) : view(other->view), text(other->text), collapsed(other->collapsed) {}

void VisualGroup::update()
{
    auto temp_items = items();
    auto itemsPerRow = view->itemsPerRow();

    int numRows = qMax(1, qCeil((qreal)temp_items.size() / (qreal)itemsPerRow));
    rows = QList<VisualRow>(numRows);

    int maxRowHeight = 0;
    int positionInRow = 0;
    int currentRow = 0;
    int offsetFromTop = 0;
    for (auto item : temp_items) {
        if (positionInRow == itemsPerRow) {
            rows[currentRow].height = maxRowHeight;
            rows[currentRow].top = offsetFromTop;
            currentRow++;
            if (currentRow >= rows.size()) {
                currentRow = rows.size() - 1;
            }
            offsetFromTop += maxRowHeight + 5;
            positionInRow = 0;
            maxRowHeight = 0;
        }
        QStyleOptionViewItem viewItemOption;
        view->initViewItemOption(&viewItemOption);

        auto itemHeight = view->itemDelegate()->sizeHint(viewItemOption, item).height();
        if (itemHeight > maxRowHeight) {
            maxRowHeight = itemHeight;
        }
        rows[currentRow].items.append(item);
        positionInRow++;
    }
    rows[currentRow].height = maxRowHeight;
    rows[currentRow].top = offsetFromTop;
}

QPair<int, int> VisualGroup::positionOf(const QModelIndex& index) const
{
    int y = 0;
    for (auto& row : rows) {
        for (auto x = 0; x < row.items.size(); x++) {
            if (row.items[x] == index) {
                return qMakePair(x, y);
            }
        }
        y++;
    }
    qWarning() << "Item" << index.row() << index.data(Qt::DisplayRole).toString() << "not found in visual group" << text;
    return qMakePair(0, 0);
}

int VisualGroup::rowTopOf(const QModelIndex& index) const
{
    auto position = positionOf(index);
    return rows[position.second].top;
}

int VisualGroup::rowHeightOf(const QModelIndex& index) const
{
    auto position = positionOf(index);
    return rows[position.second].height;
}

VisualGroup::HitResults VisualGroup::hitScan(const QPoint& pos) const
{
    VisualGroup::HitResults results = VisualGroup::NoHit;
    int y_start = verticalPosition();
    int body_start = y_start + headerHeight();
    int body_end = body_start + contentHeight();
    int y = pos.y();
    // int x = pos.x();
    if (y < y_start) {
        results = VisualGroup::NoHit;
    } else if (y < body_start) {
        results = VisualGroup::HeaderHit;
        int collapseSize = headerHeight() - 4;

        // the icon
        QRect iconRect = QRect(view->m_leftMargin + 2, 2 + y_start, view->width() - 4, collapseSize);
        if (iconRect.contains(pos)) {
            results |= VisualGroup::CheckboxHit;
        }
    } else if (y < body_end) {
        results |= VisualGroup::BodyHit;
    }
    return results;
}

void VisualGroup::drawHeader(QPainter* painter, const QStyleOptionViewItem& option) const
{
    const QRect optRect = option.rect;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QFont font(QApplication::font());
    font.setPixelSize(13);
    font.setBold(true);
    painter->setFont(font);
    const QFontMetrics fontMetrics(font);

    const QString& textToDraw = text.isEmpty() ? QObject::tr("Ungrouped") : text;
    // `option.rect` covers the whole group, header and tiles, so the header is
    // laid out inside its own strip at the top rather than centred in the lot.
    const int centerY = optRect.top() + headerHeight() / 2;
    const int arrowLeft = optRect.left() + 10;
    constexpr int arrowSize = 4;

    // Chevron: points down when the group is open, right when it is collapsed.
    QColor chevron = FrendsTheme::colors().text;
    chevron.setAlpha(210);
    painter->setPen(QPen(chevron, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPolygon arrow;
    if (collapsed) {
        arrow << QPoint(arrowLeft - arrowSize, centerY - arrowSize) << QPoint(arrowLeft + arrowSize / 2, centerY)
              << QPoint(arrowLeft - arrowSize, centerY + arrowSize);
    } else {
        arrow << QPoint(arrowLeft - arrowSize, centerY - arrowSize / 2) << QPoint(arrowLeft, centerY + arrowSize / 2)
              << QPoint(arrowLeft + arrowSize, centerY - arrowSize / 2);
    }
    painter->drawPolyline(arrow);

    painter->setPen(FrendsTheme::colors().text);
    QRect textRect(optRect);
    textRect.setLeft(arrowLeft + 14);
    textRect.setHeight(fontMetrics.height());
    textRect.moveTop(centerY - fontMetrics.height() / 2);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, textToDraw);

    painter->restore();
}

int VisualGroup::totalHeight() const
{
    return headerHeight() + contentHeight();
}

int VisualGroup::headerHeight()
{
    QFont font(QApplication::font());
    font.setPixelSize(13);
    font.setBold(true);
    QFontMetrics fontMetrics(font);

    // Room for the chevron and a little breathing space above the first row.
    return fontMetrics.height() + 14;
}

int VisualGroup::contentHeight() const
{
    if (collapsed) {
        return 0;
    }
    auto last = rows[numRows() - 1];
    return last.top + last.height;
}

int VisualGroup::numRows() const
{
    return (int)rows.size();
}

int VisualGroup::verticalPosition() const
{
    return m_verticalPosition;
}

QList<QModelIndex> VisualGroup::items() const
{
    QList<QModelIndex> indices;
    for (int i = 0; i < view->model()->rowCount(); ++i) {
        const QModelIndex index = view->model()->index(i, 0);
        if (index.data(InstanceViewRoles::GroupRole).toString() == text) {
            indices.append(index);
        }
    }
    return indices;
}
