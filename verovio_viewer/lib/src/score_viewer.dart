import 'package:dotlottie_flutter/dotlottie_flutter.dart';
import 'package:flutter/widgets.dart';

import 'lottie_file_server.dart';
import 'score_pages.dart';

/// Displays a whole-score `.lottie` package (one composition, one layer per
/// page, camera parked on a page's rest frame — see [pageRestFrames]) and
/// handles page navigation.
///
/// Serving/page-rest bookkeeping is internal; playback (play/pause/speed,
/// note highlight events) is not wrapped — get the underlying
/// [DotLottieViewController] via [onControllerReady] and drive it directly,
/// same as [DotLottieView] itself.
class ScoreViewer extends StatefulWidget {
  const ScoreViewer({
    super.key,
    required this.lottiePath,
    this.fit = BoxFit.contain,
    this.speed = 1.0,
    this.onControllerReady,
    this.onReady,
    this.onError,
    this.onPlay,
    this.onPause,
    this.onStop,
    this.onComplete,
  });

  /// Absolute path to a `.lottie` file on disk.
  final String lottiePath;

  final BoxFit fit;

  /// Initial playback speed. Change it later via [controller]/
  /// [onControllerReady]'s `setSpeed` — this only applies on (re)load.
  final double speed;

  /// Called every time the underlying player is (re)created, e.g. after
  /// [lottiePath] changes.
  final ValueChanged<DotLottieViewController>? onControllerReady;

  /// Called once the file is served and the camera is framing
  /// [ScoreViewerState.currentPage].
  final VoidCallback? onReady;

  final void Function(Object error)? onError;

  /// Passed straight through to [DotLottieView]'s own playback callbacks.
  final VoidCallback? onPlay;
  final VoidCallback? onPause;
  final VoidCallback? onStop;
  final VoidCallback? onComplete;

  @override
  ScoreViewerState createState() => ScoreViewerState();
}

class ScoreViewerState extends State<ScoreViewer> {
  final LottieFileServer _server = LottieFileServer();
  DotLottieViewController? _controller;
  List<int> _pageRests = const [0];
  int _currentPage = 0;
  String? _url;

  int get pageCount => _pageRests.length;
  int get currentPage => _currentPage;
  DotLottieViewController? get controller => _controller;

  @override
  void initState() {
    super.initState();
    _load();
  }

  @override
  void didUpdateWidget(ScoreViewer oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.lottiePath != widget.lottiePath) _load();
  }

  @override
  void dispose() {
    _server.close();
    super.dispose();
  }

  Future<void> _load() async {
    try {
      final rests = await pageRestFrames(widget.lottiePath);
      final url = await _server.serveFile(widget.lottiePath);
      if (!mounted) return;
      setState(() {
        _pageRests = rests;
        _currentPage = 0;
        // Fresh URL => fresh Key below => the player reloads the animation.
        _url = url.toString();
      });
    } catch (e) {
      widget.onError?.call(e);
    }
  }

  /// Moves the camera to [page] (clamped to the valid range) by seeking to
  /// its baked rest frame.
  Future<void> goToPage(int page) async {
    final clamped = page.clamp(0, _pageRests.length - 1);
    if (!mounted) return;
    setState(() => _currentPage = clamped);
    await _controller?.setFrame(_pageRests[clamped].toDouble());
  }

  @override
  Widget build(BuildContext context) {
    final url = _url;
    if (url == null) return const SizedBox.shrink();
    return DotLottieView(
      key: ValueKey(url),
      source: url,
      sourceType: 'url',
      // The timeline bakes the page-turn camera: autoplay would drift
      // through pages, so the host frames the current page explicitly.
      autoplay: false,
      loop: false,
      fit: widget.fit,
      speed: widget.speed,
      onViewCreated: (c) {
        _controller = c;
        widget.onControllerReady?.call(c);
      },
      onLoad: () async {
        await _controller?.setFrame(_pageRests[_currentPage].toDouble());
        widget.onReady?.call();
      },
      onLoadError: () => widget.onError?.call(
        StateError('DotLottieView failed to load $url'),
      ),
      onPlay: widget.onPlay,
      onPause: widget.onPause,
      onStop: widget.onStop,
      onComplete: widget.onComplete,
    );
  }
}
