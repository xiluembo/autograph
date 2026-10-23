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
#include <QPen>
#include <QResizeEvent>
#include <QSize>
#include <QTimer>

namespace {
constexpr QColor kCanvasColor(0x17, 0x17, 0x17);
constexpr int kSpinnerIntervalMs = 33;
constexpr int kSpinnerStepDegrees = 10;
}

PathPreviewWidget::PathPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(240, 120);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(kSpinnerIntervalMs);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this]() {
        m_spinnerAngle = (m_spinnerAngle + kSpinnerStepDegrees) % 360;
        update();
    });
}

void PathPreviewWidget::setPolylines(const ArtPolylineSet &polylines, Qt::FillRule fillRule)
{
    m_polylines = polylines;
    m_fillRule = fillRule;
    m_contentBounds = artPolylineBounds(m_polylines);
    m_shapeDirty = true;
    setLoading(false);
    fitToView();
    updateGeometry();
    update();
}

void PathPreviewWidget::clearPolylines()
{
    m_polylines.clear();
    m_fillRule = Qt::WindingFill;
    m_contentBounds = QRectF();
    m_shape = QPainterPath();
    m_shapeDirty = true;
    fitToView();
    updateGeometry();
    update();
}

void PathPreviewWidget::fitToView()
{
    update();
}

void PathPreviewWidget::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    m_spinnerAngle = 0;
    if (m_loading)
        m_spinnerTimer->start();
    else
        m_spinnerTimer->stop();
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
    return fitRectForContent(m_contentBounds, target);
}

void PathPreviewWidget::resizeEvent(QResizeEvent *event)
{
    m_shapeDirty = true;
    QWidget::resizeEvent(event);
}

void PathPreviewWidget::rebuildShapeIfNeeded()
{
    if (!m_shapeDirty)
        return;
    m_shape = polylinesToPainterPath(m_polylines, m_contentBounds, fittedContentRect());
    m_shape.setFillRule(m_fillRule);
    m_shapeDirty = false;
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

    if (m_loading) {
        const double radius = qMin(width(), height()) / 6.0;
        const QRectF spinnerRect(width() / 2.0 - radius,
                                 height() / 2.0 - radius - 10.0,
                                 radius * 2.0, radius * 2.0);
        QPen pen(QColor(0x9a, 0x9a, 0x9a), 3.0);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(spinnerRect, -m_spinnerAngle * 16, 120 * 16);

        painter.setPen(QColor(0x9a, 0x9a, 0x9a));
        painter.drawText(QRectF(16.0, spinnerRect.bottom() + 8.0,
                                width() - 32.0, height() - spinnerRect.bottom() - 8.0),
                         Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                         tr("Loading SVG..."));
        return;
    }

    if (m_polylines.isEmpty()) {
        painter.setPen(QColor(0x9a, 0x9a, 0x9a));
        painter.drawText(rect().adjusted(16, 12, -16, -12), Qt::AlignCenter | Qt::TextWordWrap,
                         tr("Open an SVG file to preview the line art"));
        return;
    }

    rebuildShapeIfNeeded();

    painter.setPen(QPen(QColor(80, 80, 80), 1.0));
    painter.setBrush(QColor(220, 220, 220));
    painter.drawPath(m_shape);
}
