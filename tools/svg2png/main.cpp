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

#include <QBuffer>
#include <QColor>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QTextStream>

#include <cstdio>

namespace {

void logError(const QString &message)
{
    const QByteArray bytes = message.toLocal8Bit();
    std::fprintf(stderr, "%s\n", bytes.constData());
}

QColor parseBackground(const QString &token, bool *ok)
{
    *ok = true;
    if (token.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0)
        return Qt::transparent;

    const QColor color(token);
    if (!color.isValid()) {
        *ok = false;
        return {};
    }
    return color;
}

QImage rasterize(QSvgRenderer &renderer, int width, int height, const QColor &background, qreal fit)
{
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(background);

    QRectF target(0, 0, width, height);
    if (fit > 0.0 && fit < 1.0) {
        const qreal padX = width * (1.0 - fit) * 0.5;
        const qreal padY = height * (1.0 - fit) * 0.5;
        target = QRectF(padX, padY, width * fit, height * fit);
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter, target);
    painter.end();
    return image;
}

bool ensureParentDir(const QString &path)
{
    const QFileInfo info(path);
    if (info.dir().exists())
        return true;
    return QDir().mkpath(info.absolutePath());
}

bool writePng(const QImage &image, const QString &path)
{
    if (!ensureParentDir(path)) {
        logError(QStringLiteral("Could not create directory for %1").arg(path));
        return false;
    }
    if (!image.save(path, "PNG")) {
        logError(QStringLiteral("Could not write PNG %1").arg(path));
        return false;
    }
    return true;
}

bool writeIco(const QList<QImage> &images, const QString &path)
{
    if (images.isEmpty()) {
        logError(QStringLiteral("ICO %1 has no images").arg(path));
        return false;
    }
    if (!ensureParentDir(path)) {
        logError(QStringLiteral("Could not create directory for %1").arg(path));
        return false;
    }

    QList<QByteArray> pngs;
    pngs.reserve(images.size());
    for (const QImage &image : images) {
        QByteArray png;
        QBuffer buffer(&png);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
            logError(QStringLiteral("Could not encode PNG frame for %1").arg(path));
            return false;
        }
        pngs.append(png);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        logError(QStringLiteral("Could not write ICO %1").arg(path));
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint16(0) << quint16(1) << quint16(images.size());

    quint32 offset = 6 + 16 * quint32(images.size());
    for (int i = 0; i < images.size(); ++i) {
        const int width = images[i].width();
        const int height = images[i].height();
        stream << quint8(width >= 256 ? 0 : width);
        stream << quint8(height >= 256 ? 0 : height);
        stream << quint8(0) << quint8(0);
        stream << quint16(1) << quint16(32);
        stream << quint32(pngs[i].size());
        stream << quint32(offset);
        offset += quint32(pngs[i].size());
    }

    for (const QByteArray &png : pngs)
        stream.writeRawData(png.constData(), png.size());

    return stream.status() == QDataStream::Ok;
}

bool processSpec(const QString &specPath)
{
    QFile specFile(specPath);
    if (!specFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logError(QStringLiteral("Could not read spec %1").arg(specPath));
        return false;
    }

    QSvgRenderer renderer;
    bool haveSvg = false;
    QTextStream input(&specFile);
    int lineNumber = 0;

    while (!input.atEnd()) {
        const QString line = input.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        const QStringList parts = line.split(QLatin1Char('|'));
        if (parts.isEmpty())
            continue;

        const QString command = parts.first().toLower();
        if (command == QLatin1String("svg")) {
            if (parts.size() != 2) {
                logError(QStringLiteral("%1:%2: svg|<path>").arg(specPath).arg(lineNumber));
                return false;
            }
            if (!renderer.load(parts.at(1)) || !renderer.isValid()) {
                logError(QStringLiteral("Invalid SVG %1").arg(parts.at(1)));
                return false;
            }
            haveSvg = true;
            continue;
        }

        if (!haveSvg) {
            logError(QStringLiteral("%1:%2: svg path must be set first").arg(specPath).arg(lineNumber));
            return false;
        }

        if (command == QLatin1String("png")) {
            if (parts.size() != 6) {
                logError(QStringLiteral("%1:%2: png|w|h|background|fit|path")
                             .arg(specPath)
                             .arg(lineNumber));
                return false;
            }
            bool ok = false;
            const int width = parts.at(1).toInt(&ok);
            const int height = ok ? parts.at(2).toInt(&ok) : 0;
            const QColor background = parseBackground(parts.at(3), &ok);
            const qreal fit = ok ? parts.at(4).toDouble(&ok) : 0.0;
            if (!ok || width <= 0 || height <= 0 || fit <= 0.0 || fit > 1.0) {
                logError(QStringLiteral("%1:%2: invalid png parameters").arg(specPath).arg(lineNumber));
                return false;
            }
            if (!writePng(rasterize(renderer, width, height, background, fit), parts.at(5)))
                return false;
            continue;
        }

        if (command == QLatin1String("ico")) {
            if (parts.size() != 5) {
                logError(QStringLiteral("%1:%2: ico|sizes|background|fit|path")
                             .arg(specPath)
                             .arg(lineNumber));
                return false;
            }
            bool ok = false;
            const QColor background = parseBackground(parts.at(2), &ok);
            const qreal fit = ok ? parts.at(3).toDouble(&ok) : 0.0;
            if (!ok || fit <= 0.0 || fit > 1.0) {
                logError(QStringLiteral("%1:%2: invalid ico parameters").arg(specPath).arg(lineNumber));
                return false;
            }

            QList<QImage> frames;
            const QStringList sizes = parts.at(1).split(QLatin1Char(','));
            for (const QString &sizeToken : sizes) {
                const int size = sizeToken.toInt(&ok);
                if (!ok || size <= 0) {
                    logError(QStringLiteral("%1:%2: invalid ico size %3")
                                 .arg(specPath)
                                 .arg(lineNumber)
                                 .arg(sizeToken));
                    return false;
                }
                frames.append(rasterize(renderer, size, size, background, fit));
            }
            if (!writeIco(frames, parts.at(4)))
                return false;
            continue;
        }

        logError(QStringLiteral("%1:%2: unknown command %3").arg(specPath).arg(lineNumber).arg(command));
        return false;
    }

    return haveSvg;
}

} // namespace

int main(int argc, char *argv[])
{
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() != 3 || args.at(1) != QLatin1String("--spec")) {
        logError(QStringLiteral("Usage: autografo_svg2png --spec <file>"));
        return 1;
    }

    return processSpec(args.at(2)) ? 0 : 1;
}
