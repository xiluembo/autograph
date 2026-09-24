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

#include "MainWindow.h"

#include "AppTranslations.h"
#include "DrawingBackend.h"
#include "MouseDrawer.h"
#include "PathPreviewWidget.h"
#include "PathScaler.h"

#ifdef Q_OS_ANDROID
#include "AndroidGestureDrawer.h"
#endif
#ifdef AUTOGRAFO_ENABLE_ADS
#include "AutografoAds.h"
#endif

#include <QApplication>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QColor>
#include <QDialog>
#include <QEvent>
#include <QCloseEvent>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#ifdef AUTOGRAFO_ENABLE_ADS
#include <QHideEvent>
#endif
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QPointer>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QScrollerProperties>
#include <QShowEvent>
#include <QSizePolicy>
#include <QSlider>
#include <QStatusBar>
#include <QTimer>
#include <QtMath>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <jni.h>
#endif

namespace {

constexpr int kPresetSlow = 100;
constexpr int kPresetNormal = 300;
constexpr int kPresetFast = 700;

#ifdef AUTOGRAFO_ENABLE_ADS
/// Height, in Qt logical pixels (dp), of the banner that overlaps the
/// already-safe Qt content area. Zero while no ad is loaded. Java reports
/// the banner in physical pixels; Qt already accounts for the nav/gesture
/// inset via WA_ContentsMarginsRespectsSafeArea, so that must not be added.
int androidAdsBottomReserve()
{
    int px = 0;
#ifdef Q_OS_ANDROID
    px = QJniObject::callStaticMethod<jint>(
        "com/autografo/android/AutografoAds", "bottomReservePx", "()I");
    if (px > 0) {
        if (const QScreen *screen = QGuiApplication::primaryScreen()) {
            const qreal ratio = screen->devicePixelRatio();
            if (ratio > 1.0)
                px = qCeil(px / ratio);
        }
    }
#endif
    return qMax(px, 0);
}

#ifdef Q_OS_ANDROID
/// Window notified when the Java side reports an ad banner state change.
QPointer<MainWindow> s_adsChromeWindow;
#endif
#endif

/// Bottom padding for in-window overlays (About). Unlike the main layout,
/// these widgets are geometry-fitted to the full window and do not inherit
/// Qt's safe-area contents margins, so the nav/gesture inset must be added
/// on top of the ad banner reserve.
int androidOverlayBottomInset(const QWidget *window)
{
    int inset = 20;
#ifdef AUTOGRAFO_ENABLE_ADS
    inset += androidAdsBottomReserve();
#endif
#ifdef Q_OS_ANDROID
    if (window && window->windowHandle())
        inset += window->windowHandle()->safeAreaMargins().bottom();
#else
    Q_UNUSED(window);
#endif
    return inset;
}

void applyReadableAboutColors(QWidget *widget)
{
    QPalette pal = widget->palette();
    const QColor bg(255, 255, 255);
    const QColor fg(17, 17, 17);
    const QColor link(11, 87, 208);
    for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        pal.setColor(group, QPalette::Window, bg);
        pal.setColor(group, QPalette::Base, bg);
        pal.setColor(group, QPalette::AlternateBase, bg);
        pal.setColor(group, QPalette::WindowText, fg);
        pal.setColor(group, QPalette::Text, fg);
        pal.setColor(group, QPalette::ButtonText, fg);
        pal.setColor(group, QPalette::PlaceholderText, fg);
        pal.setColor(group, QPalette::Link, link);
        pal.setColor(group, QPalette::LinkVisited, link);
    }
    widget->setPalette(pal);
    widget->setAutoFillBackground(true);
}

QString readableAboutHtml(const QString &html)
{
    QString styled = html;
    styled.replace(QLatin1String("<a href="),
                   QLatin1String("<a style=\"color:#0b57d0; text-decoration:underline;\" href="));
    return QStringLiteral(
               "<div style=\"color:#111111; background-color:#ffffff;\">%1</div>")
        .arg(styled);
}

QString primaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background-color: #008FD5;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 14px;"
        "  font-weight: 600;"
        "  font-size: 15px;"
        "  padding: 14px 16px;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #D7E8F4;"
        "  color: #7AA8C4;"
        "}");
}

QString secondaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background-color: #E8F4FB;"
        "  color: #0B5F8A;"
        "  border: none;"
        "  border-radius: 14px;"
        "  font-weight: 600;"
        "  font-size: 15px;"
        "  padding: 14px 16px;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #EEF1F3;"
        "  color: #9AA0A6;"
        "}");
}

QString textButtonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background: transparent;"
        "  color: #008FD5;"
        "  border: none;"
        "  font-weight: 600;"
        "  font-size: 14px;"
        "  padding: 8px 4px;"
        "}"
        "QPushButton:disabled { color: #9AA0A6; }");
}

QString surfaceCardStyle()
{
    return QStringLiteral(
        "QWidget#surfaceCard {"
        "  background-color: #FFFFFF;"
        "  border-radius: 14px;"
        "}");
}

QString segmentedFrameStyle()
{
    return QStringLiteral(
        "QFrame#speedSegment {"
        "  background-color: #EEF1F3;"
        "  border-radius: 12px;"
        "}"
        "QFrame#speedSegment QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 10px;"
        "  color: #6B6B6B;"
        "  font-weight: 600;"
        "  padding: 10px 8px;"
        "}"
        "QFrame#speedSegment QPushButton:checked {"
        "  background-color: #FFFFFF;"
        "  color: #202124;"
        "}"
        "QFrame#speedSegment QPushButton:disabled {"
        "  color: #9AA0A6;"
        "}");
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Autograph — SVG Line-Art"));
#ifdef Q_OS_ANDROID
    m_androidUi = true;
#ifdef AUTOGRAFO_ENABLE_ADS
    s_adsChromeWindow = this;
#endif
#else
    resize(720, 560);
#endif

#ifdef Q_OS_ANDROID
    m_drawer = new AndroidGestureDrawer(this);
#else
    m_drawer = new MouseDrawer(this);
#endif
    connect(m_drawer, &DrawingBackend::progressChanged, this, &MainWindow::onDrawingProgress);
    connect(m_drawer, &DrawingBackend::finished, this, &MainWindow::onDrawingFinished);
    connect(m_drawer, &DrawingBackend::cancelled, this, &MainWindow::onDrawingCancelled);
    connect(m_drawer, &DrawingBackend::error, this, &MainWindow::onDrawingError);
    connect(m_drawer, &DrawingBackend::areaSelected, this, &MainWindow::onAreaSelected);
    connect(m_drawer, &DrawingBackend::selectionCancelled,
            this, &MainWindow::onSelectionCancelled);
    connect(m_drawer, &DrawingBackend::returnedToApp, this, &MainWindow::onReturnedToApp);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
#ifdef Q_OS_ANDROID
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    QPalette pal = central->palette();
    pal.setColor(QPalette::Window, QColor(0xF8, 0xF9, 0xFA));
    pal.setColor(QPalette::WindowText, QColor(0x20, 0x21, 0x24));
    central->setPalette(pal);
    central->setAutoFillBackground(true);
#endif

    if (m_androidUi)
        buildAndroidLayout(central, layout);
    else
        buildDesktopLayout(central, layout);

    setCentralWidget(central);

    if (!m_androidUi) {
        m_statusLabel = new QLabel(central);
        statusBar()->addWidget(m_statusLabel, 1);
    } else {
        statusBar()->hide();
    }

    connect(qApp, &QGuiApplication::applicationStateChanged,
            this, &MainWindow::onApplicationStateChanged);

    setDrawingUiActive(false);
    updateStatus();
    syncFlowUi();

#ifdef Q_OS_ANDROID
    showMaximized();
#endif
}

