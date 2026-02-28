// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:pigeon/pigeon.dart';

@ConfigurePigeon(
  PigeonOptions(
    dartOut: 'lib/src/messages.g.dart',
    cppOptions: CppOptions(namespace: 'video_player_windows'),
    cppHeaderOut: 'windows/messages.g.h',
    cppSourceOut: 'windows/messages.g.cpp',
    copyrightHeader: 'pigeons/copyright.txt',
  ),
)
/// Options for creating a new video player.
class CreationOptions {
  CreationOptions({required this.uri, required this.httpHeaders});

  /// The URI of the video to play.
  String uri;

  /// HTTP headers to use when fetching the video.
  Map<String, String> httpHeaders;
}

/// The plugin-level API for the Windows video player.
@HostApi()
abstract class WindowsVideoPlayerApi {
  /// Initializes the plugin and disposes all existing players.
  void initialize();

  /// Creates a new player and returns its texture ID (used as player ID).
  @async
  int create(CreationOptions options);

  /// Disposes a player with the given ID.
  void dispose(int playerId);

  /// Sets whether audio should mix with other audio sources.
  void setMixWithOthers(bool mixWithOthers);

  /// Returns the asset file path for the given asset name.
  String getAssetUrl(String asset, String? packageName);
}

/// The per-player API for the Windows video player.
@HostApi()
abstract class VideoPlayerInstanceApi {
  /// Sets whether to automatically loop playback of the video.
  void setLooping(bool looping);

  /// Sets the volume, with 0.0 being muted and 1.0 being full volume.
  void setVolume(double volume);

  /// Sets the playback speed as a multiple of normal speed.
  void setPlaybackSpeed(double speed);

  /// Begins playback if the video is not currently playing.
  void play();

  /// Pauses playback if the video is currently playing.
  void pause();

  /// Seeks to the given playback position, in milliseconds.
  void seekTo(int position);

  /// Returns the current playback position, in milliseconds.
  int getCurrentPosition();

  /// Returns the current buffer position, in milliseconds.
  int getBufferedPosition();
}
