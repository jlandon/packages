// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:js_interop';

import 'package:web/web.dart' as web;

/// Adds a "disablePictureInPicture" setter to [web.HTMLVideoElement]s.
extension NonStandardSettersOnVideoElement on web.HTMLVideoElement {
  // TODO(srujzs): This will be added in `package:web` 0.6.0. Remove this helper
  // once it's available.
  external set disablePictureInPicture(bool disabled);
}

/// Adds a "disableRemotePlayback" and "controlsList" setters to [web.HTMLMediaElement]s.
extension NonStandardSettersOnMediaElement on web.HTMLMediaElement {
  // TODO(srujzs): This will be added in `package:web` 0.6.0. Remove this helper
  // once it's available.
  external set disableRemotePlayback(bool disabled);
  external set controlsList(String? controlsList);
}

/// Adds PiP support to [web.HTMLVideoElement]s via the Picture-in-Picture API.
extension PictureInPictureOnVideoElement on web.HTMLVideoElement {
  /// Requests Picture-in-Picture mode. Returns a Promise that resolves to
  /// a PictureInPictureWindow.
  external JSPromise requestPictureInPicture();
}

/// Adds PiP support to [web.Document].
extension PictureInPictureOnDocument on web.Document {
  /// Returns the element currently being displayed in PiP mode, or null.
  external web.Element? get pictureInPictureElement;

  /// Whether PiP is available in this document.
  external bool get pictureInPictureEnabled;

  /// Exits Picture-in-Picture mode.
  external JSPromise exitPictureInPicture();
}
