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

#include "AndroidGestureDrawer.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QPointer>
#include <QtMath>

#include <jni.h>

namespace {

constexpr auto BridgeClass = "com/autografo/android/AutografoBridge";
QPointer<AndroidGestureDrawer> activeDrawer;

template<typename Function>
void withDrawer(Function function)
{
    if (!activeDrawer)
        return;
    QMetaObject::invokeMethod(activeDrawer, [function]() {
        if (activeDrawer)
            function(activeDrawer.data());
    }, Qt::AutoConnection);
}

} // namespace

AndroidGestureDrawer::AndroidGestureDrawer(QObject *parent)
    : DrawingBackend(parent)
{
    activeDrawer = this;
}

AndroidGestureDrawer::~AndroidGestureDrawer()
{
    if (activeDrawer == this)
        activeDrawer.clear();
}

void AndroidGestureDrawer::setPolylines(const ArtPolylineSet &polylines)
{
    m_polylines = polylines;
    syncConfiguration();
}

void AndroidGestureDrawer::setSpeed(int pixelsPerSecond)
{
    m_pixelsPerSecond = qMax(1, pixelsPerSecond);
}

void AndroidGestureDrawer::setPauseBetweenPaths(int pauseMs)
{
    m_pauseBetweenPathsMs = qMax(0, pauseMs);
}

bool AndroidGestureDrawer::isBackendReady() const
{
    return QJniObject::callStaticMethod<jboolean>(BridgeClass,
                                                  "isServiceReady",
                                                  "()Z");
}

void AndroidGestureDrawer::requestEnableBackend()
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject::callStaticMethod<void>(BridgeClass, "openAccessibilitySettings",
                                       "(Landroid/content/Context;)V",
                                       context.object<jobject>());
}

void AndroidGestureDrawer::clearSelectedArea()
{
    QJniObject::callStaticMethod<void>(BridgeClass, "clearSelectedArea", "()V");
}

bool AndroidGestureDrawer::requestAreaSelection()
{
    if (!isBackendReady()) {
        requestEnableBackend();
        emit error(tr("Enable the Autograph accessibility service and try again."));
        return false;
    }

    QJniObject::callStaticMethod<void>(BridgeClass, "beginAreaSelection", "()V");
    return true;
}

bool AndroidGestureDrawer::requestAreaSelection(const ArtPolylineSet &polylines,
                                                int pixelsPerSecond,
                                                int pauseBetweenPathsMs)
{
    m_polylines = polylines;
    m_pixelsPerSecond = qMax(1, pixelsPerSecond);
    m_pauseBetweenPathsMs = qMax(0, pauseBetweenPathsMs);
    syncConfiguration(true);
    return requestAreaSelection();
}

bool AndroidGestureDrawer::requestReadyToDraw(const ArtPolylineSet &polylines,
                                             int pixelsPerSecond,
                                             int pauseBetweenPathsMs,
                                             const QRect &area)
{
    if (!area.isValid())
        return false;

    m_polylines = polylines;
    m_pixelsPerSecond = qMax(1, pixelsPerSecond);
    m_pauseBetweenPathsMs = qMax(0, pauseBetweenPathsMs);
    syncConfiguration(true);

    if (!isBackendReady()) {
        requestEnableBackend();
        emit error(tr("Enable the Autograph accessibility service and try again."));
        return false;
    }

    QJniObject::callStaticMethod<void>(BridgeClass, "beginReadyToDraw",
                                       "(IIII)V",
                                       static_cast<jint>(area.x()),
                                       static_cast<jint>(area.y()),
                                       static_cast<jint>(area.width()),
                                       static_cast<jint>(area.height()));
    return true;
}

void AndroidGestureDrawer::start()
{
    if (m_drawing || m_polylines.isEmpty())
        return;

    if (!isBackendReady()) {
        emit error(tr("The accessibility service was disconnected."));
        return;
    }

    syncConfiguration();
    m_drawing = true;
    QJniObject::callStaticMethod<void>(BridgeClass, "startDrawing", "()V");
}

void AndroidGestureDrawer::stop()
{
    if (!m_drawing)
        return;
    QJniObject::callStaticMethod<void>(BridgeClass, "stopDrawing", "()V");
}

