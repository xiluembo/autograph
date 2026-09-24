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

#include <QWidget>

#include <Qt>

class QEvent;

class PathPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PathPreviewWidget(QWidget *parent = nullptr);

    void setPolylines(const ArtPolylineSet &polylines, Qt::FillRule fillRule = Qt::WindingFill);
    void clearPolylines();
    void fitToView();
    bool hasContent() const { return !m_polylines.isEmpty(); }

    void setCompactEmpty(bool compact);
    bool compactEmpty() const { return m_compactEmpty; }

    void setCornerRadius(qreal radius);
    qreal cornerRadius() const { return m_cornerRadius; }

protected:
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QRectF fittedContentRect() const;

    ArtPolylineSet m_polylines;
    Qt::FillRule m_fillRule = Qt::WindingFill;
    bool m_compactEmpty = false;
    qreal m_cornerRadius = 0.0;
};
