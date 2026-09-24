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

package com.autografo.android;

import br.pushx.autograph.R;

import android.content.Context;
import android.content.Intent;
import android.app.Activity;
import android.provider.Settings;

public final class AutografoBridge {
    private AutografoBridge() {}

    public static boolean isServiceReady() {
        return AutografoAccessibilityService.instance() != null;
    }

    public static void openAccessibilitySettings(Context context) {
        Intent intent = new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }

    public static void beginAreaSelection() {
        AutografoAccessibilityService service = AutografoAccessibilityService.instance();
        if (service != null)
            service.beginAreaSelection();
    }

    public static void beginReadyToDraw(int x, int y, int width, int height) {
        AutografoAccessibilityService service = AutografoAccessibilityService.instance();
        if (service != null)
            service.beginReadyToDraw(x, y, width, height);
    }

    public static void clearSelectedArea() {
        AutografoAccessibilityService service = AutografoAccessibilityService.instance();
        if (service != null)
            service.clearSelectedArea();
    }

    /** Send Autograph behind other apps so the accessibility overlay is usable. */
    public static void moveTaskToBackground() {
        final Activity activity = AutografoActivity.currentActivity();
        if (activity == null)
            return;
        activity.runOnUiThread(() -> activity.moveTaskToBack(true));
    }

    /** Bring the main activity to the foreground after overlay workflows. */
    public static void bringAppToForeground() {
        Context context = AutografoActivity.applicationContext();
        if (context == null)
            return;
        Intent intent = new Intent(context, AutografoActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT
                | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        context.startActivity(intent);
    }

    public static void configureDrawing(float[] points, int[] offsets,
                                        int pixelsPerSecond, int pauseBetweenPathsMs,
                                        boolean normalizedCoordinates) {
        AutografoAccessibilityService service = AutografoAccessibilityService.instance();
        if (service != null)
            service.configureDrawing(points, offsets, pixelsPerSecond,
                                     pauseBetweenPathsMs, normalizedCoordinates);
    }

    public static void startDrawing() {
        AutografoAccessibilityService service = AutografoAccessibilityService.instance();
        if (service != null)
            service.startDrawing();
        else
            reportError(localized(R.string.accessibility_disconnected));
    }

    public static void stopDrawing() {
        AutografoAccessibilityService service = AutografoAccessibilityService.instance();
        if (service != null)
            service.stopDrawing();
    }

    static void reportAreaSelected(int x, int y, int width, int height) {
        try { nativeAreaSelected(x, y, width, height); } catch (UnsatisfiedLinkError ignored) {}
    }

    static void reportSelectionCancelled() {
        bringAppToForeground();
        try { nativeSelectionCancelled(); } catch (UnsatisfiedLinkError ignored) {}
    }

    static void reportReturnedToApp() {
        bringAppToForeground();
        try { nativeReturnedToApp(); } catch (UnsatisfiedLinkError ignored) {}
    }

    static void reportProgress(int currentPath, int totalPaths,
                               int currentPoint, int totalPoints) {
        try { nativeDrawingProgress(currentPath, totalPaths, currentPoint, totalPoints); }
        catch (UnsatisfiedLinkError ignored) {}
    }

    static void reportFinished() {
        try { nativeDrawingFinished(); } catch (UnsatisfiedLinkError ignored) {}
    }

    static void reportCancelled() {
        try { nativeDrawingCancelled(); } catch (UnsatisfiedLinkError ignored) {}
    }

    static void reportError(String message) {
        bringAppToForeground();
        try { nativeDrawingError(message); } catch (UnsatisfiedLinkError ignored) {}
    }

    private static String localized(int resId) {
        Context context = AutografoAccessibilityService.instance();
        if (context == null)
            context = AutografoActivity.applicationContext();
        return context != null ? context.getString(resId)
                               : "The accessibility service was disconnected.";
    }

    private static native void nativeAreaSelected(int x, int y, int width, int height);
    private static native void nativeSelectionCancelled();
    private static native void nativeReturnedToApp();
    private static native void nativeDrawingProgress(int currentPath, int totalPaths,
                                                     int currentPoint, int totalPoints);
    private static native void nativeDrawingFinished();
    private static native void nativeDrawingCancelled();
    private static native void nativeDrawingError(String message);
}
