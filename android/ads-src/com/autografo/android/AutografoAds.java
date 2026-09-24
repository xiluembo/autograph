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

import android.app.Activity;
import android.graphics.Insets;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.widget.FrameLayout;

import com.google.android.gms.ads.AdListener;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.AdSize;
import com.google.android.gms.ads.AdView;
import com.google.android.gms.ads.LoadAdError;
import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.ads.RequestConfiguration;

import java.util.ArrayList;
import java.util.List;

public final class AutografoAds {
    private static final int BANNER_BACKGROUND = 0xFF1E1E1E;

    private static boolean sInitialized;
    private static boolean sInitializing;
    private static boolean sWantVisible = true;
    private static boolean sAdLoaded;
    private static View sRoot;
    private static FrameLayout sAdContainer;
    private static AdView sAdView;
    private static Activity sActivity;
    /// Latest navigation-bar bottom inset, in physical pixels. Zero in
    /// landscape on devices where the bar sits on the side.
    private static int sNavBottomPx;

    private AutografoAds() {}

    public static void initialize(Activity activity) {
        sActivity = activity;
        if (sInitialized || sInitializing)
            return;
        sInitializing = true;
        applyTestDeviceConfiguration(activity);
        MobileAds.initialize(activity, status -> {
            sInitialized = true;
            sInitializing = false;
            loadBanner(activity);
        });
    }

    /**
     * Registers the configured development devices as AdMob test devices.
     * Test devices always receive test ads (guaranteed fill) and their traffic
     * is not counted as invalid by AdMob.
     */
    private static void applyTestDeviceConfiguration(Activity activity) {
        final String raw = activity.getString(R.string.admob_test_device_ids).trim();
        if (raw.isEmpty())
            return;
        final List<String> ids = new ArrayList<>();
        for (String id : raw.split(","))
            if (!id.trim().isEmpty())
                ids.add(id.trim());
        if (ids.isEmpty())
            return;
        MobileAds.setRequestConfiguration(new RequestConfiguration.Builder()
                .setTestDeviceIds(ids)
                .build());
    }

