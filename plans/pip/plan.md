# Goal

Add Picture-in-Picture (PiP) support to the `video_player` federated plugin across all supported platforms: **Android**, **iOS**, **macOS**, and **Web**.

PiP allows video playback to continue in a small floating window while the user navigates away from the player screen or app. The feature should be exposed as methods on `VideoPlayerController` with state tracked on `VideoPlayerValue`, following the existing API conventions of the plugin.

## Additional Context

### Architecture Overview

The `video_player` plugin uses a **federated plugin architecture** with five packages:

| Package                           | Role                                                                              |
| --------------------------------- | --------------------------------------------------------------------------------- |
| `video_player_platform_interface` | Defines the abstract `VideoPlayerPlatform` contract, data models, and event types |
| `video_player`                    | App-facing API (`VideoPlayerController`, `VideoPlayerValue`, widgets)             |
| `video_player_android`            | Android implementation using ExoPlayer                                            |
| `video_player_avfoundation`       | iOS/macOS implementation using AVPlayer + AVPlayerLayer                           |
| `video_player_web`                | Web implementation using HTMLVideoElement                                         |

Communication between Dart and native code uses **Pigeon** (Android and iOS/macOS) and **direct JS interop** (web). Each player is identified by an integer `playerId`. Events flow from native to Dart via event channels/streams.

### Platform PiP Characteristics

| Platform    | PiP Scope                                   | API                                                          | Notes                                                                                                                                                         |
| ----------- | ------------------------------------------- | ------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Android** | Activity-level (entire Flutter app shrinks) | `Activity.enterPictureInPictureMode(PictureInPictureParams)` | Requires `android:supportsPictureInPicture="true"` in manifest. Custom actions via `RemoteAction`. Auto-enter via `setAutoEnterEnabled`.                      |
| **iOS**     | Video-only (floating video window)          | `AVPictureInPictureController(playerLayer:)`                 | Requires "Audio, AirPlay, and Picture in Picture" background mode. Audio session must use `.playback` category. Available on iPad (iOS 9+), iPhone (iOS 14+). |
| **macOS**   | Video-only (floating video window)          | `AVPictureInPictureController(playerLayer:)`                 | Available macOS 12+. No background mode entitlement needed. No AVAudioSession configuration needed.                                                           |
| **Web**     | Video-only (floating video element)         | `HTMLVideoElement.requestPictureInPicture()`                 | Uses Picture-in-Picture API. Custom controls via Media Session API. Not supported in all browsers (check `document.pictureInPictureEnabled`).                 |

### Key Design Decisions

1. **API Style**: Methods on `VideoPlayerController` (e.g., `startPictureInPicture()`, `stopPictureInPicture()`) with state on `VideoPlayerValue`.
2. **Auto-enter**: Supported as opt-in (default off). When enabled, PiP activates automatically when the user leaves the app.
3. **Android approach**: Use standard Activity-level PiP (entire app enters PiP mode).
4. **Custom actions**: Allow developers to define custom PiP action buttons (e.g., skip forward/back).
5. **View type**: Support PiP with both `textureView` and `platformView` where feasible. On iOS/macOS, both view types already have access to `AVPlayerLayer` (the texture-based player creates an offscreen one), so PiP should work with both.

### Reference Documentation

- **Android**: https://developer.android.com/develop/ui/views/picture-in-picture
- **iOS/macOS**: https://developer.apple.com/documentation/avkit/avpictureinpicturecontroller
- **Web**: https://developer.mozilla.org/en-US/docs/Web/API/Picture-in-Picture_API

---

## Steps

### Step 1: Platform Interface (`video_player_platform_interface`)

Define the cross-platform PiP contract that all platform implementations must fulfill.

#### 1.1 Add new data models

Add the following classes to the platform interface:

```dart
/// Represents a custom action button displayed in the PiP window.
@immutable
class PictureInPictureAction {
  const PictureInPictureAction({
    required this.type,
    required this.label,
  });

  /// The type of action. Platforms map this to the appropriate native control.
  final PictureInPictureActionType type;

  /// Human-readable label for accessibility purposes.
  final String label;

  // Implement ==, hashCode, and toString following the existing
  // pattern used by VideoAudioTrack, DurationRange, etc.
}

/// Standard PiP action types that map to native controls across platforms.
enum PictureInPictureActionType {
  /// Play the media.
  play,

  /// Pause the media.
  pause,

  /// Skip forward by a platform-defined interval.
  skipForward,

  /// Skip backward by a platform-defined interval.
  skipBackward,

  /// Go to the next track/item.
  nextTrack,

  /// Go to the previous track/item.
  previousTrack,
}
```

#### 1.2 Add new event types

Extend `VideoEventType` with PiP lifecycle events:

```dart
enum VideoEventType {
  // ... existing values ...

  /// The video has entered Picture-in-Picture mode.
  pictureInPictureStarted,

  /// The video has exited Picture-in-Picture mode.
  pictureInPictureStopped,
}
```

Platform implementations should report PiP failures through the existing error stream (using `PlatformException`), consistent with how other errors (e.g., media load failures) are reported. This avoids adding a dedicated error event type while still informing the Dart side of issues like:

- Missing entitlements (iOS)
- User gesture required (web)
- Unsupported device (Android API < 26)
- PiP already active for another player

#### 1.3 Add new methods to `VideoPlayerPlatform`

```dart
abstract class VideoPlayerPlatform extends PlatformInterface {
  // ... existing methods ...

  /// Returns whether PiP is supported on this platform/device.
  Future<bool> isPictureInPictureSupported() async {
    return false;
  }

  /// Starts Picture-in-Picture mode for the given player.
  Future<void> startPictureInPicture(int playerId) {
    throw UnimplementedError(
      'startPictureInPicture() has not been implemented.',
    );
  }

  /// Stops Picture-in-Picture mode for the given player.
  Future<void> stopPictureInPicture(int playerId) {
    throw UnimplementedError(
      'stopPictureInPicture() has not been implemented.',
    );
  }

  /// Enables or disables automatic PiP when the user leaves the app.
  /// Default is false (disabled).
  Future<void> setAutoPictureInPicture(int playerId, bool enabled) {
    throw UnimplementedError(
      'setAutoPictureInPicture() has not been implemented.',
    );
  }

  /// Sets the custom actions displayed in the PiP window.
  Future<void> setPictureInPictureActions(
    int playerId,
    List<PictureInPictureAction> actions,
  ) {
    throw UnimplementedError(
      'setPictureInPictureActions() has not been implemented.',
    );
  }
}
```

#### 1.4 Action handling design

Custom PiP actions represent standard media transport controls (play, pause, skip, etc.). When a user taps an action in the PiP window, the native platform handles it directly by invoking the corresponding player operation (e.g., tapping "skip forward" seeks forward by a platform-defined interval). This means:

- **No Dart-side callback is needed** for standard actions — the player state changes are already reported via the existing event stream (e.g., `isPlayingStateUpdate` for play/pause, position changes for seek).
- The `PictureInPictureAction` list defines which controls are _shown_, not custom behavior.
- If a future iteration requires app-defined custom actions (beyond standard media controls), a new event type (e.g., `pictureInPictureActionTriggered`) with an action identifier could be added to the platform interface at that time.

This keeps the initial API surface minimal and avoids over-engineering for a use case that doesn't exist yet across all platforms.

#### 1.5 Update version and CHANGELOG

Bump the minor version (new public API surface). Update `CHANGELOG.md` with the new PiP types and methods.

---

### Step 2: App-Facing Package (`video_player`)

Expose PiP functionality through the existing `VideoPlayerController` and `VideoPlayerValue`.

#### 2.1 Extend `VideoPlayerValue`

Add PiP state fields:

```dart
class VideoPlayerValue {
  const VideoPlayerValue({
    // ... existing fields ...
    this.isPictureInPictureActive = false,
  });

  /// Whether the video is currently playing in Picture-in-Picture mode.
  final bool isPictureInPictureActive;

  // Update copyWith, ==, hashCode, toString accordingly
}
```

