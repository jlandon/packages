// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media_engine_player.h"

#include <d3d11.h>
#include <dxgi.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <windows.h>

#include <iostream>

#include "http_byte_stream.h"

#pragma comment(lib, "D3D11")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "dxgi")

namespace video_player_windows {

// Helper macro for HRESULT checking.
#define CHECK_HR(x) \
  if (FAILED(x)) {  \
    goto done;      \
  }

MediaEnginePlayer::MediaEnginePlayer(IDXGIAdapter* adapter)
    : adapter_(adapter) {
  if (adapter_) {
    adapter_->AddRef();
  }
  playing_event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  memset(&surface_descriptor_, 0, sizeof(surface_descriptor_));
}

MediaEnginePlayer::~MediaEnginePlayer() {
  Shutdown();
  if (playing_event_) {
    CloseHandle(playing_event_);
    playing_event_ = nullptr;
  }
  if (adapter_) {
    adapter_->Release();
    adapter_ = nullptr;
  }
}

STDMETHODIMP MediaEnginePlayer::QueryInterface(REFIID riid, void** ppv) {
  if (__uuidof(IMFMediaEngineNotify) == riid) {
    *ppv = static_cast<IMFMediaEngineNotify*>(this);
  } else if (__uuidof(IUnknown) == riid) {
    *ppv = static_cast<IUnknown*>(this);
  } else {
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

STDMETHODIMP_(ULONG) MediaEnginePlayer::AddRef() {
  return InterlockedIncrement(&ref_count_);
}

STDMETHODIMP_(ULONG) MediaEnginePlayer::Release() {
  ULONG count = InterlockedDecrement(&ref_count_);
  if (count == 0) {
    delete this;
  }
  return count;
}

HRESULT MediaEnginePlayer::InitD3D11() {
  HRESULT hr;
  ID3D10Multithread* multithread = nullptr;

  const UINT creation_flags =
      D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT |
      D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;

  hr = D3D11CreateDevice(adapter_, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                         creation_flags, nullptr, 0, D3D11_SDK_VERSION,
                         &d3d11_device_, nullptr, nullptr);
  CHECK_HR(hr);

  // Enable multithreading for the D3D11 device.
  hr = d3d11_device_->QueryInterface(IID_PPV_ARGS(&multithread));
  CHECK_HR(hr);
  multithread->SetMultithreadProtected(TRUE);
  multithread->Release();

  UINT reset_token;
  hr = MFCreateDXGIDeviceManager(&reset_token, &dxgi_manager_);
  CHECK_HR(hr);
  hr = dxgi_manager_->ResetDevice(d3d11_device_, reset_token);
  CHECK_HR(hr);

  // Ensure DXGI does not queue more than one frame at a time.
  IDXGIDevice2* dxgi_device = nullptr;
  hr = d3d11_device_->QueryInterface(IID_PPV_ARGS(&dxgi_device));
  CHECK_HR(hr);
  hr = dxgi_device->SetMaximumFrameLatency(1);
  dxgi_device->Release();

done:
  return hr;
}

HRESULT MediaEnginePlayer::InitTexture() {
  HRESULT hr;
  D3D11_TEXTURE2D_DESC texture_desc = {};

  texture_desc.Width = video_width_;
  texture_desc.Height = video_height_;
  texture_desc.MipLevels = 1;
  texture_desc.ArraySize = 1;
  texture_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.SampleDesc.Quality = 0;
  texture_desc.CPUAccessFlags = 0;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.BindFlags =
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  texture_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

  hr = d3d11_device_->CreateTexture2D(&texture_desc, nullptr, &texture_);
  CHECK_HR(hr);

done:
  return hr;
}

HRESULT MediaEnginePlayer::EventNotify(DWORD event, DWORD_PTR param1,
                                       DWORD param2) {
  switch (event) {
    case MF_MEDIA_ENGINE_EVENT_TIMEUPDATE:
      return S_OK;

    case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
      has_video_ = engine_->HasVideo();
      if (has_video_) {
        engine_->GetNativeVideoSize(&video_width_, &video_height_);
        frame_rect_dst_.right = video_width_;
        frame_rect_dst_.bottom = video_height_;
        InitTexture();
      }
      break;

    case MF_MEDIA_ENGINE_EVENT_CANPLAY:
      if (load_callback_) {
        // Build and send the initialized event.
        flutter::EncodableMap event_data;
        event_data[flutter::EncodableValue("event")] =
            flutter::EncodableValue("initialized");
        event_data[flutter::EncodableValue("duration")] =
            flutter::EncodableValue(GetDuration());
        event_data[flutter::EncodableValue("width")] =
            flutter::EncodableValue(static_cast<int>(video_width_));
        event_data[flutter::EncodableValue("height")] =
            flutter::EncodableValue(static_cast<int>(video_height_));
        SendEvent(MediaEnginePlayerEvent::kInitialized, event_data);

        load_callback_(true);
        load_callback_ = nullptr;
      }
      break;

    case MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY:
      UpdateFrame();
      StartVideoThread();
      break;

    case MF_MEDIA_ENGINE_EVENT_PLAY:
    case MF_MEDIA_ENGINE_EVENT_PLAYING:
      is_playing_ = true;
      is_ended_ = false;
      SetEvent(playing_event_);
      {
        flutter::EncodableMap event_data;
        event_data[flutter::EncodableValue("event")] =
            flutter::EncodableValue("isPlayingStateUpdate");
        event_data[flutter::EncodableValue("isPlaying")] =
            flutter::EncodableValue(true);
        SendEvent(MediaEnginePlayerEvent::kIsPlayingStateUpdate, event_data);
      }
      KeepScreenOn(true);
      break;

    case MF_MEDIA_ENGINE_EVENT_PAUSE:
      is_playing_ = false;
      ResetEvent(playing_event_);
      {
        flutter::EncodableMap event_data;
        event_data[flutter::EncodableValue("event")] =
            flutter::EncodableValue("isPlayingStateUpdate");
        event_data[flutter::EncodableValue("isPlaying")] =
            flutter::EncodableValue(false);
        SendEvent(MediaEnginePlayerEvent::kIsPlayingStateUpdate, event_data);
      }
      KeepScreenOn(false);
      break;

    case MF_MEDIA_ENGINE_EVENT_ENDED:
      is_ended_ = true;
      is_playing_ = false;
      ResetEvent(playing_event_);
      KeepScreenOn(false);
      if (is_looping_) {
        Seek(0);
        Play();
      } else {
        flutter::EncodableMap event_data;
        event_data[flutter::EncodableValue("event")] =
            flutter::EncodableValue("completed");
        SendEvent(MediaEnginePlayerEvent::kCompleted, event_data);
      }
      break;

    case MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED: {
      flutter::EncodableMap event_data;
      event_data[flutter::EncodableValue("event")] =
          flutter::EncodableValue("bufferingStart");
      SendEvent(MediaEnginePlayerEvent::kBufferingStart, event_data);
    } break;

    case MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED: {
      flutter::EncodableMap event_data;
      event_data[flutter::EncodableValue("event")] =
          flutter::EncodableValue("bufferingEnd");
      SendEvent(MediaEnginePlayerEvent::kBufferingEnd, event_data);
    } break;

    case MF_MEDIA_ENGINE_EVENT_ERROR:
    case MF_MEDIA_ENGINE_EVENT_ABORT:
      if (load_callback_) {
        load_callback_(false);
        load_callback_ = nullptr;
      }
      KeepScreenOn(false);
      break;

    default:
      break;
  }

  return S_OK;
}

HRESULT MediaEnginePlayer::OpenURL(
    const wchar_t* url, HWND hwnd,
    const std::vector<std::wstring>& http_headers,
    std::function<void(bool)> load_callback) {
  HRESULT hr;
  IMFMediaEngineClassFactory* factory = nullptr;
  IMFAttributes* attributes = nullptr;

  if (is_shutdown_ || engine_) {
    return E_ABORT;
  }

  load_callback_ = std::move(load_callback);

  hr = InitD3D11();
  CHECK_HR(hr);

  hr = CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr,
                        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
  CHECK_HR(hr);

  hr = MFCreateAttributes(&attributes, 3);
  CHECK_HR(hr);

  hr = attributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER,
                              static_cast<IUnknown*>(dxgi_manager_));
  CHECK_HR(hr);

  hr = attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK,
                              static_cast<IMFMediaEngineNotify*>(this));
  CHECK_HR(hr);

  hr = attributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT,
                             DXGI_FORMAT_B8G8R8A8_UNORM);
  CHECK_HR(hr);

