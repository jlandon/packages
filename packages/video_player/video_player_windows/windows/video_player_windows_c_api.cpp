// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "include/video_player_windows/video_player_windows_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "video_player_windows_plugin.h"

void VideoPlayerWindowsPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  video_player_windows::VideoPlayerWindowsPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
