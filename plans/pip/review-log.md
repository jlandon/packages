# Plan Review Log

| Iteration | Date       | Specialists Used                                                                                       | Changes Made                                                                      | Critical Findings                                                                                                                              |
| --------- | ---------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| 1         | 2026-02-17 | Technical Architect, Security Reviewer, Performance Engineer, QA Strategist, API Designer, DX Reviewer | 6 direct plan edits + Open Review Items section added                             | 1 CRITICAL: doc/impl mismatch on throw behavior                                                                                                |
| 2         | 2026-02-17 | Technical Architect, Security Reviewer, Performance Engineer, QA Strategist, API Designer, DX Reviewer | 5 direct plan edits + 7 new Open Review Items                                     | 0 CRITICAL, 3 HIGH: missing Android event type, multi-player PiP, AVFoundation event mapping                                                   |
| 3         | 2026-02-17 | Technical Architect, Security Reviewer, Performance Engineer, QA Strategist, API Designer, DX Reviewer | 4 direct plan edits + 4 new Open Review Items + 1 note update                     | 1 CRITICAL: plugin missing ActivityAware interface. 3 HIGH: no stopPiP native strategy, onUserLeaveHint inaccessible, AVPlayerLayer zero-sized |
| 4         | 2026-02-17 | Technical Architect, Security Reviewer, Performance Engineer, QA Strategist, API Designer, DX Reviewer | 3 direct plan edits + 7 new Open Review Items                                     | 0 CRITICAL, 3 HIGH: ComponentCallbacks lifecycle gap, AVFoundation events bypass protocol, restoreUI handler incomplete                        |
| 5         | 2026-02-17 | Technical Architect, Security Reviewer, Performance Engineer, QA Strategist, API Designer, DX Reviewer | 1 direct plan edit (new Step 3.2) + section renumbering + 6 new Open Review Items | 0 CRITICAL, 1 HIGH: VideoPlayer lacks Activity access for PiP Pigeon methods                                                                   |
| 6         | 2026-02-17 | Technical Architect, Security Reviewer, Performance Engineer, QA Strategist, API Designer, DX Reviewer | 0 direct plan edits + 6 new Open Review Items (items 38-43)                       | 0 CRITICAL, 0 HIGH: Plan approaching stable — findings now MEDIUM/LOW only                                                                     |
| 7         | 2026-02-17 | Technical Architect, Security Reviewer, Performance Engineer, QA Strategist, API Designer, DX Reviewer | 0 direct plan edits + 1 new Open Review Item (item 44)                            | 0 CRITICAL, 0 HIGH: Plan confirmed STABLE — early termination warranted                                                                        |

### Iteration 1 Details

**CRITICAL findings addressed (1):**

- `startPictureInPicture()` doc said "Throws" but implementation silently returns → Fixed doc comment to match actual behavior

**HIGH findings addressed (5):**

- Missing `setPictureInPictureActions` from AVFoundation Pigeon API → Added explicit note that it's intentionally omitted (no-op on iOS/macOS) with rationale
- Pigeon methods not allocated to correct APIs → Specified which Pigeon host API (global vs instance) each method belongs to for both Android and AVFoundation
- Android PiP detection approach underspecified (3 options, none chosen) → Committed to `ComponentCallbacks2.onConfigurationChanged()` with rationale and code example
- No error handling for failed PiP start → Added error reporting strategy using existing `PlatformException` error stream
- No test for dispose-during-PiP + no dispose-time PiP cleanup → Added Step 2.5 for `dispose()` update and added dispose-during-PiP test case
- Missing action callback mechanism clarity → Added Step 1.4 explaining action handling design (native-side handling, no Dart callback needed for v1)

**MEDIUM/LOW findings (13 total) deferred to Open Review Items section.**

### Iteration 2 Details

**CRITICAL findings (0):** None

**HIGH findings addressed (3):**

- Android Pigeon API missing `PictureInPictureStateEvent` in sealed `PlatformVideoEvent` class → Added event type definition, Dart-side handler in `_onStreamEvent()`, and replaced fragile `int type` with Pigeon `PipActionType` enum in `PipAction`
- Multiple Android players sharing Activity-level PiP not addressed → Added new Step 3.2 clarifying event routing (all players get events), action routing (primary player), and aspect ratio behavior
- AVFoundation Dart-side PiP event mapping missing → Added Step 4.6 specifying both native-side event sending pattern (`{'event': 'pictureInPictureStarted'}`) and Dart-side `_onStreamEvent` switch cases

**Additional plan edits (2):**

