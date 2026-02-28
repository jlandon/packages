// Copyright 2013 The Flutter Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mockito/annotations.dart';
import 'package:mockito/mockito.dart';
import 'package:video_player_platform_interface/video_player_platform_interface.dart';
import 'package:video_player_windows/src/messages.g.dart';
import 'package:video_player_windows/video_player_windows.dart';

import 'windows_video_player_test.mocks.dart';

@GenerateNiceMocks(<MockSpec<Object>>[
  MockSpec<WindowsVideoPlayerApi>(),
  MockSpec<VideoPlayerInstanceApi>(),
])
void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  (WindowsVideoPlayer, MockWindowsVideoPlayerApi, MockVideoPlayerInstanceApi)
  setUpMockPlayer({required int playerId}) {
    final pluginApi = MockWindowsVideoPlayerApi();
    final instanceApi = MockVideoPlayerInstanceApi();
    final player = WindowsVideoPlayer(
      pluginApi: pluginApi,
      playerApiProvider: (_) => instanceApi,
    );
    player.ensurePlayerInitialized(playerId);
    return (player, pluginApi, instanceApi);
  }

  test('registration', () async {
    WindowsVideoPlayer.registerWith();
    expect(VideoPlayerPlatform.instance, isA<WindowsVideoPlayer>());
  });

  group('WindowsVideoPlayer', () {
    test('init', () async {
      final (WindowsVideoPlayer player, MockWindowsVideoPlayerApi api, _) =
          setUpMockPlayer(playerId: 1);
      await player.init();

      verify(api.initialize());
    });

    test('dispose', () async {
      final (WindowsVideoPlayer player, MockWindowsVideoPlayerApi api, _) =
          setUpMockPlayer(playerId: 1);
      await player.dispose(1);

      verify(api.dispose(1));
    });

    test('create with asset', () async {
      final (WindowsVideoPlayer player, MockWindowsVideoPlayerApi api, _) =
          setUpMockPlayer(playerId: 1);
      const newPlayerId = 2;
      when(api.create(any)).thenAnswer((_) async => newPlayerId);

      const asset = 'someAsset';
      const package = 'somePackage';
      const assetUrl = 'C:\\path\\to\\asset';
      when(api.getAssetUrl(asset, package)).thenAnswer((_) async => assetUrl);

      final int? playerId = await player.create(
        DataSource(
          sourceType: DataSourceType.asset,
          asset: asset,
          package: package,
        ),
      );

      final VerificationResult verification = verify(api.create(captureAny));
      final creationOptions = verification.captured[0] as CreationOptions;
      expect(creationOptions.uri, assetUrl);
      expect(playerId, newPlayerId);
      expect(
        player.buildViewWithOptions(VideoViewOptions(playerId: playerId!)),
        isA<Texture>(),
      );
    });

    test('create with network', () async {
      final (WindowsVideoPlayer player, MockWindowsVideoPlayerApi api, _) =
          setUpMockPlayer(playerId: 1);
      const newPlayerId = 2;
      when(api.create(any)).thenAnswer((_) async => newPlayerId);

      const uri = 'https://example.com/video.mp4';
      final int? playerId = await player.create(
        DataSource(sourceType: DataSourceType.network, uri: uri),
      );

      final VerificationResult verification = verify(api.create(captureAny));
      final creationOptions = verification.captured[0] as CreationOptions;
      expect(creationOptions.uri, uri);
      expect(creationOptions.httpHeaders, <String, String>{});
      expect(playerId, newPlayerId);
      expect(
        player.buildViewWithOptions(VideoViewOptions(playerId: playerId!)),
        isA<Texture>(),
      );
    });

    test('create with network passes headers', () async {
      final (WindowsVideoPlayer player, MockWindowsVideoPlayerApi api, _) =
          setUpMockPlayer(playerId: 1);
      when(api.create(any)).thenAnswer((_) async => 2);

      const headers = <String, String>{'Authorization': 'Bearer token'};
      await player.create(
        DataSource(
          sourceType: DataSourceType.network,
          uri: 'https://example.com/video.mp4',
          httpHeaders: headers,
        ),
      );

      final VerificationResult verification = verify(api.create(captureAny));
      final creationOptions = verification.captured[0] as CreationOptions;
      expect(creationOptions.httpHeaders, headers);
    });

    test('create with file', () async {
      final (WindowsVideoPlayer player, MockWindowsVideoPlayerApi api, _) =
          setUpMockPlayer(playerId: 1);
      const newPlayerId = 2;
      when(api.create(any)).thenAnswer((_) async => newPlayerId);

      const fileUri = 'file:///C:/videos/test.mp4';
      final int? playerId = await player.create(
        DataSource(sourceType: DataSourceType.file, uri: fileUri),
      );

      final VerificationResult verification = verify(api.create(captureAny));
      final creationOptions = verification.captured[0] as CreationOptions;
      expect(creationOptions.uri, fileUri);
      expect(playerId, newPlayerId);
      expect(
        player.buildViewWithOptions(VideoViewOptions(playerId: playerId!)),
        isA<Texture>(),
      );
    });

    test('createWithOptions with asset', () async {
      final (WindowsVideoPlayer player, MockWindowsVideoPlayerApi api, _) =
          setUpMockPlayer(playerId: 1);
      const newPlayerId = 2;
      when(api.create(any)).thenAnswer((_) async => newPlayerId);

      const asset = 'someAsset';
      const package = 'somePackage';
      const assetUrl = 'C:\\path\\to\\asset';
      when(api.getAssetUrl(asset, package)).thenAnswer((_) async => assetUrl);

      final int? playerId = await player.createWithOptions(
        VideoCreationOptions(
          dataSource: DataSource(
            sourceType: DataSourceType.asset,
            asset: asset,
            package: package,
          ),
          viewType: VideoViewType.textureView,
        ),
      );

      final VerificationResult verification = verify(api.create(captureAny));
      final creationOptions = verification.captured[0] as CreationOptions;
      expect(creationOptions.uri, assetUrl);
      expect(playerId, newPlayerId);
    });

    test('createWithOptions with network', () async {
      final (WindowsVideoPlayer player, MockWindowsVideoPlayerApi api, _) =
          setUpMockPlayer(playerId: 1);
      const newPlayerId = 2;
      when(api.create(any)).thenAnswer((_) async => newPlayerId);

      const uri = 'https://example.com/video.mp4';
      final int? playerId = await player.createWithOptions(
        VideoCreationOptions(
          dataSource: DataSource(sourceType: DataSourceType.network, uri: uri),
          viewType: VideoViewType.textureView,
        ),
      );

      final VerificationResult verification = verify(api.create(captureAny));
      final creationOptions = verification.captured[0] as CreationOptions;
      expect(creationOptions.uri, uri);
      expect(playerId, newPlayerId);
    });

    test('createWithOptions with file', () async {
      final (WindowsVideoPlayer player, MockWindowsVideoPlayerApi api, _) =
          setUpMockPlayer(playerId: 1);
      const newPlayerId = 2;
      when(api.create(any)).thenAnswer((_) async => newPlayerId);

      const fileUri = 'file:///C:/videos/test.mp4';
      final int? playerId = await player.createWithOptions(
        VideoCreationOptions(
          dataSource: DataSource(sourceType: DataSourceType.file, uri: fileUri),
          viewType: VideoViewType.textureView,
        ),
      );

      final VerificationResult verification = verify(api.create(captureAny));
      final creationOptions = verification.captured[0] as CreationOptions;
      expect(creationOptions.uri, fileUri);
      expect(playerId, newPlayerId);
    });

    test('setLooping', () async {
      final (
        WindowsVideoPlayer player,
        _,
        MockVideoPlayerInstanceApi playerApi,
      ) = setUpMockPlayer(
        playerId: 1,
      );
      await player.setLooping(1, true);

      verify(playerApi.setLooping(true));
    });

    test('play', () async {
      final (
        WindowsVideoPlayer player,
        _,
        MockVideoPlayerInstanceApi playerApi,
      ) = setUpMockPlayer(
        playerId: 1,
      );
      await player.play(1);

      verify(playerApi.play());
    });

    test('pause', () async {
      final (
        WindowsVideoPlayer player,
        _,
        MockVideoPlayerInstanceApi playerApi,
      ) = setUpMockPlayer(
        playerId: 1,
      );
      await player.pause(1);

      verify(playerApi.pause());
    });

    group('setMixWithOthers', () {
      test('passes true', () async {
        final (WindowsVideoPlayer player, MockWindowsVideoPlayerApi api, _) =
            setUpMockPlayer(playerId: 1);
        await player.setMixWithOthers(true);

        verify(api.setMixWithOthers(true));
      });

      test('passes false', () async {
        final (WindowsVideoPlayer player, MockWindowsVideoPlayerApi api, _) =
            setUpMockPlayer(playerId: 1);
        await player.setMixWithOthers(false);

        verify(api.setMixWithOthers(false));
      });
    });

    test('setVolume', () async {
      final (
        WindowsVideoPlayer player,
        _,
        MockVideoPlayerInstanceApi playerApi,
      ) = setUpMockPlayer(
        playerId: 1,
      );
      const volume = 0.7;
      await player.setVolume(1, volume);

      verify(playerApi.setVolume(volume));
    });

    test('setPlaybackSpeed', () async {
      final (
        WindowsVideoPlayer player,
        _,
        MockVideoPlayerInstanceApi playerApi,
      ) = setUpMockPlayer(
        playerId: 1,
      );
      const speed = 1.5;
      await player.setPlaybackSpeed(1, speed);

      verify(playerApi.setPlaybackSpeed(speed));
    });

    test('seekTo', () async {
      final (
        WindowsVideoPlayer player,
        _,
        MockVideoPlayerInstanceApi playerApi,
      ) = setUpMockPlayer(
        playerId: 1,
      );
      const positionMilliseconds = 12345;
      await player.seekTo(
        1,
        const Duration(milliseconds: positionMilliseconds),
      );

      verify(playerApi.seekTo(positionMilliseconds));
    });

    test('getPosition', () async {
      final (
        WindowsVideoPlayer player,
        _,
        MockVideoPlayerInstanceApi playerApi,
      ) = setUpMockPlayer(
        playerId: 1,
      );
      const positionMilliseconds = 12345;
      when(
        playerApi.getCurrentPosition(),
      ).thenAnswer((_) async => positionMilliseconds);

      final Duration position = await player.getPosition(1);
      expect(position, const Duration(milliseconds: positionMilliseconds));
    });

    test('buildViewWithOptions returns Texture', () async {
      final (WindowsVideoPlayer player, _, _) = setUpMockPlayer(playerId: 1);
      final Widget widget = player.buildViewWithOptions(
        const VideoViewOptions(playerId: 1),
      );
      expect(widget, isA<Texture>());
    });

    test('videoEventsFor', () async {
      const playerId = 1;
      final (WindowsVideoPlayer player, _, _) = setUpMockPlayer(
        playerId: playerId,
      );
      const mockChannel = 'flutter.dev/videoPlayer/videoEvents$playerId';
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMessageHandler(mockChannel, (ByteData? message) async {
            final MethodCall methodCall = const StandardMethodCodec()
                .decodeMethodCall(message);
            if (methodCall.method == 'listen') {
              await TestDefaultBinaryMessengerBinding
                  .instance
                  .defaultBinaryMessenger
                  .handlePlatformMessage(
                    mockChannel,
                    const StandardMethodCodec()
                        .encodeSuccessEnvelope(<String, dynamic>{
                          'event': 'initialized',
                          'duration': 98765,
                          'width': 1920,
                          'height': 1080,
                        }),
                    (ByteData? data) {},
                  );

              await TestDefaultBinaryMessengerBinding
                  .instance
                  .defaultBinaryMessenger
                  .handlePlatformMessage(
                    mockChannel,
                    const StandardMethodCodec().encodeSuccessEnvelope(
                      <String, dynamic>{'event': 'completed'},
                    ),
                    (ByteData? data) {},
                  );

              await TestDefaultBinaryMessengerBinding
                  .instance
                  .defaultBinaryMessenger
                  .handlePlatformMessage(
                    mockChannel,
                    const StandardMethodCodec().encodeSuccessEnvelope(
                      <String, dynamic>{
                        'event': 'bufferingUpdate',
                        'values': <List<dynamic>>[
                          <int>[0, 1234],
                          <int>[1235, 4000],
                        ],
                      },
                    ),
                    (ByteData? data) {},
                  );

              await TestDefaultBinaryMessengerBinding
                  .instance
                  .defaultBinaryMessenger
                  .handlePlatformMessage(
                    mockChannel,
                    const StandardMethodCodec().encodeSuccessEnvelope(
                      <String, dynamic>{'event': 'bufferingStart'},
                    ),
                    (ByteData? data) {},
                  );

              await TestDefaultBinaryMessengerBinding
                  .instance
                  .defaultBinaryMessenger
                  .handlePlatformMessage(
                    mockChannel,
                    const StandardMethodCodec().encodeSuccessEnvelope(
                      <String, dynamic>{'event': 'bufferingEnd'},
                    ),
                    (ByteData? data) {},
                  );

              await TestDefaultBinaryMessengerBinding
                  .instance
                  .defaultBinaryMessenger
                  .handlePlatformMessage(
                    mockChannel,
                    const StandardMethodCodec().encodeSuccessEnvelope(
                      <String, dynamic>{
                        'event': 'isPlayingStateUpdate',
                        'isPlaying': true,
                      },
                    ),
                    (ByteData? data) {},
                  );

              await TestDefaultBinaryMessengerBinding
                  .instance
                  .defaultBinaryMessenger
                  .handlePlatformMessage(
                    mockChannel,
                    const StandardMethodCodec().encodeSuccessEnvelope(
                      <String, dynamic>{
                        'event': 'isPlayingStateUpdate',
                        'isPlaying': false,
                      },
                    ),
                    (ByteData? data) {},
                  );

              return const StandardMethodCodec().encodeSuccessEnvelope(null);
            } else if (methodCall.method == 'cancel') {
              return const StandardMethodCodec().encodeSuccessEnvelope(null);
            } else {
              fail('Expected listen or cancel');
            }
          });
      expect(
        player.videoEventsFor(playerId),
        emitsInOrder(<dynamic>[
          VideoEvent(
            eventType: VideoEventType.initialized,
            duration: const Duration(milliseconds: 98765),
            size: const Size(1920, 1080),
          ),
          VideoEvent(eventType: VideoEventType.completed),
          VideoEvent(
            eventType: VideoEventType.bufferingUpdate,
            buffered: <DurationRange>[
              DurationRange(Duration.zero, const Duration(milliseconds: 1234)),
              DurationRange(
                const Duration(milliseconds: 1235),
                const Duration(milliseconds: 1235 + 4000),
              ),
            ],
          ),
          VideoEvent(eventType: VideoEventType.bufferingStart),
          VideoEvent(eventType: VideoEventType.bufferingEnd),
          VideoEvent(
            eventType: VideoEventType.isPlayingStateUpdate,
            isPlaying: true,
          ),
          VideoEvent(
            eventType: VideoEventType.isPlayingStateUpdate,
            isPlaying: false,
          ),
        ]),
      );
    });
  });
}
