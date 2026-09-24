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

#include "AppTranslations.h"
#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Autograph"));
    app.setApplicationVersion(QStringLiteral("1.0"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/autografo.svg")));

    installAppTranslations(&app);

    MainWindow window;
#ifdef Q_OS_ANDROID
    // Overlay workflows hide the window; do not quit the process when that happens.
    app.setQuitOnLastWindowClosed(false);
#endif
    window.show();

    return app.exec();
}