- `setPictureInPictureActions()` doc comment misleadingly said "iOS shows media session controls" → Rewrote to explicitly state iOS/macOS no-op behavior
- `PictureInPictureAction` missing `==`/`hashCode`/`toString` → Added comment in class definition following existing codebase patterns

**MEDIUM/LOW findings (7 total) deferred to Open Review Items section (items 14-20).**

### Iteration 3 Details

**CRITICAL findings addressed (1):**

- Android `VideoPlayerPlugin` does NOT implement `ActivityAware` — the plan incorrectly claimed "The plugin already implements `ActivityAware` via `FlutterPlugin`". Verified in source: the plugin only implements `FlutterPlugin` and `AndroidVideoPlayerApi`. Without Activity access, `enterPictureInPictureMode()`, `isInPictureInPictureMode()`, and `ComponentCallbacks2` registration are all impossible. → Added full `ActivityAware` implementation with all four lifecycle methods, null-safety guards, and cleanup in `onDetachedFromActivity`.

**HIGH findings addressed (3):**

- Android has no `exitPictureInPictureMode()` API — `stopPictureInPicture()` was defined in the Pigeon API but had no native implementation strategy → Added Step 3.3.4 with `FLAG_ACTIVITY_REORDER_TO_FRONT` intent-based approach, documented UX implications and why `moveTaskToBack(false)` is unsuitable
- Pre-Android 12 auto-enter via `onUserLeaveHint()` is infeasible from a FlutterPlugin (even with `ActivityAware`, the plugin cannot override Activity methods) → Replaced Step 3.3.8 to specify Android 12+ only with silent no-op on older versions, added explicit code example
- AVFoundation `FVPTextureBasedVideoPlayer`'s `AVPlayerLayer` is zero-sized (verified in source: no frame set after `playerLayerWithPlayer:`). Apple requires non-zero layer size for `isPictureInPicturePossible` to return `true` → Updated Step 4.3 with frame-setting requirement and sublayer lifecycle notes

**Additional plan edits (1):**

- Updated Platform Behavior Differences note to document Android auto-enter limitation on pre-12

**MEDIUM findings (4 total) deferred to Open Review Items section (items 21-24):**

- Error propagation path unclear (Future vs event stream) — API Design
- `stopPictureInPicture()` during dispose needs try/catch — Performance
- Hot restart orphans native PiP state — Technical Architecture
- `setAutoPictureInPicture` state not tracked in `VideoPlayerValue` — API Design

### Iteration 4 Details

**CRITICAL findings (0):** None

**HIGH findings addressed (3):**

- Android `ComponentCallbacks2` registration/unregistration lifecycle was missing from `ActivityAware` methods — `onDetachedFromActivityForConfigChanges` only set `activity = null` without unregistering the listener, causing duplicate events on reattach. Also `wasInPipMode` was not initialized from current Activity state → Updated Step 3.3.1 with explicit `registerPipComponentCallbacks()`/`unregisterPipComponentCallbacks()` helper methods called from all four `ActivityAware` lifecycle methods, and `wasInPipMode` initialization from `activity.isInPictureInPictureMode()`
- AVFoundation PiP events bypassed the `FVPVideoEventListener` protocol — Step 4.6 showed raw `[channel sendMessage:]` calls but the codebase uses typed protocol methods (e.g., `videoPlayerDidComplete`) dispatched through `FVPEventBridge.sendOrQueue:` → Rewrote Step 4.6 as a 4-step process: add protocol methods to `FVPVideoEventListener.h`, implement in `FVPEventBridge.m`, call from PiP delegate in `FVPVideoPlayer`, handle in Dart `_onStreamEvent`
- `restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:` had comment saying "Signal the Dart side" but code immediately called `completionHandler(YES)` with no signaling mechanism → Updated code comment to document this as intentional v1 behavior with rationale, added note about always calling the completion handler, and outlined future iteration approach

**MEDIUM/LOW findings (7 total) deferred to Open Review Items section (items 25-31):**

- No guidance on Texture widget behavior during iOS/macOS PiP — Developer Experience (MEDIUM)
- `setPictureInPictureActions([])` behavior undefined — API Design (MEDIUM)
- Start/stop PiP race condition on Android — Performance (MEDIUM)
- iOS `restoreUserInterfaceForPictureInPictureStop` needs future iteration plan — Technical Architecture (MEDIUM)
- Missing test: AVPlayerItem failure during active PiP — QA Strategy (LOW)
- Missing test: auto-enter + manual start/stop interaction — QA Strategy (LOW)
- Web `dispose()` does not clean up PiP event listeners — Performance (LOW)

### Iteration 5 Details

**CRITICAL findings (0):** None

**HIGH findings addressed (1):**

