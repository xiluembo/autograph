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

import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.accessibilityservice.GestureDescription;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.List;

public final class AutografoAccessibilityService extends AccessibilityService {
    private static volatile AutografoAccessibilityService currentInstance;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private static final String PREFS_NAME = "autografo_overlay";
    private static final String PREF_CONTROLLER_X = "controller_x";
    private static final String PREF_CONTROLLER_Y = "controller_y";
    private static final int CONTROLLER_WIDTH_DP = 200;
    private static final int UNSET_POSITION = Integer.MIN_VALUE;

    private WindowManager windowManager;
    private View overlay;
    private LinearLayout controller;
    private TextView progressLabel;
    private WindowManager.LayoutParams controllerParams;
    private View.OnTouchListener controllerDragListener;
    private int controllerX = UNSET_POSITION;
    private int controllerY = UNSET_POSITION;

    private float[] points = new float[0];
    private int[] offsets = new int[0];
    private int pixelsPerSecond = 300;
    private int pauseBetweenPathsMs = 20;
    private boolean coordinatesAreNormalized;
    private Rect selectedArea;
    private int selectedOrientation = Configuration.ORIENTATION_UNDEFINED;

    private final List<StrokeChunk> chunks = new ArrayList<>();
    private int chunkIndex;
    private boolean drawing;
    private boolean stopping;
    private GestureDescription.StrokeDescription activeStroke;

    public static AutografoAccessibilityService instance() {
        return currentInstance;
    }

    @Override
    protected void onServiceConnected() {
        super.onServiceConnected();
        currentInstance = this;
        windowManager = (WindowManager) getSystemService(WINDOW_SERVICE);
        AccessibilityServiceInfo info = getServiceInfo();
        if (info != null) {
            info.flags |= AccessibilityServiceInfo.FLAG_REQUEST_FILTER_KEY_EVENTS;
            setServiceInfo(info);
        }
    }

    @Override
    public boolean onKeyEvent(KeyEvent event) {
        if (!drawing)
            return false;
        int code = event.getKeyCode();
        if (code != KeyEvent.KEYCODE_VOLUME_DOWN && code != KeyEvent.KEYCODE_VOLUME_UP)
            return false;
        if (event.getAction() == KeyEvent.ACTION_DOWN)
            stopDrawingInternal(true);
        return true;
    }

    @Override
    public void onAccessibilityEvent(AccessibilityEvent event) {
        // The service does not inspect the hierarchy or content of the target app.
    }

    @Override
    public void onInterrupt() {
        stopWithResult(false, getString(R.string.accessibility_interrupted));
    }

    @Override
    public void onDestroy() {
        removeAllOverlays();
        handler.removeCallbacksAndMessages(null);
        if (drawing)
            AutografoBridge.reportCancelled();
        drawing = false;
        if (currentInstance == this)
            currentInstance = null;
        super.onDestroy();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (selectedArea != null && selectedOrientation != Configuration.ORIENTATION_UNDEFINED
                && newConfig.orientation != selectedOrientation) {
            stopWithResult(false, getString(R.string.orientation_changed));
            selectedArea = null;
        }
        if (controller != null && controllerParams != null) {
            clampControllerPosition(controllerParams, controller);
            controllerX = controllerParams.x;
            controllerY = controllerParams.y;
            persistControllerPosition();
            try {
                windowManager.updateViewLayout(controller, controllerParams);
            } catch (RuntimeException ignored) {}
        }
    }

    public void beginAreaSelection() {
        handler.post(() -> {
            stopDrawingInternal(false);
            selectedArea = null;
            showTargetController();
        });
    }

    public void beginReadyToDraw(int x, int y, int width, int height) {
        handler.post(() -> {
            stopDrawingInternal(false);
            Rect area = new Rect(x, y, x + width, y + height);
            if (area.width() <= 5 || area.height() <= 5) {
                AutografoBridge.reportError(getString(R.string.select_area_again));
                return;
            }
            selectedArea = new Rect(area);
            selectedOrientation = getResources().getConfiguration().orientation;
            if (!scaleConfiguredDrawingToArea(area)) {
                selectedArea = null;
                notifyUser(getString(R.string.could_not_fit_drawing));
                AutografoBridge.reportError(getString(R.string.could_not_fit_drawing));
                return;
            }
            showReadyController();
        });
    }

