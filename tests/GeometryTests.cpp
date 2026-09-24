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
#include "PathScaler.h"
#include "SvgPathExtractor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QtMath>

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const QString &message)
{
    if (condition)
        return;

    std::cerr << message.toStdString() << '\n';
    ++failures;
}

bool closeTo(double actual, double expected, double tolerance = 1e-6)
{
    return qAbs(actual - expected) <= tolerance;
}

void testPolylineBounds()
{
    const ArtPolylineSet polylines{
        ArtPolyline{QPointF(-4.0, 2.0), QPointF(6.0, -3.0)},
        ArtPolyline{QPointF(1.0, 9.0)}
    };

    const QRectF bounds = artPolylineBounds(polylines);
    check(closeTo(bounds.left(), -4.0), QStringLiteral("bounds.left is incorrect"));
    check(closeTo(bounds.top(), -3.0), QStringLiteral("bounds.top is incorrect"));
    check(closeTo(bounds.width(), 10.0), QStringLiteral("bounds.width is incorrect"));
    check(closeTo(bounds.height(), 12.0), QStringLiteral("bounds.height is incorrect"));

    ArtPolylineSet normalized = polylines;
    normalizeArtPolylines(normalized);
    const QRectF normalizedBounds = artPolylineBounds(normalized);
    check(closeTo(normalizedBounds.left(), 0.0), QStringLiteral("normalized left is incorrect"));
    check(closeTo(normalizedBounds.top(), 0.0), QStringLiteral("normalized top is incorrect"));
    check(closeTo(normalizedBounds.width(), 10.0 / 12.0),
          QStringLiteral("normalized width is incorrect"));
    check(closeTo(normalizedBounds.height(), 1.0),
          QStringLiteral("normalized height is incorrect"));
}

void testScalingOnSecondaryMonitor()
{
    ArtPolylineSet polylines{
        ArtPolyline{QPointF(0.0, 0.0), QPointF(1.0, 0.0), QPointF(1.0, 0.5)},
        ArtPolyline{QPointF(0.2, 0.1), QPointF(0.8, 0.4)}
    };
    const QRect targetArea(-1920, 120, 800, 400);
    const ArtPolylineSet scaled = PathScaler::scaleToArea(polylines, targetArea);

    check(scaled.size() == polylines.size(), QStringLiteral("scaled path count changed"));
    for (const ArtPolyline &line : scaled) {
        for (int i = 0; i < line.size(); ++i) {
            const QPointF &point = line.at(i);
            check(qIsFinite(point.x()) && qIsFinite(point.y()),
                  QStringLiteral("scaled point is not finite"));
            check(closeTo(point.x(), qRound(point.x()))
                      && closeTo(point.y(), qRound(point.y())),
                  QStringLiteral("scaled point was not aligned to a screen pixel"));
            check(QRectF(targetArea).contains(point),
                  QStringLiteral("scaled point escaped the selected area: (%1, %2)")
                      .arg(point.x()).arg(point.y()));
            if (i > 0) {
                check(line.at(i - 1) != point,
                      QStringLiteral("scaled path contains consecutive duplicate pixels"));
            }
        }
    }
}

void testScalingInsideAndroidArea()
{
    const ArtPolylineSet polylines{
        ArtPolyline{QPointF(0.0, 0.0), QPointF(0.25, 0.5), QPointF(1.0, 1.0)},
        ArtPolyline{QPointF(0.1, 0.9), QPointF(0.9, 0.1)}
    };
    const QRect targetArea(24, 160, 960, 1320);
    const ArtPolylineSet scaled = PathScaler::scaleToArea(polylines, targetArea);

    check(scaled.size() == polylines.size(),
          QStringLiteral("Android scaling changed the path count"));
    for (const ArtPolyline &line : scaled) {
        for (const QPointF &point : line) {
            check(point.x() >= 0.0 && point.y() >= 0.0,
                  QStringLiteral("Android gesture contains a negative coordinate"));
            check(QRectF(targetArea).contains(point),
                  QStringLiteral("Android gesture escaped the selected area"));
        }
    }
}