- Android `VideoPlayer` class implements `VideoPlayerInstanceApi` (the Pigeon per-player interface) but has **no Activity reference** — PiP methods defined on `VideoPlayerInstanceApi` (Step 3.1) are routed to individual `VideoPlayer` instances, but `enterPictureInPictureMode()`, `isInPictureInPictureMode()`, and `setPictureInPictureParams()` all require Activity access that only `VideoPlayerPlugin` has via `ActivityAware`. Verified in source: `VideoPlayer` only holds `VideoPlayerCallbacks`, `SurfaceProducer`, and `ExoPlayer`. → Added new Step 3.2 specifying Activity supplier injection pattern (matching existing `ExoPlayerProvider` functional interface pattern), wiring in `registerPlayerInstance()`, and null-safety guards. Renumbered subsequent Step 3 subsections (3.3→3.4 through 3.9→3.10) and updated all internal cross-references.

**MEDIUM findings (2 total) deferred to Open Review Items section (items 32-33):**

- Android `PictureInPictureParams` state fragmentation — each param-setting method builds independently, but `setPictureInPictureParams()` replaces ALL params, causing one method to silently clear params set by another — Technical Architecture (MEDIUM)
- Web `stopPictureInPicture()` calls document-level `exitPictureInPicture()` which exits PiP for whatever element is active, not just the calling player — correctness bug in multi-player scenarios — Performance (MEDIUM)

**LOW findings (4 total) deferred to Open Review Items section (items 34-37):**

- Android `isPictureInPictureSupported()` checks hardware feature but not manifest `supportsPictureInPicture` attribute — API Design (LOW)
- Missing test: dynamic PiP action update during active PiP — QA Strategy (LOW)
- Android PiP aspect ratio not updated on video resolution change during PiP — Technical Architecture (LOW)
- Android BroadcastReceiver registration approach (dynamic vs manifest) unspecified — Security (LOW)

### Iteration 6 Details

**CRITICAL findings (0):** None

**HIGH findings (0):** None

Plan has reached a maturity level where no CRITICAL or HIGH issues remain to be found. All findings are subtle edge cases and polish items.

**MEDIUM findings (2) deferred to Open Review Items section (items 38-39):**

- AVFoundation `stopPictureInPicture` Pigeon call returns before PiP animation completes — Step 2.5's dispose sequence calls `stopPictureInPicture()` (which resolves when the Pigeon message returns, not when the PiP window finishes its close animation), then immediately calls `dispose(_playerId)` which executes `[self.player replaceCurrentItemWithPlayerItem:nil]` and `[self.playerLayer removeFromSuperlayer]` (verified in source: `FVPVideoPlayer.disposeWithError:` and `FVPTextureBasedVideoPlayer.disposeWithError:`). This tears down the player layer during the PiP close animation, causing a black flash. Distinct from Item 22 (try/catch). Recommend making the AVFoundation Pigeon method `@async`. — Technical Architecture (MEDIUM)
- New player registered during active Android PiP starts with stale `isPictureInPictureActive = false` — `registerPlayerInstance()` (Step 3.2) wires the Activity supplier but doesn't check current PiP state or emit an initial event. A controller created while the Activity is already in PiP mode would have incorrect state until the next PiP transition. — API Design (MEDIUM)

**LOW findings (4 total) deferred to Open Review Items section (items 40-43):**

- Multi-player PiP collision untested on iOS/macOS/web (starting PiP on player B while player A is in PiP) — QA Strategy (LOW)
- Web PiP blocked by Permissions Policy when Flutter app is embedded in iframe — Developer Experience (LOW)
- `isPictureInPictureActive` cross-platform behavioral asymmetry (Android: all controllers true vs others: only initiator true) not documented in field API docs — API Design (LOW)
- iOS/macOS `pipPossibleObservation` block-based KVO token cleanup absent from `disposeWithError:` path — Performance (LOW)

### Iteration 7 Details

**CRITICAL findings (0):** None

**HIGH findings (0):** None

**Confirming pass result:** Plan confirmed STABLE for the second consecutive iteration (iterations 6 and 7). All 6 specialists independently found no CRITICAL or HIGH issues. The plan has been through 7 iterations with findings trending decisively downward: CRITICAL(1,0,1,0,0,0,0), HIGH(6,3,3,3,1,0,0). Early termination is warranted.

**LOW findings (1) deferred to Open Review Items section (item 44):**

- Android auto-enter PiP params (`autoEnterEnabled`) not cleared on player dispose — Activity-level params persist after the player that set them is disposed, potentially causing unintended auto-enter on subsequent app-leave events — Technical Architecture (LOW)
