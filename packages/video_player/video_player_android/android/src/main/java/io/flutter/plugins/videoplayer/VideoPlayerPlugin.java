// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugins.videoplayer;

import android.app.Activity;
import android.app.PictureInPictureParams;
import android.app.RemoteAction;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Configuration;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.util.LongSparseArray;
import android.util.Rational;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.lifecycle.Lifecycle;
import androidx.media3.common.util.UnstableApi;
import io.flutter.FlutterInjector;
import io.flutter.Log;
import io.flutter.embedding.engine.plugins.FlutterPlugin;
import io.flutter.embedding.engine.plugins.activity.ActivityAware;
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding;
import io.flutter.embedding.engine.plugins.lifecycle.FlutterLifecycleAdapter;
import io.flutter.plugin.common.BinaryMessenger;
import io.flutter.plugins.videoplayer.platformview.PlatformVideoViewFactory;
import io.flutter.plugins.videoplayer.platformview.PlatformViewVideoPlayer;
import io.flutter.plugins.videoplayer.texture.TextureVideoPlayer;
import io.flutter.view.TextureRegistry;
import java.util.ArrayList;
import java.util.List;

/** Android platform implementation of the VideoPlayerPlugin. */
public class VideoPlayerPlugin implements FlutterPlugin, ActivityAware, AndroidVideoPlayerApi {
  private static final String TAG = "VideoPlayerPlugin";
  private final LongSparseArray<VideoPlayer> videoPlayers = new LongSparseArray<>();
  private FlutterState flutterState;
  private final VideoPlayerOptions sharedOptions = new VideoPlayerOptions();
  private long nextPlayerIdentifier = 1;
  @Nullable private Activity activity;
  private boolean isInPictureInPictureMode = false;
  private final LongSparseArray<Boolean> autoPipPlayers = new LongSparseArray<>();
  @Nullable private BroadcastReceiver pipActionReceiver;

  /** Register this with the v2 embedding for the plugin to respond to lifecycle callbacks. */
  public VideoPlayerPlugin() {}

  @Override
  public void onAttachedToEngine(@NonNull FlutterPluginBinding binding) {
    final FlutterInjector injector = FlutterInjector.instance();
    this.flutterState =
        new FlutterState(
            binding.getApplicationContext(),
            binding.getBinaryMessenger(),
            injector.flutterLoader()::getLookupKeyForAsset,
            injector.flutterLoader()::getLookupKeyForAsset,
            binding.getTextureRegistry());
    flutterState.startListening(this, binding.getBinaryMessenger());

    binding
        .getPlatformViewRegistry()
        .registerViewFactory(
            "plugins.flutter.dev/video_player_android",
            new PlatformVideoViewFactory(videoPlayers::get));
  }

  @Override
  public void onDetachedFromEngine(@NonNull FlutterPluginBinding binding) {
    if (flutterState == null) {
      Log.wtf(TAG, "Detached from the engine before registering to it.");
    }
    flutterState.stopListening(binding.getBinaryMessenger());
    flutterState = null;
    onDestroy();
  }

  private void disposeAllPlayers() {
    for (int i = 0; i < videoPlayers.size(); i++) {
      videoPlayers.valueAt(i).dispose();
    }
    videoPlayers.clear();
  }

  public void onDestroy() {
    // The whole FlutterView is being destroyed. Here we release resources acquired for all
    // instances
    // of VideoPlayer. Once https://github.com/flutter/flutter/issues/19358 is resolved this may
    // be replaced with just asserting that videoPlayers.isEmpty().
    // https://github.com/flutter/flutter/issues/20989 tracks this.
    disposeAllPlayers();
  }

  @Override
  public void initialize() {
    disposeAllPlayers();
  }

  @OptIn(markerClass = UnstableApi.class)
  @Override
  public long createForPlatformView(@NonNull CreationOptions options) {
    final VideoAsset videoAsset = videoAssetWithOptions(options);

    long id = nextPlayerIdentifier++;
    final String streamInstance = Long.toString(id);
    VideoPlayer videoPlayer =
        PlatformViewVideoPlayer.create(
            flutterState.applicationContext,
            VideoPlayerEventCallbacks.bindTo(flutterState.binaryMessenger, streamInstance),
            videoAsset,
            sharedOptions);

    registerPlayerInstance(videoPlayer, id);
    return id;
  }