void MainWindow::buildDesktopLayout(QWidget *central, QVBoxLayout *layout)
{
    m_preview = new PathPreviewWidget(central);
    layout->addWidget(m_preview, 1);

    auto *controls = new QGridLayout();

    m_openButton = new QPushButton(tr("Open SVG"), central);
    connect(m_openButton, &QPushButton::clicked, this, &MainWindow::openSvg);
    controls->addWidget(m_openButton, 0, 0);

    m_selectAreaButton = new QPushButton(tr("Select area"), central);
    connect(m_selectAreaButton, &QPushButton::clicked, this, &MainWindow::selectArea);
    controls->addWidget(m_selectAreaButton, 0, 1);

    m_drawButton = new QPushButton(tr("Draw"), central);
    connect(m_drawButton, &QPushButton::clicked, this, &MainWindow::startDrawing);
    controls->addWidget(m_drawButton, 0, 2);

    m_stopButton = new QPushButton(tr("Stop"), central);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopDrawing);
    m_stopButton->setEnabled(false);
    controls->addWidget(m_stopButton, 0, 3);

    m_speedLabel = new QLabel(central);
    controls->addWidget(m_speedLabel, 1, 0, 1, 2);

    m_speedSlider = new QSlider(Qt::Horizontal, central);
    m_speedSlider->setRange(50, 1000);
    m_speedSlider->setSingleStep(10);
    m_speedSlider->setPageStep(50);
    m_speedSlider->setValue(300);
    m_speedSlider->setToolTip(tr("Cursor speed in pixels per second."));
    connect(m_speedSlider, &QSlider::valueChanged, this, &MainWindow::updateSpeedLabel);
    updateSpeedLabel(m_speedSlider->value());
    controls->addWidget(m_speedSlider, 1, 2, 1, 2);
    controls->setColumnStretch(2, 1);

    m_aboutButton = new QPushButton(tr("About"), central);
    connect(m_aboutButton, &QPushButton::clicked, this, &MainWindow::showAbout);
    controls->addWidget(m_aboutButton, 2, 0, 1, 2);

    m_aboutQtButton = new QPushButton(tr("About Qt"), central);
    connect(m_aboutQtButton, &QPushButton::clicked, this, &MainWindow::showAboutQt);
    controls->addWidget(m_aboutQtButton, 2, 2, 1, 2);

    layout->addLayout(controls);

    m_warningLabel = new QLabel(
        tr("Warning: do not move the mouse while drawing. "
           "Place the cursor in the target app before starting."),
        central);
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setStyleSheet(QStringLiteral("color: #c90;"));
    layout->addWidget(m_warningLabel);
}

