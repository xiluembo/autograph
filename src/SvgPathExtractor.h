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

#include <QByteArray>
#include <QHash>
#include <QPainterPath>
#include <QPointF>
#include <QString>
#include <QTransform>
#include <Qt>
#include <QVector>

using ArtPolyline = QVector<QPointF>;
using ArtPolylineSet = QVector<ArtPolyline>;

class SvgPathExtractor
{
public:
    bool loadFromFile(const QString &filePath);
    bool loadFromData(const QByteArray &data);

    const ArtPolylineSet &polylines() const { return m_polylines; }
    Qt::FillRule fillRule() const { return m_fillRule; }
    bool isEmpty() const { return m_polylines.isEmpty(); }

private:
    bool parseSvg(const QByteArray &data, bool defsOnly);
    void appendPath(const QPainterPath &path);
    void storeOrAppendPath(const QPainterPath &path, const QTransform &transform,
                           bool inDefs, const QString &id, bool defsOnly);

    ArtPolylineSet m_polylines;
    Qt::FillRule m_fillRule = Qt::WindingFill;
    QHash<QString, QPainterPath> m_defs;
    QHash<QString, QPainterPath> m_groupPaths;
    QVector<QString> m_activeDefGroupIds;
};
