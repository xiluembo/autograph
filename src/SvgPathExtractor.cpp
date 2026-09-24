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

#include "SvgPathExtractor.h"

#include "ArtPathNormalizer.h"

#include <QFile>
#include <QLineF>
#include <QPainterPath>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QtMath>

namespace {

constexpr double kMinSegmentLength = 0.5;

QStringList tokenizePathData(const QString &data)
{
    QStringList tokens;
    static const QRegularExpression tokenPattern(QStringLiteral(
        R"(([AaCcHhLlMmQqSsTtVvZz])|([-+]?(?:(?:\d+\.\d*)|(?:\.\d+)|(?:\d+))(?:[eE][-+]?\d+)?))"));

    auto matches = tokenPattern.globalMatch(data);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        tokens.append(match.captured(0));
    }
    return tokens;
}

QString styleProperty(const QXmlStreamAttributes &attrs, const QString &property)
{
    const QString style = attrs.value(QStringLiteral("style")).toString();
    if (!style.isEmpty()) {
        const QRegularExpression pattern(
            QStringLiteral(R"((?:^|;)\s*%1\s*:\s*([^;]+))")
                .arg(QRegularExpression::escape(property)),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = pattern.match(style);
        if (match.hasMatch())
            return match.captured(1).trimmed();
    }

    return attrs.value(property).toString().trimmed();
}

bool isElementVisible(const QXmlStreamAttributes &attrs)
{
    const QString display = styleProperty(attrs, QStringLiteral("display")).toLower();
    const QString visibility = styleProperty(attrs, QStringLiteral("visibility")).toLower();
    const QString opacity = styleProperty(attrs, QStringLiteral("opacity"));

    if (display == QLatin1String("none")
        || visibility == QLatin1String("hidden")
        || visibility == QLatin1String("collapse")) {
        return false;
    }

    bool opacityOk = false;
    const double opacityValue = opacity.toDouble(&opacityOk);
    return !opacityOk || opacityValue > 0.0;
}

QTransform parseTransformAttribute(const QString &value)
{
    if (value.isEmpty())
        return {};

    QTransform result;
    static const QRegularExpression re(
        QStringLiteral(R"((matrix|translate|scale|rotate|skewX|skewY)\s*\(\s*([^)]*)\s*\))"),
        QRegularExpression::CaseInsensitiveOption);

    auto it = re.globalMatch(value);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString type = match.captured(1).toLower();
        const QStringList args = match.captured(2).split(
            QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);

        auto num = [&](int index) -> double {
            return index < args.size() ? args.at(index).toDouble() : 0.0;
        };

        if (type == QStringLiteral("matrix") && args.size() >= 6) {
            result *= QTransform(num(0), num(1), num(2), num(3), num(4), num(5));
        } else if (type == QStringLiteral("translate")) {
            const double ty = args.size() >= 2 ? num(1) : 0.0;
            result.translate(num(0), ty);
        } else if (type == QStringLiteral("scale")) {
            const double sy = args.size() >= 2 ? num(1) : num(0);
            result.scale(num(0), sy);
        } else if (type == QStringLiteral("rotate")) {
            if (args.size() >= 3) {
                const double cx = num(1);
                const double cy = num(2);
                result.translate(cx, cy);
                result.rotate(num(0));
                result.translate(-cx, -cy);
            } else {
                result.rotate(num(0));
            }
        } else if (type == QStringLiteral("skewx")) {
            result *= QTransform(1.0, 0.0, qTan(qDegreesToRadians(num(0))), 1.0, 0.0, 0.0);
        } else if (type == QStringLiteral("skewy")) {
            result *= QTransform(1.0, qTan(qDegreesToRadians(num(0))), 0.0, 1.0, 0.0, 0.0);
        }
    }

    return result;
}