    public void clearSelectedArea() {
        handler.post(() -> {
            stopDrawingInternal(false);
            selectedArea = null;
            removeAllOverlays();
        });
    }

    public void configureDrawing(float[] newPoints, int[] newOffsets,
                                 int newPixelsPerSecond, int newPauseBetweenPathsMs,
                                 boolean normalizedCoordinates) {
        handler.post(() -> {
            points = newPoints != null ? newPoints.clone() : new float[0];
            offsets = newOffsets != null ? newOffsets.clone() : new int[0];
            pixelsPerSecond = Math.max(1, newPixelsPerSecond);
            pauseBetweenPathsMs = Math.max(0, newPauseBetweenPathsMs);
            coordinatesAreNormalized = normalizedCoordinates;
        });
    }

    public void startDrawing() {
        handler.post(this::startDrawingInternal);
    }

    public void stopDrawing() {
        handler.post(() -> stopDrawingInternal(true));
    }

    private void showTargetController() {
        removeAllOverlays();
        controller = createController();
        TextView instruction = label(getString(R.string.open_target_app_and_position));
        Button select = button(getString(R.string.select_area));
        Button cancel = button(getString(R.string.cancel));
        select.setOnClickListener(view -> showSelectionOverlay());
        cancel.setOnClickListener(view -> {
            removeAllOverlays();
            AutografoBridge.reportSelectionCancelled();
        });
        controller.addView(instruction);
        controller.addView(select);
        controller.addView(cancel);
        addController(controller);
    }

    private void showSelectionOverlay() {
        removeAllOverlays();
        SelectionView selectionView = new SelectionView();
        overlay = selectionView;
        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.TYPE_ACCESSIBILITY_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
                        | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
                PixelFormat.TRANSLUCENT);
        params.gravity = Gravity.TOP | Gravity.START;
        windowManager.addView(selectionView, params);

