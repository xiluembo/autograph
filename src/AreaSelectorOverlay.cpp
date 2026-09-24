/*
 * Autograph
 * Copyright (C) 2026 Andrius da Costa Ribas <andriusmao@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "AreaSelectorOverlay.h"

#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

AreaSelectorOverlay::AreaSelectorOverlay(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    QRect virtualGeometry;
    for (QScreen *screen : QGuiApplication::screens())
        virtualGeometry = virtualGeometry.united(screen->geometry());

    setGeometry(virtualGeometry);
}

void AreaSelectorOverlay::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        update();
    QWidget::changeEvent(event);
}

void AreaSelectorOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    const QColor overlayColor(0, 0, 0, 120);

    if (m_hasSelection || m_dragging) {
        const QRect selection = QRect(mapFromGlobal(m_start), mapFromGlobal(m_end)).normalized();

        painter.fillRect(QRect(rect().left(), rect().top(), rect().width(), selection.top()), overlayColor);
        painter.fillRect(QRect(rect().left(), selection.bottom() + 1, rect().width(),
                               rect().bottom() - selection.bottom()), overlayColor);
        painter.fillRect(QRect(rect().left(), selection.top(), selection.left(), selection.height()),
                         overlayColor);
        painter.fillRect(QRect(selection.right() + 1, selection.top(),
                               rect().right() - selection.right(), selection.height()),
                         overlayColor);

        painter.setPen(QPen(QColor(0, 180, 255), 2, Qt::DashLine));
        painter.drawRect(selection);

        painter.setPen(Qt::white);
        painter.drawText(selection.adjusted(4, 4, -4, -4),
                         QStringLiteral("%1 × %2").arg(selection.width()).arg(selection.height()));
    } else {
        painter.fillRect(rect(), overlayColor);
    }

    painter.setPen(Qt::white);
    painter.drawText(QRect(20, 20, width() - 40, 40), Qt::AlignLeft,
                     tr("Drag to select the area. Enter confirms, Esc cancels."));
}

void AreaSelectorOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_hasSelection = false;
        m_start = QCursor::pos();
        m_end = m_start;
        update();
    }
}

void AreaSelectorOverlay::mouseMoveEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (m_dragging) {
        m_end = QCursor::pos();
        m_hasSelection = true;
        update();
    }
}

void AreaSelectorOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        m_end = QCursor::pos();
        m_hasSelection = true;
        update();
    }
}

void AreaSelectorOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit selectionCancelled();
        close();
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_hasSelection) {
            const QRect area = QRect(m_start, m_end).normalized();
            if (area.width() > 5 && area.height() > 5) {
                emit areaSelected(area);
                close();
                return;
            }
        }
    }

    QWidget::keyPressEvent(event);
}
