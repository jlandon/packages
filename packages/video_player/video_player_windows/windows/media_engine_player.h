// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef FLUTTER_PLUGIN_MEDIA_ENGINE_PLAYER_H_
#define FLUTTER_PLUGIN_MEDIA_ENGINE_PLAYER_H_

#include <d3d11.h>
#include <dxgi1_2.h>
#include <flutter/encodable_value.h>
#include <flutter/texture_registrar.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <windows.h>

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace video_player_windows {

// Types of events from the media engine player.
enum class MediaEnginePlayerEvent {
  kInitialized,
  kCompleted,
  kBufferingStart,
  kBufferingEnd,
  kIsPlayingStateUpdate,
  kError,
};

// Callback type for player events.
using PlayerEventCallback =
    std::function<void(MediaEnginePlayerEvent event_type,
                       const flutter::EncodableMap& event_data)>;

// A video player implementation using Windows Media Foundation's
// IMFMediaEngine API with DirectX 11 GPU-accelerated texture rendering.
class MediaEnginePlayer : public IMFMediaEngineNotify {
 public:
  explicit MediaEnginePlayer(IDXGIAdapter* adapter);
  virtual ~MediaEnginePlayer();

  // IUnknown methods.
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IMFMediaEngineNotify.
  STDMETHODIMP EventNotify(DWORD event, DWORD_PTR param1,
                           DWORD param2) override;

  // Opens a video from the given URL. The callback is invoked on the
  // main thread when loading completes (or fails).
  HRESULT OpenURL(const wchar_t* url, HWND hwnd,
                  const std::vector<std::wstring>& http_headers,
                  std::function<void(bool)> load_callback);

  // Playback controls.
  HRESULT Play();
  HRESULT Pause();
  HRESULT Seek(int64_t milliseconds);
  void Shutdown();

  // Property getters.
  int64_t GetDuration() const;
  int64_t GetCurrentPosition() const;
  void GetVideoSize(DWORD* width, DWORD* height) const;

  // Property setters.
  HRESULT SetPlaybackSpeed(float speed);
  HRESULT SetVolume(float volume);
  void SetLooping(bool looping);

  // Texture integration.
  void SetTextureRegistrar(flutter::TextureRegistrar* texture_registrar,
                           int64_t texture_id);
  const FlutterDesktopGpuSurfaceDescriptor* GetSurfaceDescriptor();

  // Event callback.
  void SetEventCallback(PlayerEventCallback callback);

 private:
  // Initializes Direct3D 11 device and DXGI device manager.
  HRESULT InitD3D11();

  // Initializes the shared texture for Flutter rendering.
  HRESULT InitTexture();

  // Starts the video rendering thread.
  HRESULT StartVideoThread();

  // Video rendering thread function.
  void VideoThreadFunc();

  // Updates the current video frame to the texture.
  bool UpdateFrame();

  // Sends a video event to the Dart side.
  void SendEvent(MediaEnginePlayerEvent type);
  void SendEvent(MediaEnginePlayerEvent type,
                 const flutter::EncodableMap& extra_data);

  // Reference counting.
  long ref_count_ = 1;

  // DirectX objects.
  IDXGIAdapter* adapter_ = nullptr;
  ID3D11Device* d3d11_device_ = nullptr;
  IMFDXGIDeviceManager* dxgi_manager_ = nullptr;
  ID3D11Texture2D* texture_ = nullptr;
  HANDLE shared_texture_handle_ = nullptr;

  // Media Foundation objects.
  IMFMediaEngine* engine_ = nullptr;
  IMFMediaEngineEx* engine_ex_ = nullptr;

  // Flutter texture integration.
  flutter::TextureRegistrar* texture_registrar_ = nullptr;
  int64_t texture_id_ = -1;
  FlutterDesktopGpuSurfaceDescriptor surface_descriptor_ = {};
  bool surface_descriptor_initialized_ = false;

  // Video dimensions.
  DWORD video_width_ = 0;
  DWORD video_height_ = 0;
  bool has_video_ = false;

  // Playback state.
  bool is_playing_ = false;
  bool is_ended_ = false;
  bool is_looping_ = false;
  bool is_shutdown_ = false;
  std::mutex mutex_;

  // Video rendering thread.
  HANDLE thread_handle_ = nullptr;
  HANDLE playing_event_ = nullptr;

  // Video frame rendering rectangles.
  MFVideoNormalizedRect frame_rect_src_ = {};
  RECT frame_rect_dst_ = {};

  // Callbacks.
  std::function<void(bool)> load_callback_;
  PlayerEventCallback event_callback_;

  // Screen-on keeper.
  HANDLE power_request_ = INVALID_HANDLE_VALUE;
  bool keeping_screen_on_ = false;
  void KeepScreenOn(bool keep_on);
};

}  // namespace video_player_windows

#endif  // FLUTTER_PLUGIN_MEDIA_ENGINE_PLAYER_H_