        controller = createController();
        final TextView sizeLabel = label(getString(R.string.drag_to_select));
        final Button confirmSelection = button(getString(R.string.confirm));
        confirmSelection.setEnabled(false);
        confirmSelection.setOnClickListener(view -> {
            Rect area = selectionView.currentSelection();
            if (area != null)
                acceptSelection(area);
        });
        Button cancelSelection = button(getString(R.string.cancel));
        cancelSelection.setOnClickListener(view -> {
            removeAllOverlays();
            AutografoBridge.reportSelectionCancelled();
        });
        selectionView.setSelectionListener((hasSelection, width, height) -> {
            if (hasSelection) {
                sizeLabel.setText(getString(R.string.area_size, width, height));
                confirmSelection.setEnabled(true);
            } else {
                sizeLabel.setText(getString(R.string.drag_to_select));
                confirmSelection.setEnabled(false);
            }
        });
        controller.addView(sizeLabel);
        controller.addView(confirmSelection);
        controller.addView(cancelSelection);
        addController(controller);
    }

    private void acceptSelection(Rect area) {
        removeAllOverlays();
        selectedArea = new Rect(area);
        selectedOrientation = getResources().getConfiguration().orientation;
        if (!scaleConfiguredDrawingToArea(area)) {
            selectedArea = null;
            notifyUser(getString(R.string.could_not_fit_drawing));
            AutografoBridge.reportError(getString(R.string.could_not_fit_drawing));
            return;
        }
        AutografoBridge.reportAreaSelected(area.left, area.top, area.width(), area.height());
        showReadyController();
    }

    private void showReadyController() {
        removeAllOverlays();
        controller = createController();
        String status = selectedArea != null
                ? getString(R.string.area_selected_size, selectedArea.width(), selectedArea.height())
                : getString(R.string.area_ready);
        controller.addView(label(status));
        controller.addView(label(getString(R.string.open_target_then_start)));
        Button start = button(getString(R.string.start_drawing));
        Button back = button(getString(R.string.return_to_autograph));
        start.setOnClickListener(view -> {
            view.setEnabled(false);
            handler.postDelayed(this::startDrawingInternal, 150);
        });
        back.setOnClickListener(view -> {
            removeAllOverlays();
            AutografoBridge.reportReturnedToApp();
        });
        controller.addView(start);
        controller.addView(back);
        addController(controller);
    }

    private void showStopController() {
        removeAllOverlays();
        controller = createController();
        progressLabel = label(getString(R.string.drawing));
        controller.addView(progressLabel);
        controller.addView(label(getString(R.string.press_volume_to_stop)));
        addController(controller);
        // Injected strokes must pass through this window; otherwise they click Stop
        // and other overlay chrome instead of the target app.
        setControllerPassthrough(true);
        controller.post(this::placeControllerOutsideSelectedArea);
    }

    private LinearLayout createController() {
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(dp(10), dp(4), dp(10), dp(8));
        GradientDrawable background = new GradientDrawable();
        background.setColor(Color.argb(238, 30, 30, 30));
        background.setCornerRadius(dp(12));
        background.setStroke(dp(1), Color.rgb(0, 160, 220));
        layout.setBackground(background);
        View handle = createDragHandle();
        layout.addView(handle);
        controllerDragListener = createControllerDragListener(layout);
        layout.setOnTouchListener(controllerDragListener);
        handle.setOnTouchListener(controllerDragListener);
        return layout;
    }

    private View createDragHandle() {
        LinearLayout handle = new LinearLayout(this);
        handle.setGravity(Gravity.CENTER_HORIZONTAL);
        handle.setPadding(0, dp(6), 0, dp(6));
        handle.setContentDescription(getString(R.string.move_panel));
        handle.setClickable(true);
        View bar = new View(this);
        GradientDrawable pill = new GradientDrawable();
        pill.setColor(Color.argb(200, 180, 180, 180));
        pill.setCornerRadius(dp(2));
        bar.setBackground(pill);
        handle.addView(bar, new LinearLayout.LayoutParams(dp(40), dp(4)));
        handle.setLayoutParams(new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));
        return handle;
    }

    private TextView label(String text) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextColor(Color.WHITE);
        view.setTextSize(14);
        view.setGravity(Gravity.CENTER);
        view.setPadding(dp(4), dp(3), dp(4), dp(3));
        view.setOnTouchListener((v, event) ->
                controllerDragListener != null && controllerDragListener.onTouch(v, event));
        return view;
    }

    private Button button(String text) {
        Button view = new Button(this);
        view.setText(text);
        view.setTextSize(13);
        view.setMinHeight(dp(44));
        view.setAllCaps(false);
        return view;
    }

    private void addController(View view) {
        ensureControllerPositionLoaded();
        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                dp(CONTROLLER_WIDTH_DP), WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.TYPE_ACCESSIBILITY_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
                PixelFormat.TRANSLUCENT);
        params.gravity = Gravity.TOP | Gravity.START;
        if (controllerX == UNSET_POSITION || controllerY == UNSET_POSITION) {
            params.x = defaultControllerX();
            params.y = dp(8);
        } else {
            params.x = controllerX;
            params.y = controllerY;
        }
        clampControllerPosition(params, view);
        controllerParams = params;
        windowManager.addView(view, params);
        view.post(() -> {
            if (controller != view || controllerParams != params)
                return;
            clampControllerPosition(params, view);
            controllerX = params.x;
            controllerY = params.y;
            try {
                windowManager.updateViewLayout(view, params);
            } catch (RuntimeException ignored) {}
        });
    }

    private View.OnTouchListener createControllerDragListener(View panel) {
        return new View.OnTouchListener() {
            private float downRawX;
            private float downRawY;
            private int startX;
            private int startY;
            private boolean moved;

            @Override
            public boolean onTouch(View view, MotionEvent event) {
                if (controllerParams == null)
                    return false;
                switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    downRawX = event.getRawX();
                    downRawY = event.getRawY();
                    startX = controllerParams.x;
                    startY = controllerParams.y;
                    moved = false;
                    return true;
                case MotionEvent.ACTION_MOVE: {
                    int dx = Math.round(event.getRawX() - downRawX);
                    int dy = Math.round(event.getRawY() - downRawY);
                    if (Math.abs(dx) > dp(4) || Math.abs(dy) > dp(4))
                        moved = true;
                    if (!moved)
                        return true;
                    controllerParams.x = startX + dx;
                    controllerParams.y = startY + dy;
                    clampControllerPosition(controllerParams, panel);
                    controllerX = controllerParams.x;
                    controllerY = controllerParams.y;
                    try {
                        windowManager.updateViewLayout(panel, controllerParams);
                    } catch (RuntimeException ignored) {}
                    return true;
                }
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    if (moved)
                        persistControllerPosition();
                    return true;
                default:
                    return false;
                }
            }
        };
    }

    private void clampControllerPosition(WindowManager.LayoutParams params, View view) {
        int screenWidth = getResources().getDisplayMetrics().widthPixels;
        int screenHeight = getResources().getDisplayMetrics().heightPixels;
        int width = view.getWidth() > 0 ? view.getWidth() : params.width;
        int height = view.getHeight() > 0 ? view.getHeight() : dp(80);
        if (width <= 0)
            width = dp(CONTROLLER_WIDTH_DP);
        params.x = Math.max(0, Math.min(params.x, Math.max(0, screenWidth - width)));
        params.y = Math.max(0, Math.min(params.y, Math.max(0, screenHeight - height)));
    }

    private int defaultControllerX() {
        return Math.max(0, getResources().getDisplayMetrics().widthPixels
                - dp(CONTROLLER_WIDTH_DP) - dp(8));
    }

    private void ensureControllerPositionLoaded() {
        if (controllerX != UNSET_POSITION && controllerY != UNSET_POSITION)
            return;
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        controllerX = prefs.getInt(PREF_CONTROLLER_X, UNSET_POSITION);
        controllerY = prefs.getInt(PREF_CONTROLLER_Y, UNSET_POSITION);
    }

    private void persistControllerPosition() {
        if (controllerX == UNSET_POSITION || controllerY == UNSET_POSITION)
            return;
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
                .edit()
                .putInt(PREF_CONTROLLER_X, controllerX)
                .putInt(PREF_CONTROLLER_Y, controllerY)
                .apply();
    }

    private void startDrawingInternal() {
        if (drawing)
            return;
        if (selectedArea == null) {
            AutografoBridge.reportError(getString(R.string.select_area_again));
            return;
        }
        if (getResources().getConfiguration().orientation != selectedOrientation) {
            selectedArea = null;
            AutografoBridge.reportError(getString(R.string.orientation_changed));
            return;
        }
        if (offsets.length < 2 || points.length < 2) {
            AutografoBridge.reportError(getString(R.string.drawing_has_no_valid_points));
            return;
        }

        buildChunks();
        if (chunks.isEmpty()) {
            AutografoBridge.reportError(getString(R.string.could_not_create_gestures));
            return;
        }

        drawing = true;
        stopping = false;
        chunkIndex = 0;
        activeStroke = null;
        showStopController();
        dispatchNextChunk();
    }

    private void buildChunks() {
        chunks.clear();
        long maxDuration = Math.max(100, GestureDescription.getMaxGestureDuration() - 100);
        maxDuration = Math.min(maxDuration, 10_000);

        for (int pathIndex = 0; pathIndex + 1 < offsets.length; ++pathIndex) {
            int first = offsets[pathIndex];
            int end = offsets[pathIndex + 1];
            if (first < 0 || end <= first || end * 2 > points.length)
                continue;

            List<PointF> expanded = new ArrayList<>();
            expanded.add(pointAt(first));
            double maxDistance = pixelsPerSecond * maxDuration / 1000.0;
            for (int i = first + 1; i < end; ++i) {
                PointF from = expanded.get(expanded.size() - 1);
                PointF to = pointAt(i);
                double distance = distance(from, to);
                int pieces = Math.max(1, (int) Math.ceil(distance / Math.max(1.0, maxDistance)));
                for (int part = 1; part <= pieces; ++part) {
                    float fraction = (float) part / pieces;
                    expanded.add(new PointF(from.x + (to.x - from.x) * fraction,
                                            from.y + (to.y - from.y) * fraction));
                }
            }

            int chunkStart = 0;
            while (chunkStart < expanded.size()) {
                Path path = new Path();
                PointF start = expanded.get(chunkStart);
                path.moveTo(start.x, start.y);
                double chunkDistance = 0.0;
                int cursor = chunkStart + 1;
                while (cursor < expanded.size()) {
                    double segment = distance(expanded.get(cursor - 1), expanded.get(cursor));
                    long candidate = durationForDistance(chunkDistance + segment);
                    if (cursor > chunkStart + 1
                            && (candidate > maxDuration || cursor - chunkStart >= 500))
                        break;
                    chunkDistance += segment;
                    PointF point = expanded.get(cursor);
                    path.lineTo(point.x, point.y);
                    ++cursor;
                }

                if (cursor == chunkStart + 1 && cursor < expanded.size()) {
                    PointF point = expanded.get(cursor);
                    chunkDistance = distance(start, point);
                    path.lineTo(point.x, point.y);
                    ++cursor;
                }

                int approximatePoint = Math.min(end - first,
                        Math.max(1, (int) Math.round((double) (cursor - 1)
                                * (end - first) / Math.max(1, expanded.size() - 1))));
                chunks.add(new StrokeChunk(path, durationForDistance(chunkDistance),
                                           pathIndex, approximatePoint, end - first));
                if (cursor >= expanded.size())
                    break;
                chunkStart = cursor - 1;
            }
        }
    }

    private void dispatchNextChunk() {
        if (!drawing || stopping)
            return;
        if (chunkIndex >= chunks.size()) {
            drawing = false;
            removeAllOverlays();
            notifyUser(getString(R.string.drawing_finished));
            AutografoBridge.reportFinished();
            return;
        }

        StrokeChunk chunk = chunks.get(chunkIndex);
        boolean continuesPrevious = chunkIndex > 0
                && chunks.get(chunkIndex - 1).pathIndex == chunk.pathIndex;
        boolean continuesNext = chunkIndex + 1 < chunks.size()
                && chunks.get(chunkIndex + 1).pathIndex == chunk.pathIndex;

        if (continuesPrevious && activeStroke != null) {
            activeStroke = activeStroke.continueStroke(chunk.path, 0, chunk.durationMs, continuesNext);
        } else {
            activeStroke = new GestureDescription.StrokeDescription(
                    chunk.path, 0, chunk.durationMs, continuesNext);
        }

        GestureDescription gesture = new GestureDescription.Builder()
                .addStroke(activeStroke)
                .build();
        boolean accepted = dispatchGesture(gesture, new AccessibilityService.GestureResultCallback() {
            @Override
            public void onCompleted(GestureDescription gestureDescription) {
                if (!drawing || stopping)
                    return;
                StrokeChunk completed = chunks.get(chunkIndex);
                AutografoBridge.reportProgress(completed.pathIndex + 1, offsets.length - 1,
                                               completed.endPoint, completed.totalPoints);
                updateProgress(completed.pathIndex + 1, offsets.length - 1);
                int previousPath = completed.pathIndex;
                ++chunkIndex;
                int delay = chunkIndex < chunks.size()
                        && chunks.get(chunkIndex).pathIndex != previousPath
                        ? pauseBetweenPathsMs : 0;
                handler.postDelayed(AutografoAccessibilityService.this::dispatchNextChunk, delay);
            }

            @Override
            public void onCancelled(GestureDescription gestureDescription) {
                // Only an explicit stop (volume) sets `stopping` and then dispatches
                // a follow-up tap that cancels the stroke in flight. Any other
                // cancellation is not treated as "user tapped to stop": the app's
                // own dispatchGesture completions use onCompleted, but a new
                // dispatchGesture or an OEM interrupt also lands here.
                if (stopping)
                    finishCancellation();
                else
                    stopWithResult(false, getString(R.string.gesture_interrupted));
            }
        }, handler);

        if (!accepted)
            stopWithResult(false, getString(R.string.gesture_rejected));
    }

    private void updateProgress(int currentPath, int totalPaths) {
        if (progressLabel != null)
            progressLabel.setText(getString(R.string.drawing_progress, currentPath, totalPaths));
    }

    private void stopDrawingInternal(boolean reportCancellation) {
        handler.removeCallbacksAndMessages(null);
        if (!drawing) {
            if (reportCancellation) {
                removeAllOverlays();
                AutografoBridge.reportCancelled();
            }
            return;
        }

        stopping = true;
        if (reportCancellation)
            cancelActiveGesture();
        else
            finishCancellation();
    }

    private void cancelActiveGesture() {
        // A follow-up gesture cancels the stroke in flight. Make the panel
        // receive that tap so it does not click the target app.
        setControllerPassthrough(false);
        handler.post(this::dispatchCancelTap);
    }

    private void dispatchCancelTap() {
        if (!stopping)
            return;
        Path tap = new Path();
        float cancelX;
        float cancelY;
        if (controller != null && controller.getWidth() > 0 && controller.getHeight() > 0) {
            int[] location = new int[2];
            controller.getLocationOnScreen(location);
            cancelX = location[0] + controller.getWidth() / 2.0f;
            cancelY = location[1] + controller.getHeight() / 2.0f;
        } else if (controllerParams != null) {
            cancelX = controllerParams.x + dp(CONTROLLER_WIDTH_DP) / 2.0f;
            cancelY = controllerParams.y + dp(44);
        } else {
            cancelX = getResources().getDisplayMetrics().widthPixels - dp(88);
            cancelY = dp(44);
        }
        tap.moveTo(cancelX, cancelY);
        GestureDescription.StrokeDescription stroke =
                new GestureDescription.StrokeDescription(tap, 0, 1);
        dispatchGesture(new GestureDescription.Builder().addStroke(stroke).build(),
                        null, handler);
        handler.postDelayed(this::finishCancellation, 50);
    }

    private void setControllerPassthrough(boolean passthrough) {
        if (controller == null || controllerParams == null || windowManager == null)
            return;
        final int flag = WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE;
        if (passthrough)
            controllerParams.flags |= flag;
        else
            controllerParams.flags &= ~flag;
        try {
            windowManager.updateViewLayout(controller, controllerParams);
        } catch (RuntimeException ignored) {}
    }

    private void placeControllerOutsideSelectedArea() {
        if (controller == null || controllerParams == null || selectedArea == null)
            return;
        int width = controller.getWidth() > 0 ? controller.getWidth() : dp(CONTROLLER_WIDTH_DP);
        int height = controller.getHeight() > 0 ? controller.getHeight() : dp(80);
        int screenWidth = getResources().getDisplayMetrics().widthPixels;
        int screenHeight = getResources().getDisplayMetrics().heightPixels;
        int margin = dp(8);
        int[][] corners = {
            {Math.max(0, screenWidth - width - margin), margin},
            {margin, margin},
            {Math.max(0, screenWidth - width - margin),
             Math.max(0, screenHeight - height - margin)},
            {margin, Math.max(0, screenHeight - height - margin)}
        };
        Rect panel = new Rect();
        for (int[] corner : corners) {
            panel.set(corner[0], corner[1], corner[0] + width, corner[1] + height);
            if (!Rect.intersects(panel, selectedArea)) {
                controllerParams.x = corner[0];
                controllerParams.y = corner[1];
                try {
                    windowManager.updateViewLayout(controller, controllerParams);
                } catch (RuntimeException ignored) {}
                return;
            }
        }
    }

    private void finishCancellation() {
        if (!drawing && !stopping)
            return;
        drawing = false;
        stopping = false;
        activeStroke = null;
        removeAllOverlays();
        AutografoBridge.reportCancelled();
    }

    private void stopWithResult(boolean cancelled, String message) {
        handler.removeCallbacksAndMessages(null);
        drawing = false;
        stopping = false;
        activeStroke = null;
        removeAllOverlays();
        if (cancelled)
            AutografoBridge.reportCancelled();
        else {
            notifyUser(message);
            AutografoBridge.reportError(message);
        }
    }

    private boolean scaleConfiguredDrawingToArea(Rect area) {
        if (!coordinatesAreNormalized)
            return points.length >= 2 && offsets.length >= 2;
        if (points.length < 2 || (points.length & 1) != 0 || offsets.length < 2)
            return false;

        float minX = Float.POSITIVE_INFINITY;
        float minY = Float.POSITIVE_INFINITY;
        float maxX = Float.NEGATIVE_INFINITY;
        float maxY = Float.NEGATIVE_INFINITY;
        for (int i = 0; i < points.length; i += 2) {
            float x = points[i];
            float y = points[i + 1];
            if (!Float.isFinite(x) || !Float.isFinite(y))
                return false;
            minX = Math.min(minX, x);
            minY = Math.min(minY, y);
            maxX = Math.max(maxX, x);
            maxY = Math.max(maxY, y);
        }

        float contentWidth = maxX - minX;
        float contentHeight = maxY - minY;
        if (contentWidth <= 0.0f || contentHeight <= 0.0f)
            return false;

        float marginX = area.width() * 0.05f;
        float marginY = area.height() * 0.05f;
        float innerLeft = area.left + marginX;
        float innerTop = area.top + marginY;
        float innerWidth = area.width() - 2.0f * marginX;
        float innerHeight = area.height() - 2.0f * marginY;
        if (innerWidth <= 0.0f || innerHeight <= 0.0f)
            return false;

        float scale = Math.min(innerWidth / contentWidth, innerHeight / contentHeight);
        float fittedLeft = innerLeft + (innerWidth - contentWidth * scale) / 2.0f;
        float fittedTop = innerTop + (innerHeight - contentHeight * scale) / 2.0f;
        for (int i = 0; i < points.length; i += 2) {
            points[i] = Math.round(fittedLeft + (points[i] - minX) * scale);
            points[i + 1] = Math.round(fittedTop + (points[i + 1] - minY) * scale);
        }
        coordinatesAreNormalized = false;
        return true;
    }

    private void notifyUser(String message) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
    }

    private void removeAllOverlays() {
        if (overlay != null) {
            try { windowManager.removeView(overlay); } catch (RuntimeException ignored) {}
            overlay = null;
        }
        if (controller != null) {
            try { windowManager.removeView(controller); } catch (RuntimeException ignored) {}
            controller = null;
            controllerParams = null;
            controllerDragListener = null;
            progressLabel = null;
        }
    }

    private PointF pointAt(int index) {
        return new PointF(points[index * 2], points[index * 2 + 1]);
    }

    private long durationForDistance(double distance) {
        return Math.max(1, Math.round(distance * 1000.0 / pixelsPerSecond));
    }

    private static double distance(PointF left, PointF right) {
        return Math.hypot(right.x - left.x, right.y - left.y);
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private static final class StrokeChunk {
        final Path path;
        final long durationMs;
        final int pathIndex;
        final int endPoint;
        final int totalPoints;

        StrokeChunk(Path path, long durationMs, int pathIndex, int endPoint, int totalPoints) {
            this.path = path;
            this.durationMs = durationMs;
            this.pathIndex = pathIndex;
            this.endPoint = endPoint;
            this.totalPoints = totalPoints;
        }
    }

    private interface SelectionListener {
        void onSelectionChanged(boolean hasSelection, int width, int height);
    }

    private final class SelectionView extends View {
        private final Paint shade = new Paint();
        private final Paint border = new Paint();
        private final Paint text = new Paint();
        private final Rect selection = new Rect();
        private float startX;
        private float startY;
        private boolean dragging;
        private boolean hasSelection;
        private SelectionListener selectionListener;

        SelectionView() {
            super(AutografoAccessibilityService.this);
            setBackgroundColor(Color.TRANSPARENT);
            shade.setColor(Color.argb(150, 0, 0, 0));
            border.setColor(Color.rgb(0, 180, 255));
            border.setStyle(Paint.Style.STROKE);
            border.setStrokeWidth(dp(2));
            text.setColor(Color.WHITE);
            text.setTextSize(dp(16));
            text.setAntiAlias(true);
        }

        void setSelectionListener(SelectionListener listener) {
            selectionListener = listener;
        }

        Rect currentSelection() {
            if (!hasSelection)
                return null;
            return toScreenCoordinates(selection);
        }

        private void notifySelectionChanged() {
            if (selectionListener != null)
                selectionListener.onSelectionChanged(hasSelection, selection.width(), selection.height());
        }

        @Override
        protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            canvas.drawRect(0, 0, getWidth(), getHeight(), shade);
            if (hasSelection || dragging) {
                canvas.save();
                canvas.clipRect(selection);
                canvas.drawColor(Color.TRANSPARENT, android.graphics.PorterDuff.Mode.CLEAR);
                canvas.restore();
                canvas.drawRect(selection, border);
                canvas.drawText(selection.width() + " × " + selection.height(),
                                selection.left + dp(6), selection.top + dp(22), text);
            }

            canvas.drawText(getString(R.string.drag_to_select), dp(18), dp(36), text);
            if (hasSelection) {
                canvas.drawText(getString(R.string.tap_inside_to_confirm), dp(18), getHeight() - dp(54), text);
                canvas.drawText(getString(R.string.tap_outside_to_redo), dp(18),
                                getHeight() - dp(24), text);
            }
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            float localX = event.getX();
            float localY = event.getY();
            switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                if (hasSelection && selection.contains((int) localX, (int) localY)) {
                    acceptSelection(toScreenCoordinates(selection));
                    return true;
                }
                startX = localX;
                startY = localY;
                selection.set((int) localX, (int) localY, (int) localX, (int) localY);
                dragging = true;
                hasSelection = false;
                notifySelectionChanged();
                invalidate();
                return true;
            case MotionEvent.ACTION_MOVE:
                if (dragging) {
                    setNormalized(selection, startX, startY, localX, localY);
                    invalidate();
                }
                return true;
            case MotionEvent.ACTION_UP:
                if (dragging) {
                    dragging = false;
                    setNormalized(selection, startX, startY, localX, localY);
                    hasSelection = selection.width() > dp(20)
                            && selection.height() > dp(20);
                    if (!hasSelection)
                        selection.setEmpty();
                    notifySelectionChanged();
                    invalidate();
                }
                return true;
            default:
                return true;
            }
        }

        @Override
        public boolean dispatchKeyEventPreIme(android.view.KeyEvent event) {
            if (event.getKeyCode() == android.view.KeyEvent.KEYCODE_BACK
                    && event.getAction() == android.view.KeyEvent.ACTION_UP) {
                removeAllOverlays();
                AutografoBridge.reportSelectionCancelled();
                return true;
            }
            return super.dispatchKeyEventPreIme(event);
        }

        private void setNormalized(Rect rect, float x1, float y1, float x2, float y2) {
            rect.set((int) Math.min(x1, x2), (int) Math.min(y1, y2),
                     (int) Math.max(x1, x2), (int) Math.max(y1, y2));
        }

        private Rect toScreenCoordinates(Rect localRect) {
            int[] location = new int[2];
            getLocationOnScreen(location);
            Rect screenRect = new Rect(localRect);
            screenRect.offset(location[0], location[1]);
            return screenRect;
        }
    }
}