void MainWindow::buildAndroidLayout(QWidget *central, QVBoxLayout *rootLayout)
{
    m_androidRootLayout = rootLayout;
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_headerWidget = new QWidget(central);
    m_headerWidget->setObjectName(QStringLiteral("appBar"));
    auto *headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(24, 8, 12, 8);
    headerLayout->setSpacing(8);
    m_titleLabel = new QLabel(tr("Autograph"), m_headerWidget);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(qMax(titleFont.pointSize() + 3, 18));
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(QStringLiteral("color: #202124;"));
    headerLayout->addWidget(m_titleLabel, 1);

    // QMenu creates a separate QWindow on Android and deadlocks with the
    // accessibility OpenGL path after an SVG is loaded. Use an in-window panel.
    m_menuButton = new QToolButton(m_headerWidget);
    m_menuButton->setText(QStringLiteral("⋮"));
    m_menuButton->setPopupMode(QToolButton::DelayedPopup);
    m_menuButton->setStyleSheet(QStringLiteral(
        "QToolButton { border: none; color: #202124; font-size: 22px; padding: 8px 12px; }"
        "QToolButton::menu-indicator { image: none; }"));
    connect(m_menuButton, &QToolButton::clicked, this, &MainWindow::showOverflowMenu);
    headerLayout->addWidget(m_menuButton, 0, Qt::AlignRight);
    rootLayout->addWidget(m_headerWidget);

    m_bodyWidget = new QWidget(central);
    m_bodyGrid = new QGridLayout(m_bodyWidget);
    m_bodyGrid->setContentsMargins(0, 0, 0, 0);
    m_bodyGrid->setSpacing(0);

    m_previewContainer = new QWidget(m_bodyWidget);
    auto *previewLayout = new QVBoxLayout(m_previewContainer);
    previewLayout->setContentsMargins(24, 8, 24, 8);
    previewLayout->setSpacing(0);
    m_preview = new PathPreviewWidget(m_previewContainer);
    m_preview->setCompactEmpty(true);
    m_preview->setCornerRadius(14);
    m_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_preview->setMinimumHeight(0);
    previewLayout->addWidget(m_preview, 1);

    m_scrollArea = new QScrollArea(m_bodyWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical {"
        "  width: 4px; background: transparent; margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #C4C7C5; border-radius: 2px; min-height: 32px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"));
    // Widgets on Android have no native touch flicking; without QScroller the
    // hidden scrollbar left the content impossible to scroll at all.
    QScroller::grabGesture(m_scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    if (QScroller *scroller = QScroller::scroller(m_scrollArea->viewport())) {
        QScrollerProperties props = scroller->scrollerProperties();
        props.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.0);
        props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.002);
        props.setScrollMetric(QScrollerProperties::DragVelocitySmoothingFactor, 0.6);
        props.setScrollMetric(QScrollerProperties::DecelerationFactor, 0.08);
        props.setScrollMetric(QScrollerProperties::FrameRate, QScrollerProperties::Fps60);
        props.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0.33);
        props.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.1);
        props.setScrollMetric(QScrollerProperties::OvershootScrollTime, 0.3);
        scroller->setScrollerProperties(props);
    }
    m_scrollContent = new QWidget(m_scrollArea);
    m_scrollContent->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *contentLayout = new QVBoxLayout(m_scrollContent);
    contentLayout->setContentsMargins(24, 4, 24, 24);
    contentLayout->setSpacing(12);

    m_fileSummaryWidget = new QWidget(m_scrollContent);
    m_fileSummaryWidget->setObjectName(QStringLiteral("surfaceCard"));
    m_fileSummaryWidget->setStyleSheet(surfaceCardStyle());
    m_fileSummaryWidget->setCursor(Qt::PointingHandCursor);
    m_fileSummaryWidget->installEventFilter(this);
    auto *fileRow = new QHBoxLayout(m_fileSummaryWidget);
    fileRow->setContentsMargins(14, 12, 12, 12);
    fileRow->setSpacing(12);
    auto *fileIcon = new QLabel(QStringLiteral("◇"), m_fileSummaryWidget);
    fileIcon->setStyleSheet(QStringLiteral("color: #008FD5; font-size: 18px;"));
    fileRow->addWidget(fileIcon, 0, Qt::AlignTop);
    auto *fileTexts = new QWidget(m_fileSummaryWidget);
    auto *fileTextLayout = new QVBoxLayout(fileTexts);
    fileTextLayout->setContentsMargins(0, 0, 0, 0);
    fileTextLayout->setSpacing(2);
    m_fileNameLabel = new QLabel(fileTexts);
    m_fileNameLabel->setStyleSheet(QStringLiteral("color: #202124; font-weight: 600; font-size: 14px;"));
    m_fileMetaLabel = new QLabel(fileTexts);
    m_fileMetaLabel->setStyleSheet(QStringLiteral("color: #6B6B6B; font-size: 13px;"));
    fileTextLayout->addWidget(m_fileNameLabel);
    fileTextLayout->addWidget(m_fileMetaLabel);
    fileRow->addWidget(fileTexts, 1);
    auto *chevron = new QLabel(QStringLiteral("›"), m_fileSummaryWidget);
    chevron->setStyleSheet(QStringLiteral("color: #9AA0A6; font-size: 22px;"));
    fileRow->addWidget(chevron, 0, Qt::AlignVCenter);
    m_fileSummaryWidget->hide();
    contentLayout->addWidget(m_fileSummaryWidget);

    m_stepperWidget = new QWidget(m_scrollContent);
    auto *stepRow = new QHBoxLayout(m_stepperWidget);
    stepRow->setContentsMargins(0, 0, 0, 0);
    stepRow->setSpacing(8);
    m_stepFileLabel = new QLabel(m_stepperWidget);
    m_stepFileLabel->setStyleSheet(QStringLiteral("color: #202124; font-weight: 600; font-size: 13px;"));
    m_stepConnectorLabel = new QLabel(QStringLiteral("────────"), m_stepperWidget);
    m_stepConnectorLabel->setStyleSheet(QStringLiteral("color: #C4C7C5;"));
    m_stepConnectorLabel->setAlignment(Qt::AlignCenter);
    m_stepAreaLabel = new QLabel(m_stepperWidget);
    m_stepAreaLabel->setStyleSheet(QStringLiteral("color: #6B6B6B; font-weight: 600; font-size: 13px;"));
    stepRow->addWidget(m_stepFileLabel, 0);
    stepRow->addWidget(m_stepConnectorLabel, 1);
    stepRow->addWidget(m_stepAreaLabel, 0);
    contentLayout->addWidget(m_stepperWidget);

    m_primaryActionButton = new QPushButton(m_scrollContent);
    m_primaryActionButton->setMinimumHeight(52);
    m_primaryActionButton->setStyleSheet(secondaryButtonStyle());
    connect(m_primaryActionButton, &QPushButton::clicked, this, [this]() {
        if (m_extractor.isEmpty())
            openSvg();
        else if (!m_hasArea)
            selectArea();
    });
    contentLayout->addWidget(m_primaryActionButton);
    m_openButton = m_primaryActionButton;
    m_selectAreaButton = m_primaryActionButton;

    m_areaSummaryWidget = new QWidget(m_scrollContent);
    m_areaSummaryWidget->setObjectName(QStringLiteral("surfaceCard"));
    m_areaSummaryWidget->setStyleSheet(surfaceCardStyle());
    auto *areaLayout = new QHBoxLayout(m_areaSummaryWidget);
    areaLayout->setContentsMargins(14, 12, 8, 12);
    areaLayout->setSpacing(8);
    auto *areaTexts = new QWidget(m_areaSummaryWidget);
    auto *areaTextLayout = new QVBoxLayout(areaTexts);
    areaTextLayout->setContentsMargins(0, 0, 0, 0);
    areaTextLayout->setSpacing(2);
    m_areaSummaryTitleLabel = new QLabel(tr("Drawing area"), areaTexts);
    m_areaSummaryTitleLabel->setStyleSheet(QStringLiteral("color: #202124; font-weight: 600; font-size: 14px;"));
    m_areaSummarySizeLabel = new QLabel(areaTexts);
    m_areaSummarySizeLabel->setStyleSheet(QStringLiteral("color: #6B6B6B; font-size: 13px;"));
    areaTextLayout->addWidget(m_areaSummaryTitleLabel);
    areaTextLayout->addWidget(m_areaSummarySizeLabel);
    areaLayout->addWidget(areaTexts, 1);
    m_changeAreaButton = new QPushButton(tr("Change"), m_areaSummaryWidget);
    m_changeAreaButton->setStyleSheet(textButtonStyle());
    m_changeAreaButton->setCursor(Qt::PointingHandCursor);
    connect(m_changeAreaButton, &QPushButton::clicked, this, &MainWindow::selectArea);
    areaLayout->addWidget(m_changeAreaButton, 0, Qt::AlignVCenter);
    m_areaSummaryWidget->hide();
    contentLayout->addWidget(m_areaSummaryWidget);

    m_accessibilityCard = new QFrame(m_scrollContent);
    m_accessibilityCard->setObjectName(QStringLiteral("a11yCard"));
    m_accessibilityCard->setStyleSheet(QStringLiteral(
        "#a11yCard {"
        "  background-color: #FFF8E1;"
        "  border: none;"
        "  border-radius: 14px;"
        "}"));
    auto *a11yLayout = new QVBoxLayout(m_accessibilityCard);
    a11yLayout->setContentsMargins(14, 14, 14, 14);
    a11yLayout->setSpacing(8);
    m_accessibilityTitleLabel = new QLabel(tr("Access required"), m_accessibilityCard);
    QFont a11yFont = m_accessibilityTitleLabel->font();
    a11yFont.setBold(true);
    m_accessibilityTitleLabel->setFont(a11yFont);
    m_accessibilityTitleLabel->setStyleSheet(QStringLiteral("color: #202124;"));
    m_accessibilityBodyLabel = new QLabel(tr("The accessibility service must be enabled."),
                                          m_accessibilityCard);
    m_accessibilityBodyLabel->setWordWrap(true);
    m_accessibilityBodyLabel->setStyleSheet(QStringLiteral("color: #6B6B6B;"));
    m_enableAccessibilityButton = new QPushButton(tr("Enable service"), m_accessibilityCard);
    m_enableAccessibilityButton->setStyleSheet(primaryButtonStyle());
    m_enableAccessibilityButton->setMinimumHeight(48);
    connect(m_enableAccessibilityButton, &QPushButton::clicked,
            this, &MainWindow::enableAccessibility);
    a11yLayout->addWidget(m_accessibilityTitleLabel);
    a11yLayout->addWidget(m_accessibilityBodyLabel);
    a11yLayout->addWidget(m_enableAccessibilityButton);
    m_accessibilityCard->hide();
    contentLayout->addWidget(m_accessibilityCard);

    m_accessibilityChip = new QLabel(m_scrollContent);
    m_accessibilityChip->setText(tr("Accessibility service active"));
    m_accessibilityChip->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_accessibilityChip->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color: #E6F4EA;"
        "  color: #137333;"
        "  border-radius: 12px;"
        "  padding: 8px 12px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"));
    m_accessibilityChip->hide();
    contentLayout->addWidget(m_accessibilityChip);

    auto *speedHeader = new QHBoxLayout();
    speedHeader->setContentsMargins(0, 8, 0, 0);
    m_speedLabel = new QLabel(tr("Speed"), m_scrollContent);
    m_speedLabel->setStyleSheet(QStringLiteral("color: #202124; font-weight: 600; font-size: 14px;"));
    m_speedValueLabel = new QLabel(m_scrollContent);
    m_speedValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_speedValueLabel->setCursor(Qt::PointingHandCursor);
    m_speedValueLabel->setStyleSheet(QStringLiteral("color: #6B6B6B; font-size: 14px;"));
    m_speedValueLabel->installEventFilter(this);
    speedHeader->addWidget(m_speedLabel, 1);
    speedHeader->addWidget(m_speedValueLabel, 0);
    contentLayout->addLayout(speedHeader);

    m_speedSlider = new QSlider(Qt::Horizontal, m_scrollContent);
    m_speedSlider->setRange(50, 1000);
    m_speedSlider->setSingleStep(10);
    m_speedSlider->setPageStep(50);
    m_speedSlider->setValue(kPresetNormal);
    m_speedSlider->setToolTip(tr("Gesture speed in pixels per second."));
    m_speedSlider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal {"
        "  height: 4px; background: #DADCE0; border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  width: 20px; height: 20px; margin: -8px 0;"
        "  background: #008FD5; border-radius: 10px;"
        "}"
        "QSlider::sub-page:horizontal { background: #008FD5; border-radius: 2px; }"));
    connect(m_speedSlider, &QSlider::valueChanged, this, &MainWindow::onSpeedSliderChanged);
    contentLayout->addWidget(m_speedSlider);

    m_speedSegment = new QFrame(m_scrollContent);
    m_speedSegment->setObjectName(QStringLiteral("speedSegment"));
    m_speedSegment->setStyleSheet(segmentedFrameStyle());
    m_speedSegment->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_speedSegment->setFixedHeight(48);
    auto *segmentLayout = new QHBoxLayout(m_speedSegment);
    segmentLayout->setContentsMargins(4, 4, 4, 4);
    segmentLayout->setSpacing(4);
    m_speedSlowButton = new QPushButton(tr("Slow"), m_speedSegment);
    m_speedNormalButton = new QPushButton(tr("Normal"), m_speedSegment);
    m_speedFastButton = new QPushButton(tr("Fast"), m_speedSegment);
    m_speedPresetGroup = new QButtonGroup(this);
    m_speedPresetGroup->setExclusive(true);
    int presetId = 0;
    for (QPushButton *button : {m_speedSlowButton, m_speedNormalButton, m_speedFastButton}) {
        button->setCheckable(true);
        button->setMinimumHeight(40);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_speedPresetGroup->addButton(button, presetId++);
        segmentLayout->addWidget(button);
    }
    connect(m_speedSlowButton, &QPushButton::clicked, this, [this]() { applySpeedPreset(kPresetSlow); });
    connect(m_speedNormalButton, &QPushButton::clicked, this, [this]() { applySpeedPreset(kPresetNormal); });
    connect(m_speedFastButton, &QPushButton::clicked, this, [this]() { applySpeedPreset(kPresetFast); });
    contentLayout->addWidget(m_speedSegment);
    updateSpeedLabel(m_speedSlider->value());
    updateSpeedSegmentSelection(m_speedSlider->value());

    contentLayout->addStretch(1);
    m_scrollArea->setWidget(m_scrollContent);

    m_footerWidget = new QWidget(m_bodyWidget);
    m_footerWidget->setAutoFillBackground(true);
    {
        QPalette footerPal = m_footerWidget->palette();
        footerPal.setColor(QPalette::Window, QColor(0xF8, 0xF9, 0xFA));
        m_footerWidget->setPalette(footerPal);
    }
    auto *footerLayout = new QVBoxLayout(m_footerWidget);
    footerLayout->setContentsMargins(24, 10, 24, 12);
    footerLayout->setSpacing(4);

    m_drawButton = new QPushButton(tr("Start drawing"), m_footerWidget);
    m_drawButton->setMinimumHeight(52);
    m_drawButton->setStyleSheet(primaryButtonStyle());
    connect(m_drawButton, &QPushButton::clicked, this, &MainWindow::startDrawing);
    footerLayout->addWidget(m_drawButton);

    m_drawHintLabel = new QLabel(m_footerWidget);
    m_drawHintLabel->setWordWrap(true);
    m_drawHintLabel->setAlignment(Qt::AlignCenter);
    m_drawHintLabel->setStyleSheet(QStringLiteral("color: #6B6B6B; font-size: 12px;"));
    footerLayout->addWidget(m_drawHintLabel);

    rootLayout->addWidget(m_bodyWidget, 1);

    m_stopButton = new QPushButton(tr("Stop"), central);
    m_stopButton->hide();

    applyAndroidOrientationLayout();
    updateAndroidChrome();
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("Autograph — SVG Line-Art"));
    if (m_titleLabel)
        m_titleLabel->setText(tr("Autograph"));
    if (m_warningLabel) {
        m_warningLabel->setText(tr("Warning: do not move the mouse while drawing. "
                                   "Place the cursor in the target app before starting."));
    }
    if (!m_androidUi) {
        if (m_openButton)
            m_openButton->setText(tr("Open SVG"));
        if (m_selectAreaButton)
            m_selectAreaButton->setText(tr("Select area"));
        if (m_drawButton)
            m_drawButton->setText(tr("Draw"));
        if (m_speedSlider)
            m_speedSlider->setToolTip(tr("Cursor speed in pixels per second."));
    } else {
        if (m_drawButton)
            m_drawButton->setText(tr("Start drawing"));
        if (m_speedSlider)
            m_speedSlider->setToolTip(tr("Gesture speed in pixels per second."));
        if (m_speedLabel)
            m_speedLabel->setText(tr("Speed"));
    }
    if (m_stopButton)
        m_stopButton->setText(tr("Stop"));
    if (m_aboutButton)
        m_aboutButton->setText(tr("About"));
    if (m_aboutQtButton)
        m_aboutQtButton->setText(tr("About Qt"));
    if (m_areaSummaryTitleLabel)
        m_areaSummaryTitleLabel->setText(tr("Drawing area"));
    if (m_changeAreaButton)
        m_changeAreaButton->setText(tr("Change"));
    if (m_accessibilityTitleLabel)
        m_accessibilityTitleLabel->setText(tr("Access required"));
    if (m_accessibilityBodyLabel)
        m_accessibilityBodyLabel->setText(tr("The accessibility service must be enabled."));
    if (m_enableAccessibilityButton)
        m_enableAccessibilityButton->setText(tr("Enable service"));
    if (m_accessibilityChip)
        m_accessibilityChip->setText(tr("Accessibility service active"));
    if (m_speedSlowButton)
        m_speedSlowButton->setText(tr("Slow"));
    if (m_speedNormalButton)
        m_speedNormalButton->setText(tr("Normal"));
    if (m_speedFastButton)
        m_speedFastButton->setText(tr("Fast"));
    if (m_speedSlider)
        updateSpeedLabel(m_speedSlider->value());
    if (m_preview)
        m_preview->update();
    if (m_aboutOverlay)
        dismissAboutOverlay();
    if (m_overflowScrim)
        dismissOverflowMenu();
    updateStatus();
    syncFlowUi();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    const bool isBack = event->key() == Qt::Key_Back || event->key() == Qt::Key_Escape;
    if (isBack && m_overflowScrim) {
        dismissOverflowMenu();
        return;
    }
    if (isBack && m_aboutOverlay) {
        dismissAboutOverlay();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_drawer->isDrawing()) {
        stopDrawing();
        return;
    }
