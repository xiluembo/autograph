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

#include "MouseDrawer.h"

#include <QCursor>
#include <QLineF>
#include <QtMath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

MouseDrawer::MouseDrawer(QObject *parent)
    : DrawingBackend(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &MouseDrawer::tick);
}

void MouseDrawer::setPolylines(const ArtPolylineSet &polylines)
{
    m_polylines = polylines;
}

void MouseDrawer::setSpeed(int pixelsPerSecond)
{
    m_pixelsPerSecond = qMax(1, pixelsPerSecond);
}

void MouseDrawer::setPauseBetweenPaths(int pauseMs)
{
    m_pauseBetweenPathsMs = qMax(0, pauseMs);
}

void MouseDrawer::start()
{
    if (m_drawing || m_polylines.isEmpty())
        return;

#ifndef Q_OS_WIN
    emit error(tr("Mouse drawing is only supported on Windows."));
    return;
#endif

    m_drawing = true;
    m_buttonDown = false;
    m_pathIndex = 0;
    m_pointIndex = 0;
    m_pausingBetweenPaths = false;
    m_timer.start(100);
}

void MouseDrawer::stop()
{
    if (!m_drawing)
        return;

    m_drawing = false;
    m_timer.stop();

    if (m_buttonDown) {
        releaseLeftButton();
        m_buttonDown = false;
    }

    emit cancelled();
}

void MouseDrawer::tick()
{
    if (!m_drawing) {
        m_timer.stop();
        return;
    }

    if (m_pausingBetweenPaths) {
        m_pausingBetweenPaths = false;
        ++m_pathIndex;
        m_pointIndex = 0;
    }

    if (m_pathIndex >= m_polylines.size()) {
        if (m_buttonDown) {
            releaseLeftButton();
            m_buttonDown = false;
        }
        m_drawing = false;
        m_timer.stop();
        emit finished();
        return;
    }

    const ArtPolyline &currentPath = m_polylines.at(m_pathIndex);

    if (m_pointIndex == 0) {
        moveMouseTo(QPoint(qRound(currentPath.first().x()),
                           qRound(currentPath.first().y())));
        pressLeftButton();
        m_buttonDown = true;
        ++m_pointIndex;
        emit progressChanged(m_pathIndex + 1, m_polylines.size(),
                             m_pointIndex, currentPath.size());
        m_timer.start(delayUntilNextPoint(currentPath));
        return;
    }

    if (m_pointIndex < currentPath.size()) {
        const QPointF &pt = currentPath.at(m_pointIndex);
        moveMouseTo(QPoint(qRound(pt.x()), qRound(pt.y())));
        ++m_pointIndex;
        emit progressChanged(m_pathIndex + 1, m_polylines.size(),
                             m_pointIndex, currentPath.size());
        m_timer.start(delayUntilNextPoint(currentPath));
        return;
    }

    if (m_buttonDown) {
        releaseLeftButton();
        m_buttonDown = false;
    }

    if (m_pathIndex + 1 < m_polylines.size()) {
        m_pausingBetweenPaths = true;
        m_timer.start(m_pauseBetweenPathsMs);
    } else {
        m_pathIndex = m_polylines.size();
        m_timer.start(0);
    }
}

int MouseDrawer::delayUntilNextPoint(const ArtPolyline &path) const
{
    if (m_pointIndex <= 0 || m_pointIndex >= path.size())
        return 1;

    const double distance = QLineF(path.at(m_pointIndex - 1), path.at(m_pointIndex)).length();
    return qMax(1, qRound(distance * 1000.0 / m_pixelsPerSecond));
}

void MouseDrawer::moveMouseTo(const QPoint &screenPos)
{
#ifdef Q_OS_WIN
    QCursor::setPos(screenPos);
#else
    Q_UNUSED(screenPos);
#endif
}

void MouseDrawer::pressLeftButton()
{
#ifdef Q_OS_WIN
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &input, sizeof(INPUT));
#endif
}

void MouseDrawer::releaseLeftButton()
{
#ifdef Q_OS_WIN
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
#endif
}
