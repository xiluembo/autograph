# Privacy Policy for Autograph

**Effective date:** 23 September 2026

This policy describes how **Autograph** (`br.pushx.autograph`) handles information. Autograph is developed by **Andrius da Costa Ribas** (“the developer”). Contact: [andriusmao@gmail.com](mailto:andriusmao@gmail.com).

Source code: [https://github.com/xiluembo/autograph](https://github.com/xiluembo/autograph)

Autograph turns SVG line art into gestures or cursor strokes on a region of the screen that you choose. On Android it can show an optional advertising banner.

This document is provided so you can understand the app’s data practices. It is not legal advice.

## 1. Information the app stores on your device

Autograph does **not** create an account and does **not** ask for your name, email, phone number, or payment details.

The following stays on the device unless you uninstall the app or clear its data:

| Data | Purpose |
| --- | --- |
| SVG files you open | Parsed locally to extract line-art paths and show a preview. The file is not uploaded by Autograph. |
| Selected drawing rectangle and speed setting | Used only for the current drawing session. |
| Floating-control position | Saved in local preferences so the overlay can return to the last place you put it. |

The Android package sets `android:allowBackup="false"`, so this local state is not included in automatic device backups.

## 2. Information Autograph does not collect

The developer does not operate an Autograph backend that receives your files, drawings, or usage history. Autograph does not include an in-app analytics SDK from the developer.

Autograph does not sell personal information.

## 3. Accessibility service (Android)

Drawing on other apps requires the optional service **Autograph — gesture drawing**. You must enable it yourself in Android Settings. You can turn it off at any time.

The service is declared with:

- `canPerformGestures` — injects strokes inside the rectangle you selected
- `canRequestFilterKeyEvents` — volume keys can cancel drawing so you are not forced to tap the overlay
- `canRetrieveWindowContent="false"` — it does **not** read the target app’s view tree, text, or on-screen content

The service listens only for window-state changes so it can keep its overlay in a valid state. It does not send screen contents, keystrokes, or accessibility-node data to the developer or to any server operated by Autograph.

Use Autograph only on apps and canvases you are allowed to operate.

## 4. Advertising (Android builds with ads)

The Autograph build published on Google Play includes a Google **AdMob** banner on the main screen. That banner is served by Google, not by an Autograph server. Because Autograph is open source, the default build scripts produce a binary **without** ads; advertising is compiled in only when you opt in (`AUTOGRAFO_ENABLE_ADS=ON`).

When ads are enabled, Google may process information such as:

- advertising identifier (AAID) and approximate device/app signals
- IP address and network information
- ad interaction and fill diagnostics

Google’s processing is governed by Google’s policies, including:

- [Google Privacy Policy](https://policies.google.com/privacy)
- [How Google uses data from apps](https://policies.google.com/technologies/partner-sites)

You can limit ad personalization in Android settings (for example **Privacy / Ads** or **Google / Ads**). Test devices may receive labeled test ads.

The banner is hidden while you select an area or draw, and it does not take layout space when no ad is loaded.

## 5. Other third-party components

- **Qt** and **Android** system libraries run on the device as part of the app.
- Opening an HTTP(S) or market link (for example from the About screen or from an ad) uses the system browser or Play Store.
- Google Play and Play services may collect install and crash information according to Google’s terms when you install Autograph from Play.

## 6. Permissions and why they exist

Beyond the optional accessibility service, Android and the AdMob SDK may require network access so ads can load. Autograph does not use location, contacts, microphone, camera, SMS, or storage access beyond the system document picker you use to open an SVG.

## 7. Children

Autograph is not directed at children under 13 (or the equivalent age in your country). The developer does not knowingly collect personal information from children. If you believe a child has provided personal information through this app, contact the developer and it will be deleted where it is under the developer’s control.

Because advertising may appear on Android builds, do not use those builds as a service aimed at children.

## 8. Data retention and your choices

- Local files and preferences remain until you delete them, clear app storage, or uninstall Autograph.
- Disable the accessibility service in Android Settings to stop gesture injection.
- Uninstalling the app removes Autograph’s local data on that device.
- Advertising data retained by Google follows Google’s retention rules.

## 9. International processing

If you use a build with ads, Google may process ad-related data in countries other than your own, including the United States, subject to Google’s contracts and safeguards.

## 10. Changes

The developer may update this policy when the app’s behavior or legal requirements change. The effective date at the top will be revised. The current text lives in this repository as `PRIVACY.md`.

## 11. Contact

Questions about this policy or Autograph’s privacy practices:

**Andrius da Costa Ribas**  
Email: [andriusmao@gmail.com](mailto:andriusmao@gmail.com)  
Project: [https://github.com/xiluembo/autograph](https://github.com/xiluembo/autograph)
