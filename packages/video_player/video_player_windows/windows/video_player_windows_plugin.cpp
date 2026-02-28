// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "video_player_windows_plugin.h"

#include <flutter/event_channel.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <mfapi.h>
#include <windows.h>

#include <memory>
#include <sstream>
#include <string>

#pragma comment(lib, "mfplat")

namespace video_player_windows {

// Per-player instance API handler that delegates to the plugin.
class PlayerInstanceApiHandler : public VideoPlayerInstanceApi {
 public:
  PlayerInstanceApiHandler(MediaEnginePlayer* player) : player_(player) {}

  std::optional<FlutterError> SetLooping(bool looping) override {
    player_->SetLooping(looping);
    return std::nullopt;
  }

  std::optional<FlutterError> SetVolume(double volume) override {
    player_->SetVolume(static_cast<float>(volume));
    return std::nullopt;
  }

  std::optional<FlutterError> SetPlaybackSpeed(double speed) override {
    player_->SetPlaybackSpeed(static_cast<float>(speed));
    return std::nullopt;
  }

  std::optional<FlutterError> Play() override {
    player_->Play();
    return std::nullopt;
  }

  std::optional<FlutterError> Pause() override {
    player_->Pause();
    return std::nullopt;
  }

  std::optional<FlutterError> SeekTo(int64_t position) override {
    player_->Seek(position);
    return std::nullopt;
  }

  ErrorOr<int64_t> GetCurrentPosition() override {
    return static_cast<int64_t>(player_->GetCurrentPosition());
  }

  ErrorOr<int64_t> GetBufferedPosition() override {
    return static_cast<int64_t>(player_->GetCurrentPosition());
  }

 private:
  MediaEnginePlayer* player_;
};

// VideoPlayerStreamHandler implementation.
VideoPlayerStreamHandler::VideoPlayerStreamHandler(
    std::function<void(flutter::EventSink<flutter::EncodableValue>*)> on_listen,
    std::function<void()> on_cancel)
    : on_listen_(std::move(on_listen)), on_cancel_(std::move(on_cancel)) {}

std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
VideoPlayerStreamHandler::OnListenInternal(
    const flutter::EncodableValue* arguments,
    std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&& events) {
  events_ = std::move(events);
  on_listen_(events_.get());
  return nullptr;
}

std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
VideoPlayerStreamHandler::OnCancelInternal(
    const flutter::EncodableValue* arguments) {
  on_cancel_();
  events_.reset();
  return nullptr;
}

// VideoPlayerWindowsPlugin implementation.
void VideoPlayerWindowsPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows* registrar) {
  auto plugin = std::make_unique<VideoPlayerWindowsPlugin>(registrar);

  WindowsVideoPlayerApi::SetUp(registrar->messenger(), plugin.get());

  registrar->AddPlugin(std::move(plugin));
}

VideoPlayerWindowsPlugin::VideoPlayerWindowsPlugin(
    flutter::PluginRegistrarWindows* registrar)
    : registrar_(registrar),
      texture_registrar_(registrar->texture_registrar()),
      messenger_(registrar->messenger()) {}

VideoPlayerWindowsPlugin::~VideoPlayerWindowsPlugin() {
  DisposeAllPlayers();
  if (mf_started_) {
    MFShutdown();
    mf_started_ = false;
  }
}

std::optional<FlutterError> VideoPlayerWindowsPlugin::Initialize() {
  DisposeAllPlayers();

  if (!mf_started_) {
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
      return FlutterError("mf_init_failed",
                          "Failed to initialize Media Foundation.");
    }
    mf_started_ = true;
  }

  return std::nullopt;
}

void VideoPlayerWindowsPlugin::Create(
    const CreationOptions& options,
    std::function<void(ErrorOr<int64_t> reply)> result) {
  auto player = std::make_unique<MediaEnginePlayer>(
      registrar_->GetView()->GetGraphicsAdapter());

  // Register a GPU texture with Flutter.
  auto texture_variant =
      std::make_unique<flutter::TextureVariant>(flutter::GpuSurfaceTexture(
          kFlutterDesktopGpuSurfaceTypeDxgiSharedHandle,
          [raw_player = player.get()](size_t width, size_t height)
              -> const FlutterDesktopGpuSurfaceDescriptor* {
            return raw_player->GetSurfaceDescriptor();
          }));

  int64_t texture_id =
      texture_registrar_->RegisterTexture(texture_variant.get());
  if (texture_id < 0) {
    result(FlutterError("texture_registration_failed",
                        "Failed to register texture."));
    return;
  }

  // Convert HTTP headers from EncodableMap to a vector of wide strings.
  std::vector<std::wstring> header_lines;
  for (const auto& entry : options.http_headers()) {
    const auto& key = std::get<std::string>(entry.first);
    const auto& value = std::get<std::string>(entry.second);
    std::wstring wide_key(key.begin(), key.end());
    std::wstring wide_value(value.begin(), value.end());
    header_lines.push_back(wide_key + L": " + wide_value);
  }

  // Convert URI to wide string.
  const std::string& uri = options.uri();
  int wide_len = MultiByteToWideChar(CP_UTF8, 0, uri.c_str(), -1, nullptr, 0);
  std::wstring wide_uri(wide_len, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, uri.c_str(), -1, &wide_uri[0], wide_len);

  // Set up the player entry.
  auto entry = std::make_unique<PlayerEntry>();
  entry->player = std::move(player);
  entry->texture_id = texture_id;

  MediaEnginePlayer* raw_player = entry->player.get();
  flutter::TextureRegistrar* tex_reg = texture_registrar_;

  // Set up callbacks for the player.
  raw_player->SetTextureRegistrar(texture_registrar_, texture_id);
  raw_player->SetEventCallback(
      [this, texture_id](MediaEnginePlayerEvent event_type,
                         const flutter::EncodableMap& event_data) {
        SendVideoEvent(texture_id, event_data);
      });

  {
    std::lock_guard<std::mutex> lock(players_mutex_);
    players_[texture_id] = std::move(entry);
  }

  // Set up the event channel and per-player instance API.
  SetupEventChannel(texture_id);

  auto instance_handler =
      std::make_unique<PlayerInstanceApiHandler>(raw_player);
  VideoPlayerInstanceApi::SetUp(messenger_, instance_handler.get(),
                                std::to_string(texture_id));

  // Open the video URL asynchronously.
  HWND hwnd = registrar_->GetView()->GetNativeWindow();
  HRESULT hr = raw_player->OpenURL(
      wide_uri.c_str(), hwnd, header_lines, [result, texture_id](bool success) {
        if (success) {
          result(texture_id);
        } else {
          result(FlutterError("video_open_failed", "Failed to open video."));
        }
      });

  if (FAILED(hr)) {
    result(
        FlutterError("video_open_failed", "Failed to initiate video loading."));
  }
}