void AndroidGestureDrawer::syncConfiguration(bool normalizedCoordinates)
{
    if (m_polylines.isEmpty())
        return;

    QVector<jfloat> points;
    QVector<jint> offsets;
    offsets.reserve(m_polylines.size() + 1);
    offsets.append(0);
    for (const ArtPolyline &polyline : m_polylines) {
        for (const QPointF &point : polyline) {
            points.append(static_cast<jfloat>(point.x()));
            points.append(static_cast<jfloat>(point.y()));
        }
        offsets.append(points.size() / 2);
    }

    QJniEnvironment env;
    jfloatArray javaPoints = env->NewFloatArray(points.size());
    jintArray javaOffsets = env->NewIntArray(offsets.size());
    if (!javaPoints || !javaOffsets) {
        emit error(tr("Not enough memory to prepare Android gestures."));
        return;
    }

    env->SetFloatArrayRegion(javaPoints, 0, points.size(), points.constData());
    env->SetIntArrayRegion(javaOffsets, 0, offsets.size(), offsets.constData());
    QJniObject::callStaticMethod<void>(BridgeClass, "configureDrawing", "([F[IIIZ)V",
                                       javaPoints, javaOffsets,
                                       static_cast<jint>(m_pixelsPerSecond),
                                       static_cast<jint>(m_pauseBetweenPathsMs),
                                       static_cast<jboolean>(normalizedCoordinates));
    env->DeleteLocalRef(javaPoints);
    env->DeleteLocalRef(javaOffsets);
    if (env.checkAndClearExceptions())
        emit error(tr("Failed to transfer the drawing to the Android service."));
}

void AndroidGestureDrawer::handleAreaSelected(const QRect &area) { emit areaSelected(area); }
void AndroidGestureDrawer::handleSelectionCancelled() { emit selectionCancelled(); }
void AndroidGestureDrawer::handleReturnedToApp() { emit returnedToApp(); }
void AndroidGestureDrawer::handleProgress(int currentPath, int totalPaths,
                                          int currentPoint, int totalPoints)
{
    emit progressChanged(currentPath, totalPaths, currentPoint, totalPoints);
}

void AndroidGestureDrawer::handleFinished()
{
    m_drawing = false;
    emit finished();
}

void AndroidGestureDrawer::handleCancelled()
{
    m_drawing = false;
    emit cancelled();
}

void AndroidGestureDrawer::handleError(const QString &message)
{
    m_drawing = false;
    emit error(message);
}

void androidAreaSelected(int x, int y, int width, int height)
{
    withDrawer([=](AndroidGestureDrawer *drawer) {
        drawer->handleAreaSelected(QRect(x, y, width, height));
    });
}

void androidSelectionCancelled()
{
    withDrawer([](AndroidGestureDrawer *drawer) { drawer->handleSelectionCancelled(); });
}

void androidReturnedToApp()
{
    withDrawer([](AndroidGestureDrawer *drawer) { drawer->handleReturnedToApp(); });
}

void androidDrawingProgress(int currentPath, int totalPaths, int currentPoint, int totalPoints)
{
    withDrawer([=](AndroidGestureDrawer *drawer) {
        drawer->handleProgress(currentPath, totalPaths, currentPoint, totalPoints);
    });
}

void androidDrawingFinished()
{
    withDrawer([](AndroidGestureDrawer *drawer) { drawer->handleFinished(); });
}

void androidDrawingCancelled()
{
    withDrawer([](AndroidGestureDrawer *drawer) { drawer->handleCancelled(); });
}

void androidDrawingError(const QString &message)
{
    withDrawer([=](AndroidGestureDrawer *drawer) { drawer->handleError(message); });
}

extern "C" {

JNIEXPORT void JNICALL
Java_com_autografo_android_AutografoBridge_nativeAreaSelected(
    JNIEnv *, jclass, jint x, jint y, jint width, jint height)
{
    androidAreaSelected(x, y, width, height);
}

JNIEXPORT void JNICALL
Java_com_autografo_android_AutografoBridge_nativeSelectionCancelled(JNIEnv *, jclass)
{
    androidSelectionCancelled();
}

JNIEXPORT void JNICALL
Java_com_autografo_android_AutografoBridge_nativeReturnedToApp(JNIEnv *, jclass)
{
    androidReturnedToApp();
}

JNIEXPORT void JNICALL
Java_com_autografo_android_AutografoBridge_nativeDrawingProgress(
    JNIEnv *, jclass, jint currentPath, jint totalPaths, jint currentPoint, jint totalPoints)
{
    androidDrawingProgress(currentPath, totalPaths, currentPoint, totalPoints);
}

JNIEXPORT void JNICALL
Java_com_autografo_android_AutografoBridge_nativeDrawingFinished(JNIEnv *, jclass)
{
    androidDrawingFinished();
}

JNIEXPORT void JNICALL
Java_com_autografo_android_AutografoBridge_nativeDrawingCancelled(JNIEnv *, jclass)
{
    androidDrawingCancelled();
}

JNIEXPORT void JNICALL
Java_com_autografo_android_AutografoBridge_nativeDrawingError(
    JNIEnv *env, jclass, jstring message)
{
    androidDrawingError(QJniObject(message).toString());
}

} // extern "C"
