# Picture-in-Picture (PiP) for `video_player` — Product Requirements Document

**Version:** 1.0
**Date:** 2026-02-17
**Author:** Generated from implementation plan
**Status:** Draft
**Source Plan:** [plan.md](plan.md)

---

## 1. Executive Summary

This PRD formalizes the addition of Picture-in-Picture (PiP) support to the `video_player` federated plugin across all supported platforms: **Android**, **iOS**, **macOS**, and **Web**. PiP allows video playback to continue in a small floating window while the user navigates away from the player screen or app. The feature is exposed as methods on `VideoPlayerController` with state tracked on `VideoPlayerValue`, following existing API conventions. Implementation spans 5 packages in the federated plugin architecture, requires platform-specific native code (Java/Kotlin, Objective-C, and Dart/JS), and introduces new data models, event types, and platform interface methods. Delivered across 6 sequential steps, the work adds a major user-facing capability while maintaining backward compatibility with existing consumers of the plugin.

---

## 2. Problem Statement

### 2.1 Current State

The `video_player` plugin provides cross-platform video playback for Flutter apps but has **no Picture-in-Picture support**. Users who navigate away from the player screen or minimize the app lose visual access to the video. This is a common and expected feature in modern media applications — native Android, iOS, macOS, and web video players all support PiP natively, but Flutter developers using `video_player` cannot access this capability.

### 2.2 Desired State

After implementation, the `video_player` plugin will:

- Expose a simple, cross-platform PiP API through `VideoPlayerController` (`startPictureInPicture()`, `stopPictureInPicture()`, `setAutoPictureInPicture()`, `setPictureInPictureActions()`).
- Track PiP state on `VideoPlayerValue` (`isPictureInPictureActive`).
- Support PiP on Android (Activity-level), iOS (video-level), macOS (video-level), and Web (element-level).
- Allow developers to check PiP availability before offering PiP UI.
- Support automatic PiP entry when the user leaves the app (opt-in).
- Support custom action buttons in the PiP window (platform-dependent).
- Handle lifecycle correctly — not pausing video during PiP, and stopping PiP on controller disposal.

### 2.3 Impact of Inaction

- **Competitive disadvantage:** Flutter media apps cannot match the PiP experience of native apps, discouraging adoption for video-centric use cases.
- **Developer workarounds:** Developers implement ad-hoc PiP solutions via method channels, leading to fragmented, unmaintained, and inconsistent implementations.
- **User experience gap:** Users expect PiP in video applications; its absence is a noticeable regression from native platform capabilities.
- **Feature requests:** PiP is one of the most requested features for the `video_player` plugin.

---

## 3. Goals & Success Metrics

### 3.1 Goals

| ID    | Goal                                                      | Measurable Target                                                     |
| ----- | --------------------------------------------------------- | --------------------------------------------------------------------- |
| G-001 | Provide cross-platform PiP API on `VideoPlayerController` | All 4 PiP methods available and functional on all supported platforms |
| G-002 | Track PiP state reactively                                | `isPictureInPictureActive` accurately reflects PiP state via events   |
| G-003 | Support PiP on all platforms the plugin supports          | Android, iOS, macOS, and Web implementations all pass platform tests  |
| G-004 | Maintain backward compatibility                           | Existing consumers compile and function without changes               |
| G-005 | Provide feature detection                                 | `isPictureInPictureSupported()` returns accurate result per platform  |
| G-006 | Handle lifecycle interactions correctly                   | Video continues during PiP; PiP stops cleanly on dispose              |

### 3.2 Non-Goals

- Providing a PiP UI widget (PiP windows are platform-managed; the plugin exposes control, not rendering).
- Supporting custom/arbitrary action callbacks from PiP (v1 uses standard media transport controls only).
- Supporting PiP on platforms not currently supported by the plugin (e.g., Linux, Windows).
- Implementing `restoreUserInterfaceForPictureInPictureStop` round-trip to Dart (deferred to future iteration).
- Providing a `Texture` widget placeholder when PiP extracts the video on iOS/macOS (guidance documented; implementation is app responsibility).

### 3.3 Success Metrics

| Metric                                                          | Target                       | Measurement Method                               |
| --------------------------------------------------------------- | ---------------------------- | ------------------------------------------------ |
| PiP methods available on `VideoPlayerController`                | 4 methods + 1 support check  | API surface review                               |
| Platforms with working PiP implementation                       | 4 (Android, iOS, macOS, Web) | Manual testing on each platform                  |
| Backward-compatible (no breaking changes to existing API)       | 0 breaking changes           | Existing test suite passes without modification  |
| All new platform interface methods have default implementations | 100%                         | Compilation of existing platform implementations |
| Unit tests per platform implementation                          | Minimum per step test plan   | Test runner output                               |

---

## 4. Scope

### 4.1 In Scope

- **Platform Interface** (`video_player_platform_interface`): New data models (`PictureInPictureAction`, `PictureInPictureActionType`), new event types (`pictureInPictureStarted`, `pictureInPictureStopped`), and new abstract methods with default implementations.
- **App-Facing Package** (`video_player`): New `VideoPlayerValue` field, new `VideoPlayerController` methods, lifecycle observer updates, dispose behavior, and event handling.
- **Android** (`video_player_android`): Activity-level PiP via `PictureInPictureParams`, `ActivityAware` integration, `ComponentCallbacks2` for PiP detection, `RemoteAction` for custom actions, `BroadcastReceiver` for action handling, multi-player event routing.
- **iOS/macOS** (`video_player_avfoundation`): `AVPictureInPictureController` integration, delegate implementation, support for both texture and platform view types, audio session configuration (iOS), auto-enter configuration.
- **Web** (`video_player_web`): Browser Picture-in-Picture API, Media Session API for custom actions, `visibilitychange`-based auto-enter approximation.
- **Documentation**: README updates, example app PiP controls, platform setup guides.

### 4.2 Out of Scope

