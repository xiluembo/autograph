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

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QMetaObject>
#include <QTranslator>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <jni.h>
#endif

namespace {

QTranslator *s_qtTranslator = nullptr;
QTranslator *s_appTranslator = nullptr;
QString s_installedTag;

QString androidConfigurationLanguageTag()
{
#ifdef Q_OS_ANDROID
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return {};

    const QJniObject resources = context.callObjectMethod(
        "getResources", "()Landroid/content/res/Resources;");
    if (!resources.isValid())
        return {};

    const QJniObject configuration = resources.callObjectMethod(
        "getConfiguration", "()Landroid/content/res/Configuration;");
    if (!configuration.isValid())
        return {};

    const QJniObject locales = configuration.callObjectMethod(
        "getLocales", "()Landroid/os/LocaleList;");
    if (!locales.isValid() || locales.callMethod<jint>("size") <= 0)
        return {};

    const QJniObject first = locales.callObjectMethod("get", "(I)Ljava/util/Locale;", 0);
    if (!first.isValid())
        return {};

    const QJniObject tag = first.callObjectMethod("toLanguageTag", "()Ljava/lang/String;");
    return tag.isValid() ? tag.toString() : QString();
#else
    return {};
#endif
}

void replaceTranslator(QCoreApplication &app, QTranslator *&slot, QTranslator *fresh)
{
    if (slot) {
        app.removeTranslator(slot);
        delete slot;
        slot = nullptr;
    }
    if (!fresh)
        return;
    if (app.installTranslator(fresh))
        slot = fresh;
    else
        delete fresh;
}

} // namespace

QString currentUiLanguageTag()
{
    const QString androidTag = androidConfigurationLanguageTag();
    if (!androidTag.isEmpty())
        return androidTag;

    const QStringList uiLanguages = QLocale::system().uiLanguages();
    if (!uiLanguages.isEmpty())
        return uiLanguages.first();

    return QLocale::system().name().replace(QLatin1Char('_'), QLatin1Char('-'));
}

void installAppTranslations(QCoreApplication *app)
{
    if (!app)
        app = QCoreApplication::instance();
    if (!app)
        return;

    const QString tag = currentUiLanguageTag();
    if (!s_installedTag.isEmpty()
            && s_installedTag.compare(tag, Qt::CaseInsensitive) == 0)
        return;
    s_installedTag = tag;

    // Build a locale from the primary tag only. QLocale::system() includes
    // every preferred language, and QTranslator would then load pt_BR even
    // when the user switched the device to English.
    const QLocale locale(tag);
    QLocale::setDefault(locale);

    const QString qtPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    auto *qtTranslator = new QTranslator(app);
    if (!(qtTranslator->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"), qtPath)
          || qtTranslator->load(locale, QStringLiteral("qt"), QStringLiteral("_"), qtPath))) {
        delete qtTranslator;
        qtTranslator = nullptr;
    }
    replaceTranslator(*app, s_qtTranslator, qtTranslator);

    auto *appTranslator = new QTranslator(app);
    if (!appTranslator->load(locale, QStringLiteral("autografo"), QStringLiteral("_"),
                             QStringLiteral(":/i18n"))) {
        delete appTranslator;
        appTranslator = nullptr;
    }
    replaceTranslator(*app, s_appTranslator, appTranslator);
}

#ifdef Q_OS_ANDROID
extern "C" JNIEXPORT void JNICALL
Java_com_autografo_android_AutografoActivity_nativeLocaleChanged(JNIEnv *, jclass)
{
    if (QCoreApplication *app = QCoreApplication::instance()) {
        QMetaObject::invokeMethod(app, [] {
            installAppTranslations();
        }, Qt::QueuedConnection);
    }
}
#endif
