# Picture-in-Picture (PiP) for video_player — Agent Progress

**Task:** Add Picture-in-Picture (PiP) support to the `video_player` federated plugin across Android, iOS, macOS, and Web platforms.

**Features complete: 4 / 29**

**Last session:** 2026-02-17

## Feature Summary

| ID   | Description                                                            | Status  |
| ---- | ---------------------------------------------------------------------- | ------- |
| F001 | Platform interface: PiP data models, event types, and abstract methods | done    |
| F002 | Platform interface: Unit tests for PiP additions                       | done    |
| F003 | Platform interface: Version bump and CHANGELOG                         | done    |
| F004 | App-facing: VideoPlayerValue and VideoPlayerController PiP API         | done    |
| F005 | App-facing: PiP event handling, lifecycle observer, dispose            | pending |
| F006 | App-facing: Unit tests for PiP features                                | pending |
| F007 | App-facing: Version bump and CHANGELOG                                 | pending |
| F008 | Android: Pigeon API definition and code generation                     | pending |
| F009 | Android: ActivityAware and isPictureInPictureSupported (Java)          | pending |
| F010 | Android: startPictureInPicture and stopPictureInPicture (Java)         | pending |
| F011 | Android: PiP state detection and multi-player event routing (Java)     | pending |
| F012 | Android: Auto-enter PiP and custom actions (Java)                      | pending |
| F013 | Android: Dart-side PiP delegation in AndroidVideoPlayer                | pending |
| F014 | Android: Dart-side unit tests                                          | pending |
| F015 | Android: Version bump and CHANGELOG                                    | pending |
| F016 | AVFoundation: Pigeon API definition and code generation                | pending |
| F017 | AVFoundation: AVPictureInPictureController and delegate (Obj-C)        | pending |
| F018 | AVFoundation: PiP events, audio session, auto-enter (Obj-C)            | pending |
| F019 | AVFoundation: Pigeon host API implementation (Obj-C)                   | pending |
| F020 | AVFoundation: Dart-side PiP delegation                                 | pending |
| F021 | AVFoundation: Dart-side unit tests                                     | pending |
| F022 | AVFoundation: Version bump and CHANGELOG                               | pending |
| F023 | Web: PiP start/stop and event listeners                                | pending |
| F024 | Web: Auto-enter via visibilitychange and Media Session actions         | pending |
| F025 | Web: VideoPlayerPlugin PiP method overrides                            | pending |
| F026 | Web: Unit tests for PiP                                                | pending |
| F027 | Web: Version bump and CHANGELOG                                        | pending |
| F028 | Documentation: README updates and platform setup guides                | pending |
| F029 | Documentation: Example app PiP controls                                | pending |

**Next recommended:** F005 (App-facing: PiP event handling, lifecycle observer, dispose)

## Session Log

### Session 1 — 2026-02-17

- **Completed:** F001 — Platform interface: PiP data models, event types, and abstract methods
- **Commit:** `bdaf989fb` (agent: impl F001 — add PiP platform interface models and defaults)
- **Details:** Added `PictureInPictureActionType` enum, `PictureInPictureAction` immutable class, `pictureInPictureStarted`/`pictureInPictureStopped` event types to `VideoEventType`, and five new methods on `VideoPlayerPlatform` (`isPictureInPictureSupported`, `startPictureInPicture`, `stopPictureInPicture`, `setAutoPictureInPicture`, `setPictureInPictureActions`) with backward-compatible default implementations.
- **Issues:** None. All verification checks passed (analysis, format, tests, API surface).

### Session 2 — 2026-02-17

- **Completed:** F002 — Platform interface: Unit tests for PiP additions
- **Commits:** `9f4177637` (agent: impl F002 — add PiP platform interface unit tests), `3aeef1442` (agent: fix F002 — remove explicit type annotations to satisfy omit_obvious_local_variable_types lint)
- **Details:** Added `picture_in_picture_test.dart` with tests for `PictureInPictureActionType` enum values, `PictureInPictureAction` equality/hashCode/toString, and `VideoEventType` PiP events. Added default implementation tests in `video_player_platform_interface_test.dart` for `isPictureInPictureSupported` (returns false), `startPictureInPicture`, `stopPictureInPicture`, `setAutoPictureInPicture`, and `setPictureInPictureActions` (throw `UnimplementedError`). All 30 tests pass.
- **Issues:** Minor lint fix needed for `omit_obvious_local_variable_types` (resolved in second commit). One low-severity QA note: hashCode inequality test is technically not guaranteed by contract but unlikely to be flaky in practice.

### Session 3 — 2026-02-17

- **Completed:** F003 — Platform interface: Version bump and CHANGELOG
- **Commit:** `872f4dc29` (agent: impl F003 — bump platform interface to 6.7.0 for PiP APIs)
- **Details:** Bumped `video_player_platform_interface` version from 6.6.0 to 6.7.0 in `pubspec.yaml`. Updated `CHANGELOG.md` replacing `## NEXT` with `## 6.7.0` and added entry describing the new Picture-in-Picture APIs: `PictureInPictureActionType` enum, `PictureInPictureAction` class, new event types, and five new `VideoPlayerPlatform` methods.
- **Issues:** One low-severity QA note: changelog bullet style was changed from `*` to `-` across the entire file; this is a cosmetic difference with no functional impact.

### Session 4 — 2026-02-17

- **Completed:** F004 — App-facing: VideoPlayerValue and VideoPlayerController PiP API
- **Commits:** `9a6ce2536` (agent: impl F004 — add app-facing PiP controller and value API), `2a53c2914` (agent: fix F004 — resolve platform interface dependency, handle PiP events, fix tests)
- **Details:** Added `isPictureInPictureActive` boolean field to `VideoPlayerValue` with full value-semantic support (constructor default false, copyWith, ==, hashCode, toString). Added five PiP methods to `VideoPlayerController` (`isPictureInPictureSupported`, `startPictureInPicture`, `stopPictureInPicture`, `setAutoPictureInPicture`, `setPictureInPictureActions`) — all check `_isDisposedOrNotInitialized` and delegate to platform. Added barrel exports for `PictureInPictureAction` and `PictureInPictureActionType`. Handled `pictureInPictureStarted`/`pictureInPictureStopped` events in controller event listener. Updated `FakeController` in tests. All 91 tests pass.
- **Issues:** Low-severity QA notes: CHANGELOG/version not yet bumped (deferred to F007), PiP methods could document platform exceptions, and dedicated PiP unit tests deferred to F006. Path-based `dependency_overrides` present for local federated testing (expected per AGENTS.md).
