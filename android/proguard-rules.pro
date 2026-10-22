# Autograph
# Copyright (C) 2026 Andrius da Costa Ribas <andriusmao@gmail.com>
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Release minify (R8). Keep names that JNI, the manifest, and Class.forName
# look up as strings. The Play Console picks the mapping up from the AAB.

-keepattributes SourceFile,LineNumberTable,InnerClasses,Signature,Exceptions,*Annotation*
-renamesourcefileattribute SourceFile

# Qt loads these by class name from the bindings / QtActivity loader.
-keep class org.qtproject.** { *; }
-dontwarn org.qtproject.**

# JNI (QJniObject), AccessibilityService, Activity, and AdMob reflection.
-keep class com.autografo.android.** { *; }

-keepclasseswithmembernames class * {
    native <methods>;
}

-keep public class * extends android.app.Activity
-keep public class * extends android.app.Application
-keep public class * extends android.app.Service
-keep public class * extends android.content.ContentProvider

-keep class **.R
-keep class **.R$* { <fields>; }