void testComplexSvgParsing()
{
    const QByteArray svg = R"SVG(
        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
          <g style="display:none">
            <path d="M 0 0 H 100 V 100 H 0 Z" />
          </g>
          <g transform="translate(30,40)">
            <path transform="matrix(2,0,0,3,0,0)"
                  style="fill-rule:evenodd"
                  d="M 0 0 C 0 1e1 10 10 10 0 L 10 2 Z" />
          </g>
        </svg>
    )SVG";

    SvgPathExtractor extractor;
    check(extractor.loadFromData(svg), QStringLiteral("failed to parse complex SVG fixture"));
    check(extractor.polylines().size() == 1,
          QStringLiteral("hidden SVG geometry was not ignored"));
    check(extractor.fillRule() == Qt::OddEvenFill,
          QStringLiteral("fill rule in style was not preserved"));

    const QRectF bounds = artPolylineBounds(extractor.polylines());
    check(closeTo(bounds.height(), 1.0),
          QStringLiteral("complex SVG height was not normalized"));
    check(bounds.width() > 0.86 && bounds.width() < 0.91,
          QStringLiteral("curve coordinates or element transform are incorrect: %1")
              .arg(bounds.width()));
}

void testNestedTransformOrder()
{
    const QByteArray svg = R"SVG(
        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
          <path d="M 0 0 H 1 V 1 H 0 Z" />
          <g transform="translate(20,0)">
            <path transform="scale(2,1)" d="M 0 0 H 10 V 10 H 0 Z" />
          </g>
        </svg>
    )SVG";

    SvgPathExtractor extractor;
    check(extractor.loadFromData(svg), QStringLiteral("failed to parse nested transforms"));
    check(extractor.polylines().size() == 2,
          QStringLiteral("nested transform fixture path count is incorrect"));
    if (extractor.polylines().size() != 2)
        return;

    const QRectF childBounds = artPolylineBounds(
        ArtPolylineSet{extractor.polylines().at(1)});
    check(closeTo(childBounds.left(), 0.5, 1e-3),
          QStringLiteral("parent translation was composed in the wrong order: %1")
              .arg(childBounds.left()));
    check(closeTo(childBounds.right(), 1.0, 1e-3),
          QStringLiteral("child scale was composed in the wrong order: %1")
              .arg(childBounds.right()));
}

bool isDark(const QRgb pixel)
{
    return qGray(pixel) < 128;
}

double maskIntersectionOverUnion(const QImage &left, const QImage &right)
{
    qsizetype intersection = 0;
    qsizetype pixelsInUnion = 0;

    for (int y = 0; y < left.height(); ++y) {
        for (int x = 0; x < left.width(); ++x) {
            const bool inLeft = isDark(left.pixel(x, y));
            const bool inRight = isDark(right.pixel(x, y));
            if (inLeft && inRight)
                ++intersection;
            if (inLeft || inRight)
                ++pixelsInUnion;
        }
    }

    return pixelsInUnion > 0
               ? static_cast<double>(intersection) / static_cast<double>(pixelsInUnion)
               : 0.0;
}

