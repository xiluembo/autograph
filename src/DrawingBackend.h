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

#pragma once

#include "SvgPathExtractor.h"

#include <QObject>
#include <QRect>

class DrawingBackend : public QObject
{
    Q_OBJECT

public:
    explicit DrawingBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~DrawingBackend() override = default;

    virtual void setPolylines(const ArtPolylineSet &polylines) = 0;
    virtual void setSpeed(int pixelsPerSecond) = 0;
    virtual void setPauseBetweenPaths(int pauseMs) = 0;
    virtual bool isDrawing() const = 0;

    virtual bool selectsAreaExternally() const { return false; }
    virtual bool isBackendReady() const { return true; }
    virtual void requestEnableBackend() {}
    virtual void clearSelectedArea() {}
    virtual bool requestAreaSelection() { return false; }
    virtual bool requestAreaSelection(const ArtPolylineSet &polylines,
                                      int pixelsPerSecond, int pauseBetweenPathsMs)
    {
        Q_UNUSED(polylines);
        Q_UNUSED(pixelsPerSecond);
        Q_UNUSED(pauseBetweenPathsMs);
        return requestAreaSelection();
    }
    /// Reopen the floating overlay in the "ready to draw" state without reselecting the area.
    virtual bool requestReadyToDraw(const ArtPolylineSet &polylines,
                                    int pixelsPerSecond, int pauseBetweenPathsMs,
                                    const QRect &area)
    {
        Q_UNUSED(polylines);
        Q_UNUSED(pixelsPerSecond);
        Q_UNUSED(pauseBetweenPathsMs);
        Q_UNUSED(area);
        return false;
    }

public slots:
    virtual void start() = 0;
    virtual void stop() = 0;

signals:
    void progressChanged(int currentPath, int totalPaths, int currentPoint, int totalPoints);
    void finished();
    void cancelled();
    void error(const QString &message);
    void areaSelected(const QRect &area);
    void selectionCancelled();
    /// User asked to return to the app while keeping the selected area.
    void returnedToApp();
};
