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
#include <QPainterPath>
#include <QPolygonF>
#include <QRegularExpression>
#include <QStringView>
#include <QXmlStreamReader>
#include <QtMath>

namespace {

// Reference size (in flattening space) used to derive an adaptive curve
// tolerance. QPainterPath::toSubpathPolygons() flattens curves with a maximum
// deviation of ~0.5 unit in the transformed space, so scaling the content to
// this size keeps the deviation below ~0.025% of the largest dimension
// regardless of the SVG coordinate scale. The polylines are normalized to the
// 0..1 range afterwards, so the scale itself does not leak into the output.
constexpr double kFlattenReferenceSize = 2048.0;

// Allocation-free scanner for the SVG number grammar used by the "d",
// "points" and "transform" attributes.
class SvgNumberScanner
{
public:
    explicit SvgNumberScanner(QStringView text)
        : m_pos(text.cbegin()), m_end(text.cend())
    {
    }

    bool atEnd()
    {
        skipSeparators();
        return m_pos >= m_end;
    }

    // Returns the pending command letter, or a null QChar when the next token
    // is not a letter.
    QChar peekCommand()
    {
        skipSeparators();
        if (m_pos < m_end && m_pos->isLetter())
            return *m_pos;
        return {};
    }

    void consumeChar()
    {
        if (m_pos < m_end)
            ++m_pos;
    }

    bool readNumber(double &out)
    {
        skipSeparators();
        const QChar *start = m_pos;
        const QChar *p = m_pos;

        if (p < m_end && (*p == QLatin1Char('+') || *p == QLatin1Char('-')))
            ++p;

        const QChar *digitsStart = p;
        while (p < m_end && p->isDigit())
            ++p;
        if (p < m_end && *p == QLatin1Char('.')) {
            ++p;
            while (p < m_end && p->isDigit())
                ++p;
        }
        // Reject empty mantissas ("", "+", "-", ".").
        if (p == digitsStart || (p == digitsStart + 1 && *digitsStart == QLatin1Char('.')))
            return false;

        if (p < m_end && (*p == QLatin1Char('e') || *p == QLatin1Char('E'))) {
            const QChar *exponentStart = p;
            ++p;
            if (p < m_end && (*p == QLatin1Char('+') || *p == QLatin1Char('-')))
                ++p;
            if (p < m_end && p->isDigit()) {
                while (p < m_end && p->isDigit())
                    ++p;
            } else {
                p = exponentStart; // not an exponent after all
            }
        }

        bool ok = false;
        out = QStringView(start, p - start).toDouble(&ok);
        if (!ok)
            return false;

        m_pos = p;
        return true;
    }

    // Arc flags are single '0'/'1' characters that may be glued to the next
    // number (e.g. "a25 25 0 0150 25" means large-arc=0, sweep=1, x=50).
    bool readFlag(bool &out)
    {
        skipSeparators();
        if (m_pos < m_end && (*m_pos == QLatin1Char('0') || *m_pos == QLatin1Char('1'))) {
            out = (*m_pos == QLatin1Char('1'));
            ++m_pos;
            return true;
        }
        double value = 0.0;
        if (!readNumber(value))
            return false;
        out = !qFuzzyIsNull(value);
        return true;
    }

private:
    void skipSeparators()
    {
        while (m_pos < m_end && (m_pos->isSpace() || *m_pos == QLatin1Char(',')))
            ++m_pos;
    }

    const QChar *m_pos;
    const QChar *m_end;
};

QString styleProperty(const QXmlStreamAttributes &attrs, QLatin1String property)
{
    const QStringView style = attrs.value(QLatin1String("style"));
    if (!style.isEmpty()) {
        const auto declarations = style.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QStringView &declaration : declarations) {
            const qsizetype colon = declaration.indexOf(QLatin1Char(':'));
            if (colon < 0)
                continue;
            const QStringView name = declaration.left(colon).trimmed();
            if (name.compare(property, Qt::CaseInsensitive) == 0)
                return declaration.mid(colon + 1).trimmed().toString();
        }
    }

