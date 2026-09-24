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

#include "SvgPathExtractor.h"
#include "AreaSelectorOverlay.h"

#include <QMainWindow>
#include <QRect>
#include <QString>

class QButtonGroup;
class QCheckBox;
class QCloseEvent;
class QEvent;
class QHideEvent;
class QShowEvent;
class QGridLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QSlider;
class QToolButton;
class QVBoxLayout;
class QWidget;
class PathPreviewWidget;
class DrawingBackend;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
#ifdef AUTOGRAFO_ENABLE_ADS
    void hideEvent(QHideEvent *event) override;
#endif
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void openSvg();
    void selectArea();
    void startDrawing();
    void stopDrawing();
    void onAreaSelected(const QRect &area);
    void onSelectionCancelled();
    void onReturnedToApp();
    void onDrawingProgress(int currentPath, int totalPaths, int currentPoint, int totalPoints);
    void onDrawingFinished();
    void onDrawingCancelled();
    void onDrawingError(const QString &message);
    void updateStatus();
    void syncFlowUi();
    void showAbout();
    void showAboutQt();
    void enableAccessibility();
    void showAccessibilityDisclosure();
    void dismissAccessibilityDisclosure();
    void acceptAccessibilityDisclosure();
    void editSpeedValue();
    void applySpeedPreset(int pixelsPerSecond);
    void onSpeedSliderChanged(int pixelsPerSecond);
    void onApplicationStateChanged(Qt::ApplicationState state);
    void updateAndroidChrome();

private:
    void buildDesktopLayout(QWidget *central, QVBoxLayout *layout);
    void buildAndroidLayout(QWidget *central, QVBoxLayout *layout);
    void setDrawingUiActive(bool active);
    void showAboutOverlay(const QString &title, const QString &html);
    void dismissAboutOverlay();
    void showOverflowMenu();
    void dismissOverflowMenu();
    void layoutOverflowMenu();
    void sendToBackground();
    void bringToForeground();
    void clearAreaSelection();
    void applyTopSafeInset();
    void updateSpeedLabel(int pixelsPerSecond);
    void updateSpeedSegmentSelection(int pixelsPerSecond);
    void updatePreviewHeightLimit();
    void applyAndroidOrientationLayout();
    void updateFileRowText();
    void updateStepperVisual(bool hasSvg, bool hasArea);
    void retranslateUi();

    SvgPathExtractor m_extractor;
    PathPreviewWidget *m_preview = nullptr;
    AreaSelectorOverlay *m_areaOverlay = nullptr;
    DrawingBackend *m_drawer = nullptr;

    QPushButton *m_openButton = nullptr;
    QPushButton *m_selectAreaButton = nullptr;
    QPushButton *m_drawButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_aboutButton = nullptr;
    QPushButton *m_aboutQtButton = nullptr;
    QPushButton *m_primaryActionButton = nullptr;
    QPushButton *m_changeAreaButton = nullptr;
    QPushButton *m_enableAccessibilityButton = nullptr;
    QPushButton *m_speedSlowButton = nullptr;
    QPushButton *m_speedNormalButton = nullptr;
    QPushButton *m_speedFastButton = nullptr;
    QButtonGroup *m_speedPresetGroup = nullptr;
    QToolButton *m_menuButton = nullptr;
    QSlider *m_speedSlider = nullptr;
    QLabel *m_speedLabel = nullptr;
    QLabel *m_speedValueLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_fileNameLabel = nullptr;
    QLabel *m_fileMetaLabel = nullptr;
    QLabel *m_stepFileLabel = nullptr;
    QLabel *m_stepAreaLabel = nullptr;
    QLabel *m_stepConnectorLabel = nullptr;
    QLabel *m_areaSummaryTitleLabel = nullptr;
    QLabel *m_areaSummarySizeLabel = nullptr;
    QLabel *m_accessibilityChip = nullptr;
    QLabel *m_drawHintLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_warningLabel = nullptr;
    QLabel *m_accessibilityTitleLabel = nullptr;
    QLabel *m_accessibilityBodyLabel = nullptr;
    QWidget *m_aboutOverlay = nullptr;
    QWidget *m_a11yDisclosureOverlay = nullptr;
    QCheckBox *m_a11yConsentCheck = nullptr;
    QLabel *m_a11yConsentBox = nullptr;
    QLabel *m_a11yConsentLabel = nullptr;
    QPushButton *m_a11yContinueButton = nullptr;
    QWidget *m_overflowScrim = nullptr;
    QWidget *m_overflowPanel = nullptr;
    QWidget *m_headerWidget = nullptr;
    QWidget *m_bodyWidget = nullptr;
    QGridLayout *m_bodyGrid = nullptr;
    QVBoxLayout *m_androidRootLayout = nullptr;
    QWidget *m_accessibilityCard = nullptr;
    QWidget *m_fileSummaryWidget = nullptr;
    QWidget *m_previewContainer = nullptr;
    QWidget *m_stepperWidget = nullptr;
    QWidget *m_areaSummaryWidget = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_scrollContent = nullptr;
    QWidget *m_speedSegment = nullptr;
    QWidget *m_footerWidget = nullptr;
    bool m_androidLandscape = false;

    QString m_loadedFileName;
    QRect m_targetArea;
    bool m_hasArea = false;
    bool m_androidUi = false;
    bool m_drawingUiActive = false;
    bool m_updatingSpeedUi = false;
};