  @OptIn(markerClass = UnstableApi.class)
  @Override
  public @NonNull TexturePlayerIds createForTextureView(@NonNull CreationOptions options) {
    final VideoAsset videoAsset = videoAssetWithOptions(options);

    long id = nextPlayerIdentifier++;
    final String streamInstance = Long.toString(id);
    TextureRegistry.SurfaceProducer handle = flutterState.textureRegistry.createSurfaceProducer();
    VideoPlayer videoPlayer =
        TextureVideoPlayer.create(
            flutterState.applicationContext,
            VideoPlayerEventCallbacks.bindTo(flutterState.binaryMessenger, streamInstance),
            handle,
            videoAsset,
            sharedOptions);

    registerPlayerInstance(videoPlayer, id);
    return new TexturePlayerIds(id, handle.id());
  }

  private @NonNull VideoAsset videoAssetWithOptions(@NonNull CreationOptions options) {
    final @NonNull String uri = options.getUri();
    if (uri.startsWith("asset:")) {
      return VideoAsset.fromAssetUrl(uri);
    } else if (uri.startsWith("rtsp:")) {
      return VideoAsset.fromRtspUrl(uri);
    } else {
      VideoAsset.StreamingFormat streamingFormat = VideoAsset.StreamingFormat.UNKNOWN;
      PlatformVideoFormat formatHint = options.getFormatHint();
      if (formatHint != null) {
        switch (formatHint) {
          case SS:
            streamingFormat = VideoAsset.StreamingFormat.SMOOTH;
            break;
          case DASH:
            streamingFormat = VideoAsset.StreamingFormat.DYNAMIC_ADAPTIVE;
            break;
          case HLS:
            streamingFormat = VideoAsset.StreamingFormat.HTTP_LIVE;
            break;
        }
      }
      return VideoAsset.fromRemoteUrl(
          uri, streamingFormat, options.getHttpHeaders(), options.getUserAgent());
    }
  }

  private void registerPlayerInstance(VideoPlayer player, long id) {
    // Set up the instance-specific API handler, and make sure it is removed when the player is
    // disposed.
    BinaryMessenger messenger = flutterState.binaryMessenger;
    final String channelSuffix = Long.toString(id);
    VideoPlayerInstanceApi.Companion.setUp(messenger, player, channelSuffix);
    player.setDisposeHandler(
        () -> VideoPlayerInstanceApi.Companion.setUp(messenger, null, channelSuffix));

    videoPlayers.put(id, player);
  }

  @NonNull
  private VideoPlayer getPlayer(long playerId) {
    VideoPlayer player = videoPlayers.get(playerId);

    // Avoid a very ugly un-debuggable NPE that results in returning a null player.
    if (player == null) {
      String message = "No player found with playerId <" + playerId + ">";
      if (videoPlayers.size() == 0) {
        message += " and no active players created by the plugin.";
      }
      throw new IllegalStateException(message);
    }

    return player;
  }

  @Override
  public void dispose(long playerId) {
    VideoPlayer player = getPlayer(playerId);
    player.dispose();
    videoPlayers.remove(playerId);
  }

  @Override
  public void setMixWithOthers(boolean mixWithOthers) {
    sharedOptions.mixWithOthers = mixWithOthers;
  }

  @Override
  public @NonNull String getLookupKeyForAsset(@NonNull String asset, @Nullable String packageName) {
    return packageName == null
        ? flutterState.keyForAsset.get(asset)
        : flutterState.keyForAssetAndPackageName.get(asset, packageName);
  }

  @Override
  public void onAttachedToActivity(@NonNull ActivityPluginBinding binding) {
    activity = binding.getActivity();
  }

  @Override
  public void onDetachedFromActivityForConfigChanges() {
    activity = null;
  }

  @Override
  public void onReattachedToActivityForConfigChanges(@NonNull ActivityPluginBinding binding) {
    activity = binding.getActivity();
  }

  @Override
  public void onDetachedFromActivity() {
    activity = null;
  }

  @Override
  public @NonNull Boolean isPictureInPictureSupported() {
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && activity != null;
  }

