#!/usr/bin/env bash
# Verification script for PiP feature implementation across the video_player federated plugin.
# This script runs analysis, formatting checks, and tests for all affected packages.
#
# Usage: bash plans/pip/agent-verify.sh [package_name]
#   If package_name is provided, only that package is verified.
#   If omitted, all video_player packages are verified.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TOOL="dart run $REPO_ROOT/script/tool/bin/flutter_plugin_tools.dart"

ALL_PACKAGES="video_player_platform_interface,video_player,video_player_android,video_player_avfoundation,video_player_web"

PACKAGES="${1:-$ALL_PACKAGES}"

echo "========================================="
echo "PiP Verification Script"
echo "Packages: $PACKAGES"
echo "========================================="

echo ""
echo "--- Step 1: Format Check ---"
$TOOL format --packages "$PACKAGES" || {
  echo "ERROR: Formatting check failed."
  exit 1
}

echo ""
echo "--- Step 2: Static Analysis ---"
$TOOL analyze --packages "$PACKAGES" || {
  echo "ERROR: Static analysis failed."
  exit 1
}

echo ""
echo "--- Step 3: Dart Unit Tests ---"
$TOOL dart-test --packages "$PACKAGES" || {
  echo "ERROR: Dart unit tests failed."
  exit 1
}

echo ""
echo "--- Step 4: Publish Check ---"
$TOOL publish-check --packages "$PACKAGES" || {
  echo "WARNING: Publish check had issues (may be expected during development)."
}

echo ""
echo "--- Step 5: Version Check ---"
$TOOL version-check --packages "$PACKAGES" || {
  echo "WARNING: Version check had issues (may be expected during development)."
}

echo ""
echo "--- Step 6: PiP API Surface Verification ---"

# Verify platform interface has PiP types and methods
PLATFORM_INTERFACE="$REPO_ROOT/packages/video_player/video_player_platform_interface/lib/video_player_platform_interface.dart"
if [ -f "$PLATFORM_INTERFACE" ]; then
  for symbol in PictureInPictureAction PictureInPictureActionType pictureInPictureStarted pictureInPictureStopped isPictureInPictureSupported startPictureInPicture stopPictureInPicture setAutoPictureInPicture setPictureInPictureActions; do
    if grep -q "$symbol" "$PLATFORM_INTERFACE"; then
      echo "  [OK] Platform interface: $symbol found"
    else
      echo "  [MISSING] Platform interface: $symbol NOT found"
    fi
  done
fi

# Verify app-facing package has PiP on VideoPlayerValue and VideoPlayerController
APP_FACING="$REPO_ROOT/packages/video_player/video_player/lib/video_player.dart"
if [ -f "$APP_FACING" ]; then
  for symbol in isPictureInPictureActive startPictureInPicture stopPictureInPicture setAutoPictureInPicture setPictureInPictureActions; do
    if grep -q "$symbol" "$APP_FACING"; then
      echo "  [OK] App-facing: $symbol found"
    else
      echo "  [MISSING] App-facing: $symbol NOT found"
    fi
  done
fi

# Verify Android Dart-side delegation
ANDROID_DART="$REPO_ROOT/packages/video_player/video_player_android/lib/src/android_video_player.dart"
if [ -f "$ANDROID_DART" ]; then
  for symbol in isPictureInPictureSupported startPictureInPicture stopPictureInPicture; do
    if grep -q "$symbol" "$ANDROID_DART"; then
      echo "  [OK] Android Dart: $symbol found"
    else
      echo "  [MISSING] Android Dart: $symbol NOT found"
    fi
  done
fi

# Verify AVFoundation Dart-side delegation
AVFOUNDATION_DART="$REPO_ROOT/packages/video_player/video_player_avfoundation/lib/src/avfoundation_video_player.dart"
if [ -f "$AVFOUNDATION_DART" ]; then
  for symbol in isPictureInPictureSupported startPictureInPicture stopPictureInPicture; do
    if grep -q "$symbol" "$AVFOUNDATION_DART"; then
      echo "  [OK] AVFoundation Dart: $symbol found"
    else
      echo "  [MISSING] AVFoundation Dart: $symbol NOT found"
    fi
  done
fi

# Verify Web Dart-side delegation
WEB_DART="$REPO_ROOT/packages/video_player/video_player_web/lib/video_player_web.dart"
if [ -f "$WEB_DART" ]; then
  for symbol in isPictureInPictureSupported startPictureInPicture stopPictureInPicture; do
    if grep -q "$symbol" "$WEB_DART"; then
      echo "  [OK] Web Dart: $symbol found"
    else
      echo "  [MISSING] Web Dart: $symbol NOT found"
    fi
  done
fi

echo ""
echo "========================================="
echo "Verification complete."
echo "========================================="