QPainterPath parsePathData(const QString &data)
{
    QPainterPath raw;
    const QStringList tokens = tokenizePathData(data);
    if (tokens.isEmpty())
        return raw;

    QPointF current;
    QPointF subStart;
    QPointF lastCtrl;
    QChar lastCmd;
    QChar lastExecutedCmd;
    int i = 0;

    auto readNumber = [&]() -> double {
        if (i >= tokens.size())
            return 0.0;
        bool ok = false;
        const double value = tokens.at(i++).toDouble(&ok);
        return ok ? value : 0.0;
    };

    auto isCommand = [](const QString &token) {
        return token.size() == 1 && token.at(0).isLetter();
    };

    auto readPoint = [&]() {
        const double x = readNumber();
        const double y = readNumber();
        return QPointF(x, y);
    };

    while (i < tokens.size()) {
        QString token = tokens.at(i);
        if (isCommand(token)) {
            lastCmd = token.at(0);
            ++i;
        } else if (lastCmd == QChar()) {
            ++i;
            continue;
        }

        const bool relative = lastCmd.isLower();
        const QChar cmd = lastCmd.toUpper();

        switch (cmd.toLatin1()) {
        case 'M': {
            const double x = readNumber();
            const double y = readNumber();
            current = relative ? current + QPointF(x, y) : QPointF(x, y);
            subStart = current;
            lastCtrl = current;
            raw.moveTo(current);
            if (i < tokens.size() && !isCommand(tokens.at(i)))
                lastCmd = relative ? QChar('l') : QChar('L');
            break;
        }
        case 'L': {
            const double x = readNumber();
            const double y = readNumber();
            const QPointF end = relative ? current + QPointF(x, y) : QPointF(x, y);
            raw.lineTo(end);
            current = end;
            lastCtrl = current;
            break;
        }
        case 'H': {
            const double x = readNumber();
            const QPointF end(relative ? current.x() + x : x, current.y());
            raw.lineTo(end);
            current = end;
            lastCtrl = current;
            break;
        }
        case 'V': {
            const double y = readNumber();
            const QPointF end(current.x(), relative ? current.y() + y : y);
            raw.lineTo(end);
            current = end;
            lastCtrl = current;
            break;
        }
        case 'C': {
            QPointF c1 = readPoint();
            QPointF c2 = readPoint();
            QPointF end = readPoint();
            if (relative) {
                c1 += current;
                c2 += current;
                end += current;
            }
            raw.cubicTo(c1, c2, end);
            lastCtrl = c2;
            current = end;
            break;
        }
        case 'S': {
            QPointF c2 = readPoint();
            QPointF end = readPoint();
            if (relative) {
                c2 += current;
                end += current;
            }
            const QPointF c1 = (lastExecutedCmd == QLatin1Char('C')
                                || lastExecutedCmd == QLatin1Char('S'))
                                   ? current + (current - lastCtrl)
                                   : current;
            raw.cubicTo(c1, c2, end);
            lastCtrl = c2;
            current = end;
            break;
        }
        case 'Q': {
            QPointF c = readPoint();
            QPointF end = readPoint();
            if (relative) {
                c += current;
                end += current;
            }
            raw.quadTo(c, end);
            lastCtrl = c;
            current = end;
            break;
        }
        case 'T': {
            QPointF end = readPoint();
            if (relative)
                end += current;
            const QPointF c = (lastExecutedCmd == QLatin1Char('Q')
                               || lastExecutedCmd == QLatin1Char('T'))
                                  ? current + (current - lastCtrl)
                                  : current;
            raw.quadTo(c, end);
            lastCtrl = c;
            current = end;
            break;
        }
        case 'A': {
            readNumber();
            readNumber();
            readNumber();
            readNumber();
            readNumber();
            QPointF end = readPoint();
            if (relative)
                end += current;
            raw.lineTo(end);
            current = end;
            lastCtrl = current;
            break;
        }
        case 'Z': {
            raw.closeSubpath();
            current = subStart;
            lastCtrl = current;
            break;
        }
        default:
            ++i;
            break;
        }

        lastExecutedCmd = cmd;
    }

    return raw;
}

