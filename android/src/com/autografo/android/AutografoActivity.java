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

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsetsController;

import org.qtproject.qt.android.bindings.QtActivity;

public class AutografoActivity extends QtActivity {
    private static Context appContext;
    private static AutografoActivity current;

    public static Context applicationContext() {
        return appContext;
    }

    public static Activity currentActivity() {
        return current;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        appContext = getApplicationContext();
        current = this;
        super.onCreate(savedInstanceState);
        applyLightStatusBar();
        final View decor = getWindow() != null ? getWindow().getDecorView() : null;
        if (decor != null)
            decor.post(this::applyLightStatusBar);
        invokeAds("initialize", Activity.class, this);
        notifyLocaleChanged();
    }

    @Override
    public void onPause() {
        invokeAds("pause");
        super.onPause();
    }

    @Override
    public void onResume() {
        current = this;
        super.onResume();
        applyLightStatusBar();
        invokeAds("resume");
        notifyLocaleChanged();
    }

    @Override
    public void onDestroy() {
        if (current == this)
            current = null;
        invokeAds("destroy");
        super.onDestroy();
    }

    /**
     * Soft back should background the task (overlay workflows stay alive) instead
     * of finishing the activity and killing the session behind a white screen.
     */
    @Override
    @SuppressWarnings("deprecation")
    public void onBackPressed() {
        if (!moveTaskToBack(true))
            super.onBackPressed();
    }

    @Override
    public void setContentView(View view) {
        super.setContentView(wrap(view));
        applyLightStatusBar();
    }

    @Override
    public void setContentView(View view, ViewGroup.LayoutParams params) {
        super.setContentView(wrap(view), params);
        applyLightStatusBar();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        applyLightStatusBar();
        invokeAds("reload", Activity.class, this);
        notifyLocaleChanged();
    }

    private void notifyLocaleChanged() {
        try {
            nativeLocaleChanged();
        } catch (UnsatisfiedLinkError ignored) {
        }
    }

    private static native void nativeLocaleChanged();

    /**
     * Qt for Android often overrides theme status-bar flags. Force a light
     * status bar (dark system icons) so icons stay readable on white UI.
     */
    private void applyLightStatusBar() {
        final Window window = getWindow();
        if (window == null)
            return;

        window.setStatusBarColor(Color.WHITE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            final WindowInsetsController controller = window.getInsetsController();
            if (controller != null) {
                controller.setSystemBarsAppearance(
                        WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS,
                        WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS);
            }
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            final View decor = window.getDecorView();
            int flags = decor.getSystemUiVisibility();
            flags |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
            decor.setSystemUiVisibility(flags);
        }
    }

    private View wrap(View view) {
        Object wrapped = invokeAds("wrapContentView",
                                   new Class<?>[] {Activity.class, View.class},
                                   new Object[] {this, view});
        return wrapped instanceof View ? (View) wrapped : view;
    }

    private Object invokeAds(String method) {
        return invokeAds(method, new Class<?>[] {}, new Object[] {});
    }

    private Object invokeAds(String method, Class<?> argumentType, Object argument) {
        return invokeAds(method, new Class<?>[] {argumentType}, new Object[] {argument});
    }

    private Object invokeAds(String method, Class<?>[] argumentTypes, Object[] arguments) {
        try {
            return Class.forName("com.autografo.android.AutografoAds")
                    .getMethod(method, argumentTypes)
                    .invoke(null, arguments);
        } catch (Throwable ignored) {
            return null;
        }
    }
}
