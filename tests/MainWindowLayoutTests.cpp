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
#include "PathPreviewWidget.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>

// Exercise the actual mobile widget tree with the desktop backend, so these
// checks can run headlessly without an Android device or accessibility service.
class MainWindowLayoutTests
{
    static void settle()
    {
        for (int i = 0; i < 12; ++i)
            QCoreApplication::processEvents(QEventLoop::AllEvents);
    }

    static void require(bool condition, const char *message)
    {
        if (!condition)
            qFatal("%s", message);
    }

    static void checkBounds(MainWindow &window, const QSize &size)
    {
        settle();
        require(window.size() == size, "Layout expanded the native window bounds");
        for (QWidget *widget : {static_cast<QWidget *>(window.m_menuButton),
                                window.m_footerWidget,
                                static_cast<QWidget *>(window.m_scrollArea)}) {
            const int right = widget->mapTo(&window, QPoint(widget->width(), 0)).x();
            require(right <= window.width(), "Main UI extends beyond the right edge");
        }
        require(window.m_menuButton->isVisible(), "Overflow menu is missing");
        require(window.m_scrollContent->width() <= window.m_scrollArea->viewport()->width(),
                "Scroll content is wider than the viewport");
    }

public:
    static void run()
    {
        for (int width : {360, 393, 412}) {
            MainWindow window(nullptr, true);
            require(!window.m_androidLandscape,
                    "Construction used the provisional landscape window size");
            const QSize portrait(width, 800);
            window.resize(portrait);
            window.show();
            checkBounds(window, portrait);
            require(!window.m_androidLandscape, "Initial portrait layout is incorrect");

            // A loaded file and the active-service label must not introduce
            // horizontal minimum sizes, even after a wider previous layout.
            window.m_loadedFileName = QString(180, QLatin1Char('W')) + ".svg";
            window.m_fileSummaryWidget->show();
            window.m_accessibilityChip->setText("Serviço de acessibilidade ativo");
            window.m_accessibilityChip->show();
            window.updateFileRowText();
            checkBounds(window, portrait);

            const QSize landscape(800, width);
            window.resize(landscape);
            checkBounds(window, landscape);
            require(window.m_androidLandscape, "Landscape layout did not update");
            window.resize(portrait);
            checkBounds(window, portrait);
            require(!window.m_androidLandscape, "Portrait layout did not recover");

            window.hide();
            window.show();
            checkBounds(window, portrait);

            // Model a content-only resize (safe area / native chrome), without
            // changing the top-level dimensions or rotating the device.
            window.centralWidget()->setContentsMargins(0, 0, 24, 0);
            checkBounds(window, portrait);
        }
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    MainWindowLayoutTests::run();
    return 0;
}