ArtPolyline sampleSubpath(const QPainterPath &subpath)
{
    ArtPolyline points;
    if (subpath.isEmpty())
        return points;

    const double length = subpath.length();
    if (length <= 0.0) {
        points.append(subpath.pointAtPercent(0.0));
        return points;
    }

    const int steps = qMax(2, static_cast<int>(length / kMinSegmentLength));
    for (int s = 0; s <= steps; ++s) {
        const double t = static_cast<double>(s) / steps;
        const QPointF p = subpath.pointAtPercent(subpath.percentAtLength(t * length));
        if (points.isEmpty() || QLineF(points.last(), p).length() > kMinSegmentLength)
            points.append(p);
    }

    return points;
}

QVector<ArtPolyline> samplePathSubpaths(const QPainterPath &path)
{
    QVector<ArtPolyline> subpaths;
    if (path.isEmpty())
        return subpaths;

    QPainterPath currentSubpath;
    const int elementCount = path.elementCount();

    auto flush = [&]() {
        const ArtPolyline sampled = sampleSubpath(currentSubpath);
        if (sampled.size() >= 2)
            subpaths.append(sampled);
        currentSubpath = QPainterPath();
    };

    for (int i = 0; i < elementCount; ++i) {
        const QPainterPath::Element element = path.elementAt(i);
        switch (element.type) {
        case QPainterPath::MoveToElement:
            flush();
            currentSubpath.moveTo(element.x, element.y);
            break;
        case QPainterPath::LineToElement:
            currentSubpath.lineTo(element.x, element.y);
            break;
        case QPainterPath::CurveToElement:
            if (i + 2 < elementCount) {
                const QPainterPath::Element c1 = path.elementAt(i);
                const QPainterPath::Element c2 = path.elementAt(i + 1);
                const QPainterPath::Element end = path.elementAt(i + 2);
                currentSubpath.cubicTo(c1.x, c1.y, c2.x, c2.y, end.x, end.y);
                i += 2;
            }
            break;
        default:
            break;
        }
    }
    flush();

    return subpaths;
}

void parsePointsAttribute(const QString &pointsData, ArtPolyline &out)
{
    const QStringList values = pointsData.split(QRegularExpression(QStringLiteral("[,\\s]+")),
                                                Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < values.size(); i += 2) {
        bool okX = false;
        bool okY = false;
        const double x = values.at(i).toDouble(&okX);
        const double y = values.at(i + 1).toDouble(&okY);
        if (okX && okY)
            out.append(QPointF(x, y));
    }
}

QString hrefTarget(const QXmlStreamAttributes &attrs)
{
    QString href = attrs.value(QStringLiteral("href")).toString();
    if (href.isEmpty())
        href = attrs.value(QStringLiteral("xlink:href")).toString();
    if (href.startsWith(QLatin1Char('#')))
        href = href.mid(1);
    return href;
}

} // namespace

bool SvgPathExtractor::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return loadFromData(file.readAll());
}

bool SvgPathExtractor::loadFromData(const QByteArray &data)
{
    m_polylines.clear();
    m_defs.clear();
    m_groupPaths.clear();
    m_activeDefGroupIds.clear();
    m_fillRule = Qt::WindingFill;

    if (!parseSvg(data, true))
        return false;

    m_polylines.clear();
    m_groupPaths.clear();
    m_activeDefGroupIds.clear();
    if (!parseSvg(data, false))
        return false;

    normalizeArtPolylines(m_polylines);
    return !m_polylines.isEmpty();
}

void SvgPathExtractor::appendPath(const QPainterPath &path)
{
    if (path.isEmpty())
        return;

    for (const ArtPolyline &sampled : samplePathSubpaths(path)) {
        if (sampled.size() >= 2)
            m_polylines.append(sampled);
    }
}