#### 2.2 Add methods to `VideoPlayerController`

```dart
class VideoPlayerController extends ValueNotifier<VideoPlayerValue> {
  // ... existing code ...

  /// Returns whether Picture-in-Picture is supported on this device/platform.
  ///
  /// Returns false if the platform does not support PiP or if the device
  /// hardware does not allow it.
  Future<bool> isPictureInPictureSupported() async {
    return _videoPlayerPlatform.isPictureInPictureSupported();
  }

  /// Starts Picture-in-Picture mode.
  ///
  /// On Android, this puts the entire app into a small floating window.
  /// On iOS/macOS, only the video floats in a system-managed window.
  /// On web, the video element enters browser PiP.
  ///
  /// Has no effect if the player is not initialized or has been disposed.
  /// The platform may emit an error event if PiP fails to start (e.g.,
  /// unsupported device, missing entitlements, or user gesture required).
  Future<void> startPictureInPicture() async {
    if (_isDisposedOrNotInitialized) {
      return;
    }
    await _videoPlayerPlatform.startPictureInPicture(_playerId);
  }

  /// Stops Picture-in-Picture mode and returns to inline playback.
  Future<void> stopPictureInPicture() async {
    if (_isDisposedOrNotInitialized) {
      return;
    }
    await _videoPlayerPlatform.stopPictureInPicture(_playerId);
  }

  /// Enables or disables automatic Picture-in-Picture.
  ///
  /// When enabled, PiP activates automatically when the user navigates
  /// away from the app (e.g., presses home button or swipes up).
  ///
  /// Default is false (disabled).
  Future<void> setAutoPictureInPicture(bool enabled) async {
    if (_isDisposedOrNotInitialized) {
      return;
    }
    await _videoPlayerPlatform.setAutoPictureInPicture(_playerId, enabled);
  }

  /// Sets the custom actions displayed in the PiP window.
  ///
  /// These actions appear as buttons in the PiP overlay.
  /// Platform support varies:
  /// - **Android**: Custom `RemoteAction` buttons in the PiP window.
  /// - **Web**: Uses the Media Session API for PiP controls (Chrome only).
  /// - **iOS/macOS**: This method has no effect. PiP controls are determined
  ///   by the system and `MPNowPlayingInfoCenter`/`MPRemoteCommandCenter`.
  Future<void> setPictureInPictureActions(
    List<PictureInPictureAction> actions,
  ) async {
    if (_isDisposedOrNotInitialized) {
      return;
    }
    await _videoPlayerPlatform.setPictureInPictureActions(_playerId, actions);
  }
}
```

#### 2.3 Handle PiP events in the event listener

In `VideoPlayerController.initialize()`, extend the `eventListener` switch to handle the new event types:

```dart
case VideoEventType.pictureInPictureStarted:
  value = value.copyWith(isPictureInPictureActive: true);
case VideoEventType.pictureInPictureStopped:
  value = value.copyWith(isPictureInPictureActive: false);
```

#### 2.4 Update `_VideoAppLifeCycleObserver`

When PiP is active, the lifecycle observer should **not** pause the video when the app goes to background. Update `didChangeAppLifecycleState` to check `isPictureInPictureActive`:

```dart
@override
void didChangeAppLifecycleState(AppLifecycleState state) {
  if (state == AppLifecycleState.paused) {
    // Don't pause if PiP is active — the video should keep playing.
    if (!_controller.value.isPictureInPictureActive) {
      _wasPlayingBeforePause = _controller.value.isPlaying;
      _controller.pause();
    }
  } else if (state == AppLifecycleState.resumed) {
    if (_wasPlayingBeforePause) {
      _controller.play();
    }
  }
}
```

#### 2.5 Update `dispose()` to stop PiP

The `VideoPlayerController.dispose()` method must stop PiP before disposing the player to prevent platform-specific issues (black frames on iOS, Activity recreation issues on Android, orphaned PiP windows on web):

```dart
@override
Future<void> dispose() async {
  if (_isDisposed) {
    return;
  }

  if (_creatingCompleter != null) {
    await _creatingCompleter!.future;
    if (!_isDisposed) {
      _isDisposed = true;
      _timer?.cancel();
      // Stop PiP before disposing to avoid platform-specific issues.
      if (value.isPictureInPictureActive) {
        await _videoPlayerPlatform.stopPictureInPicture(_playerId);
      }
      await _eventSubscription?.cancel();
      await _videoPlayerPlatform.dispose(_playerId);
    }
    _lifeCycleObserver?.dispose();
  }
  _isDisposed = true;
  super.dispose();
}
```

#### 2.6 Export new types

Update the `export` statement to include `PictureInPictureAction` and `PictureInPictureActionType`.

#### 2.7 Add tests

Add unit tests for:

- `isPictureInPictureSupported()` delegation
- `startPictureInPicture()` / `stopPictureInPicture()` delegation
- `setAutoPictureInPicture()` delegation
- `setPictureInPictureActions()` delegation
- `VideoPlayerValue.isPictureInPictureActive` state updates from events
- Lifecycle observer behavior when PiP is active
- `copyWith`, `==`, `hashCode` for updated `VideoPlayerValue`
- **Dispose while PiP is active**: Verify that `dispose()` stops PiP before disposing the player, and that no errors occur
- **Start PiP when not initialized**: Verify silent no-op behavior
- **Start PiP when already in PiP**: Verify idempotent behavior

#### 2.8 Update version and CHANGELOG

Bump the minor version. Update `CHANGELOG.md`.

---

### Step 3: Android Implementation (`video_player_android`)

Implement Activity-level PiP using Android's `PictureInPictureParams` API.

#### 3.1 Update Pigeon API definition

Add PiP methods to the Pigeon `.dart` definition file (in `pigeons/`). Per-player methods go on `VideoPlayerInstanceApi`; the platform-level support check goes on `AndroidVideoPlayerApi`:

```dart
// In AndroidVideoPlayerApi (global):
bool isPictureInPictureSupported();

// In VideoPlayerInstanceApi (per-player):
void startPictureInPicture();
void stopPictureInPicture();
void setAutoPictureInPicture(bool enabled);
void setPictureInPictureActions(List<PipAction> actions);
```

Define Pigeon data types for PiP:

```dart
/// Pigeon enum mirroring PictureInPictureActionType.
/// Must be kept in sync with the platform interface enum.
enum PipActionType { play, pause, skipForward, skipBackward, nextTrack, previousTrack }

class PipAction {
  PipAction({required this.type, required this.label});
  final PipActionType type;
  final String label;
}
```

Add a new event type to the sealed `PlatformVideoEvent` class for PiP state changes (matching the existing pattern used by `IsPlayingStateEvent`, `PlaybackStateChangeEvent`, etc.):

```dart
/// Sent when the Activity enters or exits Picture-in-Picture mode.
class PictureInPictureStateEvent extends PlatformVideoEvent {
  late final bool isInPictureInPicture;
}
```

In the Dart-side `_PlayerInstance._onStreamEvent()`, add a handler for the new event type:

```dart
case PictureInPictureStateEvent _:
  _eventStreamController.add(
    VideoEvent(
      eventType: event.isInPictureInPicture
          ? VideoEventType.pictureInPictureStarted
          : VideoEventType.pictureInPictureStopped,
    ),
  );
```

Run `dart run pigeon` to regenerate the platform channel code.

#### 3.2 Activity dependency injection into `VideoPlayer`

The Pigeon API (Step 3.1) adds PiP methods to `VideoPlayerInstanceApi`, which is implemented by the abstract `VideoPlayer` class. However, `VideoPlayer` currently has no Activity reference — it only holds `VideoPlayerCallbacks`, `SurfaceProducer`, and `ExoPlayer`. PiP operations (`enterPictureInPictureMode()`, `isInPictureInPictureMode()`, `setPictureInPictureParams()`) all require `Activity`.

To bridge this gap, `VideoPlayerPlugin` (which gains the Activity via `ActivityAware`) must inject the Activity reference into `VideoPlayer` instances. The recommended approach, following the existing dependency injection pattern used for `VideoPlayerCallbacks` and `SurfaceProducer`:

