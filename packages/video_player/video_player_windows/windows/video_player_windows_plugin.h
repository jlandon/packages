// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef FLUTTER_PLUGIN_VIDEO_PLAYER_WINDOWS_PLUGIN_H_
#define FLUTTER_PLUGIN_VIDEO_PLAYER_WINDOWS_PLUGIN_H_

#include <flutter/event_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "media_engine_player.h"
#include "messages.g.h"

namespace video_player_windows {

// Represents a single video player instance along with its event channel.
struct PlayerEntry {
  std::unique_ptr<MediaEnginePlayer> player;
  std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>>
      event_channel;
  std::unique_ptr<flutter::StreamHandler<flutter::EncodableValue>>
      stream_handler;
  flutter::EventSink<flutter::EncodableValue>* event_sink = nullptr;
  int64_t texture_id = -1;
};

// A stream handler that forwards events to the plugin's event sink.
class VideoPlayerStreamHandler
    : public flutter::StreamHandler<flutter::EncodableValue> {
 public:
  VideoPlayerStreamHandler(
      std::function<void(flutter::EventSink<flutter::EncodableValue>*)>
          on_listen,
      std::function<void()> on_cancel);

 protected:
  std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
  OnListenInternal(
      const flutter::EncodableValue* arguments,
      std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&& events)
      override;

  std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
  OnCancelInternal(const flutter::EncodableValue* arguments) override;

 private:
  std::function<void(flutter::EventSink<flutter::EncodableValue>*)> on_listen_;
  std::function<void()> on_cancel_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> events_;
};

// The Windows implementation of the video_player plugin.
class VideoPlayerWindowsPlugin : public flutter::Plugin,
                                 public WindowsVideoPlayerApi {
 public:
  static void RegisterWithRegistrar(
      flutter::PluginRegistrarWindows* registrar);

  explicit VideoPlayerWindowsPlugin(
      flutter::PluginRegistrarWindows* registrar);

  ~VideoPlayerWindowsPlugin() override;

  // Disallow copy and assign.
  VideoPlayerWindowsPlugin(const VideoPlayerWindowsPlugin&) = delete;
  VideoPlayerWindowsPlugin& operator=(const VideoPlayerWindowsPlugin&) = delete;

  // WindowsVideoPlayerApi implementation.
  std::optional<FlutterError> Initialize() override;
  void Create(
      const CreationOptions& options,
      std::function<void(ErrorOr<int64_t> reply)> result) override;
  std::optional<FlutterError> Dispose(int64_t player_id) override;
  std::optional<FlutterError> SetMixWithOthers(bool mix_with_others) override;
  ErrorOr<std::string> GetAssetUrl(const std::string& asset,
                                   const std::string* package_name) override;

 private:
  // Disposes all players and their associated resources.
  void DisposeAllPlayers();

  // Sets up the event channel for a player.
  void SetupEventChannel(int64_t player_id);

  // Sends a video event to the Dart side via the event channel.
  void SendVideoEvent(int64_t player_id,
                      const flutter::EncodableMap& event);

  flutter::PluginRegistrarWindows* registrar_;
  flutter::TextureRegistrar* texture_registrar_;
  flutter::BinaryMessenger* messenger_;

  std::map<int64_t, std::unique_ptr<PlayerEntry>> players_;
  std::mutex players_mutex_;

  bool mf_started_ = false;
};

}  // namespace video_player_windows

#endif  // FLUTTER_PLUGIN_VIDEO_PLAYER_WINDOWS_PLUGIN_H_