void SvgPathExtractor::storeOrAppendPath(const QPainterPath &path, const QTransform &transform,
                                         bool inDefs, const QString &id, bool defsOnly)
{
    if (path.isEmpty())
        return;

    const QPainterPath mapped = transform.map(path);

    if (inDefs && !m_activeDefGroupIds.isEmpty())
        m_groupPaths[m_activeDefGroupIds.last()].addPath(mapped);

    if (inDefs) {
        if (!id.isEmpty())
            m_defs.insert(id, mapped);
        return;
    }

    if (!defsOnly)
        appendPath(mapped);
}

bool SvgPathExtractor::parseSvg(const QByteArray &data, bool defsOnly)
{
    QVector<QTransform> transformStack;
    transformStack.append(QTransform());
    QVector<bool> visibilityStack;
    visibilityStack.append(true);

    struct GroupFrame {
        QString id;
        QTransform transform;
    };
    QVector<GroupFrame> groupStack;

    int defsDepth = 0;

    QXmlStreamReader reader(data);

    while (!reader.atEnd()) {
        reader.readNext();

        if (reader.isStartElement()) {
            const QString name = reader.name().toString();
            const auto attrs = reader.attributes();
            const bool inDefs = defsDepth > 0;
            const bool visible = visibilityStack.last() && isElementVisible(attrs);

            if (name == QLatin1String("g") || name == QLatin1String("svg")
                || name == QLatin1String("a")) {
                const QTransform local = parseTransformAttribute(attrs.value(QStringLiteral("transform")).toString());
                const QTransform combined = local * transformStack.last();
                transformStack.append(combined);
                visibilityStack.append(visible);

                GroupFrame frame;
                frame.id = attrs.value(QStringLiteral("id")).toString();
                frame.transform = combined;
                groupStack.append(frame);

                if (inDefs && !frame.id.isEmpty()) {
                    m_groupPaths.insert(frame.id, QPainterPath());
                    m_activeDefGroupIds.append(frame.id);
                }
            } else if (name == QLatin1String("defs") || name == QLatin1String("symbol")) {
                ++defsDepth;
            }

            const QTransform current = transformStack.last();
            const QTransform elementTransform = parseTransformAttribute(
                                                    attrs.value(QStringLiteral("transform")).toString())
                                                * current;
            const bool processElement = inDefs || visible;

            if (name == QLatin1String("path") && processElement) {
                const QString d = attrs.value(QStringLiteral("d")).toString();
                const QString id = attrs.value(QStringLiteral("id")).toString();

                if (!defsOnly && !inDefs) {
                    const QString fillRule = styleProperty(attrs, QStringLiteral("fill-rule")).toLower();
                    if (fillRule == QLatin1String("evenodd"))
                        m_fillRule = Qt::OddEvenFill;
                }

                storeOrAppendPath(parsePathData(d), elementTransform, inDefs, id, defsOnly);
            } else if (name == QLatin1String("line") && processElement) {
                QPainterPath linePath;
                const double x1 = attrs.value(QStringLiteral("x1")).toDouble();
                const double y1 = attrs.value(QStringLiteral("y1")).toDouble();
                const double x2 = attrs.value(QStringLiteral("x2")).toDouble();
                const double y2 = attrs.value(QStringLiteral("y2")).toDouble();
                linePath.moveTo(x1, y1);
                linePath.lineTo(x2, y2);
                storeOrAppendPath(linePath, elementTransform, inDefs,
                                  attrs.value(QStringLiteral("id")).toString(), defsOnly);
            } else if ((name == QLatin1String("polyline") || name == QLatin1String("polygon"))
                       && processElement) {
                ArtPolyline points;
                parsePointsAttribute(attrs.value(QStringLiteral("points")).toString(), points);
                if (points.size() >= 2) {
                    QPainterPath polyPath;
                    polyPath.moveTo(points.first());
                    for (int j = 1; j < points.size(); ++j)
                        polyPath.lineTo(points.at(j));
                    if (name == QLatin1String("polygon"))
                        polyPath.closeSubpath();
                    storeOrAppendPath(polyPath, elementTransform, inDefs,
                                      attrs.value(QStringLiteral("id")).toString(), defsOnly);
                }
            } else if (name == QLatin1String("rect") && processElement) {
                const double x = attrs.value(QStringLiteral("x")).toDouble();
                const double y = attrs.value(QStringLiteral("y")).toDouble();
                const double w = attrs.value(QStringLiteral("width")).toDouble();
                const double h = attrs.value(QStringLiteral("height")).toDouble();
                QPainterPath rectPath;
                rectPath.addRect(x, y, w, h);
                storeOrAppendPath(rectPath, elementTransform, inDefs,
                                  attrs.value(QStringLiteral("id")).toString(), defsOnly);
            } else if (name == QLatin1String("circle") && processElement) {
                const double cx = attrs.value(QStringLiteral("cx")).toDouble();
                const double cy = attrs.value(QStringLiteral("cy")).toDouble();
                const double r = attrs.value(QStringLiteral("r")).toDouble();
                QPainterPath circlePath;
                circlePath.addEllipse(QPointF(cx, cy), r, r);
                storeOrAppendPath(circlePath, elementTransform, inDefs,
                                  attrs.value(QStringLiteral("id")).toString(), defsOnly);
            } else if (name == QLatin1String("ellipse") && processElement) {
                const double cx = attrs.value(QStringLiteral("cx")).toDouble();
                const double cy = attrs.value(QStringLiteral("cy")).toDouble();
                const double rx = attrs.value(QStringLiteral("rx")).toDouble();
                const double ry = attrs.value(QStringLiteral("ry")).toDouble();
                QPainterPath ellipsePath;
                ellipsePath.addEllipse(QPointF(cx, cy), rx, ry);
                storeOrAppendPath(ellipsePath, elementTransform, inDefs,
                                  attrs.value(QStringLiteral("id")).toString(), defsOnly);
            } else if (name == QLatin1String("use") && !inDefs && !defsOnly && visible) {
                const QString ref = hrefTarget(attrs);
                QPainterPath usedPath;
                if (m_defs.contains(ref))
                    usedPath = m_defs.value(ref);
                else if (m_groupPaths.contains(ref))
                    usedPath = m_groupPaths.value(ref);

                if (!usedPath.isEmpty()) {
                    const double x = attrs.value(QStringLiteral("x")).toDouble();
                    const double y = attrs.value(QStringLiteral("y")).toDouble();
                    const QTransform useTransform = QTransform::fromTranslate(x, y)
                                                    * elementTransform;
                    appendPath(useTransform.map(usedPath));
                }
            }
        } else if (reader.isEndElement()) {
            const QString name = reader.name().toString();

            if (name == QLatin1String("g") || name == QLatin1String("svg")
                || name == QLatin1String("a")) {
                if (!groupStack.isEmpty()) {
                    const GroupFrame frame = groupStack.takeLast();
                    if (defsDepth > 0 && !frame.id.isEmpty() && m_groupPaths.contains(frame.id)) {
                        m_defs.insert(frame.id, m_groupPaths.value(frame.id));
                        m_groupPaths.remove(frame.id);
                        if (!m_activeDefGroupIds.isEmpty() && m_activeDefGroupIds.last() == frame.id)
                            m_activeDefGroupIds.removeLast();
                    }
                }
                if (transformStack.size() > 1)
                    transformStack.removeLast();
                if (visibilityStack.size() > 1)
                    visibilityStack.removeLast();
            } else if (name == QLatin1String("defs") || name == QLatin1String("symbol")) {
                defsDepth = qMax(0, defsDepth - 1);
            }
        }
    }

    return !reader.hasError();
}