1. **Add an Activity supplier to `VideoPlayer`**: Rather than passing the Activity directly (which can change across configuration changes), pass a supplier/getter that returns the current Activity:

   ```java
   public abstract class VideoPlayer implements VideoPlayerInstanceApi {
     // Existing fields...
     @Nullable private Supplier<Activity> activitySupplier;

     public void setActivitySupplier(@Nullable Supplier<Activity> supplier) {
       this.activitySupplier = supplier;
     }

     @Nullable
     protected Activity getActivity() {
       return activitySupplier != null ? activitySupplier.get() : null;
     }
   }
   ```

   Note: Since `java.util.function.Supplier` requires API level 24 but the plugin supports API 21+, use a custom functional interface (the codebase already uses this pattern — see `ExoPlayerProvider`).

2. **Wire in `VideoPlayerPlugin.registerPlayerInstance()`**: After creating each player, pass the Activity supplier:

   ```java
   private void registerPlayerInstance(VideoPlayer player, long id) {
     // Existing setup...
     player.setActivitySupplier(() -> this.activity);
     videoPlayers.put(id, player);
   }
   ```

   Since `this.activity` is updated by `ActivityAware` lifecycle methods, all players automatically get the current Activity (or `null` if detached).

3. **Guard all PiP methods in `VideoPlayer`**: Each PiP method implementation must check `getActivity() != null` and return an error or no-op when the Activity is unavailable (between detach and reattach).

#### 3.3 Multi-player considerations for Activity-level PiP

Android PiP is Activity-level — calling `enterPictureInPictureMode()` shrinks the entire Flutter Activity, not a single video. This has implications when multiple `VideoPlayerController` instances exist:

- **Event routing**: When PiP mode changes (enter/exit), the `PictureInPictureStateEvent` must be emitted to **all** active players' event channels, since the Activity-level PiP affects all of them.
- **Action routing**: Custom PiP actions (play, pause, skip) should be routed to the player that most recently called `startPictureInPicture()`. The plugin should track the "primary PiP player" ID.
- **`stopPictureInPicture()`**: Any player calling `stopPictureInPicture()` exits PiP for the entire Activity.
- **Aspect ratio**: The PiP window aspect ratio should be set from the primary PiP player's video dimensions.

The plugin should maintain a `primaryPipPlayerId` field that is set when `startPictureInPicture()` is called and cleared when PiP exits.

#### 3.4 Implement native Android PiP

**In `VideoPlayerPlugin` (or relevant Java/Kotlin class):**

1. **Add `ActivityAware` interface**: The plugin currently only implements `FlutterPlugin` and `AndroidVideoPlayerApi` — it does **not** implement `ActivityAware` and has no Activity reference. PiP requires `Activity.enterPictureInPictureMode()`, so the plugin **must** add `ActivityAware` to gain Activity access:

   ```java
   public class VideoPlayerPlugin implements FlutterPlugin, AndroidVideoPlayerApi, ActivityAware {
     private Activity activity;
     private ComponentCallbacks2 pipComponentCallbacks;

     @Override
     public void onAttachedToActivity(ActivityPluginBinding binding) {
       activity = binding.getActivity();
       registerPipComponentCallbacks();
       // Initialize wasInPipMode from current Activity state in case
       // we attach to an Activity that is already in PiP (e.g., after hot restart).
       wasInPipMode = activity.isInPictureInPictureMode();
     }

     @Override
     public void onDetachedFromActivityForConfigChanges() {
       unregisterPipComponentCallbacks();
       activity = null;
     }

     @Override
     public void onReattachedToActivityForConfigChanges(ActivityPluginBinding binding) {
       activity = binding.getActivity();
       registerPipComponentCallbacks();
       wasInPipMode = activity.isInPictureInPictureMode();
     }

     @Override
     public void onDetachedFromActivity() {
       unregisterPipComponentCallbacks();
       // Clean up PiP state: clear primaryPipPlayerId
       primaryPipPlayerId = null;
       activity = null;
     }

     private void registerPipComponentCallbacks() {
       if (activity != null && pipComponentCallbacks == null) {
         pipComponentCallbacks = new ComponentCallbacks2() { /* see Step 3.4.6 */ };
         activity.registerComponentCallbacks(pipComponentCallbacks);
       }
     }

     private void unregisterPipComponentCallbacks() {
       if (activity != null && pipComponentCallbacks != null) {
         activity.unregisterComponentCallbacks(pipComponentCallbacks);
         pipComponentCallbacks = null;
       }
     }
   }
   ```

   All PiP methods must guard against `activity == null` (which occurs between detach and reattach) and return an error or no-op.

2. **Check PiP support**:

   ```java
   boolean isPipSupported = activity != null
       && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
       && activity.getPackageManager()
           .hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE);
   ```

3. **Enter PiP mode**:

   ```java
   PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();

   // Set aspect ratio from video dimensions
   Rational aspectRatio = new Rational(videoWidth, videoHeight);
   builder.setAspectRatio(aspectRatio);

   // Set auto-enter (Android 12+)
   if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
       builder.setAutoEnterEnabled(autoEnterEnabled);
   }

   // Set custom actions
   if (actions != null && !actions.isEmpty()) {
       builder.setActions(buildRemoteActions(actions));
   }

   activity.enterPictureInPictureMode(builder.build());
   ```

4. **Exit PiP mode** (`stopPictureInPicture`):

   Android has no direct `exitPictureInPictureMode()` API. The implementation should bring the Activity back to full-screen by launching it with `FLAG_ACTIVITY_REORDER_TO_FRONT`:

   ```java
   void stopPictureInPicture() {
       if (activity == null || !activity.isInPictureInPictureMode()) {
           return; // Already not in PiP — idempotent no-op.
       }
       Intent intent = new Intent(activity, activity.getClass());
       intent.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
       activity.startActivity(intent);
   }
   ```

   This restores the Activity to full-screen without backgrounding it (unlike `moveTaskToBack(false)`, which would exit PiP but also hide the app). The `onConfigurationChanged` callback will detect the PiP exit and emit the `pictureInPictureStopped` event. Document this Android-specific behavior in the API docs: stopping PiP on Android expands the entire app back to full-screen.

5. **Custom actions with `RemoteAction`**:

   ```java
   private List<RemoteAction> buildRemoteActions(List<PipAction> actions) {
       // For each action:
       // 1. Create a PendingIntent with a BroadcastReceiver
       // 2. Map action type to an appropriate icon resource
       // 3. Create RemoteAction(icon, title, description, pendingIntent)
       // Max actions: Activity.getMaxNumPictureInPictureActions()
   }
   ```

6. **Detect PiP mode changes**: Use `Activity.registerComponentCallbacks()` with a `ComponentCallbacks2` implementation that overrides `onConfigurationChanged()`. When a configuration change occurs, check `activity.isInPictureInPictureMode()` and compare against the previous state. This approach is preferred because:
   - The plugin already requires `android:configChanges="screenSize|smallestScreenSize|screenLayout|orientation"` in the manifest, which routes configuration changes to the Activity rather than restarting it.
   - It does not require access to `Application` (unlike `ActivityLifecycleCallbacks`).
   - It provides immediate notification (unlike polling `isInPictureInPictureMode()`).

   ```java
   private boolean wasInPipMode = false;

   @Override
   public void onConfigurationChanged(Configuration newConfig) {
       boolean isInPip = activity.isInPictureInPictureMode();
       if (isInPip != wasInPipMode) {
           wasInPipMode = isInPip;
           // Emit event through event channel
       }
   }
   ```

7. **Send events**: When PiP state changes, emit `pictureInPictureStarted` / `pictureInPictureStopped` events through the existing event channel.

