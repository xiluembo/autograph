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

#include "PathPreviewWidget.h"

#include "ArtPathNormalizer.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSize>

namespace {
constexpr QColor kCanvasColor(0x17, 0x17, 0x17);
}

PathPreviewWidget::PathPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(240, 120);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void PathPreviewWidget::setPolylines(const ArtPolylineSet &polylines, Qt::FillRule fillRule)
{
    m_polylines = polylines;
    m_fillRule = fillRule;
    fitToView();
    updateGeometry();
    update();
}

void PathPreviewWidget::clearPolylines()
{
    m_polylines.clear();
    m_fillRule = Qt::WindingFill;
    fitToView();
    updateGeometry();
    update();
}

void PathPreviewWidget::fitToView()
{
    update();
}

void PathPreviewWidget::setCompactEmpty(bool compact)
{
    if (m_compactEmpty == compact)
        return;
    m_compactEmpty = compact;
    updateGeometry();
    update();
}

void PathPreviewWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        update();
    QWidget::changeEvent(event);
}

void PathPreviewWidget::setCornerRadius(qreal radius)
{
    if (qFuzzyCompare(m_cornerRadius, radius))
        return;
    m_cornerRadius = qMax(0.0, radius);
    update();
}

QSize PathPreviewWidget::sizeHint() const
{
    if (m_compactEmpty && m_polylines.isEmpty())
        return QSize(240, 96);
    if (m_compactEmpty)
        return QSize(240, 140);
    return QSize(240, 180);
}

QSize PathPreviewWidget::minimumSizeHint() const
{
    if (m_compactEmpty)
        return QSize(120, m_polylines.isEmpty() ? 72 : 80);
    return QSize(240, 120);
}

QRectF PathPreviewWidget::fittedContentRect() const
{
    const double margin = 20.0;
    const QRectF target(margin, margin,
                        width() - 2 * margin,
                        height() - 2 * margin);
    const QRectF content = artPolylineBounds(m_polylines);
    return fitRectForContent(content, target);
}

void PathPreviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath clip;
    if (m_cornerRadius > 0.0)
        clip.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                            m_cornerRadius, m_cornerRadius);
    else
        clip.addRect(rect());
    painter.setClipPath(clip);
    painter.fillPath(clip, kCanvasColor);

    if (m_polylines.isEmpty()) {
        painter.setPen(QColor(0x9a, 0x9a, 0x9a));
        painter.drawText(rect().adjusted(16, 12, -16, -12), Qt::AlignCenter | Qt::TextWordWrap,
                         tr("Open an SVG file to preview the line art"));
        return;
    }

    const QRectF content = artPolylineBounds(m_polylines);
    const QRectF fitRect = fittedContentRect();
    QPainterPath shape = polylinesToPainterPath(m_polylines, content, fitRect);
    shape.setFillRule(m_fillRule);

    painter.setPen(QPen(QColor(80, 80, 80), 1.0));
    painter.setBrush(QColor(220, 220, 220));
    painter.drawPath(shape);
}