  hr = factory->CreateInstance(0, attributes, &engine_);
  CHECK_HR(hr);

  hr = engine_->QueryInterface(IID_PPV_ARGS(&engine_ex_));
  CHECK_HR(hr);

  // Use custom byte stream for HTTP(S) URLs with headers.
  if (!http_headers.empty() && wcsncmp(url, L"http", 4) == 0) {
    auto byte_stream = new HttpByteStream(std::wstring(url), http_headers);
    hr =
        engine_ex_->SetSourceFromByteStream(byte_stream, const_cast<BSTR>(url));
    byte_stream->Release();
  } else {
    hr = engine_->SetSource(const_cast<BSTR>(url));
  }
  CHECK_HR(hr);

done:
  if (attributes) {
    attributes->Release();
  }
  if (factory) {
    factory->Release();
  }

  if (FAILED(hr) && load_callback_) {
    load_callback_(false);
    load_callback_ = nullptr;
  }

  return hr;
}

HRESULT MediaEnginePlayer::Play() {
  if (!engine_) return E_FAIL;
  return engine_->Play();
}

HRESULT MediaEnginePlayer::Pause() {
  if (!engine_) return E_FAIL;
  return engine_->Pause();
}

HRESULT MediaEnginePlayer::Seek(int64_t milliseconds) {
  if (!engine_) return E_FAIL;
  if (is_ended_) {
    Play();
  }
  return engine_ex_->SetCurrentTimeEx(
      static_cast<double>(milliseconds) / 1000.0,
      MF_MEDIA_ENGINE_SEEK_MODE_APPROXIMATE);
}

