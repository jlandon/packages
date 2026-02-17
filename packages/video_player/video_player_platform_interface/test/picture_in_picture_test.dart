// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter_test/flutter_test.dart';
import 'package:video_player_platform_interface/video_player_platform_interface.dart';

void main() {
  group('PictureInPictureActionType', () {
    test('contains all expected values', () {
      expect(PictureInPictureActionType.values.length, 6);
      expect(
        PictureInPictureActionType.values,
        contains(PictureInPictureActionType.play),
      );
      expect(
        PictureInPictureActionType.values,
        contains(PictureInPictureActionType.pause),
      );
      expect(
        PictureInPictureActionType.values,
        contains(PictureInPictureActionType.skipForward),
      );
      expect(
        PictureInPictureActionType.values,
        contains(PictureInPictureActionType.skipBackward),
      );
      expect(
        PictureInPictureActionType.values,
        contains(PictureInPictureActionType.nextTrack),
      );
      expect(
        PictureInPictureActionType.values,
        contains(PictureInPictureActionType.previousTrack),
      );
    });
  });

  group('PictureInPictureAction', () {
    test('equal instances compare equal', () {
      const first = PictureInPictureAction(
        type: PictureInPictureActionType.play,
        label: 'Play',
      );
      const second = PictureInPictureAction(
        type: PictureInPictureActionType.play,
        label: 'Play',
      );

      expect(first, equals(second));
    });

    test('instances with different type are not equal', () {
      const first = PictureInPictureAction(
        type: PictureInPictureActionType.play,
        label: 'Play',
      );
      const second = PictureInPictureAction(
        type: PictureInPictureActionType.pause,
        label: 'Play',
      );

      expect(first, isNot(equals(second)));
    });

    test('instances with different label are not equal', () {
      const first = PictureInPictureAction(
        type: PictureInPictureActionType.play,
        label: 'Play',
      );
      const second = PictureInPictureAction(
        type: PictureInPictureActionType.play,
        label: 'Pause',
      );

      expect(first, isNot(equals(second)));
    });

    test('equal instances have the same hashCode', () {
      const first = PictureInPictureAction(
        type: PictureInPictureActionType.play,
        label: 'Play',
      );
      const second = PictureInPictureAction(
        type: PictureInPictureActionType.play,
        label: 'Play',
      );

      expect(first.hashCode, equals(second.hashCode));
    });

    test('different instances are expected to have different hashCode', () {
      const first = PictureInPictureAction(
        type: PictureInPictureActionType.play,
        label: 'Play',
      );
      const second = PictureInPictureAction(
        type: PictureInPictureActionType.pause,
        label: 'Pause',
      );

      expect(first.hashCode, isNot(equals(second.hashCode)));
    });

    test('toString returns expected format', () {
      const action = PictureInPictureAction(
        type: PictureInPictureActionType.play,
        label: 'Play',
      );

      expect(
        action.toString(),
        'PictureInPictureAction(type: PictureInPictureActionType.play, label: '
        'Play)',
      );
    });
  });

  group('VideoEventType Picture-in-Picture values', () {
    test('contains pictureInPictureStarted and pictureInPictureStopped', () {
      expect(
        VideoEventType.values,
        contains(VideoEventType.pictureInPictureStarted),
      );
      expect(
        VideoEventType.values,
        contains(VideoEventType.pictureInPictureStopped),
      );
      expect(
        VideoEventType.pictureInPictureStarted,
        isNot(equals(VideoEventType.pictureInPictureStopped)),
      );
    });
  });
}