- Custom/arbitrary action callbacks from PiP beyond standard media transport controls.
- UI widgets for PiP (the PiP window is platform-managed).
- `restoreUserInterfaceForPictureInPictureStop` round-trip to Dart (v1 calls `completionHandler(YES)` immediately).
- PiP on unsupported platforms (Linux, Windows).
- Modifications to the `video_player` widget tree during PiP (app's responsibility).

### 4.3 Assumptions

- The federated plugin architecture remains unchanged (5 packages with Pigeon-based communication for Android/iOS/macOS and JS interop for Web).
- Host apps will perform the required platform-specific setup (Android manifest attributes, iOS background mode capability) as documented.
- All platforms have stable, documented PiP APIs (Android `PictureInPictureParams`, iOS/macOS `AVPictureInPictureController`, Web Picture-in-Picture API).
- Pigeon code generation tooling is available and functional for both Android and AVFoundation packages.
- PiP testing on CI is limited; physical device testing is required for full validation (see Constraints).

---

## 5. User Stories

| ID     | As a...       | I want to...                                                           | So that...                                                                       | Priority |
| ------ | ------------- | ---------------------------------------------------------------------- | -------------------------------------------------------------------------------- | -------- |
| US-001 | App developer | start PiP programmatically when the user taps a PiP button             | my video continues playing in a floating window while users browse other content | MUST     |
| US-002 | App developer | stop PiP and return to inline playback                                 | users can seamlessly transition back to the full player experience               | MUST     |
| US-003 | App developer | check if PiP is supported before showing PiP UI                        | I don't show broken UI on unsupported devices/browsers                           | MUST     |
| US-004 | App developer | know when PiP starts and stops via `VideoPlayerValue`                  | I can update my UI (e.g., show a "Playing in PiP" placeholder)                   | MUST     |
| US-005 | App developer | enable auto-enter PiP when the user leaves the app                     | video keeps playing without requiring a manual PiP button tap                    | SHOULD   |
| US-006 | App developer | set custom action buttons in the PiP window (play, pause, skip, etc.)  | users can control playback without returning to the app                          | SHOULD   |
| US-007 | App developer | have PiP stop cleanly when the controller is disposed                  | no orphaned PiP windows or black frames remain after navigation                  | MUST     |
| US-008 | App developer | have video continue playing when the app goes to background during PiP | the core PiP use case works — playback persists while multitasking               | MUST     |
| US-009 | App developer | understand platform behavior differences through documentation         | I can handle Android Activity-level vs iOS/Web video-level PiP correctly         | SHOULD   |
| US-010 | App developer | follow a setup guide for platform-specific PiP requirements            | I don't waste time debugging missing manifest attributes or entitlements         | SHOULD   |

---

## 6. Functional Requirements

### 6.0 Architecture Overview

| ID     | Requirement                                                                                                                                                                                                                          | Priority | Source                               |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | -------- | ------------------------------------ |
| FR-001 | The PiP feature MUST follow the existing federated plugin architecture: platform interface defines the contract, app-facing package exposes it on `VideoPlayerController`, and each platform package implements the native behavior. | MUST     | Plan §Architecture                   |
| FR-002 | All new platform interface methods MUST have default implementations (returning `false` or throwing `UnimplementedError`) so that existing third-party platform implementations continue to compile.                                 | MUST     | Plan §Step 1.3                       |
| FR-003 | Custom PiP actions MUST represent standard media transport controls handled natively by the platform. No Dart-side callback mechanism is needed for v1.                                                                              | MUST     | Plan §Step 1.4                       |
| FR-004 | PiP failures MUST be reported through the existing error stream (using `PlatformException`) for asynchronous failures, and through the returned `Future` for synchronous/immediate failures. Both error paths MUST be documented.    | MUST     | Plan §Step 1.2, Open Review Item #21 |

### 6.1 Step 1 — Platform Interface (`video_player_platform_interface`)

| ID     | Requirement                                                                                                                                                                                                                      | Priority | Source         |
| ------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- | -------------- |
| FR-101 | A `PictureInPictureAction` immutable class MUST be added with `type` (`PictureInPictureActionType`) and `label` (`String`) fields, plus `==`, `hashCode`, and `toString` implementations following existing data class patterns. | MUST     | Plan §Step 1.1 |
| FR-102 | A `PictureInPictureActionType` enum MUST be added with values: `play`, `pause`, `skipForward`, `skipBackward`, `nextTrack`, `previousTrack`.                                                                                     | MUST     | Plan §Step 1.1 |
| FR-103 | `VideoEventType` MUST be extended with `pictureInPictureStarted` and `pictureInPictureStopped` values.                                                                                                                           | MUST     | Plan §Step 1.2 |
| FR-104 | `VideoPlayerPlatform` MUST add `isPictureInPictureSupported()` returning `Future<bool>` with default `false`.                                                                                                                    | MUST     | Plan §Step 1.3 |
| FR-105 | `VideoPlayerPlatform` MUST add `startPictureInPicture(int playerId)` throwing `UnimplementedError` by default.                                                                                                                   | MUST     | Plan §Step 1.3 |
| FR-106 | `VideoPlayerPlatform` MUST add `stopPictureInPicture(int playerId)` throwing `UnimplementedError` by default.                                                                                                                    | MUST     | Plan §Step 1.3 |
| FR-107 | `VideoPlayerPlatform` MUST add `setAutoPictureInPicture(int playerId, bool enabled)` throwing `UnimplementedError` by default.                                                                                                   | MUST     | Plan §Step 1.3 |
| FR-108 | `VideoPlayerPlatform` MUST add `setPictureInPictureActions(int playerId, List<PictureInPictureAction> actions)` throwing `UnimplementedError` by default.                                                                        | MUST     | Plan §Step 1.3 |
| FR-109 | The package version MUST receive a minor bump with a corresponding CHANGELOG entry.                                                                                                                                              | MUST     | Plan §Step 1.5 |

### 6.2 Step 2 — App-Facing Package (`video_player`)

| ID     | Requirement                                                                                                                                                                                                                                                            | Priority | Source                     |
| ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- | -------------------------- |
| FR-201 | `VideoPlayerValue` MUST add an `isPictureInPictureActive` boolean field (default `false`), with `copyWith`, `==`, `hashCode`, and `toString` updated accordingly.                                                                                                      | MUST     | Plan §Step 2.1             |
| FR-202 | `VideoPlayerController` MUST add `isPictureInPictureSupported()` delegating to the platform interface.                                                                                                                                                                 | MUST     | Plan §Step 2.2             |
| FR-203 | `VideoPlayerController` MUST add `startPictureInPicture()` that is a silent no-op if the controller is disposed or not initialized, and otherwise delegates to the platform interface.                                                                                 | MUST     | Plan §Step 2.2             |
| FR-204 | `VideoPlayerController` MUST add `stopPictureInPicture()` with the same disposed/uninitialized guard.                                                                                                                                                                  | MUST     | Plan §Step 2.2             |
| FR-205 | `VideoPlayerController` MUST add `setAutoPictureInPicture(bool enabled)` with the same disposed/uninitialized guard.                                                                                                                                                   | MUST     | Plan §Step 2.2             |
| FR-206 | `VideoPlayerController` MUST add `setPictureInPictureActions(List<PictureInPictureAction> actions)` with the same disposed/uninitialized guard.                                                                                                                        | MUST     | Plan §Step 2.2             |
| FR-207 | The controller event listener MUST handle `pictureInPictureStarted` by setting `isPictureInPictureActive = true` and `pictureInPictureStopped` by setting `isPictureInPictureActive = false`.                                                                          | MUST     | Plan §Step 2.3             |
| FR-208 | `_VideoAppLifeCycleObserver` MUST NOT pause the video when the app goes to `paused` state while `isPictureInPictureActive` is `true`.                                                                                                                                  | MUST     | Plan §Step 2.4             |
| FR-209 | `VideoPlayerController.dispose()` MUST stop PiP (if active) before disposing the player to prevent platform-specific issues (black frames, orphaned windows).                                                                                                          | MUST     | Plan §Step 2.5             |
| FR-210 | The `stopPictureInPicture()` call during dispose MUST be wrapped in try/catch to ensure dispose always completes even if PiP cleanup fails.                                                                                                                            | MUST     | Plan §Open Review Item #22 |
| FR-211 | New types (`PictureInPictureAction`, `PictureInPictureActionType`) MUST be exported from the package.                                                                                                                                                                  | MUST     | Plan §Step 2.6             |
| FR-212 | Unit tests MUST cover: method delegation, value state updates from events, lifecycle observer behavior during PiP, `copyWith`/`==`/`hashCode` for updated value, dispose while PiP active, start when not initialized (no-op), start when already active (idempotent). | MUST     | Plan §Step 2.7             |
| FR-213 | The package version MUST receive a minor bump with a corresponding CHANGELOG entry.                                                                                                                                                                                    | MUST     | Plan §Step 2.8             |

### 6.3 Step 3 — Android Implementation (`video_player_android`)

| ID     | Requirement                                                                                                                                                                                                                                                                                             | Priority | Source                                    |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- | ----------------------------------------- |
| FR-301 | The Pigeon API MUST be updated with PiP methods: `isPictureInPictureSupported()` on `AndroidVideoPlayerApi` (global), and `startPictureInPicture()`, `stopPictureInPicture()`, `setAutoPictureInPicture(bool)`, `setPictureInPictureActions(List<PipAction>)` on `VideoPlayerInstanceApi` (per-player). | MUST     | Plan §Step 3.1                            |
| FR-302 | Pigeon data types `PipActionType` (enum) and `PipAction` (class) MUST be defined matching the platform interface types.                                                                                                                                                                                 | MUST     | Plan §Step 3.1                            |
| FR-303 | A `PictureInPictureStateEvent` Pigeon event class MUST be added for PiP state changes.                                                                                                                                                                                                                  | MUST     | Plan §Step 3.1                            |
| FR-304 | `VideoPlayerPlugin` MUST implement `ActivityAware` to gain Activity access for PiP operations.                                                                                                                                                                                                          | MUST     | Plan §Step 3.4.1                          |
| FR-305 | `VideoPlayer` MUST receive an Activity supplier (not a direct reference) via `setActivitySupplier()` to handle Activity lifecycle changes.                                                                                                                                                              | MUST     | Plan §Step 3.2                            |
| FR-306 | All PiP methods MUST guard against `getActivity() == null` and return an error or no-op when the Activity is unavailable.                                                                                                                                                                               | MUST     | Plan §Step 3.2                            |
| FR-307 | `isPictureInPictureSupported()` MUST check: `activity != null`, API level >= 26 (`Build.VERSION_CODES.O`), and `PackageManager.FEATURE_PICTURE_IN_PICTURE` system feature.                                                                                                                              | MUST     | Plan §Step 3.4.2                          |
| FR-308 | `startPictureInPicture()` MUST build `PictureInPictureParams` with video aspect ratio from the primary PiP player's video dimensions, auto-enter setting (Android 12+), and custom actions (if set), then call `activity.enterPictureInPictureMode()`.                                                  | MUST     | Plan §Step 3.4.3                          |
| FR-309 | `stopPictureInPicture()` MUST exit PiP by launching the Activity with `FLAG_ACTIVITY_REORDER_TO_FRONT` (Android has no direct exit API). It MUST be a no-op if not currently in PiP mode.                                                                                                               | MUST     | Plan §Step 3.4.4                          |
| FR-310 | `setAutoPictureInPicture(bool)` MUST call `setAutoEnterEnabled()` on Android 12+ (API 31). On pre-Android 12, it MUST be a silent no-op. This limitation MUST be documented.                                                                                                                            | MUST     | Plan §Step 3.4.8                          |
| FR-311 | PiP mode changes MUST be detected via `Activity.registerComponentCallbacks()` with a `ComponentCallbacks2` implementation that checks `activity.isInPictureInPictureMode()` on configuration changes.                                                                                                   | MUST     | Plan §Step 3.4.6                          |
| FR-312 | When PiP mode changes, `PictureInPictureStateEvent` MUST be emitted to **all** active players' event channels (since Android PiP is Activity-level).                                                                                                                                                    | MUST     | Plan §Step 3.3                            |
| FR-313 | The plugin MUST maintain a `primaryPipPlayerId` field to route custom action events to the player that most recently called `startPictureInPicture()`.                                                                                                                                                  | MUST     | Plan §Step 3.3                            |
| FR-314 | Custom actions MUST be implemented via `RemoteAction` with a `BroadcastReceiver` for action handling. Maximum actions MUST respect `Activity.getMaxNumPictureInPictureActions()`.                                                                                                                       | MUST     | Plan §Step 3.4.5, §Step 3.6               |
| FR-315 | The `BroadcastReceiver` MUST be dynamically registered (not manifest-declared), paired with `unregisterReceiver()` on plugin detach and player dispose. On Android 14+ (API 34), `RECEIVER_NOT_EXPORTED` flag MUST be used.                                                                             | MUST     | Plan §Step 3.6, Open Review Items #9, #19 |
| FR-316 | `PendingIntent` objects for custom actions MUST use `FLAG_IMMUTABLE` (required on Android 12+) and MUST be scoped to the app's package.                                                                                                                                                                 | MUST     | Plan §Open Review Item #14                |
| FR-317 | The implementation MUST maintain a single, accumulated `PictureInPictureParams.Builder` state to avoid param fragmentation when calling `setPictureInPictureParams()` from different methods.                                                                                                           | MUST     | Plan §Open Review Item #32                |
| FR-318 | When a new player is registered while the Activity is already in PiP mode, the plugin MUST immediately emit `PictureInPictureStateEvent(isInPictureInPicture: true)` to the new player.                                                                                                                 | MUST     | Plan §Open Review Item #39                |
| FR-319 | The Dart `VideoPlayerAndroid` class MUST implement all new `VideoPlayerPlatform` methods by delegating to Pigeon-generated host API calls.                                                                                                                                                              | MUST     | Plan §Step 3.8                            |
| FR-320 | The package version MUST receive a minor bump with a corresponding CHANGELOG entry.                                                                                                                                                                                                                     | MUST     | Plan §Step 3.10                           |

### 6.4 Step 4 — AVFoundation Implementation (`video_player_avfoundation`)

| ID     | Requirement                                                                                                                                                                                                                                                                                                   | Priority | Source                                 |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- | -------------------------------------- |
| FR-401 | The Pigeon API MUST be updated with: `isPictureInPictureSupported()` on `AVFoundationVideoPlayerApi` (global), and `startPictureInPicture()`, `stopPictureInPicture()`, `setAutoPictureInPicture(bool)` on `VideoPlayerInstanceApi` (per-player). `setPictureInPictureActions` MUST be intentionally omitted. | MUST     | Plan §Step 4.1                         |
| FR-402 | An `AVPictureInPictureController` MUST be created per player and held as a strong reference for the entire PiP duration.                                                                                                                                                                                      | MUST     | Plan §Step 4.2                         |
| FR-403 | The `AVPictureInPictureControllerDelegate` MUST be implemented to emit PiP lifecycle events.                                                                                                                                                                                                                  | MUST     | Plan §Step 4.2.4                       |
| FR-404 | `restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:` MUST always call `completionHandler(YES)` (v1 behavior). This MUST be documented as a known limitation.                                                                                                                                   | MUST     | Plan §Step 4.2.4, Open Review Item #28 |
| FR-405 | PiP MUST work with both platform view (`FVPNativeVideoView`) and texture view (`FVPTextureBasedVideoPlayer`). For texture views, the `AVPlayerLayer` MUST have a non-zero frame set for `isPictureInPicturePossible` to return `true`.                                                                        | MUST     | Plan §Step 4.3                         |
| FR-406 | `setAutoPictureInPicture(bool)` MUST set `canStartPictureInPictureAutomaticallyFromInline` on iOS 14.2+ and macOS 12.0+.                                                                                                                                                                                      | MUST     | Plan §Step 4.2.6                       |
| FR-407 | PiP events MUST be dispatched through the `FVPVideoEventListener` protocol (new methods `videoPlayerDidEnterPictureInPicture` and `videoPlayerDidExitPictureInPicture`) and implemented in `FVPEventBridge` using the existing `sendOrQueue:` pattern.                                                        | MUST     | Plan §Step 4.6                         |
| FR-408 | The `AVPictureInPictureController` MUST be created only after the `AVPlayerItem` status is `.readyToPlay`.                                                                                                                                                                                                    | MUST     | Plan §Step 4.4.3                       |
| FR-409 | On iOS, the audio session MUST be configured with `.playback` category for PiP to function.                                                                                                                                                                                                                   | MUST     | Plan §Step 4.4.2                       |
| FR-410 | The Dart `AVFoundationVideoPlayer` class MUST implement all new `VideoPlayerPlatform` methods. `setPictureInPictureActions()` MUST be overridden as a no-op.                                                                                                                                                  | MUST     | Plan §Step 4.7                         |
| FR-411 | If the `stopPictureInPicture` Pigeon call is synchronous, it SHOULD be made `@async` to resolve only after `pictureInPictureControllerDidStopPictureInPicture:` fires, preventing dispose race conditions.                                                                                                    | SHOULD   | Plan §Open Review Item #38             |
| FR-412 | KVO observation cleanup for `pipPossibleObservation` (if implemented) MUST be handled in the dispose path by setting `self.pipPossibleObservation = nil`.                                                                                                                                                     | MUST     | Plan §Open Review Item #43             |
| FR-413 | The package version MUST receive a minor bump with a corresponding CHANGELOG entry.                                                                                                                                                                                                                           | MUST     | Plan §Step 4.9                         |

### 6.5 Step 5 — Web Implementation (`video_player_web`)

| ID     | Requirement                                                                                                                                                                                                                                                                               | Priority | Source                                    |
| ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- | ----------------------------------------- |
| FR-501 | `isPictureInPictureSupported()` MUST check `document.pictureInPictureEnabled`.                                                                                                                                                                                                            | MUST     | Plan §Step 5.1                            |
| FR-502 | `startPictureInPicture()` MUST call `videoElement.requestPictureInPicture()`.                                                                                                                                                                                                             | MUST     | Plan §Step 5.1                            |
| FR-503 | `stopPictureInPicture()` MUST call `document.exitPictureInPicture()` only if the current `document.pictureInPictureElement` is this player's video element. Otherwise it MUST be a no-op.                                                                                                 | MUST     | Plan §Step 5.1, Open Review Item #33      |
| FR-504 | Event listeners for `enterpictureinpicture` and `leavepictureinpicture` MUST be added to the video element to emit the corresponding `VideoEventType` events.                                                                                                                             | MUST     | Plan §Step 5.2                            |
| FR-505 | Auto-enter PiP MUST be approximated using the `visibilitychange` event. The auto-enter attempt MUST be wrapped in try/catch to handle browser rejections silently.                                                                                                                        | SHOULD   | Plan §Step 5.3, Open Review Item #11      |
| FR-506 | The `visibilitychange` listener MUST be removed when the player is disposed to prevent listener accumulation.                                                                                                                                                                             | MUST     | Plan §Open Review Item #2                 |
| FR-507 | Custom actions MUST be implemented via the Media Session API (`navigator.mediaSession.setActionHandler()`).                                                                                                                                                                               | SHOULD   | Plan §Step 5.4                            |
| FR-508 | Media Session handlers MUST be cleared (`setActionHandler(name, null)`) when PiP stops or the player is disposed.                                                                                                                                                                         | MUST     | Plan §Open Review Item #10                |
| FR-509 | The programmatic PiP API MUST work regardless of the `disablePictureInPicture` attribute on the video element (which only controls native browser controls). If the attribute does block `requestPictureInPicture()`, the rejection MUST be caught and reported through the error stream. | MUST     | Plan §Step 5.5, Open Review Items #3, #20 |
| FR-510 | PiP event listeners (`enterpictureinpicture`, `leavepictureinpicture`) MUST be explicitly removed in `dispose()` following the existing cleanup pattern.                                                                                                                                  | SHOULD   | Plan §Open Review Item #31                |
| FR-511 | The Dart `VideoPlayerPlugin` (web) MUST implement all new `VideoPlayerPlatform` overrides by delegating to `VideoPlayer` instance methods.                                                                                                                                                | MUST     | Plan §Step 5.6                            |
| FR-512 | The package version MUST receive a minor bump with a corresponding CHANGELOG entry.                                                                                                                                                                                                       | MUST     | Plan §Step 5.8                            |

### 6.6 Step 6 — Documentation and Example App

| ID     | Requirement                                                                                                                                                                                                      | Priority | Source                     |
| ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- | -------------------------- |
| FR-601 | README files for each package MUST be updated with PiP support documentation including platform-specific setup, API usage, behavior differences, and browser compatibility notes.                                | MUST     | Plan §Step 6.1             |
| FR-602 | The example app MUST include: a "Toggle PiP" button, an "Auto PiP" toggle, `isPictureInPictureActive` state display, and custom action configuration UI.                                                         | SHOULD   | Plan §Step 6.2             |
| FR-603 | Platform setup documentation MUST cover Android manifest (`android:supportsPictureInPicture`, `android:configChanges`), iOS background mode capability, and note that macOS and Web require no additional setup. | MUST     | Plan §Step 6.3             |
| FR-604 | Documentation MUST include a minimal-but-complete code snippet showing PiP integration from start to finish (check support → start PiP → handle state → stop PiP).                                               | SHOULD   | Plan §Open Review Item #6  |
| FR-605 | The `isPictureInPictureActive` field doc comment MUST note the Android behavioral difference (all controllers see `true` simultaneously) vs iOS/macOS/Web (only the initiating controller).                      | SHOULD   | Plan §Open Review Item #42 |
| FR-606 | Documentation MUST include guidance on displaying a "Playing in PiP" placeholder using `ValueListenableBuilder` when `isPictureInPictureActive` is `true` on iOS/macOS.                                          | SHOULD   | Plan §Open Review Item #25 |
| FR-607 | The `allowBackgroundPlayback` interaction with PiP MUST be explicitly documented.                                                                                                                                | SHOULD   | Plan §Open Review Item #8  |

---

## 7. Non-Functional Requirements

### 7.1 Compatibility

| ID       | Requirement                                                                                                                                                      | Priority | Source         |
| -------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- | -------------- |
| NFR-C001 | All new platform interface methods MUST have default implementations preserving backward compatibility with existing third-party platform implementations.       | MUST     | Plan §Step 1.3 |
| NFR-C002 | The `VideoPlayerValue` constructor MUST default `isPictureInPictureActive` to `false` to avoid breaking existing consumers.                                      | MUST     | Plan §Step 2.1 |
| NFR-C003 | Android PiP MUST require API level 26+ (Android 8.0). Auto-enter MUST require API 31+ (Android 12). Lower API levels MUST degrade gracefully with silent no-ops. | MUST     | Plan §Step 3.4 |
| NFR-C004 | iOS PiP MUST support iPad (iOS 9+) and iPhone (iOS 14+). Auto-enter MUST require iOS 14.2+.                                                                      | MUST     | Plan §Step 4.4 |
| NFR-C005 | macOS PiP MUST require macOS 12.0+.                                                                                                                              | MUST     | Plan §Step 4.5 |
| NFR-C006 | Web PiP MUST check `document.pictureInPictureEnabled` and handle unsupported browsers gracefully.                                                                | MUST     | Plan §Step 5.1 |

### 7.2 Reliability

| ID       | Requirement                                                                                                                                                  | Priority | Source                               |
| -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------ | -------- | ------------------------------------ |
| NFR-R001 | All PiP methods on `VideoPlayerController` MUST be silent no-ops when the controller is disposed or not initialized.                                         | MUST     | Plan §Step 2.2                       |
| NFR-R002 | `dispose()` MUST stop PiP before disposing the player, wrapped in try/catch to ensure disposal always completes.                                             | MUST     | Plan §Step 2.5, Open Review Item #22 |
| NFR-R003 | Android PiP mode detection MUST use `ComponentCallbacks2.onConfigurationChanged()` (synchronous, reliable) rather than polling.                              | MUST     | Plan §Step 3.4.6                     |
| NFR-R004 | iOS `restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:` MUST always call the completion handler to prevent PiP from entering a broken state. | MUST     | Plan §Step 4.2.4                     |
| NFR-R005 | Web auto-enter PiP MUST be wrapped in try/catch to handle browser rejection without unhandled exceptions.                                                    | MUST     | Plan §Open Review Item #11           |
| NFR-R006 | On Android, `isPictureInPictureActive` state MUST be correct for newly registered players when the Activity is already in PiP mode.                          | MUST     | Plan §Open Review Item #39           |

### 7.3 Security

| ID       | Requirement                                                                                                                                              | Priority | Source                     |
| -------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- | -------------------------- |
| NFR-S001 | Android `PendingIntent` for custom actions MUST use `FLAG_IMMUTABLE` and MUST be scoped to the app's package to prevent external triggering.             | MUST     | Plan §Open Review Item #14 |
| NFR-S002 | Android `BroadcastReceiver` for PiP actions MUST use `RECEIVER_NOT_EXPORTED` flag on API 34+.                                                            | MUST     | Plan §Open Review Item #19 |
| NFR-S003 | Android `BroadcastReceiver` MUST be dynamically registered (not manifest-declared) to scope it to the process lifetime and avoid exposure to other apps. | MUST     | Plan §Open Review Item #37 |

### 7.4 Performance

| ID       | Requirement                                                                                                                                                              | Priority | Source                               |
| -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | -------- | ------------------------------------ |
| NFR-P001 | ExoPlayer MUST continue playback during PiP transitions without interruption.                                                                                            | MUST     | Plan §Step 3.7                       |
| NFR-P002 | Android PiP aspect ratio SHOULD be updated when the video resolution changes during PiP (e.g., adaptive streaming quality switch) by listening for `onVideoSizeChanged`. | SHOULD   | Plan §Open Review Item #36           |
| NFR-P003 | Web event listeners and Media Session handlers MUST be properly cleaned up on dispose to prevent memory leaks and stale handlers.                                        | MUST     | Plan §Open Review Items #2, #10, #31 |

### 7.5 Testability

| ID       | Requirement                                                                                                                                                | Priority | Source                    |
| -------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- | ------------------------- |
| NFR-T001 | PiP cannot be tested on most CI environments. Automated tests MUST be limited to unit tests with mocked platform interfaces and Pigeon API contract tests. | MUST     | Plan §Notes: Testing      |
| NFR-T002 | Manual testing MUST cover: actual PiP enter/exit on physical Android device, physical iPhone (not Simulator), macOS, and Chrome (best PiP support).        | MUST     | Plan §Notes: Testing      |
| NFR-T003 | Each platform test plan MUST include the negative case — behavior when PiP is not supported and `startPictureInPicture()` is called.                       | MUST     | Plan §Open Review Item #7 |

---

## 8. Dependencies & Constraints

### 8.1 Technical Dependencies

| Dependency                 | Version/Detail                            | Impact if Unavailable                                                 |
| -------------------------- | ----------------------------------------- | --------------------------------------------------------------------- |
| Flutter SDK                | Stable channel                            | Plugin development and testing impossible.                            |
| Pigeon                     | Current version in `dev_dependencies`     | Cannot regenerate platform channel code for Android and AVFoundation. |
| Android SDK                | API 26+ (PiP), API 31+ (auto-enter)       | PiP not available on lower API levels (graceful degradation).         |
| AVKit framework            | iOS 9+/14+, macOS 12.0+                   | `AVPictureInPictureController` unavailable (graceful degradation).    |
| Picture-in-Picture Web API | Chrome 70+, Edge 79+                      | PiP not available in unsupported browsers (graceful degradation).     |
| Media Session Web API      | Chrome 73+                                | Custom PiP actions unavailable in unsupported browsers.               |
| ExoPlayer                  | Current version in Android implementation | Video playback and PiP transitions depend on ExoPlayer.               |

### 8.2 Constraints

- **Android PiP is Activity-level**: The entire Flutter app enters PiP, not just the video. This is a fundamental platform constraint that cannot be changed.
- **Android auto-enter on pre-API 31 is not possible**: The plugin cannot override `Activity.onUserLeaveHint()` from a `FlutterPlugin` without requiring host app code changes.
- **iOS requires host app entitlements**: The "Audio, AirPlay, and Picture in Picture" background mode must be enabled by the host app — the plugin cannot add this automatically.
- **Web PiP auto-enter is best-effort**: Browsers may reject programmatic PiP entry without a user gesture.
- **CI testing limitations**: PiP cannot be reliably tested on Android emulators or iPhone Simulator. Physical devices required for manual validation.
- **No direct Android `exitPictureInPictureMode()` API**: Stopping PiP on Android requires launching the Activity with `FLAG_ACTIVITY_REORDER_TO_FRONT`.
- **iOS `restoreUserInterfaceForPictureInPictureStop` round-trip deferred**: V1 calls `completionHandler(YES)` immediately, which may cause a brief visual jump when returning from PiP.

---

## 9. Acceptance Criteria

### 9.1 Platform Interface

**AC-001:** Given the updated platform interface, when a third-party platform implementation that does not override PiP methods is compiled, then it compiles successfully with default implementations returning `false` or throwing `UnimplementedError`.

**AC-002:** Given `PictureInPictureAction` instances with the same `type` and `label`, when compared with `==`, then they are equal and have the same `hashCode`.

### 9.2 App-Facing Package

**AC-010:** Given a `VideoPlayerController` receiving a `pictureInPictureStarted` event, when the event is processed, then `controller.value.isPictureInPictureActive` is `true`.

**AC-011:** Given a `VideoPlayerController` receiving a `pictureInPictureStopped` event, when the event is processed, then `controller.value.isPictureInPictureActive` is `false`.

**AC-012:** Given PiP is active and the app lifecycle state changes to `paused`, when the lifecycle observer processes the change, then the video is NOT paused.

**AC-013:** Given PiP is active and `dispose()` is called, when dispose completes, then `stopPictureInPicture()` was called before the player was disposed, and no exception was thrown even if `stopPictureInPicture()` fails.

**AC-014:** Given a controller that is not initialized, when `startPictureInPicture()` is called, then it is a silent no-op (no exception, no platform call).

**AC-015:** Given a controller that is already in PiP, when `startPictureInPicture()` is called again, then it behaves idempotently (delegates to platform, no error).

### 9.3 Android

**AC-020:** Given Android API level 26+ with manifest configured correctly, when `isPictureInPictureSupported()` is called, then it returns `true`.

**AC-021:** Given Android API level < 26, when `isPictureInPictureSupported()` is called, then it returns `false`.

**AC-022:** Given a video playing on Android, when `startPictureInPicture()` is called, then the Activity enters PiP mode with the correct aspect ratio and a `pictureInPictureStarted` event is emitted to all active players.

**AC-023:** Given PiP is active on Android, when `stopPictureInPicture()` is called, then the Activity returns to full-screen and a `pictureInPictureStopped` event is emitted to all active players.

**AC-024:** Given Android 12+ and `setAutoPictureInPicture(true)` called, when the user presses the home button, then the app automatically enters PiP mode.

**AC-025:** Given custom actions are set and PiP is active, when the user taps an action button in the PiP window, then the corresponding player operation executes (e.g., play/pause, skip).

**AC-026:** Given a new `VideoPlayerController` initialized while the Activity is already in PiP mode, then the new controller's `isPictureInPictureActive` is `true` immediately after initialization.

**AC-027:** Given `setAutoPictureInPicture(true)` followed by `setPictureInPictureActions(...)`, when PiP is entered, then both auto-enter and custom actions are correctly applied (no param fragmentation).

### 9.4 AVFoundation (iOS/macOS)

**AC-030:** Given iOS with the "Audio, AirPlay, and Picture in Picture" background mode enabled, when `isPictureInPictureSupported()` is called, then it returns `true`.

**AC-031:** Given a video playing on iOS/macOS, when `startPictureInPicture()` is called, then the video enters a floating PiP window and `pictureInPictureStarted` event is emitted.

**AC-032:** Given PiP is active on iOS/macOS, when `stopPictureInPicture()` is called, then PiP exits and `pictureInPictureStopped` event is emitted.

**AC-033:** Given the texture-based video player on iOS, when PiP is started, then `isPictureInPicturePossible` is `true` (the `AVPlayerLayer` has a non-zero frame).

**AC-034:** Given the platform-view-based video player on iOS, when PiP is started, then it works correctly using the view's backing `AVPlayerLayer`.

**AC-035:** Given `setPictureInPictureActions()` called on iOS/macOS, then it is a silent no-op (no error, no effect).

**AC-036:** Given PiP exit triggered by the system, when `restoreUserInterfaceForPictureInPictureStop` fires, then `completionHandler(YES)` is always called.

### 9.5 Web

**AC-040:** Given a browser supporting the Picture-in-Picture API, when `isPictureInPictureSupported()` is called, then it returns `true`.

**AC-041:** Given a browser not supporting PiP, when `isPictureInPictureSupported()` is called, then it returns `false`.

**AC-042:** Given a video playing in a supported browser, when `startPictureInPicture()` is called, then the video enters browser PiP and `pictureInPictureStarted` event is emitted.

**AC-043:** Given player A is in PiP and player B calls `stopPictureInPicture()`, then it is a no-op (player B does not exit player A's PiP).

**AC-044:** Given auto-enter PiP is enabled and the user navigates away (tab hidden), when `visibilitychange` fires, then PiP is attempted with errors silently caught.

**AC-045:** Given a web player is disposed, then all PiP event listeners, `visibilitychange` listener, and Media Session handlers are cleaned up.

### 9.6 Documentation

**AC-050:** Given the updated README, when a developer searches for PiP setup instructions, then platform-specific setup steps are documented for Android, iOS, macOS, and Web.

**AC-051:** Given the example app, when PiP controls are used, then the toggle button, auto-PiP switch, and status indicator all function correctly.

---

## 10. Risks & Mitigations

| ID      | Risk                                                                                                          | Likelihood | Impact | Mitigation                                                                                                                                                       |
| ------- | ------------------------------------------------------------------------------------------------------------- | ---------- | ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| RSK-001 | Host apps missing required platform setup (Android manifest, iOS background mode) leads to silent PiP failure | High       | High   | Clear documentation with setup guides. `isPictureInPictureSupported()` returns `false` when setup is incomplete where detectable.                                |
| RSK-002 | Android Activity-level PiP confuses developers expecting video-level PiP                                      | Medium     | High   | Extensive documentation of the behavioral difference. API doc comments on `startPictureInPicture()` and `isPictureInPictureActive` explain the Android behavior. |
| RSK-003 | iOS texture-based player's `AVPlayerLayer` frame requirement causes subtle PiP failures                       | Medium     | Medium | Explicit minimum frame size set during PiP setup. Verification during implementation that `isPictureInPicturePossible` returns `true`.                           |
| RSK-004 | Web PiP API not available in all browsers, leading to inconsistent cross-browser behavior                     | High       | Medium | Feature detection via `isPictureInPictureSupported()`. Browser compatibility notes in documentation. Graceful error handling.                                    |
| RSK-005 | Dispose race condition on iOS — player torn down while PiP close animation in progress                        | Medium     | Medium | Make AVFoundation `stopPictureInPicture` Pigeon call `@async` (FR-411). Wrap dispose PiP cleanup in try/catch (FR-210).                                          |
| RSK-006 | Android `PictureInPictureParams` fragmentation across multiple methods                                        | Medium     | Medium | Maintain single accumulated params state (FR-317). Unit tests verifying params integrity across method combinations.                                             |
| RSK-007 | PiP testing limitations on CI lead to undetected regressions                                                  | Medium     | Medium | Unit tests with mocked platforms on CI. Manual test checklist for physical device validation before releases.                                                    |
| RSK-008 | Hot restart orphans native PiP state                                                                          | Low        | Low    | On `initialize()` (which calls `disposeAllPlayers()`), exit PiP if active. Document edge case. See Open Review Item #23.                                         |

---

## 11. Milestones & Timeline

| Milestone | Description                           | Dependencies | Exit Criteria                                                                                                                                     |
| --------- | ------------------------------------- | ------------ | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| M-001     | Step 1: Platform Interface            | None         | All new data models, event types, and abstract methods added with defaults. Minor version bumped. Unit tests pass.                                |
| M-002     | Step 2: App-Facing Package            | M-001        | `VideoPlayerValue` and `VideoPlayerController` updated. Lifecycle observer updated. Dispose behavior updated. All unit tests pass.                |
| M-003     | Step 3: Android Implementation        | M-001        | Activity-level PiP working. Pigeon API updated. Multi-player event routing working. Custom actions working. All unit tests pass.                  |
| M-004     | Step 4: AVFoundation Implementation   | M-001        | `AVPictureInPictureController` integrated. Both view types working. Delegate callbacks emitting events. All unit tests pass.                      |
| M-005     | Step 5: Web Implementation            | M-001        | Browser PiP API integrated. Event listeners working. Media Session custom actions working. Auto-enter approximation working. All unit tests pass. |
| M-006     | Step 6: Documentation and Example App | M-002–M-005  | READMEs updated. Example app has PiP controls. Platform setup guides complete. Code snippet in documentation.                                     |

_Note: Steps 1 must be completed first. Steps 2–5 can proceed in parallel after Step 1. Step 6 requires all other steps to be complete._

---

## Appendix

### A. Requirement Traceability Matrix

| Requirement ID | Plan Section                         | Acceptance Criteria    | Status |
| -------------- | ------------------------------------ | ---------------------- | ------ |
| FR-001         | §Architecture                        | AC-001                 | Draft  |
| FR-002         | §Step 1.3                            | AC-001                 | Draft  |
| FR-003         | §Step 1.4                            | AC-025                 | Draft  |
| FR-004         | §Step 1.2, Open Review Item #21      | —                      | Draft  |
| FR-101         | §Step 1.1                            | AC-002                 | Draft  |
| FR-102         | §Step 1.1                            | AC-002                 | Draft  |
| FR-103         | §Step 1.2                            | AC-010, AC-011         | Draft  |
| FR-104         | §Step 1.3                            | AC-001                 | Draft  |
| FR-105         | §Step 1.3                            | AC-022, AC-031, AC-042 | Draft  |
| FR-106         | §Step 1.3                            | AC-023, AC-032, AC-043 | Draft  |
| FR-107         | §Step 1.3                            | AC-024                 | Draft  |
| FR-108         | §Step 1.3                            | AC-025                 | Draft  |
| FR-109         | §Step 1.5                            | —                      | Draft  |
| FR-201         | §Step 2.1                            | AC-010, AC-011         | Draft  |
| FR-202         | §Step 2.2                            | AC-020, AC-030, AC-040 | Draft  |
| FR-203         | §Step 2.2                            | AC-014, AC-015         | Draft  |
| FR-204         | §Step 2.2                            | AC-023, AC-032         | Draft  |
| FR-205         | §Step 2.2                            | AC-024                 | Draft  |
| FR-206         | §Step 2.2                            | AC-025, AC-035         | Draft  |
| FR-207         | §Step 2.3                            | AC-010, AC-011         | Draft  |
| FR-208         | §Step 2.4                            | AC-012                 | Draft  |
| FR-209         | §Step 2.5                            | AC-013                 | Draft  |
| FR-210         | §Open Review Item #22                | AC-013                 | Draft  |
| FR-211         | §Step 2.6                            | —                      | Draft  |
| FR-212         | §Step 2.7                            | AC-010–AC-015          | Draft  |
| FR-213         | §Step 2.8                            | —                      | Draft  |
| FR-301         | §Step 3.1                            | AC-022                 | Draft  |
| FR-302         | §Step 3.1                            | AC-025                 | Draft  |
| FR-303         | §Step 3.1                            | AC-022, AC-023         | Draft  |
| FR-304         | §Step 3.4.1                          | AC-022                 | Draft  |
| FR-305         | §Step 3.2                            | AC-022                 | Draft  |
| FR-306         | §Step 3.2                            | AC-021                 | Draft  |
| FR-307         | §Step 3.4.2                          | AC-020, AC-021         | Draft  |
| FR-308         | §Step 3.4.3                          | AC-022                 | Draft  |
| FR-309         | §Step 3.4.4                          | AC-023                 | Draft  |
| FR-310         | §Step 3.4.8                          | AC-024                 | Draft  |
| FR-311         | §Step 3.4.6                          | AC-022, AC-023         | Draft  |
| FR-312         | §Step 3.3                            | AC-022, AC-026         | Draft  |
| FR-313         | §Step 3.3                            | AC-025                 | Draft  |
| FR-314         | §Step 3.4.5, §Step 3.6               | AC-025                 | Draft  |
| FR-315         | §Step 3.6, Open Review Items #9, #19 | AC-025                 | Draft  |
| FR-316         | §Open Review Item #14                | AC-025                 | Draft  |
| FR-317         | §Open Review Item #32                | AC-027                 | Draft  |
| FR-318         | §Open Review Item #39                | AC-026                 | Draft  |
| FR-319         | §Step 3.8                            | AC-020–AC-025          | Draft  |
| FR-320         | §Step 3.10                           | —                      | Draft  |
| FR-401         | §Step 4.1                            | AC-031                 | Draft  |
| FR-402         | §Step 4.2                            | AC-031                 | Draft  |
| FR-403         | §Step 4.2.4                          | AC-031, AC-032         | Draft  |
| FR-404         | §Step 4.2.4, Open Review Item #28    | AC-036                 | Draft  |
| FR-405         | §Step 4.3                            | AC-033, AC-034         | Draft  |
| FR-406         | §Step 4.2.6                          | AC-031                 | Draft  |
| FR-407         | §Step 4.6                            | AC-031, AC-032         | Draft  |
| FR-408         | §Step 4.4.3                          | AC-031                 | Draft  |
| FR-409         | §Step 4.4.2                          | AC-030                 | Draft  |
| FR-410         | §Step 4.7                            | AC-035                 | Draft  |
| FR-411         | §Open Review Item #38                | AC-032                 | Draft  |
| FR-412         | §Open Review Item #43                | —                      | Draft  |
| FR-413         | §Step 4.9                            | —                      | Draft  |
| FR-501         | §Step 5.1                            | AC-040, AC-041         | Draft  |
| FR-502         | §Step 5.1                            | AC-042                 | Draft  |
| FR-503         | §Step 5.1, Open Review Item #33      | AC-043                 | Draft  |
| FR-504         | §Step 5.2                            | AC-042                 | Draft  |
| FR-505         | §Step 5.3, Open Review Item #11      | AC-044                 | Draft  |
| FR-506         | §Open Review Item #2                 | AC-045                 | Draft  |
| FR-507         | §Step 5.4                            | AC-042                 | Draft  |
| FR-508         | §Open Review Item #10                | AC-045                 | Draft  |
| FR-509         | §Step 5.5, Open Review Items #3, #20 | AC-042                 | Draft  |
| FR-510         | §Open Review Item #31                | AC-045                 | Draft  |
| FR-511         | §Step 5.6                            | AC-040–AC-044          | Draft  |
| FR-512         | §Step 5.8                            | —                      | Draft  |
| FR-601         | §Step 6.1                            | AC-050                 | Draft  |
| FR-602         | §Step 6.2                            | AC-051                 | Draft  |
| FR-603         | §Step 6.3                            | AC-050                 | Draft  |
| FR-604         | §Open Review Item #6                 | AC-050                 | Draft  |
| FR-605         | §Open Review Item #42                | —                      | Draft  |
| FR-606         | §Open Review Item #25                | AC-050                 | Draft  |
| FR-607         | §Open Review Item #8                 | AC-050                 | Draft  |
| NFR-C001       | §Step 1.3                            | AC-001                 | Draft  |
| NFR-C002       | §Step 2.1                            | AC-010                 | Draft  |
| NFR-C003       | §Step 3.4                            | AC-020, AC-021         | Draft  |
| NFR-C004       | §Step 4.4                            | AC-030                 | Draft  |
| NFR-C005       | §Step 4.5                            | AC-030                 | Draft  |
| NFR-C006       | §Step 5.1                            | AC-040, AC-041         | Draft  |
| NFR-R001       | §Step 2.2                            | AC-014                 | Draft  |
| NFR-R002       | §Step 2.5, Open Review Item #22      | AC-013                 | Draft  |
| NFR-R003       | §Step 3.4.6                          | AC-022                 | Draft  |
| NFR-R004       | §Step 4.2.4                          | AC-036                 | Draft  |
| NFR-R005       | §Open Review Item #11                | AC-044                 | Draft  |
| NFR-R006       | §Open Review Item #39                | AC-026                 | Draft  |
| NFR-S001       | §Open Review Item #14                | AC-025                 | Draft  |
| NFR-S002       | §Open Review Item #19                | AC-025                 | Draft  |
| NFR-S003       | §Open Review Item #37                | AC-025                 | Draft  |
| NFR-P001       | §Step 3.7                            | AC-022                 | Draft  |
| NFR-P002       | §Open Review Item #36                | —                      | Draft  |
| NFR-P003       | §Open Review Items #2, #10, #31      | AC-045                 | Draft  |
| NFR-T001       | §Notes: Testing                      | —                      | Draft  |
| NFR-T002       | §Notes: Testing                      | —                      | Draft  |
| NFR-T003       | §Open Review Item #7                 | —                      | Draft  |

### B. Open Review Items Incorporated

The following open review items from the plan have been incorporated as requirements in this PRD:

| Open Review Item | Description                                                            | Incorporated As  |
| ---------------- | ---------------------------------------------------------------------- | ---------------- |
| #2               | Web `visibilitychange` listener cleanup in dispose                     | FR-506           |
| #3               | Web `disablePictureInPicture` attribute may block JS API               | FR-509           |
| #6               | No complete integration example in documentation                       | FR-604           |
| #7               | Missing test for `isPictureInPictureSupported()` returning false       | NFR-T003         |
| #8               | `allowBackgroundPlayback` interaction documentation                    | FR-607           |
| #9               | Android BroadcastReceiver unregistration on plugin detach              | FR-315           |
| #10              | Web Media Session handlers not cleared on dispose                      | FR-508           |
| #11              | Web auto-enter PiP needs try/catch for rejection                       | FR-505, NFR-R005 |
| #14              | Android `PendingIntent` FLAG_IMMUTABLE                                 | FR-316, NFR-S001 |
| #19              | Android 14+ BroadcastReceiver RECEIVER_NOT_EXPORTED                    | FR-315, NFR-S002 |
| #20              | Web `startPictureInPicture()` handle disablePictureInPicture rejection | FR-509           |
| #21              | Error propagation path: Future vs event stream                         | FR-004           |
| #22              | `stopPictureInPicture()` during dispose wrapped in try/catch           | FR-210, NFR-R002 |
| #25              | Guidance on `Texture` widget behavior during iOS/macOS PiP             | FR-606           |
| #28              | iOS `restoreUserInterfaceForPictureInPictureStop` future plan          | FR-404           |
| #31              | Web dispose does not clean up PiP event listeners                      | FR-510           |
| #32              | Android `PictureInPictureParams` state fragmentation                   | FR-317           |
| #33              | Web `stopPictureInPicture()` multi-player safety                       | FR-503           |
| #36              | Android PiP aspect ratio update on resolution change                   | NFR-P002         |
| #37              | Android BroadcastReceiver dynamic registration                         | FR-315, NFR-S003 |
| #38              | AVFoundation `stopPictureInPicture` async for dispose race             | FR-411           |
| #39              | New player registered during active Android PiP                        | FR-318, NFR-R006 |
| #42              | `isPictureInPictureActive` cross-platform asymmetry docs               | FR-605           |
| #43              | iOS/macOS `pipPossibleObservation` KVO cleanup                         | FR-412           |

### C. Open Review Items Deferred (Not Incorporated)

The following open review items from the plan are noted but deferred to implementation discretion or future iterations:

| Open Review Item | Description                                                       | Reason Deferred                                                        |
| ---------------- | ----------------------------------------------------------------- | ---------------------------------------------------------------------- |
| #1               | `isPictureInPictureSupported()` could be synchronous              | API design decision — async provides future flexibility                |
| #4               | `play/pause` action types may duplicate default controls          | Implementation detail — clarify during development                     |
| #5               | No CI testing strategy for PiP                                    | Noted in NFR-T001/T002; detailed test matrix is implementation concern |
| #12              | Missing test: multiple players during Android PiP                 | Covered by FR-312 requirement; specific test cases are implementation  |
| #13              | Missing test: PiP start during buffering/loading                  | Implementation-time decision on expected behavior                      |
| #15              | Position update timer during PiP on Android                       | Performance optimization — low priority                                |
| #16              | Platform behavior differences in API docs                         | Partially addressed by FR-605; full API doc wording is implementation  |
| #17              | Web tests for unsupported browsers                                | Covered by NFR-T003                                                    |
| #18              | iOS KVO observation for `isPictureInPicturePossible`              | May not be needed for v1 — skip to avoid overhead                      |
| #23              | Hot restart orphans native PiP state                              | Noted in RSK-008; edge case documentation                              |
| #24              | `setAutoPictureInPicture` state not tracked in `VideoPlayerValue` | App should track this state itself for v1                              |
| #26              | `setPictureInPictureActions([])` behavior undefined               | Implementation-time decision — document behavior chosen                |
| #27              | Start/stop PiP race condition on Android                          | Implementation-time optimization — `pipRequested` boolean approach     |
| #29              | Missing test: AVPlayerItem failure during active PiP              | Implementation-time test case                                          |
| #30              | Missing test: auto-enter + manual start/stop interaction          | Implementation-time test case                                          |
| #34              | Android `isPictureInPictureSupported()` manifest attribute check  | Enhancement to FR-307 — verify during implementation                   |
| #35              | Missing test: dynamic PiP action update during active PiP         | Implementation-time test case                                          |
| #40              | Multi-player PiP collision untested on iOS/macOS/web              | Implementation-time test case                                          |
| #41              | Web PiP blocked by Permissions Policy in iframes                  | Documentation enhancement — note in web setup guide                    |
| #44              | Android auto-enter PiP params not cleared on player dispose       | Implementation-time cleanup — enhancement to dispose logic             |

### D. Glossary

| Term                           | Definition                                                                                                                                |
| ------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------- |
| Picture-in-Picture (PiP)       | A feature allowing video playback to continue in a small floating window while the user interacts with other content or apps.             |
| Federated plugin               | A Flutter plugin architecture where the API, platform interface, and platform implementations are separate packages.                      |
| Platform interface             | The `video_player_platform_interface` package that defines the abstract contract for platform implementations.                            |
| App-facing package             | The `video_player` package that provides the public API (`VideoPlayerController`, `VideoPlayerValue`, widgets) to Flutter app developers. |
| Pigeon                         | A code generation tool that creates type-safe platform channel code from Dart definitions for Flutter plugin development.                 |
| Activity-level PiP             | Android's PiP implementation where the entire Activity (Flutter app window) shrinks into the floating PiP window.                         |
| Video-level PiP                | PiP implementation (iOS, macOS, Web) where only the video element/layer floats, independently of the main app window.                     |
| `AVPictureInPictureController` | Apple's AVKit class that manages Picture-in-Picture for `AVPlayer`-based video playback on iOS and macOS.                                 |
| `PictureInPictureParams`       | Android's builder-pattern class for configuring PiP behavior (aspect ratio, custom actions, auto-enter).                                  |
| Media Session API              | A Web API that allows web pages to customize media notifications and handle hardware media keys, including PiP window controls.           |
| `RemoteAction`                 | Android class representing a custom action button displayed in the PiP window, backed by a `PendingIntent`.                               |
| `BroadcastReceiver`            | Android component that receives broadcast intents, used here to handle PiP custom action button taps.                                     |
| `ComponentCallbacks2`          | Android interface providing `onConfigurationChanged()` callback, used to detect PiP mode transitions.                                     |
| Auto-enter PiP                 | Opt-in behavior where PiP activates automatically when the user leaves the app (e.g., presses home button).                               |

### E. References

- [Implementation Plan](plan.md)
- [Plan Review Log](review-log.md)
- [Android PiP Documentation](https://developer.android.com/develop/ui/views/picture-in-picture)
- [Apple AVPictureInPictureController Documentation](https://developer.apple.com/documentation/avkit/avpictureinpicturecontroller)
- [MDN Web Picture-in-Picture API](https://developer.mozilla.org/en-US/docs/Web/API/Picture-in-Picture_API)
- [MDN Media Session API](https://developer.mozilla.org/en-US/docs/Web/API/Media_Session_API)