    return attrs.value(property).toString().trimmed();
}

bool isElementVisible(const QXmlStreamAttributes &attrs)
{
    // Fast path: most elements carry none of the visibility attributes.
    if (!attrs.hasAttribute(QLatin1String("style"))
        && !attrs.hasAttribute(QLatin1String("display"))
        && !attrs.hasAttribute(QLatin1String("visibility"))
        && !attrs.hasAttribute(QLatin1String("opacity"))) {
        return true;
    }

    const QString display = styleProperty(attrs, QLatin1String("display")).toLower();
    const QString visibility = styleProperty(attrs, QLatin1String("visibility")).toLower();
    const QString opacity = styleProperty(attrs, QLatin1String("opacity"));

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

        QVector<double> args;
        SvgNumberScanner scanner(match.capturedView(2));
        double number = 0.0;
        while (scanner.readNumber(number))
            args.append(number);

        auto num = [&](int index) -> double {
            return index < args.size() ? args.at(index) : 0.0;
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

// Converts an SVG elliptical arc (endpoint parameterization, spec section
// F.6) into cubic Bezier segments appended to the path.
void appendArcToPath(QPainterPath &path, const QPointF &start, double rx, double ry,
                     double xAxisRotationDeg, bool largeArc, bool sweep, const QPointF &end)
{
    if (qFuzzyCompare(start.x(), end.x()) && qFuzzyCompare(start.y(), end.y()))
        return;

    rx = qAbs(rx);
    ry = qAbs(ry);
    if (rx < 1e-12 || ry < 1e-12) {
        path.lineTo(end);
        return;
    }

    const double phi = qDegreesToRadians(xAxisRotationDeg);
    const double cosPhi = qCos(phi);
    const double sinPhi = qSin(phi);

    const double dx2 = (start.x() - end.x()) / 2.0;
    const double dy2 = (start.y() - end.y()) / 2.0;
    const double x1p = cosPhi * dx2 + sinPhi * dy2;
    const double y1p = -sinPhi * dx2 + cosPhi * dy2;

    // Correct out-of-range radii (F.6.6).
    const double lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lambda > 1.0) {
        const double s = qSqrt(lambda);
        rx *= s;
        ry *= s;
    }

    const double rx2 = rx * rx;
    const double ry2 = ry * ry;
    const double numerator = rx2 * ry2 - rx2 * y1p * y1p - ry2 * x1p * x1p;
    const double denominator = rx2 * y1p * y1p + ry2 * x1p * x1p;
    double coefficient = 0.0;
    if (numerator > 0.0 && denominator > 0.0)
        coefficient = qSqrt(numerator / denominator);
    if (largeArc == sweep)
        coefficient = -coefficient;

    const double cxp = coefficient * rx * y1p / ry;
    const double cyp = -coefficient * ry * x1p / rx;
    const double cx = cosPhi * cxp - sinPhi * cyp + (start.x() + end.x()) / 2.0;
    const double cy = sinPhi * cxp + cosPhi * cyp + (start.y() + end.y()) / 2.0;

    const double theta1 = qAtan2((y1p - cyp) / ry, (x1p - cxp) / rx);
    const double theta2 = qAtan2((-y1p - cyp) / ry, (-x1p - cxp) / rx);
    double delta = theta2 - theta1;
    if (!sweep && delta > 0.0)
        delta -= 2.0 * M_PI;
    else if (sweep && delta < 0.0)
        delta += 2.0 * M_PI;

    const auto pointAt = [&](double angle) {
        const double c = qCos(angle);
        const double s = qSin(angle);
        return QPointF(cx + rx * c * cosPhi - ry * s * sinPhi,
                       cy + rx * c * sinPhi + ry * s * cosPhi);
    };
    const auto derivativeAt = [&](double angle) {
        const double c = qCos(angle);
        const double s = qSin(angle);
        return QPointF(-rx * s * cosPhi - ry * c * sinPhi,
                       -rx * s * sinPhi + ry * c * cosPhi);
    };

    // Approximate each sweep of at most 90 degrees with one cubic segment.
    const int segments = qMax(1, qCeil(qAbs(delta) / (M_PI / 2.0)));
    const double deltaSegment = delta / segments;
    const double kappa = (4.0 / 3.0) * qTan(deltaSegment / 4.0);

    double theta = theta1;
    QPointF from = start;
    for (int i = 0; i < segments; ++i) {
        const double next = theta + deltaSegment;
        const QPointF to = (i == segments - 1) ? end : pointAt(next);
        path.cubicTo(from + kappa * derivativeAt(theta),
                     to - kappa * derivativeAt(next),
                     to);
        from = to;
        theta = next;
    }
}

QPainterPath parsePathData(QStringView data)
{
    QPainterPath raw;
    SvgNumberScanner scanner(data);

    QPointF current;
    QPointF subStart;
    QPointF lastCtrl;
    QChar lastCmd;
    QChar lastExecutedCmd;

    const auto readPoint = [&scanner](QPointF &out) {
        double x = 0.0;
        double y = 0.0;
        if (!scanner.readNumber(x) || !scanner.readNumber(y))
            return false;
        out = QPointF(x, y);
        return true;
    };

    while (!scanner.atEnd()) {
        const QChar commandChar = scanner.peekCommand();
        if (!commandChar.isNull()) {
            lastCmd = commandChar;
            scanner.consumeChar();
        } else if (lastCmd.isNull()) {
            // Garbage before the first command; skip it.
            scanner.consumeChar();
            continue;
        }

        const bool relative = lastCmd.isLower();
        const QChar cmd = lastCmd.toUpper();
        bool parsed = true;

        switch (cmd.toLatin1()) {
        case 'M': {
            QPointF p;
            parsed = readPoint(p);
            if (!parsed)
                break;
            current = relative ? current + p : p;
            subStart = current;
            lastCtrl = current;
            raw.moveTo(current);
            // Subsequent coordinate pairs are implicit linetos.
            lastCmd = relative ? QChar('l') : QChar('L');
            break;
        }
        case 'L': {
            QPointF p;
            parsed = readPoint(p);
            if (!parsed)
                break;
            const QPointF end = relative ? current + p : p;
            raw.lineTo(end);
            current = end;
            lastCtrl = current;
            break;
        }
        case 'H': {
            double x = 0.0;
            parsed = scanner.readNumber(x);
            if (!parsed)
                break;
            const QPointF end(relative ? current.x() + x : x, current.y());
            raw.lineTo(end);
            current = end;
            lastCtrl = current;
            break;
        }
        case 'V': {
            double y = 0.0;
            parsed = scanner.readNumber(y);
            if (!parsed)
                break;
            const QPointF end(current.x(), relative ? current.y() + y : y);
            raw.lineTo(end);
            current = end;
            lastCtrl = current;
            break;
        }
        case 'C': {
            QPointF c1;
            QPointF c2;
            QPointF end;
            parsed = readPoint(c1) && readPoint(c2) && readPoint(end);
            if (!parsed)
                break;
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
            QPointF c2;
            QPointF end;
            parsed = readPoint(c2) && readPoint(end);
            if (!parsed)
                break;
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
            QPointF c;
            QPointF end;
            parsed = readPoint(c) && readPoint(end);
            if (!parsed)
                break;
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
            QPointF end;
            parsed = readPoint(end);
            if (!parsed)
                break;
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
            double rx = 0.0;
            double ry = 0.0;
            double rotation = 0.0;
            bool largeArc = false;
            bool sweep = false;
            QPointF end;
            parsed = scanner.readNumber(rx) && scanner.readNumber(ry)
                     && scanner.readNumber(rotation) && scanner.readFlag(largeArc)
                     && scanner.readFlag(sweep) && readPoint(end);
            if (!parsed)
                break;
            if (relative)
                end += current;
            appendArcToPath(raw, current, rx, ry, rotation, largeArc, sweep, end);
            current = end;
            lastCtrl = current;
            break;
        }
        case 'Z': {
            raw.closeSubpath();
            current = subStart;
            lastCtrl = current;
            // Numbers may not follow a closepath; require an explicit command.
            lastCmd = QChar();
            break;
        }
        default:
            // Unknown command: ignore it and its parameters.
            lastCmd = QChar();
            break;
        }

        if (!parsed)
            break; // malformed data; keep what was parsed so far

        lastExecutedCmd = cmd;
    }

    return raw;
}

void parsePointsAttribute(QStringView pointsData, ArtPolyline &out)
{
    SvgNumberScanner scanner(pointsData);
    double x = 0.0;
    double y = 0.0;
    while (scanner.readNumber(x) && scanner.readNumber(y))
        out.append(QPointF(x, y));
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
    m_paths.clear();
    m_pendingUses.clear();
    m_defs.clear();
    m_groupPaths.clear();
    m_activeDefGroupIds.clear();
    m_fillRule = Qt::WindingFill;

    if (!parseSvg(data))
        return false;

    resolvePendingUses();
    flattenPaths();

    m_paths.clear();
    m_defs.clear();
    m_groupPaths.clear();

    normalizeArtPolylines(m_polylines);
    return !m_polylines.isEmpty();
}

void SvgPathExtractor::resolvePendingUses()
{
    // <use> elements that referenced ids defined later in the document.
    for (const PendingUse &use : std::as_const(m_pendingUses)) {
        QPainterPath path = m_defs.value(use.ref);
        if (path.isEmpty())
            path = m_groupPaths.value(use.ref);
        if (!path.isEmpty())
            m_paths.append(use.transform.map(path));
    }
    m_pendingUses.clear();
}

void SvgPathExtractor::flattenPaths()
{
    m_polylines.clear();
    if (m_paths.isEmpty())
        return;

    QRectF bounds;
    for (const QPainterPath &path : std::as_const(m_paths))
        bounds = bounds.united(path.controlPointRect());

    const double maxDimension = qMax(bounds.width(), bounds.height());
    const double scale = maxDimension > 1e-12 ? kFlattenReferenceSize / maxDimension : 1.0;
    const QTransform flattenTransform = QTransform::fromScale(scale, scale);

    for (const QPainterPath &path : std::as_const(m_paths)) {
        const QList<QPolygonF> polygons = path.toSubpathPolygons(flattenTransform);
        for (const QPolygonF &polygon : polygons) {
            ArtPolyline polyline;
            polyline.reserve(polygon.size());
            for (const QPointF &point : polygon) {
                if (polyline.isEmpty() || polyline.last() != point)
                    polyline.append(point);
            }
            if (polyline.size() >= 2)
                m_polylines.append(polyline);
        }
    }
}

void SvgPathExtractor::storeOrAppendPath(const QPainterPath &path, const QTransform &transform,
                                         bool inDefs, const QString &id)
{
    if (path.isEmpty())
        return;

    const QPainterPath mapped = transform.map(path);

    if (inDefs) {
        if (!m_activeDefGroupIds.isEmpty())
            m_groupPaths[m_activeDefGroupIds.last()].addPath(mapped);
        if (!id.isEmpty())
            m_defs.insert(id, mapped);
        return;
    }

    m_paths.append(mapped);
}

bool SvgPathExtractor::parseSvg(const QByteArray &data)
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
            const QStringView name = reader.name();
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

            const bool processElement = inDefs || visible;
            if (!processElement)
                continue;

            const QTransform current = transformStack.last();
            const QTransform elementTransform = parseTransformAttribute(
                                                    attrs.value(QStringLiteral("transform")).toString())
                                                * current;

            if (name == QLatin1String("path")) {
                const QString d = attrs.value(QStringLiteral("d")).toString();
                const QString id = attrs.value(QStringLiteral("id")).toString();

                if (!inDefs) {
                    const QString fillRule = styleProperty(attrs, QLatin1String("fill-rule")).toLower();
                    if (fillRule == QLatin1String("evenodd"))
                        m_fillRule = Qt::OddEvenFill;
                }

                storeOrAppendPath(parsePathData(d), elementTransform, inDefs, id);
            } else if (name == QLatin1String("line")) {
                QPainterPath linePath;
                const double x1 = attrs.value(QStringLiteral("x1")).toDouble();
                const double y1 = attrs.value(QStringLiteral("y1")).toDouble();
                const double x2 = attrs.value(QStringLiteral("x2")).toDouble();
                const double y2 = attrs.value(QStringLiteral("y2")).toDouble();
                linePath.moveTo(x1, y1);
                linePath.lineTo(x2, y2);
                storeOrAppendPath(linePath, elementTransform, inDefs,
                                  attrs.value(QStringLiteral("id")).toString());
            } else if (name == QLatin1String("polyline") || name == QLatin1String("polygon")) {
                ArtPolyline points;
                parsePointsAttribute(attrs.value(QStringLiteral("points")), points);
                if (points.size() >= 2) {
                    QPainterPath polyPath;
                    polyPath.moveTo(points.first());
                    for (int j = 1; j < points.size(); ++j)
                        polyPath.lineTo(points.at(j));
                    if (name == QLatin1String("polygon"))
                        polyPath.closeSubpath();
                    storeOrAppendPath(polyPath, elementTransform, inDefs,
                                      attrs.value(QStringLiteral("id")).toString());
                }
            } else if (name == QLatin1String("rect")) {
                const double x = attrs.value(QStringLiteral("x")).toDouble();
                const double y = attrs.value(QStringLiteral("y")).toDouble();
                const double w = attrs.value(QStringLiteral("width")).toDouble();
                const double h = attrs.value(QStringLiteral("height")).toDouble();
                QPainterPath rectPath;
                rectPath.addRect(x, y, w, h);
                storeOrAppendPath(rectPath, elementTransform, inDefs,
                                  attrs.value(QStringLiteral("id")).toString());
            } else if (name == QLatin1String("circle")) {
                const double cx = attrs.value(QStringLiteral("cx")).toDouble();
                const double cy = attrs.value(QStringLiteral("cy")).toDouble();
                const double r = attrs.value(QStringLiteral("r")).toDouble();
                QPainterPath circlePath;
                circlePath.addEllipse(QPointF(cx, cy), r, r);
                storeOrAppendPath(circlePath, elementTransform, inDefs,
                                  attrs.value(QStringLiteral("id")).toString());
            } else if (name == QLatin1String("ellipse")) {
                const double cx = attrs.value(QStringLiteral("cx")).toDouble();
                const double cy = attrs.value(QStringLiteral("cy")).toDouble();
                const double rx = attrs.value(QStringLiteral("rx")).toDouble();
                const double ry = attrs.value(QStringLiteral("ry")).toDouble();
                QPainterPath ellipsePath;
                ellipsePath.addEllipse(QPointF(cx, cy), rx, ry);
                storeOrAppendPath(ellipsePath, elementTransform, inDefs,
                                  attrs.value(QStringLiteral("id")).toString());
            } else if (name == QLatin1String("use") && !inDefs && visible) {
                const QString ref = hrefTarget(attrs);
                if (!ref.isEmpty()) {
                    const double x = attrs.value(QStringLiteral("x")).toDouble();
                    const double y = attrs.value(QStringLiteral("y")).toDouble();
                    const QTransform useTransform = QTransform::fromTranslate(x, y)
                                                    * elementTransform;

                    QPainterPath usedPath = m_defs.value(ref);
                    if (usedPath.isEmpty())
                        usedPath = m_groupPaths.value(ref);

                    if (!usedPath.isEmpty())
                        m_paths.append(useTransform.map(usedPath));
                    else
                        m_pendingUses.append({ref, useTransform}); // forward reference
                }
            }
        } else if (reader.isEndElement()) {
            const QStringView name = reader.name();

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
