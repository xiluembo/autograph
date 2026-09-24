#
# Autograph
# Copyright (C) 2026 Andrius da Costa Ribas <andriusmao@gmail.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

# Copies android/ into the build tree, overlays generated launcher icons, and
# writes the AdMob toggle used by Gradle.

if(NOT DEFINED SRC_DIR OR NOT DEFINED DST_DIR)
    message(FATAL_ERROR "SRC_DIR and DST_DIR are required")
endif()

file(MAKE_DIRECTORY "${DST_DIR}")
file(COPY "${SRC_DIR}/" DESTINATION "${DST_DIR}")

if(ICON_RES_DIR AND EXISTS "${ICON_RES_DIR}")
    file(COPY "${ICON_RES_DIR}/" DESTINATION "${DST_DIR}/res")
endif()

if(AUTOGRAFO_ENABLE_ADS)
    set(_enable "true")
else()
    set(_enable "false")
endif()

if(NOT AUTOGRAFO_ADMOB_APP_ID)
    set(AUTOGRAFO_ADMOB_APP_ID "ca-app-pub-3940256099942544~3347511713")
endif()
if(NOT AUTOGRAFO_ADMOB_BANNER_UNIT_ID)
    set(AUTOGRAFO_ADMOB_BANNER_UNIT_ID "ca-app-pub-3940256099942544/6300978111")
endif()

if(NOT DEFINED AUTOGRAFO_ADMOB_TEST_DEVICE_IDS)
    set(AUTOGRAFO_ADMOB_TEST_DEVICE_IDS "")
endif()

file(WRITE "${DST_DIR}/autografo-ads.properties"
    "enable=${_enable}\nadmobAppId=${AUTOGRAFO_ADMOB_APP_ID}\nadmobBannerUnitId=${AUTOGRAFO_ADMOB_BANNER_UNIT_ID}\nadmobTestDeviceIds=${AUTOGRAFO_ADMOB_TEST_DEVICE_IDS}\n"
)

set(_manifest "${DST_DIR}/AndroidManifest.xml")
file(READ "${_manifest}" _manifest_text)
if(AUTOGRAFO_ENABLE_ADS)
    set(_admob_meta
        "        <meta-data
            android:name=\"com.google.android.gms.ads.APPLICATION_ID\"
            android:value=\"${AUTOGRAFO_ADMOB_APP_ID}\" />")
    set(_admob_queries
        "    <queries>
        <package android:name=\"com.android.vending\" />
        <package android:name=\"com.google.android.gms\" />
        <intent>
            <action android:name=\"android.intent.action.VIEW\" />
            <category android:name=\"android.intent.category.BROWSABLE\" />
            <data android:scheme=\"https\" />
        </intent>
        <intent>
            <action android:name=\"android.intent.action.VIEW\" />
            <category android:name=\"android.intent.category.BROWSABLE\" />
            <data android:scheme=\"http\" />
        </intent>
        <intent>
            <action android:name=\"android.intent.action.VIEW\" />
            <data android:scheme=\"market\" />
        </intent>
        <intent>
            <action android:name=\"android.support.customtabs.action.CustomTabsService\" />
        </intent>
    </queries>")
    set(_admob_activities
        "        <activity
            android:name=\"com.google.android.gms.ads.AdActivity\"
            android:configChanges=\"keyboard|keyboardHidden|orientation|screenLayout|uiMode|screenSize|smallestScreenSize\"
            android:exported=\"false\"
            android:hardwareAccelerated=\"true\" />")
else()
    set(_admob_meta "")
    set(_admob_queries "")
    set(_admob_activities "")
endif()
string(REPLACE "    <!-- AUTOGRAFO_ADMOB_QUERIES -->" "${_admob_queries}" _manifest_text "${_manifest_text}")
string(REPLACE "        <!-- AUTOGRAFO_ADMOB_METADATA -->" "${_admob_meta}" _manifest_text "${_manifest_text}")
string(REPLACE "        <!-- AUTOGRAFO_ADMOB_ACTIVITIES -->" "${_admob_activities}" _manifest_text "${_manifest_text}")
file(WRITE "${_manifest}" "${_manifest_text}")