#ifdef Q_OS_ANDROID
    if (event->key() == Qt::Key_Back) {
        // Soft back backgrounds the task; do not quit during overlay workflows.
        sendToBackground();
        return;
    }
#endif
    QMainWindow::keyPressEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
#ifdef Q_OS_ANDROID
    event->ignore();
    sendToBackground();
#else
    QMainWindow::closeEvent(event);
#endif
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_androidUi) {
        applyAndroidOrientationLayout();
        updateAndroidChrome();
    }
    if (m_aboutOverlay)
        m_aboutOverlay->setGeometry(rect());
    layoutOverflowMenu();
    updatePreviewHeightLimit();
    applyTopSafeInset();
    updateFileRowText();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
#ifdef AUTOGRAFO_ENABLE_ADS
    AutografoAds::setBannerVisible(true);
#endif
    applyTopSafeInset();
    updatePreviewHeightLimit();
    syncFlowUi();
#ifdef AUTOGRAFO_ENABLE_ADS
    // The banner strip is only measurable after the Android side lays it out;
    // refresh the reserved bottom margin shortly after the window appears.
    QTimer::singleShot(300, this, &MainWindow::updateAndroidChrome);
    QTimer::singleShot(1200, this, &MainWindow::updateAndroidChrome);
#endif
}

