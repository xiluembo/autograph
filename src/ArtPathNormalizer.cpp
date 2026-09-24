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

#include "ArtPathNormalizer.h"

#include <QPainterPath>
#include <QtMath>

#include <limits>

QRectF artPolylineBounds(const ArtPolylineSet &polylines)
{
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    bool hasPoint = false;

    for (const ArtPolyline &line : polylines) {
        for (const QPointF &p : line) {
            if (!qIsFinite(p.x()) || !qIsFinite(p.y()))
                continue;

            minX = qMin(minX, p.x());
            minY = qMin(minY, p.y());
            maxX = qMax(maxX, p.x());
            maxY = qMax(maxY, p.y());
            hasPoint = true;
        }
    }

    if (!hasPoint)
        return {};

    QRectF bounds(minX, minY, maxX - minX, maxY - minY);

    if (bounds.width() < 1e-6)
        bounds.setWidth(1e-6);
    if (bounds.height() < 1e-6)
        bounds.setHeight(1e-6);

    return bounds;
}

void normalizeArtPolylines(ArtPolylineSet &polylines)
{
    if (polylines.isEmpty())
        return;

    const QRectF bounds = artPolylineBounds(polylines);
    if (!bounds.isValid())
        return;

    const double scale = qMax(bounds.width(), bounds.height());
    if (scale <= 0.0)
        return;

    const QPointF origin = bounds.topLeft();

    for (ArtPolyline &line : polylines) {
        for (QPointF &p : line) {
            p = QPointF((p.x() - origin.x()) / scale,
                        (p.y() - origin.y()) / scale);
        }
    }
}

QRectF fitRectForContent(const QRectF &content, const QRectF &target)
{
    if (!content.isValid() || !target.isValid())
        return target;

    const double contentAspect = content.width() / content.height();
    const double targetAspect = target.width() / target.height();

    if (targetAspect > contentAspect) {
        const double fitWidth = target.height() * contentAspect;
        return QRectF(target.left() + (target.width() - fitWidth) / 2.0,
                      target.top(),
                      fitWidth,
                      target.height());
    }

    const double fitHeight = target.width() / contentAspect;
    return QRectF(target.left(),
                  target.top() + (target.height() - fitHeight) / 2.0,
                  target.width(),
                  fitHeight);
}

QPointF mapToFitRect(const QPointF &point, const QRectF &content, const QRectF &fitRect)
{
    return QPointF(
        fitRect.left() + (point.x() - content.left()) / content.width() * fitRect.width(),
        fitRect.top() + (point.y() - content.top()) / content.height() * fitRect.height());
}

QPainterPath polylinesToPainterPath(const ArtPolylineSet &polylines,
                                    const QRectF &content,
                                    const QRectF &fitRect)
{
    QPainterPath compound;
    for (const ArtPolyline &line : polylines) {
        if (line.size() < 2)
            continue;

        QPainterPath subpath;
        subpath.moveTo(mapToFitRect(line.first(), content, fitRect));
        for (int i = 1; i < line.size(); ++i)
            subpath.lineTo(mapToFitRect(line.at(i), content, fitRect));
        subpath.closeSubpath();
        compound.addPath(subpath);
    }
    return compound;
}
