# Copyright 2013 The Flutter Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
cmake_minimum_required(VERSION 3.14)
set(PROJECT_NAME "video_player_windows")
project(${PROJECT_NAME} LANGUAGES CXX)

cmake_policy(VERSION 3.14...3.24)

# This value is used when generating builds using this plugin, so it must
# not be changed.
set(PLUGIN_NAME "${PROJECT_NAME}_plugin")

list(APPEND PLUGIN_SOURCES
  "video_player_windows_plugin.h"
  "video_player_windows_plugin.cpp"
  "media_engine_player.h"
  "media_engine_player.cpp"
  "http_byte_stream.h"
  "http_byte_stream.cpp"
  "messages.g.h"
  "messages.g.cpp"
)

add_library(${PLUGIN_NAME} SHARED
  "video_player_windows_c_api.cpp"
  "include/video_player_windows/video_player_windows_c_api.h"
  ${PLUGIN_SOURCES}
)

apply_standard_settings(${PLUGIN_NAME})
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin)
target_link_libraries(${PLUGIN_NAME} PRIVATE mfplat mfuuid d3d11 dxgi winhttp)

# List of absolute paths to libraries that should be bundled with the plugin.
set(video_player_windows_bundled_libraries
  ""
  PARENT_SCOPE
)