#ifdef AUTOGRAFO_ENABLE_ADS
void MainWindow::hideEvent(QHideEvent *event)
{
    AutografoAds::setBannerVisible(false);
    QMainWindow::hideEvent(event);
}
#endif

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_overflowScrim
        && (event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonRelease)) {
        dismissOverflowMenu();
        return true;
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        if (watched == m_fileSummaryWidget && !m_extractor.isEmpty() && !m_drawingUiActive) {
            openSvg();
            return true;
        }
        if (watched == m_speedValueLabel && !m_drawingUiActive) {
            editSpeedValue();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::openSvg()
{
#ifdef Q_OS_ANDROID
    QFileDialog dialog(this, tr("Open SVG"));
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setMimeTypeFilters({QStringLiteral("image/svg+xml")});
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
        return;
    const QString path = dialog.selectedFiles().constFirst();
#else
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open SVG"),
        QString(),
        tr("SVG files (*.svg)"));

    if (path.isEmpty())
        return;
#endif

    if (!m_extractor.loadFromFile(path)) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Could not load the SVG or extract vector paths.\n"
                                "The file must contain elements such as path, line, rect, circle, and so on."));
        return;
    }

    m_loadedFileName = QFileInfo(path).fileName();
    clearAreaSelection();
    m_preview->setPolylines(m_extractor.polylines(), m_extractor.fillRule());
    updatePreviewHeightLimit();
    updateStatus();
    syncFlowUi();
}

void MainWindow::selectArea()
{
    if (m_drawer->selectsAreaExternally()) {
        if (m_extractor.isEmpty()) {
            QMessageBox::information(this, tr("Warning"),
                                     tr("Load an SVG before selecting the area."));
            return;
        }

        const ArtPolylineSet prepared = PathScaler::reorderByProximity(m_extractor.polylines());
        if (m_drawer->requestAreaSelection(prepared, m_speedSlider->value(), 20)) {
            sendToBackground();
            if (m_statusLabel)
                m_statusLabel->setText(tr("Waiting for selection in the target app..."));
            if (m_drawHintLabel)
                m_drawHintLabel->setText(tr("Waiting for selection in the target app..."));
        }
        return;
    }

    if (m_areaOverlay) {
        m_areaOverlay->close();
        m_areaOverlay->deleteLater();
    }

    m_areaOverlay = new AreaSelectorOverlay();
    connect(m_areaOverlay, &AreaSelectorOverlay::areaSelected,
            this, &MainWindow::onAreaSelected);
    connect(m_areaOverlay, &AreaSelectorOverlay::selectionCancelled,
            this, &MainWindow::onSelectionCancelled);
    connect(m_areaOverlay, &QObject::destroyed, this, [this]() {
        m_areaOverlay = nullptr;
    });

    m_areaOverlay->show();
    m_areaOverlay->raise();
    m_areaOverlay->activateWindow();
    m_areaOverlay->setFocus();
}

void MainWindow::startDrawing()
{
    if (m_extractor.isEmpty()) {
        QMessageBox::information(this, tr("Warning"),
                                 tr("Load an SVG before drawing."));
        return;
    }

    if (!m_hasArea) {
        QMessageBox::information(this, tr("Warning"),
                                 tr("Select the drawing area on the screen."));
        return;
    }

    if (m_drawer->selectsAreaExternally()) {
        if (!m_drawer->isBackendReady()) {
            m_drawer->requestEnableBackend();
            syncFlowUi();
            return;
        }

        const ArtPolylineSet prepared = PathScaler::reorderByProximity(m_extractor.polylines());
        if (m_drawer->requestReadyToDraw(prepared, m_speedSlider->value(), 20, m_targetArea)) {
            sendToBackground();
            if (m_drawHintLabel)
                m_drawHintLabel->setText(tr("Open the target app, then start drawing from the floating control."));
        }
        return;
    }

    const ArtPolylineSet scaled = PathScaler::scaleToArea(m_extractor.polylines(), m_targetArea);
    if (scaled.isEmpty()) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Could not scale the paths to the selected area."));
        return;
    }

    m_drawer->setSpeed(m_speedSlider->value());
    m_drawer->setPauseBetweenPaths(20);
    m_drawer->setPolylines(scaled);

    hide();
    constexpr int startDelayMs = 500;
    QTimer::singleShot(startDelayMs, this, [this]() {
        m_drawer->start();
    });

    setDrawingUiActive(true);
    updateStatus();
}

void MainWindow::stopDrawing()
{
    m_drawer->stop();
}

void MainWindow::onAreaSelected(const QRect &area)
{
    m_targetArea = area;
    m_hasArea = area.isValid() && area.width() > 5 && area.height() > 5;
    updateStatus();
    syncFlowUi();
}

void MainWindow::onSelectionCancelled()
{
    if (m_drawer->selectsAreaExternally())
        bringToForeground();
    updateStatus();
    syncFlowUi();
}

void MainWindow::onReturnedToApp()
{
    bringToForeground();
    updateStatus();
    syncFlowUi();
}

void MainWindow::onDrawingProgress(int currentPath, int totalPaths,
                                   int currentPoint, int totalPoints)
{
    const QString text = tr("Drawing: path %1/%2, point %3/%4")
                             .arg(currentPath)
                             .arg(totalPaths)
                             .arg(currentPoint)
                             .arg(totalPoints);
    if (m_statusLabel)
        m_statusLabel->setText(text);
    if (m_drawHintLabel)
        m_drawHintLabel->setText(text);
}

void MainWindow::onDrawingFinished()
{
    // Stay in the target app; user returns to Autograph explicitly later.
    setDrawingUiActive(false);
    if (m_statusLabel)
        m_statusLabel->setText(tr("Drawing finished."));
    if (m_drawHintLabel)
        m_drawHintLabel->setText(tr("Drawing finished."));
    syncFlowUi();
}

void MainWindow::onDrawingCancelled()
{
    setDrawingUiActive(false);
    if (m_statusLabel)
        m_statusLabel->setText(tr("Drawing cancelled."));
    if (m_drawHintLabel)
        m_drawHintLabel->setText(tr("Drawing cancelled."));
    syncFlowUi();
}

void MainWindow::onDrawingError(const QString &message)
{
    bringToForeground();
    setDrawingUiActive(false);
    QMessageBox::warning(this, tr("Error"), message);
    updateStatus();
    syncFlowUi();
}

void MainWindow::updateStatus()
{
    if (m_drawer->isDrawing())
        return;

    if (!m_androidUi && m_statusLabel) {
        const int pathCount = m_extractor.polylines().size();
        QString areaText = m_hasArea
                               ? tr("%1×%2 at (%3,%4)")
                                     .arg(m_targetArea.width())
                                     .arg(m_targetArea.height())
                                     .arg(m_targetArea.x())
                                     .arg(m_targetArea.y())
                               : tr("not set");

        m_statusLabel->setText(
            tr("Paths: %1 | Area: %2")
                .arg(pathCount)
                .arg(areaText));
    }

    if (!m_androidUi && m_drawButton)
        m_drawButton->setEnabled(!m_extractor.isEmpty() && m_hasArea && !m_drawingUiActive);
}

void MainWindow::updateFileRowText()
{
    if (!m_fileNameLabel || !m_fileMetaLabel)
        return;

    const QString name = m_loadedFileName.isEmpty() ? tr("SVG loaded") : m_loadedFileName;
    const int maxWidth = qMax(80, m_fileSummaryWidget ? m_fileSummaryWidget->width() - 80 : 200);
    m_fileNameLabel->setText(QFontMetrics(m_fileNameLabel->font())
                                 .elidedText(name, Qt::ElideMiddle, maxWidth));
    m_fileMetaLabel->setText(tr("%1 strokes").arg(m_extractor.polylines().size()));
}