int64_t MediaEnginePlayer::GetDuration() const {
  if (!engine_) return -1;
  return static_cast<int64_t>(engine_->GetDuration() * 1000);
}

int64_t MediaEnginePlayer::GetCurrentPosition() const {
  if (!engine_) return -1;
  return static_cast<int64_t>(engine_->GetCurrentTime() * 1000);
}

void MediaEnginePlayer::GetVideoSize(DWORD* width, DWORD* height) const {
  *width = video_width_;
  *height = video_height_;
}

HRESULT MediaEnginePlayer::SetPlaybackSpeed(float speed) {
  if (!engine_) return E_FAIL;
  return engine_->SetPlaybackRate(static_cast<double>(speed));
}

HRESULT MediaEnginePlayer::SetVolume(float volume) {
  if (!engine_) return E_FAIL;
  return engine_->SetVolume(static_cast<double>(volume));
}

void MediaEnginePlayer::SetLooping(bool looping) { is_looping_ = looping; }

void MediaEnginePlayer::SetTextureRegistrar(
    flutter::TextureRegistrar* texture_registrar, int64_t texture_id) {
  texture_registrar_ = texture_registrar;
  texture_id_ = texture_id;
}

const FlutterDesktopGpuSurfaceDescriptor*
MediaEnginePlayer::GetSurfaceDescriptor() {
  return surface_descriptor_initialized_ ? &surface_descriptor_ : nullptr;
}

void MediaEnginePlayer::SetEventCallback(PlayerEventCallback callback) {
  event_callback_ = std::move(callback);
}

void MediaEnginePlayer::SendEvent(MediaEnginePlayerEvent type) {
  flutter::EncodableMap empty;
  SendEvent(type, empty);
}

void MediaEnginePlayer::SendEvent(MediaEnginePlayerEvent type,
                                  const flutter::EncodableMap& extra_data) {
  if (event_callback_) {
    event_callback_(type, extra_data);
  }
}

static DWORD WINAPI VideoThreadFunction(LPVOID param) {
  auto* player = static_cast<MediaEnginePlayer*>(param);
  player->VideoThreadFunc();
  return 0;
}

HRESULT MediaEnginePlayer::StartVideoThread() {
  if (!has_video_) return S_OK;
  if (thread_handle_ != nullptr) return E_FAIL;

  // AddRef to keep the player alive during the thread.
  AddRef();
  thread_handle_ =
      CreateThread(nullptr, 0, VideoThreadFunction, this, 0, nullptr);
  if (thread_handle_ == nullptr) {
    Release();
    return E_FAIL;
  }
  SetThreadPriority(thread_handle_, THREAD_PRIORITY_HIGHEST);

  return S_OK;
}

