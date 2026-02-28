# video\_player\_windows

The Windows implementation of [`video_player`][1].

## Usage

This package is [endorsed][2], which means you can simply use `video_player`
normally. This package will be automatically included in your app when you do,
so you do not need to add it to your `pubspec.yaml`.

However, if you `import` this package to use any of its APIs directly, you
should add it to your `pubspec.yaml` as usual.

## Limitations

This plugin uses the Windows Media Foundation API, so video format support
depends on the codecs installed on the user's system. Formats playable by
Windows Media Player are generally supported. For additional codec support,
users can install codec packs such as the K-Lite Codec Pack.

[1]: https://pub.dev/packages/video_player
[2]: https://flutter.dev/to/endorsed-federated-plugin