  @Override
  public void startPictureInPicture(long playerId, @NonNull List<PipAction> actions) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O || activity == null) {
      return;
    }
    PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();
    builder.setAspectRatio(new Rational(16, 9));
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
      builder.setAutoEnterEnabled(false);
    }
    List<RemoteAction> remoteActions = buildRemoteActions(actions);
    if (!remoteActions.isEmpty()) {
      builder.setActions(remoteActions);
    }
    activity.enterPictureInPictureMode(builder.build());
  }

  @Override
  public void stopPictureInPicture(long playerId) {
    if (activity != null && isInPictureInPictureMode) {
      activity.moveTaskToBack(false);
    }
  }

  @Override
  public void setAutoPictureInPicture(long playerId, boolean enabled) {
    autoPipPlayers.put(playerId, enabled);
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && activity != null) {
      PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();
      builder.setAutoEnterEnabled(enabled);
      builder.setAspectRatio(new Rational(16, 9));
      activity.setPictureInPictureParams(builder.build());
    }
  }

  @Override
  public void setPictureInPictureActions(long playerId, @NonNull List<PipAction> actions) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O || activity == null) {
      return;
    }
    List<RemoteAction> remoteActions = buildRemoteActions(actions);
    PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();
    builder.setActions(remoteActions);
    builder.setAspectRatio(new Rational(16, 9));
    activity.setPictureInPictureParams(builder.build());
  }

  @NonNull
  private List<RemoteAction> buildRemoteActions(@NonNull List<PipAction> actions) {
    List<RemoteAction> remoteActions = new ArrayList<>();
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O || activity == null) {
      return remoteActions;
    }
    int requestCode = 0;
    for (PipAction action : actions) {
      int iconRes = android.R.drawable.ic_media_play;
      String intentAction = "io.flutter.plugins.videoplayer.PIP_ACTION";
      switch (action.getType()) {
        case PLAY:
          iconRes = android.R.drawable.ic_media_play;
          intentAction += ".play";
          break;
        case PAUSE:
          iconRes = android.R.drawable.ic_media_pause;
          intentAction += ".pause";
          break;
        case SKIP_FORWARD:
          iconRes = android.R.drawable.ic_media_ff;
          intentAction += ".skipForward";
          break;
        case SKIP_BACKWARD:
          iconRes = android.R.drawable.ic_media_rew;
          intentAction += ".skipBackward";
          break;
        case NEXT_TRACK:
          iconRes = android.R.drawable.ic_media_next;
          intentAction += ".nextTrack";
          break;
        case PREVIOUS_TRACK:
          iconRes = android.R.drawable.ic_media_previous;
          intentAction += ".previousTrack";
          break;
      }
      Icon icon = Icon.createWithResource(activity, iconRes);
      Intent intent = new Intent(intentAction);
      PendingIntent pendingIntent =
          PendingIntent.getBroadcast(
              activity, requestCode++, intent, PendingIntent.FLAG_IMMUTABLE);
      remoteActions.add(
          new RemoteAction(icon, action.getLabel(), action.getLabel(), pendingIntent));
    }
    return remoteActions;
  }

  /**
   * Checks if the activity is currently in Picture-in-Picture mode.
   * Uses isInPictureInPictureMode from the Activity API.
   */
  private boolean checkIsInPictureInPictureMode() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && activity != null) {
      return activity.isInPictureInPictureMode();
    }
    return false;
  }

  private interface KeyForAssetFn {
    String get(String asset);
  }

  private interface KeyForAssetAndPackageName {
    String get(String asset, String packageName);
  }

  private static final class FlutterState {
    final Context applicationContext;
    final BinaryMessenger binaryMessenger;
    final KeyForAssetFn keyForAsset;
    final KeyForAssetAndPackageName keyForAssetAndPackageName;
    final TextureRegistry textureRegistry;

    FlutterState(
        Context applicationContext,
        BinaryMessenger messenger,
        KeyForAssetFn keyForAsset,
        KeyForAssetAndPackageName keyForAssetAndPackageName,
        TextureRegistry textureRegistry) {
      this.applicationContext = applicationContext;
      this.binaryMessenger = messenger;
      this.keyForAsset = keyForAsset;
      this.keyForAssetAndPackageName = keyForAssetAndPackageName;
      this.textureRegistry = textureRegistry;
    }

    void startListening(VideoPlayerPlugin methodCallHandler, BinaryMessenger messenger) {
      AndroidVideoPlayerApi.Companion.setUp(messenger, methodCallHandler);
    }

    void stopListening(BinaryMessenger messenger) {
      AndroidVideoPlayerApi.Companion.setUp(messenger, null);
    }
  }
}