    public static View wrapContentView(Activity activity, View qtView) {
        sActivity = activity;
        if (qtView.getParent() instanceof ViewGroup)
            ((ViewGroup) qtView.getParent()).removeView(qtView);

        // Keep the Qt surface at the full size Qt assumes for its window
        // (shrinking a parent container is only picked up by Qt after a
        // configuration change, which clipped the bottom UI at startup).
        // The banner overlays the bottom strip instead, and the Qt side
        // reserves that strip via bottomReservePx().
        FrameLayout root = new FrameLayout(activity);
        sRoot = root;
        root.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));

        root.addView(qtView, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));

        sAdContainer = new FrameLayout(activity);
        sAdContainer.setBackgroundColor(BANNER_BACKGROUND);
        final int bannerHeight = Math.max(1, AdSize.BANNER.getHeightInPixels(activity));
        sAdContainer.setMinimumHeight(bannerHeight);
        // Collapsed until an ad is actually loaded; the Qt side is notified
        // whenever the reserved space changes.
        sAdContainer.setVisibility(View.GONE);
        root.addView(sAdContainer, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM));

        root.setOnApplyWindowInsetsListener((v, insets) -> applyNavigationInsets(insets));
        requestInsetsWhenAttached(root);

        initialize(activity);
        loadBanner(activity);
        applyVisibility();
        return root;
    }

    /**
     * Height in pixels of the bottom strip (banner plus navigation padding)
     * that the Qt UI must keep clear. Called from C++ via JNI.
     */
    public static int bottomReservePx() {
        // Qt Widgets already grow contents margins by the bottom safe area
        // (the gesture/nav bar). The ad strip's nav padding lives in that
        // same band, so only the banner itself overlaps the Qt content.
        // Adding nav.bottom here reserved that band twice in portrait and
        // left a white gap the height of the ad. Landscape is unaffected
        // because nav.bottom is already 0 there.
        Activity activity = sActivity;
        if (activity == null || !sWantVisible || !sAdLoaded)
            return 0;
        return Math.max(1, AdSize.BANNER.getHeightInPixels(activity));
    }

    public static void setBannerVisible(boolean visible) {
        sWantVisible = visible;
        runOnUi(AutografoAds::applyVisibility);
    }

    public static void reload(Activity activity) {
        sActivity = activity;
        runOnUi(() -> loadBanner(activity));
    }

    public static void pause() {
        runOnUi(() -> {
            if (sAdView != null)
                sAdView.pause();
        });
    }

    public static void resume() {
        runOnUi(() -> {
            if (sAdView != null)
                sAdView.resume();
        });
    }

    public static void destroy() {
        runOnUi(() -> {
            if (sAdView != null) {
                sAdView.destroy();
                sAdView = null;
            }
            sAdLoaded = false;
            applyVisibility();
        });
    }

    private static void loadBanner(Activity activity) {
        if (activity == null || sAdContainer == null || !sInitialized)
            return;

        if (sAdView != null) {
            sAdContainer.removeView(sAdView);
            sAdView.destroy();
            sAdView = null;
        }
        sAdLoaded = false;

        AdView adView = new AdView(activity);
        adView.setAdUnitId(activity.getString(R.string.admob_banner_unit_id));
        adView.setAdSize(AdSize.BANNER);
        adView.setAdListener(new AdListener() {
            @Override
            public void onAdLoaded() {
                sAdLoaded = true;
                applyVisibility();
            }

            @Override
            public void onAdFailedToLoad(LoadAdError error) {
                sAdLoaded = false;
                applyVisibility();
            }
        });

        sAdContainer.addView(adView, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                Math.max(1, AdSize.BANNER.getHeightInPixels(activity)),
                Gravity.CENTER_HORIZONTAL | Gravity.TOP));
        sAdView = adView;
        adView.loadAd(new AdRequest.Builder().build());
        applyVisibility();
    }

    private static void applyVisibility() {
        if (sAdContainer == null)
            return;
        // Show the strip only while a loaded ad can actually be displayed;
        // otherwise collapse it so the Qt UI reclaims the space.
        final boolean showAd = sWantVisible && sAdLoaded && sAdView != null;
        sAdContainer.setVisibility(showAd ? View.VISIBLE : View.GONE);
        if (sAdView != null)
            sAdView.setVisibility(showAd ? View.VISIBLE : View.INVISIBLE);
        if (sRoot != null)
            requestInsetsWhenAttached(sRoot);
        notifyReserveChanged();
    }

    /** Tells the Qt side that bottomReservePx() may have changed. */
    private static void notifyReserveChanged() {
        try {
            nativeAdsReserveChanged();
        } catch (UnsatisfiedLinkError ignored) {
            // Native library not loaded yet; the Qt side reads the reserve on startup anyway.
        }
    }

    private static native void nativeAdsReserveChanged();

    private static WindowInsets applyNavigationInsets(WindowInsets insets) {
        if (sAdContainer == null)
            return insets;

        Insets nav = navigationInsets(insets);
        // Padding grows WRAP_CONTENT height; do not put nav insets into a fixed-height box.
        sAdContainer.setPadding(nav.left, 0, nav.right, nav.bottom);
        if (sNavBottomPx != nav.bottom) {
            sNavBottomPx = nav.bottom;
            notifyReserveChanged();
        }
        // Do not consume anything: the Qt view handles its own insets and must
        // see the originals, since it now keeps the full window size.
        return insets;
    }

    private static Insets navigationInsets(WindowInsets insets) {
        Insets nav = insets.getInsets(WindowInsets.Type.navigationBars());
        if (nav.bottom == 0 && nav.left == 0 && nav.right == 0)
            nav = insets.getInsetsIgnoringVisibility(WindowInsets.Type.navigationBars());

        if ((nav.bottom == 0 && nav.left == 0 && nav.right == 0) && sActivity != null) {
            final WindowInsets rootInsets = sActivity.getWindow().getDecorView().getRootWindowInsets();
            if (rootInsets != null)
                nav = rootInsets.getInsetsIgnoringVisibility(WindowInsets.Type.navigationBars());
        }

        // Trust the real insets. Do not fall back to the navigation_bar_height
        // resource: it reports the bottom bar height even when the bar actually
        // sits on the side (landscape), creating a dead strip under the ad.
        return nav;
    }

    private static void requestInsetsWhenAttached(View view) {
        if (view.isAttachedToWindow()) {
            view.requestApplyInsets();
            return;
        }
        view.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View v) {
                v.requestApplyInsets();
                v.removeOnAttachStateChangeListener(this);
            }

            @Override
            public void onViewDetachedFromWindow(View v) {}
        });
    }

    private static void runOnUi(Runnable runnable) {
        if (sActivity == null) {
            runnable.run();
            return;
        }
        sActivity.runOnUiThread(runnable);
    }
}