8. **Handle `setAutoPictureInPicture`**: On Android 12+ (API 31), call `setPictureInPictureParams()` with `setAutoEnterEnabled(true/false)`. On pre-Android 12, auto-enter is **not supported** and the method should be a silent no-op. The common workaround of overriding `onUserLeaveHint()` is not feasible because that is an Activity method which a `FlutterPlugin` cannot override — even with `ActivityAware`, the plugin only receives Activity lifecycle callbacks, not the ability to override Activity methods. Requiring host app code changes for a single plugin feature would break the plugin contract. This limitation must be documented.

   ```java
   void setAutoPictureInPicture(boolean enabled) {
       if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && activity != null) {
           PictureInPictureParams params = new PictureInPictureParams.Builder()
               .setAutoEnterEnabled(enabled)
               .build();
           activity.setPictureInPictureParams(params);
       }
       // Pre-Android 12: no-op. Document this limitation.
   }
   ```

#### 3.5 Update AndroidManifest considerations

Document that the host app must add `android:supportsPictureInPicture="true"` and appropriate `android:configChanges` to their Activity in `AndroidManifest.xml`:

```xml
<activity
    android:name=".MainActivity"
    android:supportsPictureInPicture="true"
    android:configChanges="screenSize|smallestScreenSize|screenLayout|orientation"
    .../>
```

This cannot be done automatically by the plugin — it must be documented as a setup requirement.

#### 3.6 Register BroadcastReceiver for custom actions

Create a `BroadcastReceiver` to handle PiP action button taps. When an action is received:

1. Identify which action was tapped (via intent extras)
2. Execute the corresponding player operation (play, pause, seek, etc.)
3. Update the PiP actions to reflect the new state (e.g., swap play/pause icon)

#### 3.7 Handle PiP lifecycle with ExoPlayer

