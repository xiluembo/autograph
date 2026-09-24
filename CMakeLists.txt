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

cmake_minimum_required(VERSION 3.21)

project(autografo LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Android-only AdMob banner. Off by default so regular APKs ship without ads.
option(AUTOGRAFO_ENABLE_ADS "Enable a non-intrusive AdMob banner on the Android main screen" OFF)
set(AUTOGRAFO_ADMOB_APP_ID "ca-app-pub-3940256099942544~3347511713" CACHE STRING
    "AdMob application ID used when AUTOGRAFO_ENABLE_ADS=ON")
set(AUTOGRAFO_ADMOB_BANNER_UNIT_ID "ca-app-pub-3940256099942544/6300978111" CACHE STRING
    "AdMob banner ad unit ID used when AUTOGRAFO_ENABLE_ADS=ON")
set(AUTOGRAFO_ADMOB_TEST_DEVICE_IDS "" CACHE STRING
    "Comma-separated AdMob test device IDs (development builds); test devices always receive test ads")

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

# lupdate/lrelease ship with desktop (host) kits, not with Qt for Android.
if(ANDROID AND QT_HOST_PATH)
    find_package(Qt6 REQUIRED COMPONENTS LinguistTools
        PATHS "${QT_HOST_PATH}"
        NO_DEFAULT_PATH)
else()
    find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
endif()

qt_standard_project_setup()
if(COMMAND qt_policy)
    qt_policy(SET QTP0002 NEW)
endif()

include(cmake/AppIcon.cmake)

set(SOURCES
    src/main.cpp
    src/AppTranslations.cpp
    src/MainWindow.cpp
    src/SvgPathExtractor.cpp
    src/ArtPathNormalizer.cpp
    src/PathPreviewWidget.cpp
    src/AreaSelectorOverlay.cpp
    src/MouseDrawer.cpp
    src/PathScaler.cpp
)

set(HEADERS
    src/AppTranslations.h
    src/MainWindow.h
    src/SvgPathExtractor.h
    src/ArtPathNormalizer.h
    src/PathPreviewWidget.h
    src/AreaSelectorOverlay.h
    src/MouseDrawer.h
    src/PathScaler.h
    src/DrawingBackend.h
)

if(ANDROID)
    list(APPEND SOURCES src/AndroidGestureDrawer.cpp)
    list(APPEND HEADERS src/AndroidGestureDrawer.h)
endif()

qt_add_executable(autografo ${SOURCES} ${HEADERS})

autografo_setup_app_icon(autografo
    SVG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/autografo.svg"
)

target_link_libraries(autografo PRIVATE Qt6::Widgets Qt6::Svg)

if(WIN32)
    target_link_libraries(autografo PRIVATE user32)
endif()

# Embed compiled .qm files as Qt resources so they ship on Desktop and Android (APK).
# Scan AndroidGestureDrawer even on Desktop so those strings stay in the .qm files.
qt_add_translations(autografo
    IMMEDIATE_CALL
    SOURCE_TARGETS autografo
    TS_FILES
        translations/autografo_pt_BR.ts
        translations/autografo_es.ts
    RESOURCE_PREFIX "/i18n"
    SOURCES
        src/AndroidGestureDrawer.cpp
)

if(ANDROID AND AUTOGRAFO_ENABLE_ADS)
    target_sources(autografo PRIVATE src/AutografoAds.cpp src/AutografoAds.h)
    target_compile_definitions(autografo PRIVATE AUTOGRAFO_ENABLE_ADS)
elseif(AUTOGRAFO_ENABLE_ADS)
    message(STATUS "AUTOGRAFO_ENABLE_ADS is ignored because this is not an Android build.")
endif()

if(ANDROID)
    set(AUTOGRAFO_ANDROID_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/android-package")
    set(_prepare_android_package
        "${CMAKE_COMMAND}"
        "-DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}/android"
        "-DDST_DIR=${AUTOGRAFO_ANDROID_PACKAGE_DIR}"
        "-DICON_RES_DIR=${AUTOGRAFO_GENERATED_ANDROID_ICON_RES}"
        "-DAUTOGRAFO_ENABLE_ADS=${AUTOGRAFO_ENABLE_ADS}"
        "-DAUTOGRAFO_ADMOB_APP_ID=${AUTOGRAFO_ADMOB_APP_ID}"
        "-DAUTOGRAFO_ADMOB_BANNER_UNIT_ID=${AUTOGRAFO_ADMOB_BANNER_UNIT_ID}"
        "-DAUTOGRAFO_ADMOB_TEST_DEVICE_IDS=${AUTOGRAFO_ADMOB_TEST_DEVICE_IDS}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PrepareAndroidPackage.cmake"
    )

    execute_process(COMMAND ${_prepare_android_package} RESULT_VARIABLE _android_package_result)
    if(NOT _android_package_result EQUAL 0)
        message(FATAL_ERROR "Failed to prepare Android package sources")
    endif()

    file(GLOB_RECURSE AUTOGRAFO_ANDROID_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/android/*")
    add_custom_command(
        OUTPUT "${AUTOGRAFO_ANDROID_PACKAGE_DIR}/.stamp"
        COMMAND ${_prepare_android_package}
        COMMAND "${CMAKE_COMMAND}" -E touch "${AUTOGRAFO_ANDROID_PACKAGE_DIR}/.stamp"
        DEPENDS
            ${AUTOGRAFO_ANDROID_FILES}
            autografo_app_icons
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PrepareAndroidPackage.cmake"
        COMMENT "Preparing Android package sources"
        VERBATIM
    )
    add_custom_target(autografo_android_package
        DEPENDS "${AUTOGRAFO_ANDROID_PACKAGE_DIR}/.stamp")
    add_dependencies(autografo autografo_android_package)

    set_target_properties(autografo PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${AUTOGRAFO_ANDROID_PACKAGE_DIR}"
        QT_ANDROID_PACKAGE_NAME "br.pushx.autograph"
        QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
        QT_ANDROID_MIN_SDK_VERSION 31
        QT_ANDROID_TARGET_SDK_VERSION 36
    )
endif()

if(BUILD_TESTING)
    qt_add_executable(autografo_tests
        tests/GeometryTests.cpp
        src/SvgPathExtractor.cpp
        src/ArtPathNormalizer.cpp
        src/PathScaler.cpp
    )

    target_include_directories(autografo_tests PRIVATE src)
    target_compile_definitions(autografo_tests PRIVATE
        AUTOGRAFO_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(autografo_tests PRIVATE Qt6::Widgets Qt6::Svg)
    set_target_properties(autografo_tests PROPERTIES WIN32_EXECUTABLE FALSE)

    add_test(NAME autografo_geometry COMMAND autografo_tests)
endif()