std::optional<FlutterError> VideoPlayerWindowsPlugin::Dispose(
    int64_t player_id) {
  std::lock_guard<std::mutex> lock(players_mutex_);
  auto it = players_.find(player_id);
  if (it != players_.end()) {
    auto& entry = it->second;
    if (entry->player) {
      entry->player->Shutdown();
    }
    if (entry->texture_id >= 0) {
      texture_registrar_->UnregisterTexture(entry->texture_id);
    }
    // Clear the per-player instance API.
    VideoPlayerInstanceApi::SetUp(messenger_, nullptr,
                                  std::to_string(player_id));
    players_.erase(it);
  }
  return std::nullopt;
}

std::optional<FlutterError> VideoPlayerWindowsPlugin::SetMixWithOthers(
    bool mix_with_others) {
  // Not supported on Windows; no-op.
  return std::nullopt;
}

ErrorOr<std::string> VideoPlayerWindowsPlugin::GetAssetUrl(
    const std::string& asset, const std::string* package_name) {
  // Construct the path to the asset relative to the executable.
  // On Windows, assets are located in data/flutter_assets/ next to the
  // executable.
  wchar_t exe_path[MAX_PATH];
  GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
  std::wstring wide_exe_path(exe_path);
  size_t last_sep = wide_exe_path.find_last_of(L'\\');
  std::wstring dir = wide_exe_path.substr(0, last_sep);

  std::wstring wide_asset(asset.begin(), asset.end());
  std::wstring full_path = dir + L"\\data\\flutter_assets\\" + wide_asset;

  // Convert back to UTF-8.
  int utf8_len = WideCharToMultiByte(CP_UTF8, 0, full_path.c_str(), -1, nullptr,
                                     0, nullptr, nullptr);
  std::string result(utf8_len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, full_path.c_str(), -1, &result[0], utf8_len,
                      nullptr, nullptr);
  // Remove trailing null.
  if (!result.empty() && result.back() == '\0') {
    result.pop_back();
  }

  return result;
}

void VideoPlayerWindowsPlugin::DisposeAllPlayers() {
  std::lock_guard<std::mutex> lock(players_mutex_);
  for (auto& pair : players_) {
    auto& entry = pair.second;
    if (entry->player) {
      entry->player->Shutdown();
    }
    if (entry->texture_id >= 0) {
      texture_registrar_->UnregisterTexture(entry->texture_id);
    }
  }
  players_.clear();
}

void VideoPlayerWindowsPlugin::SetupEventChannel(int64_t player_id) {
  std::string channel_name =
      "flutter.dev/videoPlayer/videoEvents" + std::to_string(player_id);

  auto handler = std::make_unique<VideoPlayerStreamHandler>(
      [this, player_id](flutter::EventSink<flutter::EncodableValue>* sink) {
        std::lock_guard<std::mutex> lock(players_mutex_);
        auto it = players_.find(player_id);
        if (it != players_.end()) {
          it->second->event_sink = sink;
        }
      },
      [this, player_id]() {
        std::lock_guard<std::mutex> lock(players_mutex_);
        auto it = players_.find(player_id);
        if (it != players_.end()) {
          it->second->event_sink = nullptr;
        }
      });

  std::lock_guard<std::mutex> lock(players_mutex_);
  auto it = players_.find(player_id);
  if (it != players_.end()) {
    auto event_channel =
        std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
            messenger_, channel_name,
            &flutter::StandardMethodCodec::GetInstance());
    event_channel->SetStreamHandler(std::move(handler));
    it->second->event_channel = std::move(event_channel);
  }
}

void VideoPlayerWindowsPlugin::SendVideoEvent(
    int64_t player_id, const flutter::EncodableMap& event) {
  std::lock_guard<std::mutex> lock(players_mutex_);
  auto it = players_.find(player_id);
  if (it != players_.end() && it->second->event_sink != nullptr) {
    it->second->event_sink->Success(flutter::EncodableValue(event));
  }
}

}  // namespace video_player_windows
