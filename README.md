# Autograph

**Turn SVG line art into real strokes on any app**

Qt 6 / C++ Widgets application (no QML) that imports an SVG, extracts line-art paths, and draws them into a user-selected region on the screen. On Windows it drives the system cursor with `SendInput`. On Android it sends gestures through a user-enabled accessibility service.

The UI source language is **English**. Translations load from the system locale. Bundled catalogs cover Brazilian Portuguese (`pt_BR`) and Spanish (`es`). If no matching catalog is found, the app stays in English.

## License

Copyright (C) 2026 Andrius da Costa Ribas \<andriusmao@gmail.com\>

Autograph is free software under the **GNU Lesser General Public License version 2.1 or later** (LGPL-2.1-or-later). The full license text is in [`COPYING.LESSER`](COPYING.LESSER). [`LICENSE`](LICENSE) states the license choice.

## Requirements

- Windows 10/11 or Android 12+
- CMake 3.21+
- C++17 compiler (MSVC, MinGW, or Clang)
- Qt 6 with **Widgets**, **Svg**, and **LinguistTools** (to compile `.qm` catalogs)

### Install Qt 6

Official installer: [https://www.qt.io/download](https://www.qt.io/download)

Or aqtinstall:

```powershell
pip install aqtinstall
aqt install-qt windows desktop 6.8.0 win64_msvc2022_64 -m qtshadertools
```

Adjust the version and kit for your environment.

## Build

### Windows (desktop)

```powershell
cd d:\dev\autografo
cmake -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2022_64"
cmake --build build --config Release
```

Replace `CMAKE_PREFIX_PATH` with your Qt install path. CMake uses `qt_add_translations` to compile `translations/autografo_*.ts` and embed them in the executable.

### Android

Install a Qt for Android kit from the Qt Maintenance Tool, plus a matching Android SDK, NDK, and JDK. In Qt Creator, pick an `arm64-v8a` Android kit, configure with CMake, and use **Build APK**. The project targets minimum API 31 and uses Play Store application id `br.pushx.autograph`.

From the command line (adjust paths and versions):

```powershell
cmake -S . -B D:\b\autografo-a64 -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="C:\Qt\6.10.2\android_arm64_v8a\lib\cmake\Qt6\qt.toolchain.cmake" `
  -DQT_HOST_PATH="C:\Qt\6.10.2\msvc2022_64"
cmake --build D:\b\autografo-a64
```

The APK is produced by `androiddeployqt` in the build directory. Prefer a short build path such as `D:\b\autografo-a64`; Gradle on Windows can fail on long paths.

Release packages need a signature or Android refuses to install them (`INSTALL_PARSE_FAILED_NO_CERTIFICATES`). For local Qt Creator deploys, Gradle signs **APKs** with the Android **debug** keystore (`~/.android/debug.keystore`).

For a Play Store **AAB**, do **not** enable **Sign package** in Qt Creator together with Gradle signing — `androiddeployqt` would sign the bundle a second time and Play rejects it (*more than one certificate chain*). Copy `android/keystore.properties.example` to `android/keystore.properties`, point it at your upload `.jks`, leave **Sign package** unchecked, enable **Build Android App Bundle**, and rebuild. The AAB is `android-build-autografo/build/outputs/bundle/release/*.aab`.

By default the APK **does not include ads**. To show a small [AdMob](https://admob.google.com/) banner at the bottom of the main screen, configure CMake with `-DAUTOGRAFO_ENABLE_ADS=ON`. Until you have an AdMob account, the project uses Google’s official test IDs. For production, replace them with yours:

```powershell
cmake -S . -B D:\b\autografo-a64 -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="C:\Qt\6.10.2\android_arm64_v8a\lib\cmake\Qt6\qt.toolchain.cmake" `
  -DQT_HOST_PATH="C:\Qt\6.10.2\msvc2022_64" `
  -DAUTOGRAFO_ENABLE_ADS=ON `
  -DAUTOGRAFO_ADMOB_APP_ID="ca-app-pub-xxxxxxxxxxxxxxxx~yyyyyyyyyy" `
  -DAUTOGRAFO_ADMOB_BANNER_UNIT_ID="ca-app-pub-xxxxxxxxxxxxxxxx/zzzzzzzzzz"
```

The banner only takes space after an ad loads, disappears if loading fails, and stays hidden during area selection and drawing.

## Translations (i18n)

- After creating `QApplication` and **before** creating `MainWindow`, the app reads `QLocale::system()`.
- It loads the application translator (`autografo_<locale>`, then language-only such as `pt_BR` then `pt`) from the Qt resource `:/i18n`.
- It also tries to load Qt’s own catalogs (`qtbase`) when they are available.
- Without a matching `.qm`, the English source strings are used.
- On **Android**, `.qm` files ship in the APK via `qrc` (`qt_add_translations` with `RESOURCE_PREFIX "/i18n"`). Locale detection still uses `QLocale::system()`. Native accessibility overlays use Android resources (`values-pt-rBR`, `values-es`).

Files:

```
translations/autografo_pt_BR.ts
translations/autografo_es.ts
```

## Run

```powershell
.\build\Release\autografo.exe
```

## Usage

Button names below are the English source strings. With a `pt_BR` or `es` system locale they appear translated.

1. Click **Open SVG** and choose a file (for example `examples/sample.svg` or `examples/pushx_arax.svg`).
2. Check the line-art preview in the window.
3. Click **Select area** and drag a rectangle on the screen.
4. Press **Enter** to confirm (or **Esc** to cancel).
5. Focus the target app (Paint, a browser canvas, and similar).
6. Adjust **Speed** (cursor speed in pixels per second).
7. Click **Draw**. The window hides briefly and the cursor traces the paths.
8. Press **Stop** or **Esc** to cancel.

Do not move the mouse while drawing.

### Android

1. Install the APK and open Autograph.
2. On first selection, enable **Autograph — gesture drawing** in accessibility settings, then return to the app.
3. Import the SVG from the document picker and tap **Select area**.
4. Use the floating control to open the target app and start selection.
5. Drag the area without covering the reserved top-right corner; tap inside it to confirm.
6. Tap **Draw** on the floating control. Use **Stop** to cancel.

The accessibility service only declares the ability to perform gestures. It does not read the target app’s view tree, text, or content. A screen rotation cancels the flow to avoid stale coordinates. Some apps may ignore or reinterpret injected gestures.

## Limitations (v1)

- Basic geometry only: `path`, `line`, `polyline`, `polygon`, `rect`, `circle`, `ellipse`.
- SVG arcs (`A`) are approximated as a straight line between endpoints.
- Groups (`<g>`) with `transform`, `<use>` references, and multi-subpath `path` data are supported.
- Text, raster `<image>`, filters, and clip-path are not supported.

## Layout

```
src/
  MainWindow.*           — main UI
  SvgPathExtractor.*     — SVG parsing → normalized polylines
  PathPreviewWidget.*    — line-art preview
  AreaSelectorOverlay.*  — on-screen area selection
  PathScaler.*           — scale and reorder paths
  DrawingBackend.h       — shared drawing backend interface
  MouseDrawer.*          — Windows mouse simulation (SendInput)
  AndroidGestureDrawer.* — Qt/JNI bridge for Android gestures
  AutografoAds.*         — Qt/JNI AdMob banner bridge (AUTOGRAFO_ENABLE_ADS only)
android/                 — manifest, accessibility service, overlays, optional ads
cmake/                   — Android packaging scripts
translations/            — Qt Linguist catalogs (pt_BR and es)
COPYING.LESSER           — GNU LGPL v2.1 text
LICENSE                  — LGPL-2.1-or-later notice
```
