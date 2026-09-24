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

#include "PathScaler.h"

#include "ArtPathNormalizer.h"

#include <QLineF>
#include <QtMath>

#include <limits>

namespace {

double distance(const QPointF &a, const QPointF &b)
{
    return QLineF(a, b).length();
}

} // namespace

ArtPolylineSet PathScaler::scaleToArea(const ArtPolylineSet &normalized,
                                    const QRect &targetArea,
                                    double marginFraction)
{
    ArtPolylineSet result;
    if (normalized.isEmpty() || !targetArea.isValid())
        return result;

    const double marginX = targetArea.width() * marginFraction;
    const double marginY = targetArea.height() * marginFraction;
    const QRectF inner(targetArea.x() + marginX,
                       targetArea.y() + marginY,
                       targetArea.width() - 2 * marginX,
                       targetArea.height() - 2 * marginY);

    const QRectF content = artPolylineBounds(normalized);
    const QRectF fitRect = fitRectForContent(content, inner);

    for (const ArtPolyline &line : normalized) {
        ArtPolyline scaled;
        scaled.reserve(line.size());
        for (const QPointF &p : line) {
            const QPointF mapped = mapToFitRect(p, content, fitRect);
            const QPointF screenPoint(qRound(mapped.x()), qRound(mapped.y()));
            if (scaled.isEmpty() || scaled.last() != screenPoint)
                scaled.append(screenPoint);
        }
        if (!scaled.isEmpty())
            result.append(scaled);
    }

    return reorderByProximity(result);
}

ArtPolylineSet PathScaler::reorderByProximity(const ArtPolylineSet &polylines)
{
    if (polylines.size() <= 1)
        return polylines;

    ArtPolylineSet remaining = polylines;
    ArtPolylineSet ordered;
    ordered.append(remaining.takeFirst());

    while (!remaining.isEmpty()) {
        const QPointF lastPoint = ordered.last().last();
        int bestIndex = 0;
        double bestDistance = std::numeric_limits<double>::max();
        bool reverseBest = false;

        for (int i = 0; i < remaining.size(); ++i) {
            const ArtPolyline &candidate = remaining.at(i);
            const double dStart = distance(lastPoint, candidate.first());
            const double dEnd = distance(lastPoint, candidate.last());

            if (dStart < bestDistance) {
                bestDistance = dStart;
                bestIndex = i;
                reverseBest = false;
            }
            if (dEnd < bestDistance) {
                bestDistance = dEnd;
                bestIndex = i;
                reverseBest = true;
            }
        }

        ArtPolyline next = remaining.takeAt(bestIndex);
        if (reverseBest) {
            ArtPolyline reversed;
            reversed.reserve(next.size());
            for (auto it = next.crbegin(); it != next.crend(); ++it)
                reversed.append(*it);
            next = reversed;
        }
        ordered.append(next);
    }

    return ordered;
}