void testReportedSvg()
{
    const QString svgPath = QStringLiteral(AUTOGRAFO_SOURCE_DIR "/examples/pushx_arax.svg");
    check(QFileInfo::exists(svgPath), QStringLiteral("reported SVG fixture is missing"));

    SvgPathExtractor extractor;
    check(extractor.loadFromFile(svgPath), QStringLiteral("failed to load reported SVG"));
    check(extractor.fillRule() == Qt::OddEvenFill,
          QStringLiteral("SVG fill rule was not preserved"));
    check(extractor.polylines().size() == 22,
          QStringLiteral("expected 22 SVG subpaths, got %1").arg(extractor.polylines().size()));

    const QRectF content = artPolylineBounds(extractor.polylines());
    check(closeTo(content.left(), 0.0), QStringLiteral("SVG normalized left is incorrect"));
    check(closeTo(content.top(), 0.0), QStringLiteral("SVG normalized top is incorrect"));
    check(closeTo(content.width(), 1.0), QStringLiteral("SVG normalized width is incorrect"));
    check(closeTo(content.height(), 644.0 / 1291.0, 1e-4),
          QStringLiteral("SVG normalized aspect ratio is incorrect: %1").arg(content.height()));

    const QRect secondaryArea(-2560, 140, 1200, 600);
    const ArtPolylineSet scaled = PathScaler::scaleToArea(extractor.polylines(), secondaryArea);
    for (const ArtPolyline &line : scaled) {
        for (int i = 0; i < line.size(); ++i) {
            const QPointF &point = line.at(i);
            check(QRectF(secondaryArea).contains(point),
                  QStringLiteral("reported SVG escaped the secondary-monitor selection"));
            if (i > 0) {
                check(line.at(i - 1) != point,
                      QStringLiteral("reported SVG contains duplicate screen pixels"));
            }
        }
    }

    const QSize imageSize(1341, 694);
    QImage sourceImage(imageSize, QImage::Format_ARGB32_Premultiplied);
    QImage extractedImage(imageSize, QImage::Format_ARGB32_Premultiplied);
    sourceImage.fill(Qt::white);
    extractedImage.fill(Qt::white);

    QSvgRenderer renderer(svgPath);
    check(renderer.isValid(), QStringLiteral("Qt SVG renderer rejected the reported SVG"));
    {
        QPainter painter(&sourceImage);
        painter.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&painter, QRectF(QPointF(0.0, 0.0), QSizeF(imageSize)));
    }

    {
        QPainter painter(&extractedImage);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath shape = polylinesToPainterPath(
            extractor.polylines(), content, QRectF(24.0, 25.0, 1291.0, 644.0));
        shape.setFillRule(extractor.fillRule());
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        painter.drawPath(shape);
    }

    const double similarity = maskIntersectionOverUnion(sourceImage, extractedImage);
    check(similarity > 0.98,
          QStringLiteral("extracted preview differs from the SVG (mask IoU %1)")
              .arg(similarity, 0, 'f', 4));

    const QString outputDir = qEnvironmentVariable("AUTOGRAFO_TEST_IMAGE_DIR");
    if (!outputDir.isEmpty()) {
        QDir().mkpath(outputDir);
        sourceImage.save(QDir(outputDir).filePath(QStringLiteral("svg-source.png")));
        extractedImage.save(QDir(outputDir).filePath(QStringLiteral("svg-extracted.png")));
    }
}

void renderDiagnosticSvg(const QString &svgPath, const QString &outputDir)
{
    SvgPathExtractor extractor;
    check(extractor.loadFromFile(svgPath),
          QStringLiteral("failed to extract diagnostic SVG: %1").arg(svgPath));

    QSvgRenderer renderer(svgPath);
    check(renderer.isValid(),
          QStringLiteral("Qt SVG renderer rejected diagnostic SVG: %1").arg(svgPath));
    if (extractor.isEmpty() || !renderer.isValid())
        return;

    const QSize imageSize(900, 900);
    QImage sourceImage(imageSize, QImage::Format_ARGB32_Premultiplied);
    QImage extractedImage(imageSize, QImage::Format_ARGB32_Premultiplied);
    sourceImage.fill(Qt::white);
    extractedImage.fill(QColor(30, 30, 30));

    {
        QPainter painter(&sourceImage);
        painter.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&painter, QRectF(QPointF(0.0, 0.0), QSizeF(imageSize)));
    }

    {
        QPainter painter(&extractedImage);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF content = artPolylineBounds(extractor.polylines());
        const QRectF target(20.0, 20.0, 860.0, 860.0);
        const QRectF fitRect = fitRectForContent(content, target);
        QPainterPath shape = polylinesToPainterPath(extractor.polylines(), content, fitRect);
        shape.setFillRule(extractor.fillRule());
        painter.setPen(QPen(QColor(80, 80, 80), 1.0));
        painter.setBrush(QColor(220, 220, 220));
        painter.drawPath(shape);
    }

    QDir().mkpath(outputDir);
    sourceImage.save(QDir(outputDir).filePath(QStringLiteral("diagnostic-source.png")));
    extractedImage.save(QDir(outputDir).filePath(QStringLiteral("diagnostic-extracted.png")));
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testPolylineBounds();
    testScalingOnSecondaryMonitor();
    testScalingInsideAndroidArea();
    testComplexSvgParsing();
    testNestedTransformOrder();
    testReportedSvg();

    const QString diagnosticSvg = qEnvironmentVariable("AUTOGRAFO_DIAGNOSTIC_SVG");
    const QString diagnosticOutput = qEnvironmentVariable("AUTOGRAFO_TEST_IMAGE_DIR");
    if (!diagnosticSvg.isEmpty() && !diagnosticOutput.isEmpty())
        renderDiagnosticSvg(diagnosticSvg, diagnosticOutput);

    if (failures == 0)
        std::cout << "All geometry tests passed.\n";
    return failures == 0 ? 0 : 1;
}