void MainWindow::updateStepperVisual(bool hasSvg, bool hasArea)
{
    if (!m_stepFileLabel || !m_stepAreaLabel)
        return;

    m_stepFileLabel->setText(hasSvg ? QStringLiteral("✓  %1").arg(tr("File"))
                                    : tr("File"));
    m_stepFileLabel->setStyleSheet(hasSvg
                                       ? QStringLiteral("color: #137333; font-weight: 600; font-size: 13px;")
                                       : QStringLiteral("color: #202124; font-weight: 600; font-size: 13px;"));

    m_stepAreaLabel->setText(hasArea ? QStringLiteral("✓  %1").arg(tr("Area"))
                                     : tr("Area"));
    m_stepAreaLabel->setStyleSheet(hasArea
                                       ? QStringLiteral("color: #137333; font-weight: 600; font-size: 13px;")
                                       : QStringLiteral("color: #6B6B6B; font-weight: 600; font-size: 13px;"));

    if (m_stepConnectorLabel) {
        m_stepConnectorLabel->setStyleSheet(
            hasSvg ? QStringLiteral("color: #81C995;")
                   : QStringLiteral("color: #C4C7C5;"));
    }
}

void MainWindow::syncFlowUi()
{
    if (!m_androidUi || !m_drawer)
        return;

    const bool hasSvg = !m_extractor.isEmpty();
    const bool backendReady = m_drawer->isBackendReady();
    const bool canDraw = hasSvg && m_hasArea && backendReady && !m_drawingUiActive;

    if (m_fileSummaryWidget) {
        m_fileSummaryWidget->setVisible(hasSvg);
        if (hasSvg)
            updateFileRowText();
    }

    updateStepperVisual(hasSvg, m_hasArea);

    if (m_primaryActionButton) {
        if (!hasSvg) {
            m_primaryActionButton->setText(tr("Open SVG"));
            m_primaryActionButton->setVisible(true);
            m_primaryActionButton->setEnabled(!m_drawingUiActive);
        } else if (!m_hasArea) {
            m_primaryActionButton->setText(tr("Select area"));
            m_primaryActionButton->setVisible(true);
            m_primaryActionButton->setEnabled(!m_drawingUiActive && backendReady);
        } else {
            m_primaryActionButton->setVisible(false);
        }
    }

    if (m_areaSummaryWidget) {
        m_areaSummaryWidget->setVisible(hasSvg && m_hasArea);
        if (m_hasArea && m_areaSummarySizeLabel) {
            m_areaSummarySizeLabel->setText(
                tr("%1 × %2 px")
                    .arg(m_targetArea.width())
                    .arg(m_targetArea.height()));
        }
        if (m_changeAreaButton)
            m_changeAreaButton->setEnabled(!m_drawingUiActive && backendReady);
    }

    if (m_accessibilityCard)
        m_accessibilityCard->setVisible(hasSvg && !backendReady);
    if (m_accessibilityChip)
        m_accessibilityChip->setVisible(hasSvg && backendReady);

    if (m_drawButton) {
        m_drawButton->setEnabled(canDraw);
        m_drawButton->setVisible(true);
    }
    if (m_drawHintLabel) {
        if (!hasSvg)
            m_drawHintLabel->setText(tr("Open an SVG to begin."));
        else if (!backendReady)
            m_drawHintLabel->setText(tr("Enable accessibility to continue."));
        else if (!m_hasArea)
            m_drawHintLabel->setText(tr("Disabled until you select the area."));
        else
            m_drawHintLabel->setText(tr("Opens the floating control over the target app."));
    }

    if (m_speedSlider)
        m_speedSlider->setEnabled(!m_drawingUiActive);
    for (QPushButton *button : {m_speedSlowButton, m_speedNormalButton, m_speedFastButton}) {
        if (button)
            button->setEnabled(!m_drawingUiActive);
    }

    updatePreviewHeightLimit();
    if (m_scrollArea && hasSvg) {
        // Prefer the next-step / status widget. Never scroll to the speed
        // presets — that pushed the area summary and a11y card off-screen.
        QWidget *scrollTarget = nullptr;
        if (m_accessibilityCard && m_accessibilityCard->isVisible())
            scrollTarget = m_accessibilityCard;
        else if (m_areaSummaryWidget && m_areaSummaryWidget->isVisible())
            scrollTarget = m_areaSummaryWidget;
        else if (m_primaryActionButton && m_primaryActionButton->isVisible())
            scrollTarget = m_primaryActionButton;
        else if (m_accessibilityChip && m_accessibilityChip->isVisible())
            scrollTarget = m_accessibilityChip;

        if (scrollTarget) {
            QTimer::singleShot(0, this, [this, scrollTarget]() {
                if (!m_scrollArea || !scrollTarget)
                    return;
                m_scrollArea->ensureWidgetVisible(scrollTarget, 0, 24);
            });
        }
    }
}

void MainWindow::clearAreaSelection()
{
    m_hasArea = false;
    m_targetArea = QRect();
    m_drawer->clearSelectedArea();
}

void MainWindow::updateSpeedLabel(int pixelsPerSecond)
{
    if (m_androidUi) {
        if (m_speedValueLabel)
            m_speedValueLabel->setText(tr("%1 px/s").arg(pixelsPerSecond));
    } else if (m_speedLabel) {
        m_speedLabel->setText(tr("Speed: %1 px/s").arg(pixelsPerSecond));
    }
}

void MainWindow::updateSpeedSegmentSelection(int pixelsPerSecond)
{
    if (!m_androidUi || !m_speedPresetGroup)
        return;

    m_updatingSpeedUi = true;
    QAbstractButton *checked = nullptr;
    if (pixelsPerSecond == kPresetSlow)
        checked = m_speedSlowButton;
    else if (pixelsPerSecond == kPresetNormal)
        checked = m_speedNormalButton;
    else if (pixelsPerSecond == kPresetFast)
        checked = m_speedFastButton;

    for (QAbstractButton *button : m_speedPresetGroup->buttons())
        button->setChecked(button == checked);
    m_updatingSpeedUi = false;
}

void MainWindow::onSpeedSliderChanged(int pixelsPerSecond)
{
    updateSpeedLabel(pixelsPerSecond);
    if (!m_updatingSpeedUi)
        updateSpeedSegmentSelection(pixelsPerSecond);
}

void MainWindow::editSpeedValue()
{
    bool ok = false;
    const int value = QInputDialog::getInt(
        this,
        tr("Drawing speed"),
        tr("Pixels per second:"),
        m_speedSlider->value(),
        m_speedSlider->minimum(),
        m_speedSlider->maximum(),
        10,
        &ok);
    if (ok)
        m_speedSlider->setValue(value);
}

void MainWindow::applySpeedPreset(int pixelsPerSecond)
{
    if (!m_speedSlider)
        return;
    m_updatingSpeedUi = true;
    m_speedSlider->setValue(pixelsPerSecond);
    updateSpeedLabel(pixelsPerSecond);
    updateSpeedSegmentSelection(pixelsPerSecond);
    m_updatingSpeedUi = false;
}

void MainWindow::enableAccessibility()
{
    m_drawer->requestEnableBackend();
}

void MainWindow::onApplicationStateChanged(Qt::ApplicationState state)
{
    if (state == Qt::ApplicationActive) {
        installAppTranslations();
        // After overlay/drawing workflows the window may stay hidden in the
        // background; restore it when the user opens Autograph again.
        if (!isVisible()) {
            show();
            raise();
            activateWindow();
        }
        QTimer::singleShot(400, this, &MainWindow::syncFlowUi);
    }
}

