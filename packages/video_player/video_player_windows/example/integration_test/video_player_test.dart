// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:video_player_example/mini_controller.dart';
import 'package:video_player_platform_interface/video_player_platform_interface.dart';
import 'package:video_player_windows/video_player_windows.dart';

const Duration _playDuration = Duration(seconds: 1);

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('can be registered', (_) async {
    WindowsVideoPlayer.registerWith();
    expect(VideoPlayerPlatform.instance, isA<WindowsVideoPlayer>());
  });

  testWidgets('can initialize from network', (WidgetTester tester) async {
    final controller = MiniController.network(
      'https://flutter.github.io/assets-for-api-docs/assets/videos/bee.mp4',
    );
    await controller.initialize();

    expect(controller.value.isInitialized, true);
    expect(controller.value.duration, greaterThan(Duration.zero));
    expect(controller.value.size.width, greaterThan(0));
    expect(controller.value.size.height, greaterThan(0));

    await controller.dispose();
  });

  testWidgets('can initialize from asset', (WidgetTester tester) async {
    final controller = MiniController.asset('assets/Butterfly-209.mp4');
    await controller.initialize();

    expect(controller.value.isInitialized, true);
    expect(controller.value.duration, greaterThan(Duration.zero));

    await controller.dispose();
  });

  testWidgets('can play and pause', (WidgetTester tester) async {
    final controller = MiniController.network(
      'https://flutter.github.io/assets-for-api-docs/assets/videos/bee.mp4',
    );
    await controller.initialize();

    await controller.play();
    await tester.pumpAndSettle();
    await Future<void>.delayed(_playDuration);
    expect(controller.value.isPlaying, true);

    await controller.pause();
    await tester.pumpAndSettle();
    expect(controller.value.isPlaying, false);

    await controller.dispose();
  });

  testWidgets('can seek', (WidgetTester tester) async {
    final controller = MiniController.network(
      'https://flutter.github.io/assets-for-api-docs/assets/videos/bee.mp4',
    );
    await controller.initialize();

    await controller.seekTo(const Duration(seconds: 3));

    expect(
      await controller.position,
      (Duration position) => position >= const Duration(seconds: 3),
    );

    await controller.dispose();
  });
}