- When entering PiP, hide non-video UI elements (this is the app's responsibility, but the plugin should emit the event so the app can react).
- Ensure ExoPlayer continues playback during PiP transitions.
- When exiting PiP, restore the player state.

#### 3.8 Implement the Dart platform class methods

In the Dart `VideoPlayerAndroid` class, implement the new `VideoPlayerPlatform` methods by delegating to the Pigeon-generated host API calls.

#### 3.9 Add tests

- Unit tests for Pigeon API calls
- Integration tests for PiP enter/exit
- Test auto-enter behavior
- Test custom actions

#### 3.10 Update version and CHANGELOG

Bump the minor version. Update `CHANGELOG.md`.

---

### Step 4: AVFoundation Implementation (`video_player_avfoundation`)

Implement video-level PiP using `AVPictureInPictureController` for both iOS and macOS.

#### 4.1 Update Pigeon API definition

Add PiP methods to the Pigeon `.dart` definition file. Per-player methods go on `VideoPlayerInstanceApi`; the platform-level support check goes on `AVFoundationVideoPlayerApi`:

```dart
// In AVFoundationVideoPlayerApi (global):
bool isPictureInPictureSupported();

// In VideoPlayerInstanceApi (per-player):
void startPictureInPicture();
void stopPictureInPicture();
void setAutoPictureInPicture(bool enabled);
```

Note: `setPictureInPictureActions` is intentionally omitted from the AVFoundation Pigeon API. On iOS/macOS, PiP controls are determined by the system and `MPNowPlayingInfoCenter` / `MPRemoteCommandCenter`, not by `AVPictureInPictureController` directly. The Dart-side `setPictureInPictureActions()` implementation in the AVFoundation platform class should be a no-op (override the platform interface method with an empty body) and this limitation should be documented.

Run `dart run pigeon` to regenerate the platform channel code.

#### 4.2 Implement native PiP controller

**In `FVPVideoPlayer` (Objective-C):**

1. **Import AVKit**:

   ```objc
   #import <AVKit/AVKit.h>
   ```

2. **Add PiP controller property**:

   ```objc
   @property(nonatomic, strong) AVPictureInPictureController *pipController;
   @property(nonatomic, strong) id pipPossibleObservation; // KVO
   ```

3. **Create PiP controller after player is ready**:

   ```objc
   - (void)setupPictureInPicture {
       if (![AVPictureInPictureController isPictureInPictureSupported]) {
           return;
       }

       // Both FVPVideoPlayer (platform view) and FVPTextureBasedVideoPlayer
       // have access to AVPlayerLayer — use it for PiP.
       AVPlayerLayer *playerLayer = /* get from existing player setup */;

       self.pipController = [[AVPictureInPictureController alloc]
           initWithPlayerLayer:playerLayer];
       self.pipController.delegate = self;
   }
   ```

4. **Implement `AVPictureInPictureControllerDelegate`**:

   ```objc
   - (void)pictureInPictureControllerDidStartPictureInPicture:
       (AVPictureInPictureController *)controller {
       // Emit pictureInPictureStarted event
   }

   - (void)pictureInPictureControllerDidStopPictureInPicture:
       (AVPictureInPictureController *)controller {
       // Emit pictureInPictureStopped event
   }

   - (void)pictureInPictureController:(AVPictureInPictureController *)controller
       failedToStartPictureInPictureWithError:(NSError *)error {
       // Log error, optionally emit error event
   }

   - (void)pictureInPictureController:(AVPictureInPictureController *)controller
       restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:
       (void (^)(BOOL))completionHandler {
       // V1 behavior: immediately call completionHandler(YES) to allow
       // the PiP window to dismiss. This means the Flutter UI may not
       // be fully restored (e.g., navigated back to the player screen)
       // before the PiP window animates closed, which can cause a brief
       // visual jump. A future iteration could add a round-trip mechanism
       // to Dart (e.g., via a method channel call that resolves a
       // completion block) to let the app rebuild its widget tree before
       // the completion handler is called.
       //
       // IMPORTANT: The completion handler MUST always be called (with
       // YES or NO). Failing to call it will leave PiP in a broken
       // state where it cannot be restarted.
       completionHandler(YES);
   }
   ```

5. **Start/Stop PiP**:

   ```objc
   - (void)startPictureInPicture {
       [self.pipController startPictureInPicture];
   }

   - (void)stopPictureInPicture {
       [self.pipController stopPictureInPicture];
   }
   ```

6. **Auto-enter PiP**:
   ```objc
   - (void)setAutoPictureInPicture:(BOOL)enabled {
       if (@available(iOS 14.2, macOS 12.0, *)) {
           self.pipController.canStartPictureInPictureAutomaticallyFromInline = enabled;
       }
   }
   ```

#### 4.3 Handle both view types

- **Platform view (`FVPNativeVideoView`)**: Already has an `AVPlayerLayer` as its backing layer. Pass this layer to `AVPictureInPictureController`.
- **Texture view (`FVPTextureBasedVideoPlayer`)**: Already creates an `AVPlayerLayer` (added as a sublayer of the Flutter view's layer for DRM/orientation fix). However, this layer has **no frame set** (zero-sized by default). Apple's `AVPictureInPictureController` requires the backing `AVPlayerLayer` to have a **non-zero size** for `isPictureInPicturePossible` to return `true`. The implementation must set a minimum frame on the player layer for PiP to work:

  ```objc
  // Set a 1x1 frame — enough for PiP to consider the layer valid.
  // The actual PiP window size is determined by the video dimensions, not the layer frame.
  self.playerLayer.frame = CGRectMake(0, 0, 1, 1);
  ```

  Verify during implementation that this minimum size is sufficient across iOS and macOS versions. If `isPictureInPicturePossible` remains `false`, a larger minimum (e.g., 100x100) may be needed. If one doesn't exist yet (edge case), create it specifically for PiP.

Important: The `AVPictureInPictureController` must be kept alive (strong reference) for the entire duration of PiP. Ensure it is not deallocated when the Flutter view is removed. Similarly, the `AVPlayerLayer` must remain in the view hierarchy — `FVPTextureBasedVideoPlayer.disposeWithError:` calls `[self.playerLayer removeFromSuperlayer]`, so PiP must be stopped before dispose (enforced by Step 2.5).

#### 4.4 iOS-specific requirements

1. **Background mode**: Document that the host app must enable "Audio, AirPlay, and Picture in Picture" background mode in the Xcode project capabilities.

2. **Audio session**: The plugin already configures `AVAudioSession` with `.playback` category when `setMixWithOthers` or `setAllowBackgroundPlayback` is called. Ensure that PiP setup also configures the audio session appropriately:

   ```objc
   AVAudioSession *session = [AVAudioSession sharedInstance];
   [session setCategory:AVAudioSessionCategoryPlayback error:nil];
   [session setActive:YES error:nil];
   ```

3. **Timing**: Create the `AVPictureInPictureController` only after the `AVPlayerItem` status is `.readyToPlay`. Observe `isPictureInPicturePossible` via KVO to know when PiP is ready.

#### 4.5 macOS-specific considerations

- No background mode entitlement is needed.
- No `AVAudioSession` configuration (not available on macOS).
- `AVPictureInPictureController` works the same way but availability starts at macOS 12.0.

#### 4.6 Implement PiP event dispatch through `FVPVideoEventListener` protocol

The AVFoundation codebase uses the `FVPVideoEventListener` protocol (defined in `FVPVideoEventListener.h`) as an abstraction between `FVPVideoPlayer` and the event channel. `FVPEventBridge` implements this protocol and dispatches events via `sendOrQueue:`. PiP events **must** follow this same pattern rather than sending raw dictionary messages directly.

**Step 1 — Add new methods to `FVPVideoEventListener` protocol** (in `FVPVideoEventListener.h`):

```objc
/// Called when the video player enters Picture-in-Picture mode.
- (void)videoPlayerDidEnterPictureInPicture;
/// Called when the video player exits Picture-in-Picture mode.
- (void)videoPlayerDidExitPictureInPicture;
```

**Step 2 — Implement in `FVPEventBridge.m`** using the existing `sendOrQueue:` pattern:

```objc
- (void)videoPlayerDidEnterPictureInPicture {
  [self sendOrQueue:@{@"event" : @"pictureInPictureStarted"}];
}

- (void)videoPlayerDidExitPictureInPicture {
  [self sendOrQueue:@{@"event" : @"pictureInPictureStopped"}];
}
```

**Step 3 — Call from `AVPictureInPictureControllerDelegate`** (in `FVPVideoPlayer`):

```objc
- (void)pictureInPictureControllerDidStartPictureInPicture:
    (AVPictureInPictureController *)controller {
    [self.eventListener videoPlayerDidEnterPictureInPicture];
}

- (void)pictureInPictureControllerDidStopPictureInPicture:
    (AVPictureInPictureController *)controller {
    [self.eventListener videoPlayerDidExitPictureInPicture];
}
```

**Step 4 — Dart side** (in `_PlayerInstance._onStreamEvent`): Add cases to the existing switch:

```dart
'pictureInPictureStarted' => VideoEvent(
  eventType: VideoEventType.pictureInPictureStarted,
),
'pictureInPictureStopped' => VideoEvent(
  eventType: VideoEventType.pictureInPictureStopped,
),
```

#### 4.7 Implement the Dart platform class methods

In the Dart `AVFoundationVideoPlayer` class, implement the new `VideoPlayerPlatform` methods by delegating to the Pigeon-generated host API calls. For `setPictureInPictureActions()`, override with an empty body (no-op on iOS/macOS) since PiP controls are determined by the system, not by `AVPictureInPictureController` directly.

#### 4.8 Add tests

- Unit tests for PiP controller setup
- Test delegate callbacks produce correct events
- Test auto-enter configuration
- Test PiP with both texture and platform view types
- Test iOS vs macOS behavior differences
- Test `restoreUserInterfaceForPictureInPictureStop` always calls completion handler

#### 4.9 Update version and CHANGELOG

Bump the minor version. Update `CHANGELOG.md`.

---

### Step 5: Web Implementation (`video_player_web`)

Implement element-level PiP using the browser Picture-in-Picture API.

#### 5.1 Implement PiP methods in `VideoPlayer` (web)

```dart
class VideoPlayer {
  // ... existing code ...

  /// Check if PiP is supported
  bool isPictureInPictureSupported() {
    return web.document.pictureInPictureEnabled;
  }

  /// Start PiP
  Future<void> startPictureInPicture() async {
    await videoElement.requestPictureInPicture().toDart;
  }

  /// Stop PiP
  Future<void> stopPictureInPicture() async {
    await web.document.exitPictureInPicture().toDart;
  }
}
```

#### 5.2 Listen for PiP events

Add event listeners on the `HTMLVideoElement`:

```dart
videoElement.addEventListener('enterpictureinpicture', (event) {
  eventController.add(VideoEvent(
    eventType: VideoEventType.pictureInPictureStarted,
  ));
});

videoElement.addEventListener('leavepictureinpicture', (event) {
  eventController.add(VideoEvent(
    eventType: VideoEventType.pictureInPictureStopped,
  ));
});
```

#### 5.3 Auto-enter PiP on web

The Web Picture-in-Picture API does not natively support auto-entering PiP when the user leaves the page. However, this can be approximated using the `visibilitychange` event:

```dart
web.document.addEventListener('visibilitychange', (event) {
  if (_autoEnterPip && web.document.hidden && !_isInPip) {
    startPictureInPicture();
  }
});
```

Note: This is a best-effort approximation. Browser permissions and user gestures may prevent auto-enter from working in all cases.

#### 5.4 Custom actions via Media Session API

Use the Media Session API to set action handlers that appear in the PiP window (Chrome):

```dart
void setPictureInPictureActions(List<PictureInPictureAction> actions) {
  for (final action in actions) {
    final actionName = _mapActionType(action.type);
    web.navigator.mediaSession.setActionHandler(actionName, (details) {
      // Handle the action (e.g., seek forward/back, next/previous)
    }.toJS);
  }
}

String _mapActionType(PictureInPictureActionType type) {
  switch (type) {
    case PictureInPictureActionType.play: return 'play';
    case PictureInPictureActionType.pause: return 'pause';
    case PictureInPictureActionType.skipForward: return 'seekforward';
    case PictureInPictureActionType.skipBackward: return 'seekbackward';
    case PictureInPictureActionType.nextTrack: return 'nexttrack';
    case PictureInPictureActionType.previousTrack: return 'previoustrack';
  }
}
```

#### 5.5 Handle existing `allowPictureInPicture` web option

The web implementation already has `VideoPlayerWebOptionsControls.allowPictureInPicture` which controls whether the native PiP button appears in browser controls. Ensure the new programmatic PiP API works alongside this existing option:

- If `allowPictureInPicture` is false in web options, `startPictureInPicture()` should still work (it's a programmatic request, not a native control visibility toggle).
- The `disablePictureInPicture` attribute on the video element only affects the native browser control, not the JS API.

#### 5.6 Implement the Dart platform class methods

In `VideoPlayerPlugin` (web), implement the new `VideoPlayerPlatform` overrides by delegating to the `VideoPlayer` instance methods.

#### 5.7 Add tests

- Unit tests for PiP support detection
- Test start/stop PiP API calls
- Test event emission for enter/leave PiP
- Test Media Session API integration for custom actions
- Test interaction with existing `allowPictureInPicture` web option

#### 5.8 Update version and CHANGELOG

Bump the minor version. Update `CHANGELOG.md`.

---

### Step 6: Documentation and Example App

#### 6.1 Update README files

Update the README for each package to document PiP support:

- Platform-specific setup requirements (Android manifest, iOS background modes)
- API usage examples
- Platform behavior differences
- Browser compatibility notes (web)

#### 6.2 Update example app

Add PiP controls to the example app (`video_player/example/`):

- A "Toggle PiP" button that calls `startPictureInPicture()` / `stopPictureInPicture()`
- An "Auto PiP" toggle switch
- Display of `isPictureInPictureActive` state
- Custom action configuration UI

#### 6.3 Platform setup documentation

Create or update a setup guide covering:

**Android setup:**

```xml
<!-- android/app/src/main/AndroidManifest.xml -->
<activity
    android:name=".MainActivity"
    android:supportsPictureInPicture="true"
    android:configChanges="screenSize|smallestScreenSize|screenLayout|orientation">
```

**iOS setup:**

1. In Xcode: Signing & Capabilities > + Capability > Background Modes > check "Audio, AirPlay, and Picture in Picture"
2. Or add to `Info.plist`:

```xml
<key>UIBackgroundModes</key>
<array>
    <string>audio</string>
</array>
```

**macOS setup:**
No additional setup required.

**Web setup:**
No additional setup required. PiP availability depends on browser support.

---

## Notes

### Platform Behavior Differences

- **Android PiP is Activity-level**: When PiP starts on Android, the entire Flutter app shrinks into the floating window, not just the video. The app should hide non-video UI when it detects PiP mode (by listening to `isPictureInPictureActive`). This is fundamentally different from iOS/macOS/web where only the video floats.

- **Android PiP requires API 26+**: Picture-in-picture is only available on Android 8.0 (API level 26) and above. `setAutoEnterEnabled` requires Android 12 (API level 31) — **auto-enter is not supported on pre-Android 12** because the plugin cannot override `Activity.onUserLeaveHint()`. The plugin should gracefully handle older API levels with silent no-ops.

- **iOS requires entitlements**: Without the "Audio, AirPlay, and Picture in Picture" background mode and proper audio session configuration, `isPictureInPicturePossible` will remain `false` and PiP will not work. This is a common source of confusion.

- **Web browser support is limited**: The Picture-in-Picture API is not part of Baseline and is not supported in all browsers (notably Firefox has limited support). Always check `isPictureInPictureSupported()` before offering PiP UI.

- **Auto-enter on web is approximate**: The web platform does not have a native auto-enter PiP API. The `visibilitychange`-based approximation may not work reliably due to browser security restrictions on programmatic PiP entry (often requires a user gesture).

### Lifecycle Observer Interaction

The existing `_VideoAppLifeCycleObserver` pauses video when the app goes to background (unless `allowBackgroundPlayback` is true). When PiP is active, the observer must **not** pause the video, since the whole point of PiP is to continue playback while the user is elsewhere.

### Dispose Behavior

When `VideoPlayerController.dispose()` is called while PiP is active:

- iOS/macOS: Stop PiP first, then dispose the player. Disposing without stopping PiP can cause the floating window to show a black frame briefly.
- Android: Exiting PiP mode may trigger Activity recreation. Ensure the player cleanup is robust.
- Web: Call `document.exitPictureInPicture()` before disposing the video element.

### Custom Actions Limitations

- **Android**: Maximum number of actions is device-dependent (check `Activity.getMaxNumPictureInPictureActions()`). Typically 3.
- **iOS/macOS**: Custom actions beyond play/pause/skip are not directly supported via `AVPictureInPictureController`. The controls are determined by the active `MPNowPlayingInfoCenter` / media session. Setting `requiresLinearPlayback = true` hides skip controls.
- **Web**: Custom actions rely on the Media Session API, which may not be supported in all browsers. The PiP window shows whatever controls the browser supports.

### Testing Considerations

- **Android**: PiP cannot be tested on emulator easily. Use a physical device.
- **iOS**: PiP works on iPad Simulator but NOT on iPhone Simulator. Use physical iPhone for testing.
- **macOS**: PiP can be tested directly on macOS.
- **Web**: Test on Chrome (best PiP support). Firefox and Safari have varying levels of support.

### Code Generation

After modifying any Pigeon definition file, run the Pigeon code generator:

```bash
# From the package directory (e.g., video_player_android/)
dart run pigeon --input pigeons/<file>.dart
```

After modifying any code that uses Mockito mocks, run the build_runner:

```bash
dart run build_runner build -d
```

### Versioning Strategy

Since PiP adds new public API surface, all packages should receive a **minor** version bump:

- `video_player_platform_interface`: Minor bump (new abstract methods with defaults)
- `video_player`: Minor bump (new controller methods and value fields)
- `video_player_android`: Minor bump (new PiP implementation)
- `video_player_avfoundation`: Minor bump (new PiP implementation)
- `video_player_web`: Minor bump (new PiP implementation)

Use the repository's `update-release-info` tool with `--version=minor`.

---

## Open Review Items

Items identified during plan review iterations 1-4 that should be considered during implementation or future review iterations.

### MEDIUM Priority

1. **`isPictureInPictureSupported()` could be synchronous** (API Design)
   The existing `isAudioTrackSupportAvailable()` is a synchronous `bool` method. The PiP support check is also synchronous on all platforms (Android: SDK version check, iOS/macOS: class method, Web: property). Consider making it `bool isPictureInPictureSupported()` instead of `Future<bool>` for consistency. Counter-argument: keeping it async allows future platforms that need async checks.

2. **Web `visibilitychange` listener cleanup in dispose** (Performance)
   The `visibilitychange` listener added to `document` for auto-enter PiP (Step 5.3) must be removed when the player is disposed. Otherwise, listeners accumulate when multiple players are created and destroyed. Add cleanup in the web `VideoPlayer.dispose()` method.

3. **Web `disablePictureInPicture` attribute may also block JS API** (API Design)
   Step 5.5 claims `disablePictureInPicture` only affects the native browser control, not `requestPictureInPicture()`. Per the W3C spec, the attribute may also cause `requestPictureInPicture()` to reject. Verify this behavior during implementation and adjust the documentation accordingly.

4. **`PictureInPictureActionType.play/pause` may duplicate default controls** (API Design)
   PiP windows on all platforms already show play/pause controls by default. Including `play` and `pause` in `PictureInPictureActionType` could lead to duplicate buttons. Clarify during implementation whether setting these replaces or supplements default controls on each platform.

5. **No CI testing strategy for PiP** (QA Strategy)
   PiP cannot be tested on most CI environments (Android emulators, iPhone Simulator). Define explicitly which tests are automated (unit tests with mocked platform, Pigeon API contract tests) vs. which require manual testing (actual PiP enter/exit on physical devices). Add a test matrix to the plan.

6. **No complete integration example in documentation** (Developer Experience)
   Step 6.2 lists individual UI elements for the example app but should also include a minimal-but-complete code snippet in the README showing PiP integration from start to finish (check support, start PiP, handle state, stop PiP).

7. **Missing test for `isPictureInPictureSupported()` returning false** (QA Strategy)
   Ensure all platform test plans include the negative case — testing behavior when PiP is not supported and the app attempts to start PiP.

8. **`allowBackgroundPlayback` interaction with PiP should be documented** (Technical Architecture)
   When `allowBackgroundPlayback` is true, the lifecycle observer is not created, so PiP state has no effect on lifecycle pause behavior (which is correct). This interaction should be explicitly documented to avoid confusion.

9. **Android BroadcastReceiver must be unregistered on plugin detach** (Performance)
   Step 3.6 creates a `BroadcastReceiver` for custom actions but does not mention unregistering it when the plugin detaches from the Activity or the player is disposed. Failure to unregister causes memory leaks and unexpected behavior. Add cleanup in `onDetachedFromActivity()`.

10. **Web Media Session handlers not cleared on dispose** (Performance)
    Step 5.4 sets `navigator.mediaSession.setActionHandler(name, handler)` but doesn't clear these handlers (`setActionHandler(name, null)`) when PiP stops or the player is disposed. Since `navigator.mediaSession` is a singleton, multiple players will overwrite each other's handlers, and stale handlers will persist after dispose.

11. **Web auto-enter PiP needs try/catch for rejection** (QA Strategy)
    The `visibilitychange` handler in Step 5.3 calls `startPictureInPicture()` without error handling. Browsers will reject this call if no user gesture is present. Wrap in try/catch to prevent unhandled promise rejection errors:

    ```dart
    try {
      await startPictureInPicture();
    } catch (_) {
      // Silently ignore — auto-enter is best-effort on web
    }
    ```

12. **Missing test: multiple players during Android PiP** (QA Strategy)
    Add a test verifying that when multiple `VideoPlayerController` instances exist on Android and one starts PiP, all players receive the `pictureInPictureStarted` event. Also test that `stopPictureInPicture()` from either player exits PiP for all.

13. **Missing test: PiP start during buffering/loading state** (QA Strategy)
    No test case covers calling `startPictureInPicture()` while the video is still buffering or loading. Define expected behavior: should it queue the request, fail silently, or emit an error?

### LOW Priority

14. **Android `PendingIntent` for custom actions should use `FLAG_IMMUTABLE`** (Security)
    When creating `PendingIntent` objects for the `BroadcastReceiver` in Step 3.6, use `PendingIntent.FLAG_IMMUTABLE` (required on Android 12+) and scope to the app's package to prevent external triggering.

15. **Position update timer during PiP on Android** (Performance)
    The 100ms position polling timer continues during Android Activity-level PiP. Consider pausing the timer during PiP if the reduced UI doesn't benefit from frequent position updates.

16. **Platform behavior differences should be in API docs** (Developer Experience)
    The fundamental difference between Android Activity-level PiP and iOS/macOS/web video-level PiP should be more prominently surfaced. Consider adding it to the `startPictureInPicture()` doc comment on `VideoPlayerController`, not just in README notes.

17. **Web tests for unsupported browsers** (QA Strategy)
    Web test plan should include mocking `document.pictureInPictureEnabled` as `false` and verifying that `isPictureInPictureSupported()` returns `false` and `startPictureInPicture()` handles the rejection gracefully.

18. **iOS KVO observation for `isPictureInPicturePossible`** (Performance)
    Step 4.4.3 mentions observing this property via KVO but doesn't use it for anything. If this observation isn't needed for the initial implementation, skip it to avoid overhead. It could be added later if a `isPictureInPicturePossible` state field is added to `VideoPlayerValue`.

19. **Android 14+ BroadcastReceiver requires `RECEIVER_NOT_EXPORTED` flag** (Security)
    On Android 14+ (API 34), `Context.registerReceiver()` requires specifying `RECEIVER_NOT_EXPORTED` flag for receivers that should not be accessible to other apps. The PiP action BroadcastReceiver is internal and should use this flag.

20. **Web `startPictureInPicture()` should handle `disablePictureInPicture` rejection** (QA Strategy)
    If the `disablePictureInPicture` attribute is set on the video element (via web options), `requestPictureInPicture()` may reject per the W3C spec. The web implementation's `startPictureInPicture()` should catch this rejection and report it through the error stream rather than letting it propagate as an unhandled exception.

21. **Error propagation path unclear: Future vs event stream** (API Design)
    Step 1.2 says PiP failures are reported through the event stream via `PlatformException`, but Pigeon calls naturally propagate exceptions through the returned `Future`. The plan doesn't clarify whether `startPictureInPicture()` errors come from the Future (synchronous Pigeon errors like "Activity is null"), the event stream (asynchronous delegate failures), or both. Developers need to know which error path to handle. Recommend: synchronous/immediate failures throw from the Future; asynchronous failures (e.g., `failedToStartPictureInPictureWithError` delegate callback) go through the event stream. Document both paths.

22. **`stopPictureInPicture()` during dispose should be wrapped in try/catch** (Performance)
    Step 2.5 calls `stopPictureInPicture()` during dispose, but if PiP was already exited externally (e.g., user dismissed the PiP window) and the platform throws, the dispose flow would halt, leaking the player. Wrap in try/catch to ensure dispose always completes:

    ```dart
    try {
      await _videoPlayerPlatform.stopPictureInPicture(_playerId);
    } catch (_) {
      // Best effort — don't let PiP cleanup failure block disposal.
    }
    ```

23. **Hot restart orphans native PiP state** (Technical Architecture)
    Flutter hot restart destroys all Dart state but does not notify native plugins via their normal dispose path. If PiP is active during hot restart, the native PiP controller/Activity-level PiP continues running with no Dart-side controller to manage it. Consider: on Android, the `initialize()` method (which is called on restart and calls `disposeAllPlayers()`) should also exit PiP if active. On iOS/macOS, `FVPVideoPlayerPlugin.initialize` should stop any active PiP controllers. Document this edge case.

24. **`setAutoPictureInPicture` state not tracked in `VideoPlayerValue`** (API Design)
    The plan adds `isPictureInPictureActive` to `VideoPlayerValue` but doesn't track whether auto-enter is currently enabled. Developers have no way to query the current auto-enter setting after calling `setAutoPictureInPicture()`. Consider adding `bool isAutoPictureInPictureEnabled` to `VideoPlayerValue`, or document that the app should track this state itself.

25. **No guidance on `Texture` widget behavior during iOS/macOS PiP** (Developer Experience)
    When PiP extracts the video on iOS/macOS, the `Texture` widget in the Flutter tree continues rendering but the `AVPlayerLayer` is owned by the PiP window. Developers need guidance on what appears in the original widget position (black frame, last frame, or transparent) and how to display a "Playing in PiP" placeholder using `isPictureInPictureActive`. Add a recommendation in the documentation/example app (Step 6) showing a `ValueListenableBuilder` that swaps the `VideoPlayer` widget for a placeholder when `isPictureInPictureActive` is true.

26. **`setPictureInPictureActions([])` behavior undefined** (API Design)
    Step 1.3 defines the method but doesn't specify behavior when passed an empty list. Should it clear all custom actions and revert to platform defaults, or remove all action buttons including defaults? On Android, passing an empty `RemoteAction` list to `PictureInPictureParams.Builder.setActions()` removes custom actions and shows default controls. On Web, clearing all Media Session handlers reverts to browser defaults. Define and document this behavior explicitly, and add test cases for the empty-list scenario.

27. **Start/stop PiP race condition on Android** (Performance)
    Calling `stopPictureInPicture()` immediately after `startPictureInPicture()` — before `onConfigurationChanged` fires — results in `isInPictureInPictureMode()` returning `false`, making the stop a no-op. PiP then activates with no way to cancel until the next call. Consider tracking a `pipRequested` boolean that is set in `startPictureInPicture()` and checked in `stopPictureInPicture()`, so that if the Activity hasn't entered PiP yet the flag can suppress the pending transition.

28. **iOS `restoreUserInterfaceForPictureInPictureStop` needs future iteration plan** (Technical Architecture)
    The v1 implementation immediately calls `completionHandler(YES)` without signaling Dart to restore UI (e.g., navigate back to the player screen). This causes a visual jump when returning from PiP if the user navigated away. Document this as a known v1 limitation and outline the future solution: a method channel call from native to Dart that returns a Future, with the completion handler called when the Future resolves. This requires a new Pigeon callback or a dedicated method channel.

### LOW Priority (continued)

29. **Missing test: AVPlayerItem failure during active PiP** (QA Strategy)
    No test covers what happens when the media fails (e.g., network error, `AVPlayerItemStatusFailed`) while PiP is active. Does the `AVPictureInPictureController` automatically dismiss the PiP window? Does the delegate receive both `failedToStart` and `didStop` callbacks? Define the expected behavior and add a test case.

30. **Missing test: auto-enter + manual start/stop interaction** (QA Strategy)
    No test verifies: (a) if `setAutoPictureInPicture(true)` then `startPictureInPicture()` then `stopPictureInPicture()`, does auto-enter remain enabled for subsequent app-leave events? (b) On web, if auto-enter triggers PiP via `visibilitychange`, does manually calling `stopPictureInPicture()` disable the auto-enter behavior or just exit PiP? Clarify and test both scenarios.

31. **Web `dispose()` does not clean up PiP event listeners** (Performance)
    Step 5.2 adds `addEventListener` for `enterpictureinpicture` and `leavepictureinpicture` on the video element but the web `VideoPlayer.dispose()` method (which currently only removes `src` and `contextmenu` listener) does not remove these PiP event listeners. While the video element is removed from the DOM on dispose (which should GC the listeners), explicitly removing them follows the existing cleanup pattern and prevents potential issues if the element reference is retained elsewhere.

32. **Android `PictureInPictureParams` state fragmentation across methods** (Technical Architecture)
    Steps 3.4.3, 3.4.8, and `setPictureInPictureActions` each build `PictureInPictureParams` independently. Android's `setPictureInPictureParams()` **replaces** all params entirely — fields not set on the new `Builder` revert to defaults. For example, calling `setAutoPictureInPicture(true)` as shown in Step 3.4.8 (which only sets `autoEnterEnabled`) clears any previously set custom actions and aspect ratio. The implementation must maintain a single, accumulated `PictureInPictureParams.Builder` (or equivalent state object) that is updated incrementally and used for all param-setting calls (`enterPictureInPictureMode`, `setPictureInPictureParams`).

33. **Web `stopPictureInPicture()` exits PiP for wrong player in multi-player scenario** (Performance)
    Step 5.1 calls `web.document.exitPictureInPicture()` which is document-level — it exits PiP for whatever element is currently in PiP, regardless of which player called it. If player A is in PiP and player B calls `stopPictureInPicture()`, it incorrectly exits player A's PiP. The implementation should guard with `document.pictureInPictureElement === videoElement` before calling `exitPictureInPicture()`, and return a no-op otherwise.

### LOW Priority (continued from iteration 5)

34. **Android `isPictureInPictureSupported()` doesn't verify manifest attribute** (API Design)
    Step 3.4.2 checks `hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)` (hardware capability) but not `android:supportsPictureInPicture="true"` in the manifest. These are independent: `isPictureInPictureSupported()` can return `true` while `startPictureInPicture()` fails because the manifest attribute is missing. The check should also inspect `ActivityInfo.FLAG_SUPPORTS_PICTURE_IN_PICTURE` via `PackageManager.getActivityInfo()` for a more reliable support signal.

35. **Missing test: dynamic PiP action update during active PiP** (QA Strategy)
    No test in Steps 3.9 or 5.7 covers calling `setPictureInPictureActions()` while PiP is already active. On Android, this requires `setPictureInPictureParams()` with updated actions (and must include all other current params per item 32). On web, Media Session handlers update live. Verify actions update correctly in the PiP window without restarting PiP.

36. **Android PiP aspect ratio not updated on video resolution change** (Technical Architecture)
    Step 3.4.3 sets the PiP window aspect ratio from video dimensions only when entering PiP. If the video resolution changes during PiP (e.g., adaptive streaming/HLS quality switch), the PiP window aspect ratio becomes stale. On iOS/macOS, `AVPictureInPictureController` handles this automatically via the player layer. On web, the browser handles it. On Android, the implementation should listen for ExoPlayer `onVideoSizeChanged` during PiP and call `setPictureInPictureParams()` with the updated aspect ratio.

37. **Android BroadcastReceiver registration approach unspecified** (Security)
    Step 3.6 says "Create a `BroadcastReceiver`" but doesn't specify dynamic registration (`context.registerReceiver()`) vs manifest declaration. For PiP action receivers, dynamic registration is required: it scopes the receiver to the process lifetime, avoids exposing it to other apps, and supports proper lifecycle cleanup. Explicitly specify `activity.registerReceiver()` and pair with `unregisterReceiver()` in cleanup (complementing items 9 and 19).

### MEDIUM Priority (continued from iteration 6)

38. **AVFoundation `stopPictureInPicture` Pigeon call returns before PiP animation completes, causing dispose race** (Technical Architecture)
    The Pigeon `stopPictureInPicture()` is defined as synchronous `void` (Step 4.1), so the Dart-side `await stopPictureInPicture()` in Step 2.5 resolves as soon as native `[pipController stopPictureInPicture]` is invoked — NOT when the PiP window finishes animating closed (~0.3s). Dart then immediately calls `dispose(_playerId)`, which executes `[self.player replaceCurrentItemWithPlayerItem:nil]` and `[self.playerLayer removeFromSuperlayer]` (verified in source: `FVPVideoPlayer.disposeWithError:` and `FVPTextureBasedVideoPlayer.disposeWithError:`). This tears down the player layer while the PiP close animation is still in progress, causing a visible black flash in the PiP window. Recommendation: make the AVFoundation Pigeon `stopPictureInPicture` method `@async`, resolving only after the `pictureInPictureControllerDidStopPictureInPicture:` delegate fires. This ensures `await stopPictureInPicture()` actually waits for the animation before disposal proceeds. (Distinct from item 22, which covers try/catch for error resilience.)

39. **New player registered during active Android PiP starts with incorrect `isPictureInPictureActive = false`** (API Design)
    On Android, PiP is Activity-level, so all players are affected. Step 3.3 correctly requires `PictureInPictureStateEvent` emission to all active players when PiP transitions occur. However, if a new `VideoPlayerController` is created and initialized while the Activity is already in PiP mode, the new controller misses the original `pictureInPictureStarted` event. Its `isPictureInPictureActive` starts `false` even though PiP is active. Step 3.2's `registerPlayerInstance()` wires the Activity supplier but does not check or emit the current PiP state. Recommendation: in `registerPlayerInstance()`, after wiring the Activity supplier, check `activity.isInPictureInPictureMode()` and if true, immediately emit `PictureInPictureStateEvent(isInPictureInPicture: true)` to the new player.

### LOW Priority (continued from iteration 6)

40. **Multi-player PiP collision untested on iOS/macOS/web** (QA Strategy)
    Item 12 covers Android multi-player event routing, but no test covers starting PiP on player B while player A is already in PiP on iOS/macOS/web. On iOS/macOS, `AVPictureInPictureController` allows only one active PiP — starting a second automatically stops the first (firing `didStopPictureInPicture` on the first controller). On web, `requestPictureInPicture()` on a second element exits PiP on the first (firing `leavepictureinpicture`). Add tests verifying the resulting cross-controller event sequences and state transitions.

41. **Web PiP blocked by Permissions Policy when Flutter app is embedded in iframe** (Developer Experience)
    If a Flutter web app is embedded in a cross-origin `<iframe>`, `requestPictureInPicture()` is blocked unless the parent grants `<iframe allow="picture-in-picture">`. Step 6.3 says "No additional setup required" for web but should note this iframe caveat — `isPictureInPictureSupported()` may return `true` while `startPictureInPicture()` still fails due to the Permissions Policy.

42. **`isPictureInPictureActive` cross-platform behavioral asymmetry undocumented in field API docs** (API Design)
    On Android (Activity-level PiP), ALL controllers see `isPictureInPictureActive = true` simultaneously (Step 3.3). On iOS/macOS/web (video-level PiP), only the initiating controller sees `true`. This is documented in "Platform Behavior Differences" notes but NOT in the `isPictureInPictureActive` field's doc comment (Step 2.1). Developers reading API docs without reading platform notes could write Android-broken code. Add a `/// Note: On Android...` remark to the field doc comment.

43. **iOS/macOS `pipPossibleObservation` KVO cleanup absent from dispose path** (Performance)
    Step 4.2.2 declares `@property(nonatomic, strong) id pipPossibleObservation` for block-based KVO on `isPictureInPicturePossible`. Verified in source: `FVPVideoPlayer.disposeWithError:` uses `FVPRemoveKeyValueObservers` for existing observations but has no mechanism for block-based KVO tokens. If this KVO is implemented, the dispose path must set `self.pipPossibleObservation = nil` (releasing the token removes the observation). Without this, the observation outlives the observer, risking a crash. (Distinct from Item 18, which questions whether to implement KVO at all; this addresses cleanup IF implemented.)

44. **Android auto-enter PiP params not cleared on player dispose** (Technical Architecture)
    On Android, `setAutoPictureInPicture(true)` sets `autoEnterEnabled` on Activity-level `PictureInPictureParams` (Step 3.4.8). Step 2.5's dispose path only stops active PiP — it does not call `setAutoPictureInPicture(false)`. Since `PictureInPictureParams` are Activity-level, they persist after the player that enabled auto-enter is disposed. On subsequent app-leave events, the Activity may auto-enter PiP with no primary PiP player (or with a different player that never requested auto-enter). Distinct from item 9 (BroadcastReceiver cleanup), item 24 (state tracking), and item 32 (params fragmentation). Fix: in dispose, if auto-enter was enabled by this player, call `setAutoPictureInPicture(false)` before disposing.
