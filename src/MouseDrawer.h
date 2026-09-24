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

#include <QRect>
#include <QTimer>

class MouseDrawer : public DrawingBackend
{
    Q_OBJECT

public:
    explicit MouseDrawer(QObject *parent = nullptr);

    void setPolylines(const ArtPolylineSet &polylines) override;
    void setSpeed(int pixelsPerSecond) override;
    void setPauseBetweenPaths(int pauseMs) override;

    bool isDrawing() const override { return m_drawing; }

public slots:
    void start() override;
    void stop() override;

private slots:
    void tick();

private:
    void moveMouseTo(const QPoint &screenPos);
    void pressLeftButton();
    void releaseLeftButton();
    int delayUntilNextPoint(const ArtPolyline &path) const;

    ArtPolylineSet m_polylines;
    QTimer m_timer;
    bool m_drawing = false;
    bool m_buttonDown = false;
    int m_pathIndex = 0;
    int m_pointIndex = 0;
    int m_pixelsPerSecond = 300;
    int m_pauseBetweenPathsMs = 100;
    bool m_pausingBetweenPaths = false;
};