void MediaEnginePlayer::VideoThreadFunc() {
  IDXGIOutput* dxgi_output = nullptr;
  if (adapter_) {
    adapter_->EnumOutputs(0, &dxgi_output);
  }

  while (!is_shutdown_) {
    // Wait for VSync for smooth frame delivery.
    if (dxgi_output) {
      dxgi_output->WaitForVBlank();
    } else {
      Sleep(16);  // ~60fps fallback
    }

    if (is_shutdown_) break;
    UpdateFrame();

    if (is_shutdown_) break;

    // Pause the thread when video is paused.
    if (!is_playing_) {
      WaitForSingleObject(playing_event_, INFINITE);
    }
  }

  if (dxgi_output) {
    dxgi_output->Release();
  }

  CloseHandle(thread_handle_);
  thread_handle_ = nullptr;
  Release();
}

bool MediaEnginePlayer::UpdateFrame() {
  if (!engine_ || !texture_) return false;

  LONGLONG pts = -1;
  HRESULT hr = engine_->OnVideoStreamTick(&pts);
  if (hr != S_OK) return false;

  hr = engine_->TransferVideoFrame(texture_, &frame_rect_src_, &frame_rect_dst_,
                                   nullptr);
  if (FAILED(hr)) return false;

  // Initialize the surface descriptor on first frame.
  if (!surface_descriptor_initialized_) {
    IDXGIResource1* resource = nullptr;
    hr = texture_->QueryInterface(IID_PPV_ARGS(&resource));
    if (SUCCEEDED(hr)) {
      hr = resource->GetSharedHandle(&shared_texture_handle_);
      resource->Release();
      if (SUCCEEDED(hr)) {
        surface_descriptor_.struct_size =
            sizeof(FlutterDesktopGpuSurfaceDescriptor);
        surface_descriptor_.width = video_width_;
        surface_descriptor_.height = video_height_;
        surface_descriptor_.format = kFlutterDesktopPixelFormatBGRA8888;
        surface_descriptor_.handle = shared_texture_handle_;
        surface_descriptor_initialized_ = true;
      }
    }
  }

  // Mark the texture as having a new frame.
  if (texture_registrar_ && texture_id_ >= 0) {
    texture_registrar_->MarkTextureFrameAvailable(texture_id_);
  }

  return true;
}

void MediaEnginePlayer::Shutdown() {
  std::unique_lock<std::mutex> guard(mutex_);
  if (is_shutdown_) return;
  is_shutdown_ = true;

  KeepScreenOn(false);

  if (engine_) {
    // Wake up the video thread so it can exit.
    SetEvent(playing_event_);
    engine_->Shutdown();
  }

  if (engine_ex_) {
    engine_ex_->Release();
    engine_ex_ = nullptr;
  }
  if (engine_) {
    engine_->Release();
    engine_ = nullptr;
  }
  if (texture_) {
    texture_->Release();
    texture_ = nullptr;
  }
  if (dxgi_manager_) {
    dxgi_manager_->Release();
    dxgi_manager_ = nullptr;
  }
  if (d3d11_device_) {
    d3d11_device_->Release();
    d3d11_device_ = nullptr;
  }
}

void MediaEnginePlayer::KeepScreenOn(bool keep_on) {
  if (video_width_ <= 0) return;  // Audio-only media.

  if (keep_on == keeping_screen_on_) return;
  keeping_screen_on_ = keep_on;

  if (keep_on) {
    if (power_request_ == INVALID_HANDLE_VALUE) {
      REASON_CONTEXT ctx;
      ctx.Version = POWER_REQUEST_CONTEXT_VERSION;
      ctx.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
      ctx.Reason.SimpleReasonString = L"video_player_windows";
      power_request_ = PowerCreateRequest(&ctx);
    }
    PowerSetRequest(power_request_, PowerRequestSystemRequired);
    PowerSetRequest(power_request_, PowerRequestDisplayRequired);
  } else {
    if (power_request_ != INVALID_HANDLE_VALUE) {
      PowerClearRequest(power_request_, PowerRequestSystemRequired);
      PowerClearRequest(power_request_, PowerRequestDisplayRequired);
    }
  }
}

}  // namespace video_player_windows
