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

#include <QPainterPath>
#include <QWidget>

#include <Qt>

class QEvent;
class QResizeEvent;
class QTimer;

class PathPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PathPreviewWidget(QWidget *parent = nullptr);

    void setPolylines(const ArtPolylineSet &polylines, Qt::FillRule fillRule = Qt::WindingFill);
    void clearPolylines();
    void fitToView();
    bool hasContent() const { return !m_polylines.isEmpty(); }

    void setLoading(bool loading);
    bool isLoading() const { return m_loading; }

    void setCompactEmpty(bool compact);
    bool compactEmpty() const { return m_compactEmpty; }

    void setCornerRadius(qreal radius);
    qreal cornerRadius() const { return m_cornerRadius; }

protected:
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QRectF fittedContentRect() const;
    void rebuildShapeIfNeeded();

    ArtPolylineSet m_polylines;
    Qt::FillRule m_fillRule = Qt::WindingFill;
    bool m_compactEmpty = false;
    qreal m_cornerRadius = 0.0;

    // Cached rendering state: rebuilding the compound path on every repaint is
    // expensive for dense line art, so it is only refreshed when the polylines
    // or the widget size change.
    QRectF m_contentBounds;
    QPainterPath m_shape;
    bool m_shapeDirty = true;

    // Busy indicator shown while an SVG is being parsed in the background.
    bool m_loading = false;
    int m_spinnerAngle = 0;
    QTimer *m_spinnerTimer = nullptr;
};
