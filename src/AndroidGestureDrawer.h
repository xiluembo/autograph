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

#include "DrawingBackend.h"

class AndroidGestureDrawer : public DrawingBackend
{
    Q_OBJECT

public:
    explicit AndroidGestureDrawer(QObject *parent = nullptr);
    ~AndroidGestureDrawer() override;

    void setPolylines(const ArtPolylineSet &polylines) override;
    void setSpeed(int pixelsPerSecond) override;
    void setPauseBetweenPaths(int pauseMs) override;
    bool isDrawing() const override { return m_drawing; }

    bool selectsAreaExternally() const override { return true; }
    bool isBackendReady() const override;
    void requestEnableBackend() override;
    void clearSelectedArea() override;
    bool requestAreaSelection() override;
    bool requestAreaSelection(const ArtPolylineSet &polylines,
                              int pixelsPerSecond, int pauseBetweenPathsMs) override;
    bool requestReadyToDraw(const ArtPolylineSet &polylines,
                            int pixelsPerSecond, int pauseBetweenPathsMs,
                            const QRect &area) override;

public slots:
    void start() override;
    void stop() override;

private:
    void syncConfiguration(bool normalizedCoordinates = false);
    void handleAreaSelected(const QRect &area);
    void handleSelectionCancelled();
    void handleReturnedToApp();
    void handleProgress(int currentPath, int totalPaths, int currentPoint, int totalPoints);
    void handleFinished();
    void handleCancelled();
    void handleError(const QString &message);

    ArtPolylineSet m_polylines;
    int m_pixelsPerSecond = 300;
    int m_pauseBetweenPathsMs = 20;
    bool m_drawing = false;

    friend void androidAreaSelected(int, int, int, int);
    friend void androidSelectionCancelled();
    friend void androidReturnedToApp();
    friend void androidDrawingProgress(int, int, int, int);
    friend void androidDrawingFinished();
    friend void androidDrawingCancelled();
    friend void androidDrawingError(const QString &);
};