void MainWindow::updatePreviewHeightLimit()
{
    // The preview lives inside the scroll column in portrait, so it only needs
    // its own height; every other widget flows in the scroll area and can never
    // be clipped. No global height budget is computed anymore.
    if (!m_androidUi || !m_preview || !m_previewContainer)
        return;

    auto *previewLayout = qobject_cast<QVBoxLayout *>(m_previewContainer->layout());

    if (m_androidLandscape) {
        // Landscape: preview fills the whole left column.
        if (previewLayout)
            previewLayout->setContentsMargins(16, 8, 8, 8);
        m_preview->setMinimumHeight(0);
        m_preview->setMaximumHeight(QWIDGETSIZE_MAX);
        m_previewContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        return;
    }

    // Portrait: side margins come from the scroll content layout.
    if (previewLayout)
        previewLayout->setContentsMargins(0, 4, 0, 4);
    const int target = qBound(120, height() * 30 / 100, 340);
    m_preview->setMinimumHeight(target);
    m_preview->setMaximumHeight(target);
    m_previewContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void MainWindow::updateAndroidChrome()
{
    if (!m_androidUi || !m_androidRootLayout)
        return;

    int bottomReserve = 0;
#ifdef AUTOGRAFO_ENABLE_ADS
    bottomReserve = androidAdsBottomReserve();
#endif
    m_androidRootLayout->setContentsMargins(0, 0, 0, bottomReserve);

    if (m_aboutOverlay) {
        if (auto *overlayLayout = qobject_cast<QVBoxLayout *>(m_aboutOverlay->layout()))
            overlayLayout->setContentsMargins(24, 24, 24, androidOverlayBottomInset(this));
    }

    if (m_footerWidget) {
        auto *footerLayout = qobject_cast<QVBoxLayout *>(m_footerWidget->layout());
        if (footerLayout) {
            if (m_androidLandscape)
                footerLayout->setContentsMargins(16, 8, 16, 8);
            else
                footerLayout->setContentsMargins(24, 10, 24, 12);
        }
    }
    if (m_drawHintLabel)
        m_drawHintLabel->setVisible(!m_androidLandscape);
    if (m_scrollContent) {
        auto *contentLayout = qobject_cast<QVBoxLayout *>(m_scrollContent->layout());
        if (contentLayout) {
            if (m_androidLandscape) {
                contentLayout->setContentsMargins(8, 4, 16, 16);
                contentLayout->setSpacing(8);
            } else {
                contentLayout->setContentsMargins(24, 4, 24, 24);
                contentLayout->setSpacing(12);
            }
        }
    }
}

void MainWindow::applyAndroidOrientationLayout()
{
    if (!m_androidUi || !m_bodyGrid || !m_previewContainer || !m_scrollArea || !m_footerWidget)
        return;

    const bool landscape = width() > height();
    if (landscape == m_androidLandscape && m_bodyGrid->count() > 0)
        return;
    m_androidLandscape = landscape;

    auto *contentLayout = qobject_cast<QVBoxLayout *>(m_scrollContent->layout());

    // Detach without deleting so we can re-slot widgets for the new orientation.
    m_bodyGrid->removeWidget(m_previewContainer);
    m_bodyGrid->removeWidget(m_scrollArea);
    m_bodyGrid->removeWidget(m_footerWidget);
    if (contentLayout)
        contentLayout->removeWidget(m_previewContainer);

    // Clear every stretch left over from the previous orientation.
    for (int row = 0; row < m_bodyGrid->rowCount(); ++row)
        m_bodyGrid->setRowStretch(row, 0);
    for (int column = 0; column < m_bodyGrid->columnCount(); ++column)
        m_bodyGrid->setColumnStretch(column, 0);

    if (landscape) {
        // Preview fills the left column; everything else scrolls on the right,
        // with the action footer pinned under the scroll column.
        m_bodyGrid->addWidget(m_previewContainer, 0, 0, 2, 1);
        m_bodyGrid->addWidget(m_scrollArea, 0, 1);
        m_bodyGrid->addWidget(m_footerWidget, 1, 1);
        m_bodyGrid->setRowStretch(0, 1);
        m_bodyGrid->setColumnStretch(0, 5);
        m_bodyGrid->setColumnStretch(1, 6);
        m_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    } else {
        // Portrait: the preview is simply the first scrollable card, so the
        // scroll area owns all vertical space and nothing can be clipped.
        if (contentLayout)
            contentLayout->insertWidget(0, m_previewContainer);
        m_bodyGrid->addWidget(m_scrollArea, 0, 0);
        m_bodyGrid->addWidget(m_footerWidget, 1, 0);
        m_bodyGrid->setRowStretch(0, 1);
        m_bodyGrid->setColumnStretch(0, 1);
        m_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    updatePreviewHeightLimit();

    m_previewContainer->show();
    m_scrollArea->show();
    m_footerWidget->show();
    if (m_scrollArea && m_scrollArea->verticalScrollBar())
        m_scrollArea->verticalScrollBar()->setValue(0);
}

void MainWindow::applyTopSafeInset()
{
    if (!m_androidUi || !m_headerWidget)
        return;

    // Qt for Android already places the window below the status bar — the About
    // overlay proves it: it touches the window top with no gap. Every attempt to
    // compensate here (status bar resource, safeAreaMargins) only created or
    // doubled the top gap, so no manual inset is applied at all.
    const int topInset = 0;
#ifdef Q_OS_ANDROID
    if (windowHandle()) {
        qInfo() << "autografo-layout:"
                << "windowGeo" << windowHandle()->geometry()
                << "screenGeo" << (windowHandle()->screen()
                                       ? windowHandle()->screen()->geometry() : QRect())
                << "safeMargins" << windowHandle()->safeAreaMargins()
                << "topInset" << topInset
#ifdef AUTOGRAFO_ENABLE_ADS
                << "bottomReserve" << androidAdsBottomReserve()
#endif
            ;
    }
#endif
    m_headerWidget->setContentsMargins(24, topInset + 8, 12, 8);
}

void MainWindow::showAboutQt()
{
#ifdef Q_OS_ANDROID
    showAboutOverlay(
        tr("About Qt"),
        tr("<p>This program uses Qt version %1.</p>"
           "<p>Qt is a C++ toolkit for cross-platform application development.</p>"
           "<p>See <a href=\"https://www.qt.io/\">https://www.qt.io/</a> for more information.</p>")
            .arg(QString::fromLatin1(qVersion())));
#else
    QMessageBox::aboutQt(this, tr("About Qt"));
#endif
}

void MainWindow::showAbout()
{
    const QString html =
        tr("Autograph - Turn SVG line art into real strokes on any app<br>"
           "Copyright (C) 2026 Andrius da Costa Ribas &lt;andriusmao@gmail.com&gt;<br>"
           "Autograph is free software under the GNU Lesser General Public License "
           "version 2.1 or later (LGPL-2.1-or-later) - Sources are available in "
           "<a href=\"https://github.com/xiluembo/autograph\">https://github.com/xiluembo/autograph</a>");

#ifdef Q_OS_ANDROID
    showAboutOverlay(tr("About Autograph"), html);
#else
    QDialog dialog(this);
    dialog.setWindowTitle(tr("About Autograph"));
    applyReadableAboutColors(&dialog);

    auto *layout = new QVBoxLayout(&dialog);
    auto *label = new QLabel(&dialog);
    applyReadableAboutColors(label);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    label->setOpenExternalLinks(true);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label->setText(readableAboutHtml(html));
    layout->addWidget(label);

    auto *okButton = new QPushButton(tr("OK"), &dialog);
    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(okButton, 0, Qt::AlignRight);

    dialog.exec();
#endif
}

void MainWindow::showAboutOverlay(const QString &title, const QString &html)
{
    dismissAboutOverlay();

    m_aboutOverlay = new QWidget(this);
    m_aboutOverlay->setObjectName(QStringLiteral("aboutOverlay"));
    applyReadableAboutColors(m_aboutOverlay);
    m_aboutOverlay->setStyleSheet(
        QStringLiteral("#aboutOverlay, #aboutOverlay QScrollArea, "
                       "#aboutOverlay QScrollArea > QWidget, #aboutOverlay QLabel {"
                       "  background-color: #ffffff;"
                       "  color: #111111;"
                       "}"
                       "#aboutOverlay QPushButton {"
                       "  background-color: #008FD5;"
                       "  color: #ffffff;"
                       "  border: none;"
                       "  border-radius: 14px;"
                       "  font-weight: 600;"
                       "  padding: 14px 16px;"
                       "}"));

    auto *layout = new QVBoxLayout(m_aboutOverlay);
    layout->setContentsMargins(24, 24, 24, androidOverlayBottomInset(this));
    layout->setSpacing(16);

    auto *titleLabel = new QLabel(title, m_aboutOverlay);
    applyReadableAboutColors(titleLabel);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(qMax(titleFont.pointSize() + 4, 20));
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto *body = new QLabel(m_aboutOverlay);
    applyReadableAboutColors(body);
    body->setWordWrap(true);
    body->setTextFormat(Qt::RichText);
    body->setOpenExternalLinks(true);
    body->setTextInteractionFlags(Qt::TextBrowserInteraction);
    body->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QFont bodyFont = body->font();
    bodyFont.setPointSize(qMax(bodyFont.pointSize(), 16));
    body->setFont(bodyFont);
    body->setText(readableAboutHtml(html));

    auto *scroll = new QScrollArea(m_aboutOverlay);
    applyReadableAboutColors(scroll);
    applyReadableAboutColors(scroll->viewport());
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(body);
    layout->addWidget(scroll, 1);

    auto *okButton = new QPushButton(tr("OK"), m_aboutOverlay);
    okButton->setMinimumHeight(52);
    connect(okButton, &QPushButton::clicked, this, &MainWindow::dismissAboutOverlay);
    layout->addWidget(okButton);

    m_aboutOverlay->setGeometry(rect());
    m_aboutOverlay->raise();
    m_aboutOverlay->show();
}

void MainWindow::dismissAboutOverlay()
{
    if (!m_aboutOverlay)
        return;
    m_aboutOverlay->hide();
    m_aboutOverlay->deleteLater();
    m_aboutOverlay = nullptr;
}

void MainWindow::showOverflowMenu()
{
    if (m_overflowScrim) {
        dismissOverflowMenu();
        return;
    }

    dismissAboutOverlay();

    m_overflowScrim = new QWidget(this);
    m_overflowScrim->setObjectName(QStringLiteral("overflowScrim"));
    m_overflowScrim->setStyleSheet(QStringLiteral(
        "#overflowScrim { background-color: rgba(0, 0, 0, 0.18); }"));
    m_overflowScrim->installEventFilter(this);

    m_overflowPanel = new QWidget(m_overflowScrim);
    m_overflowPanel->setObjectName(QStringLiteral("overflowPanel"));
    m_overflowPanel->setStyleSheet(QStringLiteral(
        "#overflowPanel {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #DADCE0;"
        "  border-radius: 12px;"
        "}"
        "#overflowPanel QPushButton {"
        "  background-color: transparent;"
        "  color: #202124;"
        "  border: none;"
        "  text-align: left;"
        "  padding: 14px 20px;"
        "  font-size: 15px;"
        "}"
        "#overflowPanel QPushButton:pressed {"
        "  background-color: #E8F4FB;"
        "}"));
    auto *panelLayout = new QVBoxLayout(m_overflowPanel);
    panelLayout->setContentsMargins(0, 6, 0, 6);
    panelLayout->setSpacing(0);

    auto *aboutBtn = new QPushButton(tr("About"), m_overflowPanel);
    aboutBtn->setMinimumHeight(48);
    connect(aboutBtn, &QPushButton::clicked, this, [this]() {
        dismissOverflowMenu();
        showAbout();
    });
    panelLayout->addWidget(aboutBtn);

    auto *aboutQtBtn = new QPushButton(tr("About Qt"), m_overflowPanel);
    aboutQtBtn->setMinimumHeight(48);
    connect(aboutQtBtn, &QPushButton::clicked, this, [this]() {
        dismissOverflowMenu();
        showAboutQt();
    });
    panelLayout->addWidget(aboutQtBtn);

    layoutOverflowMenu();
    m_overflowScrim->show();
    m_overflowScrim->raise();
}

void MainWindow::dismissOverflowMenu()
{
    if (!m_overflowScrim)
        return;
    m_overflowScrim->hide();
    m_overflowScrim->deleteLater();
    m_overflowScrim = nullptr;
    m_overflowPanel = nullptr;
}

void MainWindow::sendToBackground()
{
#ifdef Q_OS_ANDROID
    hide();
    QJniObject::callStaticMethod<void>("com/autografo/android/AutografoBridge",
                                       "moveTaskToBackground", "()V");
#else
    hide();
#endif
}

void MainWindow::bringToForeground()
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("com/autografo/android/AutografoBridge",
                                       "bringAppToForeground", "()V");
#endif
    show();
    raise();
    activateWindow();
}

void MainWindow::layoutOverflowMenu()
{
    if (!m_overflowScrim)
        return;

    m_overflowScrim->setGeometry(rect());

    if (!m_overflowPanel || !m_menuButton)
        return;

    m_overflowPanel->adjustSize();
    const QSize panelSize = m_overflowPanel->sizeHint().expandedTo(QSize(200, 0));
    const QPoint anchor = m_menuButton->mapTo(this, QPoint(m_menuButton->width(), m_menuButton->height()));
    int x = anchor.x() - panelSize.width();
    int y = anchor.y() + 4;
    x = qBound(12, x, width() - panelSize.width() - 12);
    y = qBound(12, y, height() - panelSize.height() - 12);
    m_overflowPanel->setGeometry(x, y, panelSize.width(), panelSize.height());
    m_overflowPanel->raise();
}

void MainWindow::setDrawingUiActive(bool active)
{
    m_drawingUiActive = active;
    if (!m_androidUi) {
        if (m_openButton)
            m_openButton->setEnabled(!active);
        if (m_selectAreaButton)
            m_selectAreaButton->setEnabled(!active);
        if (m_drawButton)
            m_drawButton->setEnabled(!active && !m_extractor.isEmpty() && m_hasArea);
        if (m_stopButton)
            m_stopButton->setEnabled(active);
        if (m_speedSlider)
            m_speedSlider->setEnabled(!active);
        return;
    }

    if (m_stopButton)
        m_stopButton->setEnabled(active);
    syncFlowUi();
}

#if defined(AUTOGRAFO_ENABLE_ADS) && defined(Q_OS_ANDROID)
/// Called from Java whenever the banner appears or disappears, so the layout
/// reserves exactly the space the ad actually occupies.
extern "C" JNIEXPORT void JNICALL
Java_com_autografo_android_AutografoAds_nativeAdsReserveChanged(JNIEnv *, jclass)
{
    QMetaObject::invokeMethod(qApp, [] {
        if (s_adsChromeWindow)
            QMetaObject::invokeMethod(s_adsChromeWindow, "updateAndroidChrome",
                                      Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
#endif
